#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/multiplexer.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/tcpserver.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../include/handshake_forger.h"
#include "../include/return_filter.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

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

// Heartbeat interval that keeps a forged session alive; the Guacamole session
// times out without a periodic sync.
constexpr int SYNC_INTERVAL_MS = 1000;

/*
 * @brief Per-channel approval flags, shared between the bridge-recv thread and
 * the per-connection reader threads.
 *
 * The APPROVAL verdict arrives on the bridge-recv thread, but the browser input
 * is forwarded by the per-connection reader thread. This lets the former signal
 * the latter to start piping once a channel is approved.
 */
class ApprovalRegistry {
  public:
    void Create(uint8_t channel) {
        std::lock_guard<std::mutex> lock(mutex);
        flags[channel] = std::make_shared<std::atomic<bool>>(false);
    }
    std::shared_ptr<std::atomic<bool>> Get(uint8_t channel) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = flags.find(channel);
        return it == flags.end() ? nullptr : it->second;
    }
    void Remove(uint8_t channel) {
        std::lock_guard<std::mutex> lock(mutex);
        flags.erase(channel);
    }

  private:
    std::mutex mutex;
    std::unordered_map<uint8_t, std::shared_ptr<std::atomic<bool>>> flags;
};

/*
 * @brief Builds a `sync` instruction with a millisecond timestamp
 */
std::string sync_instruction() {
    long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count();
    std::string ts = std::to_string(ms);
    return "4.sync," + std::to_string(ts.size()) + "." + ts + ";";
}

/*
 * @brief Replays the captured handshake across the bridge as NONE frames
 *
 * Once the forged handshake with the web server is established, the stored
 * client handshake is pushed to gcdbroker (chunked to the payload limit). The
 * forwarded `connect` doubles as the approval request; gcdbroker holds it until
 * an operator decides, then (if approved) hands it to the real guacd.
 */
void replay_handshake(NetQueue &send_queue, uint8_t channel,
                      const std::string &handshake) {
    constexpr size_t CHUNK = Multiplexer::MAX_PAYLOAD_SIZE;
    for (size_t off = 0; off < handshake.size(); off += CHUNK) {
        BridgeMessage msg;
        msg.channel = channel;
        msg.action = ChannelAction::NONE;
        msg.payload = handshake.substr(off, CHUNK);
        send_queue.Enqueue(std::move(msg));
    }
    std::cout << "tcp_reader: channel " << (int)channel << " replayed "
              << handshake.size() << " handshake bytes to the bridge"
              << std::endl;
}

/*
 * @brief Serves one accepted web-server connection
 *
 * Forges the guacd handshake locally (canned args, ready, waiting screen) and
 * does not forward handshake bytes across the bridge. While the session waits
 * for an approval verdict, browser input is dropped and a periodic sync keeps
 * the waiting screen alive; once the channel is approved, browser input is piped
 * across the bridge (and guacd's own sync drives the keepalive). On close it
 * removes the channel; if it was first to remove it (local-initiated close), it
 * announces SHUTDOWN to the peer. The reader is the sole owner of close().
 */
void tcp_reader(TCPServer &tcp_server, ChannelTable &table,
                NetQueue &send_queue, ApprovalRegistry &approvals,
                uint8_t channel, int fd) {
    char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];
    HandshakeForger forger; // forges the guacd handshake toward the web server
    std::shared_ptr<std::atomic<bool>> approved = approvals.Get(channel);

    while (running) {
        // Poll so we can emit a heartbeat (4.sync,...;) even when the web server is idle.
        int ready = tcp_server.WaitReadable(fd, SYNC_INTERVAL_MS);
        if (ready < 0)
            // TODO: error handling
            break;
        // If timeout occurred
        if (ready == 0) {
            // Keep the waiting screen alive only until approval; afterwards
            // guacd's own sync drives the keepalive.
            if (forger.GetHandshakeState() == HandshakeState::ESTABLISHED &&
                !(approved && approved->load())) {
                std::string s = sync_instruction();
                tcp_server.Send(fd, s.data(), s.size());
            }
            continue;
        }

        // If no timeout occurred, then there is data waiting
        int received = tcp_server.Receive(fd, buffer, sizeof(buffer));
        if (received <= 0)
            break; // 0: client closed, <0: error

        // Until the forged handshake is established, gmlbroker answers the web
        // server itself and forwards nothing across the bridge.
        if (forger.GetHandshakeState() != HandshakeState::ESTABLISHED) {
            std::string reply = forger.Feed(buffer, received);
            if (!reply.empty())
                tcp_server.Send(fd, reply.data(), reply.size());

            // On the transition to ESTABLISHED, replay the captured handshake
            // across the bridge so gcdbroker can request operator approval.
            if (forger.GetHandshakeState() == HandshakeState::ESTABLISHED)
                replay_handshake(send_queue, channel, forger.Handshake());
            continue;
        }

        // Established: once approved, pipe browser input across the bridge.
        // Before approval the waiting screen stands and input is dropped.
        if (approved && approved->load()) {
            BridgeMessage msg;
            msg.channel = channel;
            msg.action = ChannelAction::NONE;
            msg.payload.assign(buffer, received);
            send_queue.Enqueue(std::move(msg));
        }
    }

    approvals.Remove(channel);

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
                    NetQueue &send_queue, ApprovalRegistry &approvals) {
    while (running) {
        int fd = tcp_server.Accept();
        if (fd < 0) {
            if (running)
                continue;
            break;
        }

        // Try to allocate the lowest channel not yet taken
        std::optional<uint8_t> channel = table.Allocate(fd);
        if (!channel) {
            std::cerr << "accept_handler: channel table full, rejecting client"
                      << std::endl;
            tcp_server.Close(fd);
            continue;
        }

        // Register the channel's approval flag before its reader can run.
        approvals.Create(*channel);

        // Announce the new channel to the bridge before any of its data
        BridgeMessage create;
        create.channel = *channel;
        create.action = ChannelAction::CREATE_CHANNEL;
        send_queue.Enqueue(std::move(create));
        std::cout << "accept_handler: new channel " << (int)*channel << " (fd "
                  << fd << "), sent CREATE" << std::endl;

        // Create a thread for reading one specific channel.
        // Detach it, so the accept_handler thread can keep accepting connections.
        std::thread(tcp_reader, std::ref(tcp_server), std::ref(table),
                    std::ref(send_queue), std::ref(approvals), *channel, fd)
            .detach();
    }
}

