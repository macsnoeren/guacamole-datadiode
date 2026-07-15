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

#include "../../include/nethandlers/udp_recv_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include <iostream>

/*
 * @brief Receives datagrams from the bridge and queues the parsed messages
 */
std::thread UDPRecvHandler::Run(NetQueue &queue, UDPReceiver &udp_receiver) {
    return std::thread([&queue, &udp_receiver]() {
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

            queue.Enqueue(std::move(msg));
        }
    });
}
