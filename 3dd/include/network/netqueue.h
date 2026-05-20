#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdlib.h>

class NetQueue {
private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::queue<char*> queue;
public:

    NetQueue() = default;

    void Enqueue(char* message) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(message);
        }
        cv.notify_one();
    }

    char* Dequeue() {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait until a message arrives if queue is empty
        cv.wait(lock, [this] { return !queue.empty(); });
        char* value = std::move(queue.front());
        queue.pop();
        return value;
    }

    std::optional<char*> Peek() const {
        std::lock_guard<std::mutex> lock(mtx);
        if (queue.empty()) {
            return std::nullopt;
        }
        return queue.front();
    }

    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};
