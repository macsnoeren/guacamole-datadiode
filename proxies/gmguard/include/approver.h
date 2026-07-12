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

#include <atomic>
#include <string>

/**
 * @brief An approval verdict with an optional human-readable reason
 */
struct ApprovalResult {
    bool approved;
    std::string reason; // populated on denial, empty on approval
};

/**
 * @brief Decides whether a connection request may proceed toward guacd.
 *
 * The request is an inert, unique identifier minted by gmlbroker — deliberately
 * NOT Guacamole traffic, so the gate never parses attacker-influenced bytes
 * before a human authorizes the connection. This is the OT-side gate, and it
 * lives at the guard: the operator commands the guard directly.
 *
 * For the PoC the decision is a single global approve/deny switch, flipped at
 * runtime over the control port (see control_channel.h). It defaults to
 * deny, so a connection is refused until an operator explicitly approves.
 * SetApprove is called from the control-listener thread while HandleRequest
 * runs on the main receive loop, so the switch is atomic.
 */
class Approver {
  public:
    Approver() = default;

    /**
     * @brief Flips the global approval switch at runtime.
     * @param approve: true to approve subsequent requests, false to deny
     */
    void SetApprove(bool approve) {
        approve_.store(approve, std::memory_order_relaxed);
    }

    /**
     * @brief Whether the switch is currently set to approve.
     */
    bool IsApproving() const {
        return approve_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Decides on a connection request.
     * @param request_id: the inert unique request identifier
     * @return The approval result
     */
    ApprovalResult HandleRequest(const std::string &request_id);

  private:
    std::atomic<bool> approve_{false};
};
