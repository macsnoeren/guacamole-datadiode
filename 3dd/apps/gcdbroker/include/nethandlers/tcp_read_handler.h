#pragma once

#include "../../../shared/include/network/channeltable.h"
#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/guacd_client.h"
#include <thread>

class TCPReadHandler {
    public:
        std::thread Run(NetQueue &send_queue, GuacdClient &guacd_client, ChannelTable &table, uint8_t channel, int fd);
};
