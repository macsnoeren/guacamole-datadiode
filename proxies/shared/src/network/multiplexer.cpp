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

#include "../../include/network/multiplexer.h"

// [ISSUE] MS: You have created a constant MAX_PAYLOAD_SIZE but your are not using this in below function.
std::string Multiplexer::Serialize(const BridgeMessage &message) {
    // Copy message into the out string
    std::string out;
    out.reserve(HEADER_SIZE + message.payload.size());
    out.push_back(static_cast<char>(message.channel >> 8));
    out.push_back(static_cast<char>(message.channel & 0xFF));
    out.push_back(static_cast<char>(static_cast<uint8_t>(message.action)));
    out.append(message.payload);
    return out;
}

bool Multiplexer::TryCast(const char *buffer, size_t len, BridgeMessage &message) {
    // Buffer is null or not large enough
    if (buffer == nullptr || len < static_cast<size_t>(HEADER_SIZE))
        return false;

    uint16_t channel = static_cast<uint16_t>(
        (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]));
    uint8_t flags = static_cast<uint8_t>(buffer[2]);

    // Lower 6 bits are reserved and must be zero
    if (flags & RESERVED_MASK)
        return false;

    // Set the channel action
    ChannelAction action;
    switch (flags & ACTION_MASK) {
    case static_cast<uint8_t>(ChannelAction::NONE):
        action = ChannelAction::NONE;
        break;
    case static_cast<uint8_t>(ChannelAction::CREATE_CHANNEL):
        action = ChannelAction::CREATE_CHANNEL;
        break;
    case static_cast<uint8_t>(ChannelAction::SHUTDOWN_CHANNEL):
        action = ChannelAction::SHUTDOWN_CHANNEL;
        break;
    case static_cast<uint8_t>(ChannelAction::APPROVAL):
        action = ChannelAction::APPROVAL;
        break;
    default:
        return false;
    }

    // Payload too large
    size_t payload_len = len - HEADER_SIZE;
    if (payload_len > static_cast<size_t>(MAX_PAYLOAD_SIZE))
        return false;

    message.channel = channel;
    message.action = action;
    message.payload.assign(buffer + HEADER_SIZE, payload_len);
    return true;
}
