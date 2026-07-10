#include "../../include/nethandlers/guacamole_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/handshake_forger.h"
#include "../../include/return_filter.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

/*
 * @brief Routes bridge messages to the right client connection by channel.
 *
 * This thread never touches a socket: the per-channel reader owns all socket
 * I/O. Return traffic and teardown requests are handed to the channel's
 * ChannelMailbox, which wakes the reader to do the actual write/close on its
 * own thread.
 */
std::thread GuacamoleSendHandler::Run(NetQueue &queue, MailboxRegistry &mailboxes,
                                ApprovalRegistry &approvals) {
    return std::thread([&queue, &mailboxes, &approvals]() {
        // Per-channel return-path filter that swallows guacd's real args/ready.
        std::unordered_map<uint16_t, ReturnFilter> filters;

        while (running) {
            std::optional<BridgeMessage> opt = queue.Dequeue();
            if (!opt)
                break; // queue closed and drained: shutting down
            BridgeMessage msg = std::move(*opt);

            switch (msg.action) {
            case ChannelAction::SHUTDOWN_CHANNEL: {
                filters.erase(msg.channel);
                // Ask the reader to tear down without re-announcing: the peer
                // initiated this SHUTDOWN, so echoing it back would loop.
                if (auto mailbox = mailboxes.Get(msg.channel)) {
                    mailbox->RequestTeardown(/*announce=*/false);
                    std::cout << "guacamole_send_handler: received channel "
                              << (int)msg.channel << " SHUTDOWN from peer"
                              << std::endl;
                }
                break;
            }
            case ChannelAction::CREATE_CHANNEL:
                // gmlbroker is the allocator; it never receives CREATE
                break;
            case ChannelAction::APPROVAL: {
                char verdict =
                    msg.payload.empty() ? APPROVAL_DENY : msg.payload[0];
                std::string id = msg.payload.size() > 1 ? msg.payload.substr(1)
                                                        : std::string();
                if (verdict == APPROVAL_APPROVE) {
                    // Match the verdict to the outstanding request before acting,
                    // so a stale verdict can't approve a reused channel.
                    if (!approvals.Approve(msg.channel, id)) {
                        std::cerr << "guacamole_send_handler: channel "
                                  << (int)msg.channel
                                  << " ignoring unmatched approval" << std::endl;
                        break;
                    }
                    // The reader will replay the handshake and pipe input; arm
                    // the return filter to swallow guacd's real args/ready.
                    filters[msg.channel] = ReturnFilter{};
                    std::cout << "guacamole_send_handler: channel " << (int)msg.channel
                              << " APPROVED" << std::endl;
                } else {
                    if (!approvals.Matches(msg.channel, id)) {
                        std::cerr << "guacamole_send_handler: channel "
                                  << (int)msg.channel
                                  << " ignoring unmatched denial" << std::endl;
                        break;
                    }
                    std::cout << "guacamole_send_handler: channel " << (int)msg.channel
                              << " DENIED" << std::endl;
                    // Paint the denied screen, then have the reader tear down and
                    // announce SHUTDOWN to the peer.
                    if (auto mailbox = mailboxes.Get(msg.channel)) {
                        mailbox->Post(HandshakeForger::DeniedScreen());
                        mailbox->RequestTeardown(/*announce=*/true);
                    }
                }
                break;
            }
            case ChannelAction::NONE:
            default: {
                auto mailbox = mailboxes.Get(msg.channel);
                if (!mailbox) {
                    std::cerr << "guacamole_send_handler: no socket for channel "
                              << (int)msg.channel << ", dropping "
                              << msg.payload.size() << " bytes" << std::endl;
                    break;
                }
                // Swallow guacd's real args/ready before piping to the web
                // server. By default the payload is forwarded as-is (no copy);
                // a new string is only materialized when a return filter is
                // active and actually rewrites it.
                const std::string *out = &msg.payload;
                std::string filtered;
                auto fit = filters.find(msg.channel);
                if (fit != filters.end()) {
                    filtered = fit->second.Feed(msg.payload.data(),
                                                msg.payload.size());
                    out = &filtered;
                }
                if (out->empty())
                    break; // fully swallowed (handshake reply)
                mailbox->Post(*out);
                break;
            }
            }
        }
    });
}
