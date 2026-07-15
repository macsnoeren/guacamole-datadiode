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

#include "../include/approver.h"
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &request_id) {
    // PoC operator stand-in: honour the runtime global approve/deny switch.
    if (!approve_.load(std::memory_order_relaxed)) {
        std::cout << "Approver: DENY request " << request_id << std::endl;
        return {false, "operator denied the request"};
    }

    // request_id is validated (is_valid_request_id) by the guard's receive loop
    // before this is called, so it is safe to log: a hex-only id can carry no
    // terminal escapes or forged log lines even when it arrived over UDP.
    std::cout << "Approver: APPROVE request " << request_id << std::endl;
    return {true, ""};
}
