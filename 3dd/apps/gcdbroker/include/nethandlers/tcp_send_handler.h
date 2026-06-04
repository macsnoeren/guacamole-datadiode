#pragma once

#include "../../../shared/include/network/channeltable.h"
#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/tcpclient.h"
#include <thread>

class TCPSendHandler {
    public:
        std::thread Run(NetQueue &recv_queue, NetQueue &send_queue, TCPClient &tcp_client, ChannelTable &table);
};
