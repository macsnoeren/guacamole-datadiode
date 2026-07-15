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

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/*
 * @brief Per-channel approval state, shared between the bridge-recv thread and
 * the per-connection reader threads.
 *
 * The reader mints a request id and waits; the APPROVAL verdict arrives on the
 * bridge-recv thread, which matches it against that id before flipping the
 * channel's approved flag. The reader polls the flag to start replaying the
 * handshake and piping browser input.
 */
class ApprovalRegistry {
    public:
        /**
         * @brief Creates a channel-to-approval association to store
         */
        void Create(uint16_t channel);

        /**
         * @brief Returns whether the approval request for this channel is approved or denied
         */
        std::shared_ptr<std::atomic<bool>> Flag(uint16_t channel);

        /**
         * @brief Sets the approval request's request ID
         */
        void SetRequestId(uint16_t channel, const std::string &id);

        /**
         * @brief Marks the channel approved if the verdict's id matches its request.
         */
        bool Approve(uint16_t channel, const std::string &id);

        /**
         * @brief Whether a verdict's id matches the channel's outstanding request.
         */
        bool Matches(uint16_t channel, const std::string &id);

        /**
         * @brief Removes the channel-to-approval association/entry
         */
        void Remove(uint16_t channel);

    private:
        /**
         * @brief Stores the approval request along with unique request ID
         */
        struct Entry {
            // Use shared_ptr so the verdict doesn't get destroyed when 
            // ApprovalRegistry::Remove() is called, if it is still referenced
            // by other code.
            std::shared_ptr<std::atomic<bool>> approved;
            std::string request_id;
        };
        std::mutex mutex;
        std::unordered_map<uint16_t, Entry> entries;
};
