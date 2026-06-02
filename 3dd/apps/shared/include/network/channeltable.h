#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

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
    std::unordered_map<uint8_t, int> channel_to_fd;

  public:
    /**
     * @brief Allocates the lowest free channel ID and binds it to fd
     * @return The allocated channel, or std::nullopt if all 256 are in use
     */
    std::optional<uint8_t> Allocate(int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        for (int candidate = 0; candidate <= 0xFF; ++candidate) {
            uint8_t channel = static_cast<uint8_t>(candidate);
            // If channel does not appear in map, create a new mapping
            if (channel_to_fd.find(channel) == channel_to_fd.end()) {
                channel_to_fd[channel] = fd;
                return channel;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Binds a specific (peer-chosen) channel to fd
     * @return False if the channel was already in use
     */
    bool Insert(uint8_t channel, int fd) {
        std::lock_guard<std::mutex> lock(mtx);
        return channel_to_fd.emplace(channel, fd).second;
    }

    /**
     * @brief Looks up the fd bound to a channel
     */
    std::optional<int> Get(uint8_t channel) const {
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
    std::optional<int> Remove(uint8_t channel) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = channel_to_fd.find(channel);
        if (it == channel_to_fd.end())
            return std::nullopt;
        int fd = it->second;
        channel_to_fd.erase(it);
        return fd;
    }
};
