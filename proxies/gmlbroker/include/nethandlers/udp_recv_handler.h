#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/udpreceiver.h"
#include <thread>

class UDPRecvHandler {
    public:
        std::thread Run(NetQueue &queue, UDPReceiver &udp_receiver);
};
