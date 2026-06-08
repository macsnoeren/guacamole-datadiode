#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/guacamole_server.h"
#include "../../../shared/include/network/channeltable.h"
#include "../approval_registry.h"
#include <thread>

class TCPSendHandler {
    public:
        std::thread Run(NetQueue &queue, GuacamoleServer &guacamole_server, ChannelTable &table, ApprovalRegistry &approvals);
};
