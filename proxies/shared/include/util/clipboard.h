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

#pragma once

#include <cstdint>

/**
 * @brief Shared clipboard-cap constants.
 *
 * The guard caps clipboard payload entering the OT environment at this size and
 * drops anything larger; gmlbroker uses the same cap to know which blobs the
 * guard will drop, so it can fake the `ack` the browser is waiting for (guacd
 * never acks a dropped blob). Keeping the cap here stops the two from drifting.
 */
namespace clipboard {
// Maximum plaintext bytes of clipboard payload allowed inbound.
constexpr uint32_t MAX_INPUT_BYTES = 50;
// Payload arrives base64-encoded (~33% larger), so this is the on-wire cap.
constexpr uint32_t MAX_BYTES = static_cast<uint32_t>(MAX_INPUT_BYTES * 1.33f);
} // namespace clipboard
