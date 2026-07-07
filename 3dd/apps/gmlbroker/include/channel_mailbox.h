#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * @brief A per-channel outbound mailbox for the web-server connection.
 *
 * Only the channel's reader thread is allowed to touch its socket (so that,
 * once the connection is wrapped in TLS, every SSL_* call for a channel happens
 * on a single thread). Any other thread that wants to push bytes to the browser
 * — or tear the channel down — does so through this mailbox instead of writing
 * the socket itself: it Post()s bytes or RequestTeardown()s, both of which wake
 * the reader through an eventfd. The reader poll()s WakeFd() alongside its
 * socket and Drain()s the mailbox on its own thread.
 */
class ChannelMailbox {
  public:
    ChannelMailbox();
    ~ChannelMailbox();

    ChannelMailbox(const ChannelMailbox &) = delete;
    ChannelMailbox &operator=(const ChannelMailbox &) = delete;

    /**
     * @brief The eventfd the reader poll()s to learn the mailbox has work
     */
    int WakeFd() const { return event_fd; }

    /**
     * @brief Queues bytes for the reader to write to the browser, then wakes it
     */
    void Post(std::string bytes);

    /**
     * @brief Asks the reader to stop; @p announce says whether it should emit a
     * SHUTDOWN_CHANNEL to the peer (false when the peer initiated the teardown).
     */
    void RequestTeardown(bool announce);

    /**
     * @brief Reader-side: clears the wake state and moves out all queued bytes
     * and the teardown request. Call only from the owning reader thread.
     */
    void Drain(std::vector<std::string> &out, bool &out_teardown,
               bool &out_announce);

  private:
    void Signal();

    int event_fd = -1;
    std::mutex mtx;
    std::deque<std::string> outbox;
    bool teardown = false;
    bool announce = true;
};

/*
 * @brief Thread-safe channel -> mailbox map, mirroring ApprovalRegistry.
 *
 * The accept thread Create()s a channel's mailbox before spawning its reader;
 * the guacamole_send thread Get()s it to route return traffic; the reader
 * Remove()s it on close. Entries are shared_ptr so a mailbox a sender still
 * holds outlives Remove().
 */
class MailboxRegistry {
  public:
    std::shared_ptr<ChannelMailbox> Create(uint16_t channel);
    std::shared_ptr<ChannelMailbox> Get(uint16_t channel) const;
    void Remove(uint16_t channel);

  private:
    mutable std::mutex mtx;
    std::unordered_map<uint16_t, std::shared_ptr<ChannelMailbox>> mailboxes;
};
