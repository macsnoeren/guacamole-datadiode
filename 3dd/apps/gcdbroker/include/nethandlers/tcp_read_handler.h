#pragma once

#include "../../../shared/include/network/channeltable.h"
#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/tcpclient.h"
#include <thread>

class TCPReadHandler {
    public:
        std::thread Run(NetQueue &send_queue, TCPClient &tcp_client, ChannelTable &table, uint8_t channel, int fd);
};
