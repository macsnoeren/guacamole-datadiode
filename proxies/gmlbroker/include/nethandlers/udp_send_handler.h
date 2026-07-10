#pragma once

#include "../../../shared/include/network/netqueue.h"
#include "../../../shared/include/network/udpsender.h"
#include <thread>

class UDPSendHandler {
    public:
        std::thread Run(NetQueue &queue, UDPSender &udp_sender);
};
