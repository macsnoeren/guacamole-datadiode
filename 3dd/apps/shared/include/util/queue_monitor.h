#pragma once

#include "../network/netqueue.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

/**
 * @brief Optional diagnostic: periodically log the depth of the two NetQueues.
 *
 * A growing queue means one side of the bridge cannot drain what the other
 * produces — e.g. under heavy RDP, guacd floods the return path faster than the
 * UDP bridge carries it. Enabled only when `QUEUE_STATS_MS` is set to a positive
 * interval in milliseconds; otherwise this returns a non-joinable (no-op) thread
 * and nothing is logged. The returned thread polls `running` so it stops on
 * shutdown; join it before the queues are destroyed.
 *
 * @param recv_queue  the inbound queue (logged as recv_queue=)
 * @param send_queue  the outbound queue (logged as send_queue=)
 * @param running     the process run flag; the monitor exits when it clears
 * @param tag         a short label (e.g. the broker name) for the log line
 */
inline std::thread StartQueueMonitor(const NetQueue &recv_queue,
                                     const NetQueue &send_queue,
                                     const std::atomic<bool> &running,
                                     const char *tag) {
    const char *env = std::getenv("QUEUE_STATS_MS");
    int interval = env ? std::atoi(env) : 0;
    if (interval <= 0)
        return std::thread(); // disabled: no-op thread

    return std::thread([&recv_queue, &send_queue, &running, tag, interval]() {
        int elapsed = 0;
        while (running.load()) {
            // Sleep in small steps so shutdown stays responsive regardless of
            // the chosen interval.
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            elapsed += 200;
            if (elapsed >= interval) {
                // Report the peak since the last line too, so a burst between
                // samples is not invisible.
                std::cout << tag
                          << " qstats: recv_queue=" << recv_queue.Size()
                          << " (peak " << recv_queue.TakeHighWater() << ")"
                          << " send_queue=" << send_queue.Size()
                          << " (peak " << send_queue.TakeHighWater() << ")"
                          << std::endl;
                elapsed = 0;
            }
        }
    });
}
