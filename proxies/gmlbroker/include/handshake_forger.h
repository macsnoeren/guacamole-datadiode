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
#include <vector>

/**
 * @brief States that the forged handshake can be in
 */
enum class HandshakeState {
    UNESTABLISHED,         // before select
    EXCHANGING_PARAMETERS, // args sent, awaiting connect
    ESTABLISHED,           // ready + waiting screen sent
    INVALID_HANDSHAKE      // input could not be parsed
};

/**
 * @brief Forges the guacd side of the Guacamole handshake toward the web server.
 *
 * Subclasses OpcodeParser and drives off the incoming client opcode stream:
 *   select       -> reply with canned `args` (per protocol, pinned to guacd 1.6.0)
 *   size/.../name-> captured (display size, connection params)
 *   connect      -> reply `ready,<FAKE-ID>` + a solid-colour waiting screen
 *
 * It does not contact guacd; the stored protocol and connect values are kept so a
 * later stage can replay the real handshake once a connection has been approved.
 */
class HandshakeForger : public OpcodeParser {
  public:
    HandshakeForger() = default;

    /**
     * @brief Feed bytes received from the web server.
     * @return Bytes to write back to the web server (may be empty).
     */
    std::string Feed(const char *data, size_t len);

    HandshakeState GetHandshakeState() const { return hs_state; }

    /** @brief Protocol named in `select` (e.g. "ssh"); valid after select. */
    const std::string &Protocol() const { return protocol; }

    /** @brief Positional `connect` values; valid once ESTABLISHED. */
    const std::vector<std::string> &ConnectValues() const {
        return connect_values;
    }

    /** @brief Forged connection id sent in `ready`. */
    const std::string &FakeId() const { return fake_id; }

    /**
     * @brief The raw handshake bytes received from the web server, captured
     * verbatim up to and including `connect`. Replayed to the real guacd once a
     * connection is approved.
     */
    const std::string &Handshake() const { return handshake_raw; }

    /**
     * @brief A solid-red "approval denied" overlay followed by a disconnect.
     *
     * Static (no handshake state needed) so the bridge-recv thread can paint it
     * straight onto the web-server socket when a deny verdict arrives.
     */
    static std::string DeniedScreen();

  protected:
    /**
     * @brief Stores the opcode currently being parsed
     */
    bool OnInstructionBegin(const GuacElement &instr) override;

    /**
     * Stores the arguments sent from the client, to replay them to guacd later
     */
    bool OnArgument(const GuacElement &arg) override;

    /**
     * @brief Sets handshake state based on the opcode that was received
     */
    bool OnInstructionEnd() override;

  private:
    HandshakeState hs_state = HandshakeState::UNESTABLISHED;
    std::string current_opcode;          // opcode currently being parsed
    std::string protocol;                // from select
    std::vector<std::string> connect_values; // from connect
    std::vector<std::string> size_args;  // client display size (w, h, dpi)
    std::string fake_id;                 // sent in ready
    std::string handshake_raw;           // verbatim client handshake (for replay)
    std::string out;                     // response accumulator for one Feed

    std::string CannedArgs() const;      // per-protocol args reply
    std::string WaitingScreen() const;   // solid-colour fill (provisional)
};
