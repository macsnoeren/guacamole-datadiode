#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/tcpserver.h"
#include "../../../shared/include/network/channeltable.h"
#include "../approval_registry.h"
#include <thread>

class TCPSendHandler {
    public:
        std::thread Run(NetQueue &queue, TCPServer &tcp_server, ChannelTable &table, ApprovalRegistry &approvals);
};
