#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/multiplexer.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/tcpserver.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include <atomic>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>

const char *guac_listen_ip;
int guac_listen_port;
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
 * @brief Reads one client's TCP stream and wraps each read as a NONE message
 *
 * Runs once per accepted client. On close it removes the channel; if it was the
 * first to remove it (local-initiated close), it announces SHUTDOWN to the peer.
 * The reader is the sole owner of close() for its fd.
 */
void tcp_reader(TCPServer &tcp_server, ChannelTable &table,
                NetQueue &send_queue, uint8_t channel, int fd) {
    char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];

    while (running) {
        int received = tcp_server.Receive(fd, buffer, sizeof(buffer));
        if (received <= 0)
            break; // 0: client closed, <0: error

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
                  << " closed locally, sent SHUTDOWN" << std::endl;
    }
    tcp_server.Close(fd);
}

/*
 * @brief Accepts Guacamole connections, allocating a channel for each
 */
void accept_handler(TCPServer &tcp_server, ChannelTable &table,
                    NetQueue &send_queue) {
    while (running) {
        int fd = tcp_server.Accept();
        if (fd < 0) {
            if (running)
                continue;
            break;
        }

        // Try to allocate a channel
        std::optional<uint8_t> channel = table.Allocate(fd);
        if (!channel) {
            std::cerr << "accept_handler: channel table full, rejecting client"
                      << std::endl;
            tcp_server.Close(fd);
            continue;
        }

        // Announce the new channel to the bridge before any of its data
        BridgeMessage create;
        create.channel = *channel;
        create.action = ChannelAction::CREATE_CHANNEL;
        send_queue.Enqueue(std::move(create));
        std::cout << "accept_handler: new channel " << (int)*channel << " (fd "
                  << fd << "), sent CREATE" << std::endl;

        std::thread(tcp_reader, std::ref(tcp_server), std::ref(table),
                    std::ref(send_queue), *channel, fd)
            .detach();
    }
}

/*
 * @brief Routes bridge messages to the right client socket by channel
 */
void tcp_send_handler(TCPServer &tcp_server, ChannelTable &table,
                      NetQueue &recv_queue) {
    while (running) {
        BridgeMessage msg = recv_queue.Dequeue();

        switch (msg.action) {
        case ChannelAction::SHUTDOWN_CHANNEL: {
            // Remove reference to channel
            std::optional<int> fd = table.Remove(msg.channel);
            if (fd) {
                tcp_server.Shutdown(*fd); // wakes the reader, which closes it
                std::cout << "tcp_send_handler: channel " << (int)msg.channel
                          << " SHUTDOWN from peer" << std::endl;
            }
            break;
        }
        case ChannelAction::CREATE_CHANNEL:
            // gmlbroker is the allocator; it never receives CREATE
            break;
        case ChannelAction::NONE:
        default: {
            std::optional<int> fd = table.Get(msg.channel);
            if (!fd) {
                std::cerr << "tcp_send_handler: no socket for channel "
                          << (int)msg.channel << ", dropping "
                          << msg.payload.size() << " bytes" << std::endl;
                break;
            }
            if (tcp_server.Send(*fd, msg.payload.data(), msg.payload.size()) <
                0) {
                std::optional<int> dead = table.Remove(msg.channel);
                if (dead)
                    tcp_server.Shutdown(*dead);
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
 * @brief Starts the Guacamole broker that imitates the guacd program and
 * bridges Guacamole traffic.
 *
 * Accepts multiple Guacamole connections, multiplexing each onto its own
 * channel over a single UDP bridge. Handlers run on separate threads and
 * synchronize messages using thread-safe queues.
 */
int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "\t<guac_listen_ip>: Guacamole broker's listening IP "
                     "address (for the web server)\n"
                  << "\t<guac_listen_port>: Guacamole broker's listening port\n"
                  << "\t<udp_recv_ip>: address where the broker receives "
                     "traffic from (lrx_proxy)\n"
                  << "\t<udp_recv_port>: port where the broker receives "
                     "traffic from\n"
                  << "\t<udp_send_ip>: address where the broker sends guacd "
                     "traffic to (the guard)\n"
                  << "\t<udp_send_port>: port where the broker sends guacd "
                     "traffic to\n"
                  << "\nExample: " << argv[0]
                  << " 127.0.0.1 4822 0.0.0.0 5501 10.0.0.2 5601" << std::endl;
        return 1;
    }

    guac_listen_ip = argv[1];
    guac_listen_port = std::stoi(argv[2]);
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
    auto tcp_server = TCPServer(guac_listen_ip, guac_listen_port);
    if ((exit = tcp_server.Initialize()) != 0)
        return exit;

    std::cout << "Listening on TCP port " << guac_listen_port << "..."
              << std::endl;

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

    ChannelTable table;
    auto recv_queue = NetQueue();
    auto send_queue = NetQueue();

    // Start threads
    std::thread t_accept(accept_handler, std::ref(tcp_server), std::ref(table),
                         std::ref(send_queue));
    std::thread t_tcp_send(tcp_send_handler, std::ref(tcp_server),
                           std::ref(table), std::ref(recv_queue));
    std::thread t_udp_send(udp_send_handler, std::ref(udp_sender),
                           std::ref(send_queue));
    std::thread t_udp_recv(udp_recv_handler, std::ref(udp_receiver),
                           std::ref(recv_queue));

    t_accept.join();
    t_tcp_send.join();
    t_udp_recv.join();
    t_udp_send.join();
}
