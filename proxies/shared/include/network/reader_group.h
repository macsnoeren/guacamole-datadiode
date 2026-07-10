#pragma once

#include <condition_variable>
#include <mutex>

/**
 * @brief Counts the live per-connection reader threads so shutdown can wait for
 * them to finish before the shared state they reference is destroyed.
 *
 * Readers stay detached (no thread-handle bookkeeping that would accumulate over
 * the lifetime of the process). Instead each reader is counted: the spawning
 * thread calls Enter() before launching the reader, and the reader's body holds
 * a Sentinel whose destructor calls Leave() as its very last act. On shutdown, once
 * the spawning thread has been joined (so the count is final) and every
 * connection fd has been shut down to wake the blocked recv()s, WaitAll() blocks
 * until the last reader has left — only then may main tear down the queues and
 * tables the readers capture by reference.
 *
 * Enter() is deliberately called by the spawner rather than the reader so the
 * count is already accurate by the time that spawner is joined; a reader that
 * has been created but not yet scheduled would otherwise be invisible to a
 * WaitAll() racing its first instruction.
 */
class ReaderGroup {
  private:
    std::mutex mtx;
    std::condition_variable cv;
    int count = 0;

  public:
    /** @brief Counts a reader in. Call from the spawner, before launching it. */
    void Enter() {
        std::lock_guard<std::mutex> lock(mtx);
        ++count;
    }

    /** @brief Counts a reader out and wakes a waiting WaitAll(). */
    void Leave() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            --count;
        }
        cv.notify_all();
    }

    /** @brief Blocks until every counted reader has left. */
    void WaitAll() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return count == 0; });
    }

    /**
     * @brief RAII handle for a reader body; Leave()s on destruction.
     *
     * Declare it as the reader's first local so it is destroyed last, after the
     * reader has finished touching all shared state.
     */
    struct Sentinel {
        ReaderGroup &group;
        explicit Sentinel(ReaderGroup &group) : group(group) {}
        ~Sentinel() { group.Leave(); }
        Sentinel(const Sentinel &) = delete;
        Sentinel &operator=(const Sentinel &) = delete;
    };
};
