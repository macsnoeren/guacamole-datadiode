#include "../../include/nethandlers/tcp_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/handshake_forger.h"
#include "../../include/return_filter.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

/*
 * @brief Routes bridge messages to the right client socket by channel
 */
std::thread TCPSendHandler::Run(NetQueue &queue, TCPServer &tcp_server,
                                ChannelTable &table,
                                ApprovalRegistry &approvals) {
    return std::thread([&queue, &tcp_server, &table, &approvals]() {
        // Per-channel return-path filter that swallows guacd's real args/ready.
        std::unordered_map<uint8_t, ReturnFilter> filters;

        while (running) {
            BridgeMessage msg = queue.Dequeue();

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
                char verdict =
                    msg.payload.empty() ? APPROVAL_DENY : msg.payload[0];
                std::string id = msg.payload.size() > 1 ? msg.payload.substr(1)
                                                        : std::string();
                if (verdict == APPROVAL_APPROVE) {
                    // Match the verdict to the outstanding request before acting,
                    // so a stale verdict can't approve a reused channel.
                    if (!approvals.Approve(msg.channel, id)) {
                        std::cerr << "tcp_send_handler: channel "
                                  << (int)msg.channel
                                  << " ignoring unmatched approval" << std::endl;
                        break;
                    }
                    // The reader will replay the handshake and pipe input; arm
                    // the return filter to swallow guacd's real args/ready.
                    filters[msg.channel] = ReturnFilter{};
                    std::cout << "tcp_send_handler: channel " << (int)msg.channel
                              << " APPROVED" << std::endl;
                } else {
                    if (!approvals.Matches(msg.channel, id)) {
                        std::cerr << "tcp_send_handler: channel "
                                  << (int)msg.channel
                                  << " ignoring unmatched denial" << std::endl;
                        break;
                    }
                    std::cout << "tcp_send_handler: channel " << (int)msg.channel
                              << " DENIED" << std::endl;
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
                if (tcp_server.Send(*fd, out->data(), out->size()) < 0) {
                    std::optional<int> dead = table.Remove(msg.channel);
                    if (dead)
                        tcp_server.Shutdown(*dead);
                }
                break;
            }
            }
        }
    });
}
