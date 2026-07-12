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

#include <string>
#include "../include/return_filter.h"

std::string ReturnFilter::Feed(const char *data, size_t len) {
    if (piping)
        return std::string(data, len);

    ready_end = 0;
    Parse(data, len); // may flip `piping` via OnInstructionEnd
    if (piping)
        return std::string(data + ready_end, data + len);
    return std::string();
}

bool ReturnFilter::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);
    return true;
}

bool ReturnFilter::OnInstructionEnd() {
    if (!piping && current_opcode == "ready") {
        piping = true;
        ready_end = CurrentIndex() + 1; // first byte after ready's ';'
    }
    return true;
}
