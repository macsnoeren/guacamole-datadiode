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

#include "../../include/nethandlers/udp_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include <string>

/*
 * @brief Serializes queued messages and sends them on the bridge
 */
std::thread UDPSendHandler::Run(NetQueue &queue, UDPSender &udp_sender) {
    return std::thread([&queue, &udp_sender]() {
        while (running) {
            std::optional<BridgeMessage> opt = queue.Dequeue();
            if (!opt)
                break; // queue closed and drained: shutting down
            BridgeMessage msg = std::move(*opt);
            std::string wire = Multiplexer::Serialize(msg);
            udp_sender.Send(wire.data(), wire.size());
        }
    });
}
