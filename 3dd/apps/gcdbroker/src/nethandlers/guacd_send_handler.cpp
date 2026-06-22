#include "../../include/nethandlers/guacd_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/nethandlers/guacd_read_handler.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>
#include <string>

/*
 * @brief Routes bridge messages to guacd connections by channel.
 *
 * The approval decision now lives at the guard: gcdbroker no longer parses the
 * request or verdicts anything. It dials guacd only when the guard's
 * APPROVAL('A') verdict arrives, relays every verdict onto the return path back
 * to gmlbroker, and drops any NONE for a channel it has not dialed. So no
 * Guacamole reaches guacd before the operator approves. Once dialed, NONE
 * traffic is forwarded to guacd untouched (the guard validated it en route).
 */
std::thread GuacdSendHandler::Run(NetQueue &recv_queue, NetQueue &send_queue,
                                GuacdClient &guacd_client, ChannelTable &table,
                                ReaderGroup &readers) {
    return std::thread([&recv_queue, &send_queue, &guacd_client, &table, &readers]() {
        while (running) {
            std::optional<BridgeMessage> opt = recv_queue.Dequeue();
            if (!opt)
                break; // queue closed and drained: shutting down
            BridgeMessage msg = std::move(*opt);

            switch (msg.action) {
            case ChannelAction::CREATE_CHANNEL:
                // The guard owns the decision; gcdbroker waits for the verdict
                // and dials guacd only on APPROVAL('A').
                std::cout << "guacd_send_handler: channel " << (int)msg.channel
                          << " CREATE seen (awaiting verdict)" << std::endl;
                break;

            case ChannelAction::APPROVAL: {
                // The guard's verdict, arriving on the forward path. On approval
                // dial guacd; either way relay it onto the return path so
                // gmlbroker can act on it.
                char verdict =
                    msg.payload.empty() ? APPROVAL_DENY : msg.payload[0];
                if (verdict == APPROVAL_APPROVE) {
                    int fd = guacd_client.Connect();
                    if (fd >= 0 && table.Insert(msg.channel, fd)) {
                        // Count the reader in before launching it; this handler
                        // thread is joined on shutdown before WaitAll runs, so
                        // the count is final by then.
                        readers.Enter();
                        GuacdReadHandler reader;
                        reader.Run(send_queue, guacd_client, table, readers, msg.channel, fd)
                            .detach();
                        std::cout << "guacd_send_handler: channel "
                                  << (int)msg.channel << " APPROVED, dialed guacd"
                                  << " (fd " << fd << ")" << std::endl;
                    } else {
                        if (fd >= 0)
                            guacd_client.Close(fd);
                        // Dial failed: downgrade the relayed verdict so gmlbroker
                        // tears down instead of waiting forever.
                        msg.payload[0] = APPROVAL_DENY;
                        std::cerr << "guacd_send_handler: channel "
                                  << (int)msg.channel
                                  << " approved but guacd dial failed; relaying"
                                  << " DENY" << std::endl;
                    }
                }
                send_queue.Enqueue(std::move(msg));
                break;
            }

            case ChannelAction::SHUTDOWN_CHANNEL: {
                std::optional<int> fd = table.Remove(msg.channel);
                if (fd) {
                    guacd_client.Shutdown(*fd); // wakes the reader, which closes it
                    std::cout << "guacd_send_handler: channel " << (int)msg.channel
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
                    std::cerr << "guacd_send_handler: channel " << (int)msg.channel
                              << " not approved, dropping " << msg.payload.size()
                              << " bytes" << std::endl;
                    break;
                }
                if (guacd_client.Send(*fd, msg.payload.data(),
                                    msg.payload.size()) < 0) {
                    std::optional<int> dead = table.Remove(msg.channel);
                    if (dead)
                        guacd_client.Shutdown(*dead);
                }
                break;
            }
            }
        }
    });
}
