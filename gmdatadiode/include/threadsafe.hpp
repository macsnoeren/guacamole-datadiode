// thread_safe_containers.hpp
#pragma once

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * @brief Thread-safe FIFO queue.
 * 
 * Provides blocking and non-blocking pop operations.
 */
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // Push an item into the queue
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            q_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // Blocking pop
    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !q_.empty(); });
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }

    // Non-blocking pop, returns std::optional
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (q_.empty()) return std::nullopt;
        T value = std::move(q_.front());
        q_.pop();
        return value;
    }

    // Check if queue is empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return q_.empty();
    }

    // Get current size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return q_.size();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<T> q_;
};

/**
 * @brief Thread-safe unordered_map wrapper.
 * 
 * Provides locked access to insert, erase, lookup, etc.
 */
template <typename Key, typename Value>
class ThreadSafeUnorderedMap {
public:
    ThreadSafeUnorderedMap() = default;
    ThreadSafeUnorderedMap(const ThreadSafeUnorderedMap&) = delete;
    ThreadSafeUnorderedMap& operator=(const ThreadSafeUnorderedMap&) = delete;

    // Insert or assign value
    void insert_or_assign(const Key& key, Value value) {
        std::lock_guard<std::mutex> lock(mtx_);
        map_[key] = std::move(value);
    }

    // Erase a key, returns true if erased
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.erase(key) > 0;
    }

    // Check if key exists
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.find(key) != map_.end();
    }

    // Try get value by key
    std::optional<Value> get(const Key& key) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = map_.find(key);
        if (it != map_.end())
            return it->second;
        return std::nullopt;
    }

    // Get size of map
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return map_.size();
    }

    // Clear map
    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        map_.clear();
    }

    // Apply function under lock (safe iteration)
    template <typename Func>
    void for_each(Func f) const {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& kv : map_) {
            f(kv.first, kv.second);
        }
    }

private:
    mutable std::mutex mtx_;
    std::unordered_map<Key, Value> map_;
};
