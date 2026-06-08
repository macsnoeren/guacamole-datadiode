#pragma once

#include "../../../shared/include/network/channeltable.h"
#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/guacd_client.h"
#include <thread>

class GuacdSendHandler {
    public:
        std::thread Run(NetQueue &recv_queue, NetQueue &send_queue, GuacdClient &guacd_client, ChannelTable &table);
};
