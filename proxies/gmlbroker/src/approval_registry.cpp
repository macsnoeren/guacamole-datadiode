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

#include "../include/approval_registry.h"
#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

void ApprovalRegistry::Create(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    entries[channel] = Entry{std::make_shared<std::atomic<bool>>(false), ""};
}

std::shared_ptr<std::atomic<bool>> ApprovalRegistry::Flag(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    return it == entries.end() ? nullptr : it->second.approved;
}

void ApprovalRegistry::SetRequestId(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    if (it != entries.end())
        it->second.request_id = id;
}

bool ApprovalRegistry::Approve(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    if (it == entries.end() || it->second.request_id != id)
        return false;
    it->second.approved->store(true);
    return true;
}

bool ApprovalRegistry::Matches(uint16_t channel, const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(channel);
    return it != entries.end() && it->second.request_id == id;
}

void ApprovalRegistry::Remove(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mutex);
    entries.erase(channel);
}
