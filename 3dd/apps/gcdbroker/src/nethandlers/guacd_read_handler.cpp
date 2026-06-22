#include "../../include/nethandlers/guacd_read_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include "../../include/sync_faker.h"
#include <iostream>
#include <sstream>
#include <string>

/*
 * @brief Reads one guacd connection and wraps each read as a NONE message
 *
 * Runs once per channel. On close it removes the channel; if it was the first
 * to remove it (guacd-initiated close), it announces SHUTDOWN to the peer. The
 * reader is the sole owner of close() for its fd.
 *
 * The guard blocks the client's `sync` on the inbound path, so this reader fakes
 * it: a per-channel SyncFaker watches guacd's output for each `sync` it emits
 * and the matching acknowledgement is routed back toward guacd via recv_queue —
 * the same path the bridge's forward traffic takes, so GuacdSendHandler stays
 * the sole writer to guacd (no extra locking on the connection).
 */
std::thread GuacdReadHandler::Run(NetQueue &recv_queue, NetQueue &send_queue,
                                GuacdClient &guacd_client,
                                ChannelTable &table, ReaderGroup &readers,
                                uint16_t channel, int fd) {
    return std::thread([&recv_queue, &send_queue, &guacd_client, &table, &readers, channel, fd]() {
        // Declared first so it is destroyed last: Leave() runs only after all
        // shared-state access below is done, letting main's WaitAll() proceed.
        ReaderGroup::Sentinel sentinel(readers);

        char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];
        SyncFaker sync_faker; // synthesises the client's sync ack toward guacd
        std::string last_ack; // most recent sync ack, re-sent as an idle keepalive

        while (running) {
            int received = guacd_client.Receive(fd, buffer, sizeof(buffer));

            if (received == GuacdClient::RECV_TIMEOUT) {
                // Idle: guacd sent nothing this interval, so there is no sync to
                // echo. The browser's own periodic keepalive was swallowed on the
                // forward path, so re-send the last sync ack ourselves — otherwise
                // guacd trips its "user not responding" timeout when idle.
                if (!last_ack.empty()) {
                    BridgeMessage keepalive{channel, ChannelAction::NONE, last_ack};
                    recv_queue.Enqueue(std::move(keepalive));
                }
                continue;
            }
            if (received <= 0)
                break; // 0: guacd closed, <0: error

            BridgeMessage msg;
            msg.channel = channel;
            msg.action = ChannelAction::NONE;
            msg.payload.assign(buffer, received);
            send_queue.Enqueue(std::move(msg));

            // Fake the client's sync acknowledgement toward guacd for every sync
            // guacd just emitted (the guard dropped the real one).
            std::string ack = sync_faker.Feed(buffer, received);
            if (!ack.empty()) {
                last_ack = ack; // remember for the idle keepalive above
                BridgeMessage sync{channel, ChannelAction::NONE, std::move(ack)};
                recv_queue.Enqueue(std::move(sync));
            }
        }

        // Only the side that initiates the close announces SHUTDOWN to the peer
        if (table.Remove(channel).has_value()) {
            BridgeMessage shutdown;
            shutdown.channel = channel;
            shutdown.action = ChannelAction::SHUTDOWN_CHANNEL;
            send_queue.Enqueue(std::move(shutdown));
            std::cout << "guacd_reader: channel " << (int)channel
                      << " closed by guacd, sent SHUTDOWN" << std::endl;
        }
        guacd_client.Close(fd);
    });
}
