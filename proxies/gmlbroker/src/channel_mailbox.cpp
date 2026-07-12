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
