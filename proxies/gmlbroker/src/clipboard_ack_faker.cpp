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

#include "../include/clipboard_ack_faker.h"
#include "../../shared/include/util/clipboard.h"
#include <charconv>
#include <system_error>

namespace {
// Parse a stream-index argument; -1 if it is not a clean whole number.
int parse_index(const GuacElement &arg) {
    int v = 0;
    auto [p, ec] = std::from_chars(arg.ptr, arg.ptr + arg.len, v);
    if (ec != std::errc() || p != arg.ptr + arg.len)
        return -1;
    return v;
}

// A success ack for a stream: `ack,<idx>,OK,0`.
std::string make_ack(int sidx) {
    std::string idx = std::to_string(sidx);
    return "3.ack," + std::to_string(idx.size()) + "." + idx + ",2.OK,1.0;";
}
} // namespace

std::string ClipboardAckFaker::Feed(const char *data, size_t len) {
    acks.clear();
    // Browser input is well-formed ASCII Guacamole, but fail open like the other
    // faker: a stray byte should resync, not permanently stop ack-faking.
    size_t off = 0;
    while (off < len) {
        if (Parse(data + off, len - off) != ParserState::STREAM_CORRUPTED)
            break;
        off += CurrentIndex() + 1;
        Reset();
    }
    return acks;
}

bool ClipboardAckFaker::OnInstructionBegin(const GuacElement &instr) {
    current_opcode.assign(instr.ptr, instr.len);
    current_arg = 0;
    blob_on_clipboard = false;
    return true; // observe only; never deny
}

bool ClipboardAckFaker::OnArgument(const GuacElement &arg) {
    ++current_arg;

    // The first argument of clipboard/blob/end is the stream index.
    if (current_arg == 1) {
        int sidx = parse_index(arg);
        if (current_opcode == "clipboard")
            clipboard_sidx = sidx; // start tracking this stream
        else if (current_opcode == "blob")
            blob_on_clipboard = (sidx >= 0 && sidx == clipboard_sidx);
        else if (current_opcode == "end" && sidx == clipboard_sidx)
            clipboard_sidx = -1; // stream closed
    }
    // The second argument of a clipboard blob is its payload. If it exceeds the
    // cap the guard will drop the blob, so guacd won't ack it — fake the ack.
    // (arg.len is clamped to the buffer for an oversized element, but a clamped
    // length is still > the cap, so the test holds either way.)
    else if (current_arg == 2 && blob_on_clipboard &&
             arg.len > clipboard::MAX_BYTES) {
        acks += make_ack(clipboard_sidx);
    }

    return true;
}
