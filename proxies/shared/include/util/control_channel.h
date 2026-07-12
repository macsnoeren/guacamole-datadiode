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

#include <cctype>
#include <optional>
#include <string>

/**
 * @brief namespace containing functions for handling approval requests.
 *
 * This functionality is temporary; it will be replaced by a more robust
 * system in the future.
 */
namespace ControlChannel {
    /**
     * @brief control channel for the runtime approval switch.
     *
     * The approval policy is a global, runtime-toggleable approve/deny switch.
     * The operator commands the guard directly from the OT side: the approver
     * console (apps/approver, co-located with the guard) sends a plaintext
     * "approve"/"deny" datagram to this port and the guard applies it. Nothing
     * on the IT side can reach it, so IT cannot influence the gate.
     */
    constexpr int APPROVAL_CONTROL_PORT = 4999;

    /**
     * @brief Parses a plaintext approval-toggle command.
     *
     * Accepts "approve"/"deny" case-insensitively, ignoring surrounding whitespace.
     *
     * @return The desired approve flag (true = approve, false = deny), or
     *         std::nullopt for anything unrecognised.
     */
    inline std::optional<bool> ParseApprovalToggle(const std::string &raw) {
        std::string s;
        for (char c : raw) {
            if (!std::isspace(static_cast<unsigned char>(c)))
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (s == "approve")
            return true;
        if (s == "deny")
            return false;
        return std::nullopt;
    }
}
