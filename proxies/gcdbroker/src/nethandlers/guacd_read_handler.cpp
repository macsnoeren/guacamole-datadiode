/*
 * Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
 * Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../../include/nethandlers/guacd_read_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include "../../include/sync_faker.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {
// How long guacd may go without a sync from us before we re-send the last one
// as a keepalive. Must stay well under guacd's "user not responding" timeout.
// Matches the GUACD_KEEPALIVE_MS used for the socket receive timeout.
std::chrono::milliseconds keepalive_interval() {
    const char *env = std::getenv("GUACD_KEEPALIVE_MS");
    int v = env ? std::atoi(env) : 0;
    return std::chrono::milliseconds(v > 0 ? v : 3000);
}
} // namespace

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
        std::string last_ack; // most recent sync ack, re-sent as a keepalive
        const auto keepalive = keepalive_interval();
        auto last_sent = std::chrono::steady_clock::now(); // last sync sent to guacd

        while (running) {
            int received = guacd_client.Receive(fd, buffer, sizeof(buffer));
            auto now = std::chrono::steady_clock::now();

            if (received > 0) {
                BridgeMessage msg;
                msg.channel = channel;
                msg.action = ChannelAction::NONE;
                msg.payload.assign(buffer, received);
                send_queue.Enqueue(std::move(msg));

                // Fake the client's sync acknowledgement toward guacd for every
                // sync guacd just emitted (the guard dropped the real one).
                std::string ack = sync_faker.Feed(buffer, received);
                if (!ack.empty()) {
                    last_ack = ack; // remember for the keepalive below
                    BridgeMessage sync{channel, ChannelAction::NONE, std::move(ack)};
                    recv_queue.Enqueue(std::move(sync));
                    last_sent = now;
                    continue; // a real ack just went out; no keepalive needed
                }
                // Data arrived but carried no sync to echo — fall through to the
                // time-based keepalive so a busy-but-syncless trickle (panel
                // clock, cursor) still can't starve guacd.
            } else if (received != GuacdClient::RECV_TIMEOUT) {
                break; // 0: guacd closed, <0: error
            }

            // Time-based keepalive: if guacd hasn't heard a sync from us within
            // `keepalive`, re-send the last one. The browser's own periodic
            // keepalive was swallowed on the forward path, so without this an idle
            // (or syncless-trickle) session trips guacd's read timeout.
            if (!last_ack.empty() && now - last_sent >= keepalive) {
                BridgeMessage ka{channel, ChannelAction::NONE, last_ack};
                recv_queue.Enqueue(std::move(ka));
                last_sent = now;
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
