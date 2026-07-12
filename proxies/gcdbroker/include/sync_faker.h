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

#include "../../shared/include/parser/opcode_parser.h"
#include <cstddef>
#include <string>

/**
 * @brief Synthesises the client's `sync` acknowledgement toward guacd.
 *
 * The guard blocks the browser's `sync` on the inbound path, so guacd would
 * otherwise never receive a sync reply and would throttle or stall rendering.
 * This watches guacd's *output* stream for each `sync,<timestamp>;` it emits and
 * produces the matching reply — the same timestamp echoed back, exactly what a
 * real client sends.
 *
 * It reuses the shared `OpcodeParser` as a neutral Guacamole framer: it allows
 * every opcode (no allowlist — that is the guard's policy) and tolerates the
 * large drawing elements in guacd's output (`ToleratesOversizedElements`), so a
 * full-screen image blob streams past instead of corrupting the parse.
 */
class SyncFaker : public OpcodeParser {
  public:
    /**
     * @brief Feed guacd output bytes; returns the sync replies to send back to
     *        guacd (concatenated `sync` instructions), or "" if none completed
     *        in this chunk.
     */
    std::string Feed(const char *data, size_t len);

  protected:
    bool ToleratesOversizedElements() override { return true; }
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool OnArgument(const GuacElement &arg) override;
    bool OnInstructionEnd() override;

  private:
    bool is_sync = false;    // the current instruction is a `sync`
    int arg_index = 0;       // 1-based index of the argument being parsed
    std::string timestamp;   // the sync's timestamp argument
    std::string echoes;      // replies accumulated during the current Feed()
};
