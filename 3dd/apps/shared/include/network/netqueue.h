#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdlib.h>

/**
 * @brief Implements a thread-safe queue intended for queueing network messages
 */
class NetQueue {
  private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::string> queue;

  public:
    NetQueue() = default;

    /**
     * @brief Adds a network message to the queue
     */
    void Enqueue(std::string&& message) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(message));
        }
        cv.notify_one();
    }

    /**
     * @brief Wait until the queue contains elements, and remove its first element
     * @return The network message from the queue
     */
    std::string Dequeue() {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait until a message arrives if queue is empty
        cv.wait(lock, [this] { return !queue.empty(); });
        std::string value = std::move(queue.front());
        queue.pop();
        return value;
    }

    /**
     * @brief Get the first value inside the queue without removing it
     * @return The first value when queue is not empty, else std::nullopt
     */
    std::optional<std::string> Peek() const {
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
