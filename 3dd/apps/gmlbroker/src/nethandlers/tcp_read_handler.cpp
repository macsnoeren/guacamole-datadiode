#include "../../include/nethandlers/tcp_read_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/handshake_forger.h"
#include "../../include/running.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>

namespace {

// Heartbeat interval that keeps a forged session alive; the Guacamole session
// times out without a periodic sync.
constexpr int SYNC_INTERVAL_MS = 1000;

/*
 * @brief A unique, inert connection-request identifier (12 hex chars).
 *
 * Sent as the CREATE payload to request approval. Deliberately not derived from
 * Guacamole traffic, and NUL-free so it logs cleanly.
 */
std::string make_request_id() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> hex(0, 15);
    const char *digits = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 12; ++i)
        id += digits[hex(rng)];
    return id;
}

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

} // namespace

/*
 * @brief Serves one accepted web-server connection
 *
 * Forges the guacd handshake locally (canned args, ready, waiting screen) and
 * forwards no Guacamole bytes across the bridge until the connection is
 * approved. Once the forged handshake is established it sends an inert CREATE
 * (carrying a unique request id) as the approval request, and keeps the waiting
 * screen alive with a periodic sync. Only after the matching APPROVAL verdict
 * does it replay the captured handshake and pipe browser input across the bridge
 * (guacd's own sync then drives the keepalive). On close it removes the channel;
 * if it was first to remove it, it announces SHUTDOWN. The reader is the sole
 * owner of close().
 */
std::thread TCPReadHandler::Run(NetQueue &queue, TCPServer &tcp_server,
                                ChannelTable &table, ApprovalRegistry &approvals,
                                uint8_t channel, int fd) {
    return std::thread([&queue, &tcp_server, &table, &approvals, channel, fd]() {
        char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];
        HandshakeForger forger; // forges the guacd handshake toward the web server
        std::shared_ptr<std::atomic<bool>> approved = approvals.Flag(channel);
        bool replayed = false;

        // Once approved, replay the captured handshake across the bridge exactly
        // once. The guard validates it en route; gcdbroker forwards it to guacd.
        auto maybe_replay = [&]() {
            if (!replayed &&
                forger.GetHandshakeState() == HandshakeState::ESTABLISHED &&
                approved && approved->load()) {
                replay_handshake(queue, channel, forger.Handshake());
                replayed = true;
            }
        };

        while (running) {
            // Poll so we can emit a heartbeat (4.sync,...;) even when the web
            // server is idle.
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
                maybe_replay();
                continue;
            }

            // If no timeout occurred, then there is data waiting
            int received = tcp_server.Receive(fd, buffer, sizeof(buffer));
            if (received <= 0)
                break; // 0: client closed, <0: error

            // Until the forged handshake is established, gmlbroker answers the
            // web server itself and forwards nothing across the bridge.
            if (forger.GetHandshakeState() != HandshakeState::ESTABLISHED) {
                std::string reply = forger.Feed(buffer, received);
                if (!reply.empty())
                    tcp_server.Send(fd, reply.data(), reply.size());

                // A corrupted handshake can no longer be parsed: stop this
                // reader. The teardown below announces SHUTDOWN and closes fd.
                if (forger.GetHandshakeState() ==
                    HandshakeState::INVALID_HANDSHAKE) {
                    std::cerr << "tcp_reader: channel " << (int)channel
                              << " handshake corrupted, closing" << std::endl;
                    break;
                }

                // On the transition to ESTABLISHED, request approval with an
                // inert CREATE carrying a unique id — no Guacamole traffic
                // crosses yet.
                if (forger.GetHandshakeState() == HandshakeState::ESTABLISHED) {
                    std::string req_id = make_request_id();
                    approvals.SetRequestId(channel, req_id);
                    BridgeMessage create;
                    create.channel = channel;
                    create.action = ChannelAction::CREATE_CHANNEL;
                    create.payload = req_id;
                    queue.Enqueue(std::move(create));
                    std::cout << "tcp_reader: channel " << (int)channel
                              << " requesting approval (id " << req_id << ")"
                              << std::endl;
                }
                continue;
            }

            // Established: replay once approved, then pipe browser input across
            // the bridge. Before approval the waiting screen stands and input is
            // dropped.
            maybe_replay();
            if (replayed) {
                BridgeMessage msg;
                msg.channel = channel;
                msg.action = ChannelAction::NONE;
                msg.payload.assign(buffer, received);
                queue.Enqueue(std::move(msg));
            }
        }

        approvals.Remove(channel);

        // Only the side that initiates the close announces SHUTDOWN to the peer
        if (table.Remove(channel).has_value()) {
            BridgeMessage shutdown;
            shutdown.channel = channel;
            shutdown.action = ChannelAction::SHUTDOWN_CHANNEL;
            queue.Enqueue(std::move(shutdown));
            std::cout << "tcp_reader: channel " << (int)channel
                      << " closed locally, sent SHUTDOWN" << std::endl;
        }
        tcp_server.Close(fd);
    });
}
