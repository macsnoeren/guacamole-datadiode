#include "../../include/nethandlers/tcp_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/approver.h"
#include "../../include/nethandlers/tcp_read_handler.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>
#include <string>

namespace {

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

} // namespace

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
std::thread TCPSendHandler::Run(NetQueue &recv_queue, NetQueue &send_queue,
                                TCPClient &tcp_client, ChannelTable &table) {
    return std::thread([&recv_queue, &send_queue, &tcp_client, &table]() {
        Approver approver;

        while (running) {
            BridgeMessage msg = recv_queue.Dequeue();

            switch (msg.action) {
            case ChannelAction::CREATE_CHANNEL: {
                // The CREATE payload is the inert approval request id; decide on
                // it.
                const std::string &request_id = msg.payload;
                ApprovalResult verdict = approver.HandleRequest(request_id);
                if (!verdict.approved) {
                    send_verdict(send_queue, msg.channel, APPROVAL_DENY,
                                 request_id);
                    std::cout << "tcp_send_handler: channel " << (int)msg.channel
                              << " DENIED (" << verdict.reason << ")"
                              << std::endl;
                    break;
                }

                // Approved: dial guacd now, ready to receive the handshake
                // replay.
                int fd = tcp_client.Connect();
                if (fd < 0 || !table.Insert(msg.channel, fd)) {
                    if (fd >= 0)
                        tcp_client.Close(fd);
                    std::cerr << "tcp_send_handler: channel " << (int)msg.channel
                              << " approved but guacd dial failed" << std::endl;
                    send_verdict(send_queue, msg.channel, APPROVAL_DENY,
                                 request_id);
                    break;
                }
                TCPReadHandler reader;
                reader.Run(send_queue, tcp_client, table, msg.channel, fd)
                    .detach();
                send_verdict(send_queue, msg.channel, APPROVAL_APPROVE,
                             request_id);
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
                // Echo the teardown back on the return path so gmlbroker tears
                // down the browser. The guard can originate a SHUTDOWN (corrupt
                // stream) that gmlbroker never initiated, and the return path
                // bypasses the guard. gmlbroker's SHUTDOWN handler is idempotent,
                // so a redundant echo for a browser-initiated close is a no-op,
                // and the "Remove decides who announces" rule stops any loop.
                BridgeMessage echo{msg.channel, ChannelAction::SHUTDOWN_CHANNEL,
                                   ""};
                send_queue.Enqueue(std::move(echo));
                break;
            }

            case ChannelAction::NONE:
            default: {
                // Forward only to approved (dialed) channels. Anything else is
                // dropped uninspected — no Guacamole reaches guacd before
                // approval.
                std::optional<int> fd = table.Get(msg.channel);
                if (!fd) {
                    std::cerr << "tcp_send_handler: channel " << (int)msg.channel
                              << " not approved, dropping " << msg.payload.size()
                              << " bytes" << std::endl;
                    break;
                }
                if (tcp_client.Send(*fd, msg.payload.data(),
                                    msg.payload.size()) < 0) {
                    std::optional<int> dead = table.Remove(msg.channel);
                    if (dead)
                        tcp_client.Shutdown(*dead);
                }
                break;
            }
            }
        }
    });
}
