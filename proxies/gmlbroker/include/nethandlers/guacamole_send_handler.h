#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../approval_registry.h"
#include "../channel_mailbox.h"
#include <thread>

class GuacamoleSendHandler {
    public:
        std::thread Run(NetQueue &queue, MailboxRegistry &mailboxes, ApprovalRegistry &approvals);
};
