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

#include "../include/forward_keepalive_filter.h"
#include <cstring>

void ForwardKeepaliveFilter::Filter(char *data, size_t &len) {
    ParserState st = Parse(data, len);
    if (st == ParserState::STREAM_CORRUPTED) {
        // Could not analyse this chunk cleanly (e.g. a keepalive split across
        // reads). Leave it for the guard and resync the FSM at the next read.
        Reset();
        return;
    }
    // Drop the client keepalive(s) the analysis recorded; a no-op otherwise.
    Excise(data, len);
}

bool ForwardKeepaliveFilter::OnInstructionBegin(const GuacElement &instr) {
    // Deny the broker-handled keepalives `sync` and `nop` (so the base records
    // them for excision); everything else is forwarded for the guard to validate.
    bool is_sync = instr.len == 4 && memcmp(instr.ptr, "sync", 4) == 0;
    bool is_nop = instr.len == 3 && memcmp(instr.ptr, "nop", 3) == 0;
    return !(is_sync || is_nop);
}
