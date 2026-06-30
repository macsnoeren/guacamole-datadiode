#include "../include/channel_mailbox.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sys/eventfd.h>
#include <unistd.h>

ChannelMailbox::ChannelMailbox() {
    // EFD_NONBLOCK so the reader's drain read() never blocks and a saturated
    // counter write() fails with EAGAIN rather than stalling a poster.
    event_fd = ::eventfd(0, EFD_NONBLOCK);
    if (event_fd < 0)
        perror("eventfd");
}

ChannelMailbox::~ChannelMailbox() {
    if (event_fd >= 0)
        ::close(event_fd);
}

void ChannelMailbox::Signal() {
    if (event_fd < 0)
        return;
    uint64_t one = 1;
    // A full counter (EAGAIN) means a wake is already pending — fine to drop.
    ssize_t n = ::write(event_fd, &one, sizeof(one));
    (void)n;
}

void ChannelMailbox::Post(std::string bytes) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        outbox.push_back(std::move(bytes));
    }
    Signal();
}

void ChannelMailbox::RequestTeardown(bool announce_on_close) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        teardown = true;
        announce = announce_on_close;
    }
    Signal();
}

void ChannelMailbox::Drain(std::vector<std::string> &out, bool &out_teardown,
                           bool &out_announce) {
    // Clear the wake state first, then take the queue under lock. A Post() that
    // races in between re-signals the eventfd, so the next poll wakes us again
    // (at worst a spurious empty drain) — no item or teardown is ever lost.
    uint64_t cnt;
    while (::read(event_fd, &cnt, sizeof(cnt)) > 0) {
    }

    std::lock_guard<std::mutex> lock(mtx);
    out.insert(out.end(), std::make_move_iterator(outbox.begin()),
               std::make_move_iterator(outbox.end()));
    outbox.clear();
    out_teardown = teardown;
    out_announce = announce;
}

std::shared_ptr<ChannelMailbox> MailboxRegistry::Create(uint16_t channel) {
    auto mailbox = std::make_shared<ChannelMailbox>();
    std::lock_guard<std::mutex> lock(mtx);
    mailboxes[channel] = mailbox;
    return mailbox;
}

std::shared_ptr<ChannelMailbox> MailboxRegistry::Get(uint16_t channel) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = mailboxes.find(channel);
    if (it == mailboxes.end())
        return nullptr;
    return it->second;
}

void MailboxRegistry::Remove(uint16_t channel) {
    std::lock_guard<std::mutex> lock(mtx);
    mailboxes.erase(channel);
}
