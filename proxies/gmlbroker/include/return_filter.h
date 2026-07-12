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
#include <string>

/**
 * @brief Strips guacd's real handshake reply from the return stream.
 *
 * gmlbroker already gave the web server canned `args` and a forged
 * `ready,FAKE-ID`. When the approved connection reaches guacd, guacd replies
 * with its own `args` and `ready,REAL-ID` before any drawing. This filter
 * swallows everything up to and including guacd's `ready;`, then pipes the rest
 * (the drawing) through unchanged — so the web server keeps the FAKE-ID and
 * never sees the duplicate handshake.
 *
 * Once piping, frames are forwarded verbatim without parsing, so large drawing
 * elements (e.g. image blobs) are never measured against the parser's limits.
 */
class ReturnFilter : public OpcodeParser {
  public:
    /**
     * @brief Feed guacd return bytes; returns the bytes to forward to the web
     * server (empty while still swallowing the handshake).
     */
    std::string Feed(const char *data, size_t len);

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;

    bool OnInstructionEnd() override;

  private:
    std::string current_opcode;
    bool piping = false;  // true once guacd's `ready` has been swallowed
    size_t ready_end = 0; // offset of the drawing tail within the ready frame
};
