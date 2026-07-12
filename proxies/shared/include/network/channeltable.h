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
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

/**
 * @brief Thread-safe mapping between multiplexing channel IDs and socket fds
 *
 * One side of the bridge (gmlbroker) allocates channels with Allocate(); the
 * other side (gcdbroker) records the peer's choice with Insert(). Both look up
 * fds with Get() and tear channels down with Remove().
 */
class ChannelTable {
  private:
    mutable std::mutex mtx;
    std::unordered_map<uint16_t, int> channel_to_fd;
    // Round-robin cursor for Allocate. Channels are handed out sequentially and
    // a freed channel is not reused until the cursor wraps the whole 16-bit
    // space. This matters because control frames (a peer SHUTDOWN echo, a stale
    // APPROVAL) for a just-closed channel can still be in flight on the bridge;
    // reusing that id immediately — as lowest-free allocation did — let such a
    // frame tear down or disrupt the new occupant. Delaying reuse lets the old
    // frames drain first.
    uint16_t next_channel = 0;

  public:
    /**
     * @brief Allocates the next free channel ID (round-robin) and binds it to fd
     * @return The allocated channel, or std::nullopt if all 65536 are in use
     */
    std::optional<uint16_t> Allocate(int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        for (int i = 0; i <= 0xFFFF; ++i) {
            // next_channel + i wraps mod 65536 on the cast, so the scan covers
            // every slot starting from the cursor.
            uint16_t channel = static_cast<uint16_t>(next_channel + i);
            if (channel_to_fd.find(channel) == channel_to_fd.end()) {
                channel_to_fd[channel] = fd;
                next_channel = static_cast<uint16_t>(channel + 1);
                return channel;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Binds a specific (peer-chosen) channel to fd
     * @return False if the channel was already in use
     */
    bool Insert(uint16_t channel, int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        return channel_to_fd.emplace(channel, fd).second;
    }

    /**
     * @brief Looks up the fd bound to a channel
     */
    std::optional<int> Get(uint16_t channel) const {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = channel_to_fd.find(channel);
        if (it == channel_to_fd.end())
            return std::nullopt;
        return it->second;
    }

    /**
     * @brief Removes a channel's mapping
     * @return The fd that was bound, or std::nullopt if the channel was unknown.
     *         The caller that receives the fd is responsible for closing it.
     */
    std::optional<int> Remove(uint16_t channel) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = channel_to_fd.find(channel);
        if (it == channel_to_fd.end())
            return std::nullopt;
        int fd = it->second;
        channel_to_fd.erase(it);
        return fd;
    }

    /**
     * @brief Snapshots the fds of all currently bound channels
     *
     * Used on shutdown to shutdown() every live connection and wake the reader
     * threads blocked in recv(). The fds are not removed: each channel's reader
     * remains the sole owner of close().
     */
    std::vector<int> Fds() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<int> fds;
        fds.reserve(channel_to_fd.size());
        for (const auto &entry : channel_to_fd)
            fds.push_back(entry.second);
        return fds;
    }
};
