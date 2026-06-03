#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/multiplexer.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/tcpclient.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../include/approver.h"
#include <atomic>
#include <iostream>
#include <optional>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>

const char *guacd_ip;
int guacd_port;
const char *udp_recv_ip;
int udp_recv_port;
const char *udp_send_ip;
int udp_send_port;
std::atomic<bool> running = true;

/*
 * @brief Signals all threads to stop when an interrupt signal is received
 */
void interrupt_handler(int signum) {
    std::cout << "Stopping program..." << std::endl;
    running = false;
}

/*
 * @brief Reads one guacd connection and wraps each read as a NONE message
 *
 * Runs once per channel. On close it removes the channel; if it was the first
 * to remove it (guacd-initiated close), it announces SHUTDOWN to the peer. The
 * reader is the sole owner of close() for its fd.
 */
void tcp_reader(TCPClient &tcp_client, ChannelTable &table,
                NetQueue &send_queue, uint8_t channel, int fd) {
    char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];

    while (running) {
        int received = tcp_client.Receive(fd, buffer, sizeof(buffer));
        if (received <= 0)
            break; // 0: guacd closed, <0: error

        BridgeMessage msg;
        msg.channel = channel;
        msg.action = ChannelAction::NONE;
        msg.payload.assign(buffer, received);
        send_queue.Enqueue(std::move(msg));

        std::stringstream info;
        info << "tcp_reader: queued " << received << " bytes on channel "
             << (int)channel << std::endl;
        std::cout << info.str();
    }

    // Only the side that initiates the close announces SHUTDOWN to the peer
    if (table.Remove(channel).has_value()) {
        BridgeMessage shutdown;
        shutdown.channel = channel;
        shutdown.action = ChannelAction::SHUTDOWN_CHANNEL;
        send_queue.Enqueue(std::move(shutdown));
        std::cout << "tcp_reader: channel " << (int)channel
                  << " closed by guacd, sent SHUTDOWN" << std::endl;
    }
    tcp_client.Close(fd);
}

/*
 * @brief Sends an APPROVAL verdict for a channel back on the return path
 *
 * Payload byte 0 is the printable verdict ('A'/'D'); the inert request id
 * follows, so gmlbroker can match the verdict to its outstanding request. This
 * rides the return path (bypassing the guard) straight back to gmlbroker.
 */
void send_verdict(NetQueue &send_queue, uint8_t channel, char verdict,
                  const std::string &request_id) {
    BridgeMessage msg;
    msg.channel = channel;
    msg.action = ChannelAction::APPROVAL;
    msg.payload.push_back(verdict);
    msg.payload.append(request_id);
    send_queue.Enqueue(std::move(msg));
}

/*
 * @brief Routes bridge messages to guacd connections by channel, gating on
 * approval.
 *
 * The approval request is the inert CREATE payload (a unique id), never
 * Guacamole traffic — so no attacker-influenced bytes are parsed before a human
 * authorizes the connection. guacd is dialed only on approval. Any NONE traffic
 * for a channel that is not approved is dropped without inspection, so no
 * Guacamole reaches guacd before the verdict. Once approved, NONE traffic is
 * forwarded to guacd untouched (the guard validated it en route).
 */
void tcp_send_handler(TCPClient &tcp_client, ChannelTable &table,
                      NetQueue &recv_queue, NetQueue &send_queue) {
    Approver approver;

    while (running) {
        BridgeMessage msg = recv_queue.Dequeue();

        switch (msg.action) {
        case ChannelAction::CREATE_CHANNEL: {
            // The CREATE payload is the inert approval request id; decide on it.
            const std::string &request_id = msg.payload;
            ApprovalResult verdict = approver.HandleRequest(request_id);
            if (!verdict.approved) {
                send_verdict(send_queue, msg.channel, APPROVAL_DENY, request_id);
                std::cout << "tcp_send_handler: channel " << (int)msg.channel
                          << " DENIED (" << verdict.reason << ")" << std::endl;
                break;
            }

            // Approved: dial guacd now, ready to receive the handshake replay.
            int fd = tcp_client.Connect();
            if (fd < 0 || !table.Insert(msg.channel, fd)) {
                if (fd >= 0)
                    tcp_client.Close(fd);
                std::cerr << "tcp_send_handler: channel " << (int)msg.channel
                          << " approved but guacd dial failed" << std::endl;
                send_verdict(send_queue, msg.channel, APPROVAL_DENY, request_id);
                break;
            }
            std::thread(tcp_reader, std::ref(tcp_client), std::ref(table),
                        std::ref(send_queue), msg.channel, fd)
                .detach();
            send_verdict(send_queue, msg.channel, APPROVAL_APPROVE, request_id);
            std::cout << "tcp_send_handler: channel " << (int)msg.channel
                      << " APPROVED, dialed guacd (fd " << fd << ")"
                      << std::endl;
            break;
        }

        case ChannelAction::SHUTDOWN_CHANNEL: {
            std::optional<int> fd = table.Remove(msg.channel);
            if (fd) {
                tcp_client.Shutdown(*fd); // wakes the reader, which closes it
                std::cout << "tcp_send_handler: channel " << (int)msg.channel
                          << " SHUTDOWN from peer" << std::endl;
            }
            break;
        }

        case ChannelAction::NONE:
        default: {
            // Forward only to approved (dialed) channels. Anything else is
            // dropped uninspected — no Guacamole reaches guacd before approval.
            std::optional<int> fd = table.Get(msg.channel);
            if (!fd) {
                std::cerr << "tcp_send_handler: channel " << (int)msg.channel
                          << " not approved, dropping " << msg.payload.size()
                          << " bytes" << std::endl;
                break;
            }
            if (tcp_client.Send(*fd, msg.payload.data(), msg.payload.size()) <
                0) {
                std::optional<int> dead = table.Remove(msg.channel);
                if (dead)
                    tcp_client.Shutdown(*dead);
            }
            break;
        }
        }
    }
}