/*
 * @brief Routes bridge messages to the right client socket by channel
 */
void tcp_send_handler(TCPServer &tcp_server, ChannelTable &table,
                      NetQueue &recv_queue, ApprovalRegistry &approvals) {
    // Per-channel return-path filter that swallows guacd's real args/ready.
    std::unordered_map<uint8_t, ReturnFilter> filters;

    while (running) {
        BridgeMessage msg = recv_queue.Dequeue();

        switch (msg.action) {
        case ChannelAction::SHUTDOWN_CHANNEL: {
            filters.erase(msg.channel);
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
        case ChannelAction::APPROVAL: {
            char verdict = msg.payload.empty() ? APPROVAL_DENY : msg.payload[0];
            if (verdict == APPROVAL_APPROVE) {
                // Let the reader start piping browser input across the bridge,
                // and arm the return filter to swallow guacd's real args/ready.
                if (auto flag = approvals.Get(msg.channel))
                    flag->store(true);
                filters[msg.channel] = ReturnFilter{};
                std::cout << "tcp_send_handler: channel " << (int)msg.channel
                          << " APPROVED" << std::endl;
            } else {
                std::string reason = msg.payload.size() > 1
                                         ? msg.payload.substr(1)
                                         : std::string();
                std::cout << "tcp_send_handler: channel " << (int)msg.channel
                          << " DENIED (" << reason << ")" << std::endl;
                // Paint the denied screen, then wake the reader to tear down.
                if (auto fd = table.Get(msg.channel)) {
                    std::string screen = HandshakeForger::DeniedScreen();
                    tcp_server.Send(*fd, screen.data(), screen.size());
                    tcp_server.Shutdown(*fd);
                }
            }
            break;
        }
        case ChannelAction::NONE:
        default: {
            std::optional<int> fd = table.Get(msg.channel);
            if (!fd) {
                std::cerr << "tcp_send_handler: no socket for channel "
                          << (int)msg.channel << ", dropping "
                          << msg.payload.size() << " bytes" << std::endl;
                break;
            }
            // Swallow guacd's real args/ready before piping to the web server.
            std::string out = msg.payload;
            auto fit = filters.find(msg.channel);
            if (fit != filters.end())
                out = fit->second.Feed(msg.payload.data(), msg.payload.size());
            if (out.empty())
                break; // fully swallowed (handshake reply)
            if (tcp_server.Send(*fd, out.data(), out.size()) < 0) {
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
    // + 1 for null byte - c-style strings
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

    ChannelTable table; // Shared by accept thread and tcp_send thread to keep track of connections
    ApprovalRegistry approvals; // Per-channel approval flags
    auto recv_queue = NetQueue();
    auto send_queue = NetQueue();

    // Start threads
    std::thread t_accept(accept_handler, std::ref(tcp_server), std::ref(table),
                         std::ref(send_queue), std::ref(approvals));
    std::thread t_tcp_send(tcp_send_handler, std::ref(tcp_server),
                           std::ref(table), std::ref(recv_queue),
                           std::ref(approvals));
    std::thread t_udp_send(udp_send_handler, std::ref(udp_sender),
                           std::ref(send_queue));
    std::thread t_udp_recv(udp_recv_handler, std::ref(udp_receiver),
                           std::ref(recv_queue));

    t_accept.join();
    t_tcp_send.join();
    t_udp_recv.join();
    t_udp_send.join();
}
