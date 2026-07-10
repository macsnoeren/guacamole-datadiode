#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/guacamole_server.h"
#include "../../../shared/include/network/channeltable.h"
#include "../../../shared/include/network/reader_group.h"
#include "../approval_registry.h"
#include "../channel_mailbox.h"
#include <thread>

class GuacamoleReadHandler {
    public:
        std::thread Run(NetQueue &queue, NetQueue &recv_queue, GuacamoleServer &guacamole_server, ChannelTable &table, ApprovalRegistry &approvals, MailboxRegistry &mailboxes, ReaderGroup &readers, uint16_t channel, int fd);
};
