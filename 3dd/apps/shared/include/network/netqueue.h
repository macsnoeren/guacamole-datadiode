#pragma once

#include "multiplexer.h"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdlib.h>

/**
 * @brief Implements a thread-safe queue intended for queueing bridge messages
 */
class NetQueue {
  private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::queue<BridgeMessage> queue;
    bool closed = false;

    // Peak queue depth since the last TakeHighWater(); catches transient bursts a
    // periodic size sampler would miss. Mutable so monitoring stays const.
    mutable size_t high_water = 0;

  public:
    NetQueue() = default;

    /**
     * @brief Adds a bridge message to the queue
     */
    void Enqueue(BridgeMessage&& message) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(message));
            if (queue.size() > high_water)
                high_water = queue.size();
        }
        cv.notify_one();
    }

    /**
     * @brief Returns the peak depth observed since the previous call and resets
     *        the peak to the current depth. Use to spot transient backlog that a
     *        point-in-time Size() reading between bursts would not reveal.
     */
    size_t TakeHighWater() const {
        std::lock_guard<std::mutex> lock(mtx);
        size_t peak = high_water;
        high_water = queue.size();
        return peak;
    }

    /**
     * @brief Wakes every blocked Dequeue so consumer threads can stop.
     *
     * After Close, Dequeue still drains whatever is already queued, then returns
     * std::nullopt once empty. Used on shutdown to unblock the consumer threads,
     * which would otherwise wait forever on an empty queue.
     */
    void Close() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            closed = true;
        }
        cv.notify_all();
    }

    /**
     * @brief Wait until the queue contains elements, and remove its first element
     * @return The first message, or std::nullopt once the queue is closed and
     *         drained (the signal for the caller to stop).
     */
    std::optional<BridgeMessage> Dequeue() {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait for a message, or for Close() to drain us out of the loop
        cv.wait(lock, [this] { return !queue.empty() || closed; });
        if (queue.empty())
            return std::nullopt;
        BridgeMessage value = std::move(queue.front());
        queue.pop();
        return value;
    }

    /**
     * @brief Get the first value inside the queue without removing it
     * @return The first value when queue is not empty, else std::nullopt
     */
    std::optional<BridgeMessage> Peek() const {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) {
            return std::nullopt;
        }
        return queue.front();
    }

    /**
     * @brief Check if the queue is empty
     */
    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    /**
     * @brief Check the queue's size
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};