/*
 * @brief Receives datagrams from the bridge and queues the parsed messages
 */
void udp_recv_handler(UDPReceiver &udp_receiver, NetQueue &recv_queue) {
    char buffer[Multiplexer::MAX_DATAGRAM_SIZE + 1];

    while (running) {
        int received = udp_receiver.Receive(buffer, sizeof(buffer));
        if (received <= 0)
            continue;

        BridgeMessage msg;
        if (!Multiplexer::TryCast(buffer, received, msg)) {
            std::cerr << "udp_recv_handler: dropped malformed datagram ("
                      << received << " bytes)" << std::endl;
            continue;
        }

        recv_queue.Enqueue(std::move(msg));
    }
}

/*
 * @brief Serializes queued messages and sends them on the bridge
 */
void udp_send_handler(UDPSender &udp_sender, NetQueue &send_queue) {
    while (running) {
        BridgeMessage msg = send_queue.Dequeue();
        std::string wire = Multiplexer::Serialize(msg);
        udp_sender.Send(wire.data(), wire.size());

        std::stringstream info;
        info << "udp_send_handler: sent " << wire.size() << " bytes on channel "
             << (int)msg.channel << std::endl;
        std::cout << info.str();
    }
}

/*
 * @brief Starts the guacd broker that imitates the Guacamole web server and
 * bridges Guacamole traffic.
 *
 * Reacts to channel lifecycle messages from the bridge, opening one guacd
 * connection per channel. Handlers run on separate threads and synchronize
 * messages using thread-safe queues.
 */
int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "\t<guacd_ip>: guacd's IP address\n"
                  << "\t<guacd_port>: guacd's listening port\n"
                  << "\t<udp_recv_ip>: address where the broker receives traffic from (hrx_proxy)\n"
                  << "\t<udp_recv_port>: port where the broker receives traffic from\n"
                  << "\t<udp_send_ip>: address where the broker sends guacd traffic to (lrx_proxy)\n"
                  << "\t<udp_send_port>: port where the broker sends guacd traffic to\n"
                  << "\nExample: " << argv[0]
                  << " 127.0.0.1 4822 0.0.0.0 5501 10.0.0.2 5601" << std::endl;
        return 1;
    }

    guacd_ip = argv[1];
    guacd_port = std::stoi(argv[2]);
    udp_recv_ip = argv[3];
    udp_recv_port = std::stoi(argv[4]);
    udp_send_ip = argv[5];
    udp_send_port = std::stoi(argv[6]);

    // Set interrupt handler
    struct sigaction sa{};
    sa.sa_handler = interrupt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    // Initialize UDP and TCP infrastructure
    int exit;
    UDPReceiver udp_receiver(udp_recv_port);
    if ((exit = udp_receiver.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP receiver on port " << udp_recv_port
              << std::endl;

    UDPSender udp_sender(udp_send_ip, udp_send_port);
    if ((exit = udp_sender.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP sender for " << udp_send_ip << ":"
              << udp_send_port << std::endl;

    auto tcp_client = TCPClient(guacd_ip, guacd_port);
    ChannelTable table;
    auto recv_queue = NetQueue();
    auto send_queue = NetQueue();

    // Start threads
    std::thread t_tcp_send(tcp_send_handler, std::ref(tcp_client),
                           std::ref(table), std::ref(recv_queue),
                           std::ref(send_queue));
    std::thread t_udp_send(udp_send_handler, std::ref(udp_sender),
                           std::ref(send_queue));
    std::thread t_udp_recv(udp_recv_handler, std::ref(udp_receiver),
                           std::ref(recv_queue));

    t_tcp_send.join();
    t_udp_recv.join();
    t_udp_send.join();
}
