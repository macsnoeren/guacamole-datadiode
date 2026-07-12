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

#include "../../include/nethandlers/guacamole_accept_handler.h"
#include "../../include/nethandlers/guacamole_read_handler.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>

// [ISSUE] MS: Also in this case there is no cap on the created threads which opens up DoS possibilties again.
//             A client that opens many connections (up to 65536) and can exhaust memory.

/*
 * @brief Accepts Guacamole connections, allocating a channel for each
 */
std::thread GuacamoleAcceptHandler::Run(NetQueue &queue, NetQueue &recv_queue,
                                  GuacamoleServer &guacamole_server,
                                  ChannelTable &table,
                                  ApprovalRegistry &approvals,
                                  MailboxRegistry &mailboxes,
                                  ReaderGroup &readers) {
    return std::thread([&queue, &recv_queue, &guacamole_server, &table, &approvals, &mailboxes, &readers]() {
        while (running) {
            int fd = guacamole_server.Accept();
            if (fd < 0) {
                if (running)
                    continue;
                break;
            }

            // Try to allocate the lowest channel not yet taken
            std::optional<uint16_t> channel = table.Allocate(fd);
            if (!channel) {
                std::cerr
                    << "accept_handler: channel table full, rejecting client"
                    << std::endl;
                guacamole_server.Close(fd);
                continue;
            }

            // Register the channel's approval state and outbound mailbox before
            // its reader can run, so the guacamole_send thread can always route
            // to it. The CREATE (approval request) is sent later, once the forged
            // handshake is done — nothing crosses the bridge for an incomplete
            // handshake.
            approvals.Create(channel.value());
            mailboxes.Create(channel.value());
            std::cout << "accept_handler: new channel " << (int)channel.value()
                      << std::endl;

            // Hand the connection to its own reader thread and detach it, so the
            // accept loop can keep accepting connections. The reader's thread
            // body captures only the shared refs (not the handler), so the
            // temporary handler going out of scope here is safe. Count the reader
            // in before launching it (this thread is joined on shutdown before
            // WaitAll runs, so the count is final by then).
            readers.Enter();
            GuacamoleReadHandler reader;
            reader.Run(queue, recv_queue, guacamole_server, table, approvals, mailboxes, readers, channel.value(), fd)
                .detach();
        }
    });
}
