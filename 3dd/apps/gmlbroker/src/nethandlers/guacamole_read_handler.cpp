#include "../../include/nethandlers/guacamole_read_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/clipboard_ack_faker.h"
#include "../../include/forward_keepalive_filter.h"
#include "../../include/handshake_forger.h"
#include "../../include/running.h"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <memory>
#include <poll.h>
#include <random>
#include <string>
#include <vector>

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
void replay_handshake(NetQueue &send_queue, uint16_t channel,
                      const std::string &handshake) {
    constexpr size_t CHUNK = Multiplexer::MAX_PAYLOAD_SIZE;
    for (size_t off = 0; off < handshake.size(); off += CHUNK) {
        BridgeMessage msg;
        msg.channel = channel;
        msg.action = ChannelAction::NONE;
        msg.payload = handshake.substr(off, CHUNK);
        send_queue.Enqueue(std::move(msg));
    }
    std::cout << "guacamole_reader: channel " << (int)channel << " replayed "
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
std::thread GuacamoleReadHandler::Run(NetQueue &queue, NetQueue &recv_queue,
                                GuacamoleServer &guacamole_server,
                                ChannelTable &table, ApprovalRegistry &approvals,
                                MailboxRegistry &mailboxes, ReaderGroup &readers,
                                uint16_t channel, int fd) {
    return std::thread([&queue, &recv_queue, &guacamole_server, &table, &approvals, &mailboxes, &readers, channel, fd]() {
        // Declared first so it is destroyed last: Leave() runs only after all
        // shared-state access below is done, letting main's WaitAll() proceed.
        ReaderGroup::Sentinel sentinel(readers);

        char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];
        HandshakeForger forger; // forges the guacd handshake toward the web server
        ForwardKeepaliveFilter keepalive_filter; // swallows the browser's sync/nop keepalives
        ClipboardAckFaker clipboard_faker; // fakes acks for guard-dropped clipboard blobs
        std::shared_ptr<std::atomic<bool>> approved = approvals.Flag(channel);
        std::shared_ptr<ChannelMailbox> mailbox = mailboxes.Get(channel);
        bool replayed = false;
        // Whether to announce SHUTDOWN to the peer on close. Stays true for any
        // locally-initiated teardown (client close, error, denial); flipped off
        // only when the peer's own SHUTDOWN drove the teardown (no echo).
        bool announce = true;

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
            // Poll the socket and the mailbox together: the socket carries
            // browser input, the mailbox carries return traffic / teardown
            // requests from the guacamole_send thread. A timeout lets us emit a
            // heartbeat (4.sync,...;) even when both are idle. All socket writes
            // happen here, on this one thread.
            // Under TLS, OpenSSL may hold a fully-buffered record the socket
            // poll won't report, so don't block when there's pending plaintext.
            bool ssl_pending = guacamole_server.HasPending(fd);

            struct pollfd pfds[2];
            pfds[0].fd = fd;
            pfds[0].events = POLLIN;
            pfds[0].revents = 0;
            pfds[1].fd = mailbox ? mailbox->WakeFd() : -1;
            pfds[1].events = POLLIN;
            pfds[1].revents = 0;

            int ready = ::poll(pfds, 2, ssl_pending ? 0 : SYNC_INTERVAL_MS);
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                perror("poll");
                break;
            }

            // Outbound: drain return traffic the send thread handed us, writing
            // it to the browser ourselves, and honour a teardown request.
            if (mailbox && (pfds[1].revents & POLLIN)) {
                std::vector<std::string> chunks;
                bool teardown = false, do_announce = true;
                mailbox->Drain(chunks, teardown, do_announce);
                bool write_failed = false;
                for (const std::string &chunk : chunks) {
                    if (guacamole_server.Send(fd, chunk.data(), chunk.size()) < 0) {
                        write_failed = true;
                        break;
                    }
                }
                if (teardown) {
                    announce = do_announce;
                    break;
                }
                if (write_failed)
                    break; // announce stays true: tell the peer the browser died
            }

            // Readable if the socket signalled or TLS has a buffered record.
            bool readable =
                (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) || ssl_pending;
            if (!readable) {
                // Genuine idle timeout (no socket event, nothing buffered).
                if (ready == 0) {
                    // Keep the waiting screen alive only until approval;
                    // afterwards guacd's own sync drives the keepalive.
                    if (forger.GetHandshakeState() == HandshakeState::ESTABLISHED &&
                        !(approved && approved->load())) {
                        std::string s = sync_instruction();
                        guacamole_server.Send(fd, s.data(), s.size());
                    }
                    maybe_replay();
                }
                continue;
            }

            // There is data waiting from the browser
            int received = guacamole_server.Receive(fd, buffer, sizeof(buffer));
            if (received == GuacamoleServer::RETRY)
                continue; // TLS handshake/partial record: nothing yet, retry
            if (received <= 0)
                break; // 0: client closed, <0: error

            // Until the forged handshake is established, gmlbroker answers the
            // web server itself and forwards nothing across the bridge.
            if (forger.GetHandshakeState() != HandshakeState::ESTABLISHED) {
                std::string reply = forger.Feed(buffer, received);
                if (!reply.empty())
                    guacamole_server.Send(fd, reply.data(), reply.size());

                // A corrupted handshake can no longer be parsed: stop this
                // reader. The teardown below announces SHUTDOWN and closes fd.
                if (forger.GetHandshakeState() ==
                    HandshakeState::INVALID_HANDSHAKE) {
                    std::cerr << "guacamole_reader: channel " << (int)channel
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
                    std::cout << "guacamole_reader: channel " << (int)channel
                              << " requesting approval"
                              << std::endl;
                }
                continue;
            }

            // Established: replay once approved, then pipe browser input across
            // the bridge. Before approval the waiting screen stands and input is
            // dropped.
            maybe_replay();
            if (replayed) {
                // For a clipboard paste the guard will drop (payload over the
                // cap), fake the success ack back to the browser so its clipboard
                // stream doesn't stall waiting for guacd. Read the original bytes
                // before the keepalive filter rewrites the buffer.
                std::string acks = clipboard_faker.Feed(buffer, received);
                if (!acks.empty()) {
                    BridgeMessage ack{channel, ChannelAction::NONE, std::move(acks)};
                    recv_queue.Enqueue(std::move(ack));
                }

                // Swallow the browser's keepalives (sync/nop) here so they never
                // cross the bridge; the guard validates the rest.
                size_t len = static_cast<size_t>(received);
                keepalive_filter.Filter(buffer, len);
                if (len > 0) {
                    BridgeMessage msg;
                    msg.channel = channel;
                    msg.action = ChannelAction::NONE;
                    msg.payload.assign(buffer, len);
                    queue.Enqueue(std::move(msg));
                }
            }
        }

        approvals.Remove(channel);
        mailboxes.Remove(channel);
        table.Remove(channel);

        // The reader is the only thread that removes the channel, so `announce`
        // alone decides whether to notify the peer: true for a locally-initiated
        // close, false when the peer's own SHUTDOWN drove this teardown.
        if (announce) {
            BridgeMessage shutdown;
            shutdown.channel = channel;
            shutdown.action = ChannelAction::SHUTDOWN_CHANNEL;
            queue.Enqueue(std::move(shutdown));
            std::cout << "guacamole_reader: channel " << (int)channel
                      << " closed locally, sent SHUTDOWN" << std::endl;
        }
        guacamole_server.Close(fd);
    });
}
