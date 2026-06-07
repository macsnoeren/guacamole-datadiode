#include "../../include/nethandlers/udp_recv_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include <iostream>

/*
 * @brief Receives datagrams from the bridge and queues the parsed messages
 */
std::thread UDPRecvHandler::Run(NetQueue &queue, UDPReceiver &udp_receiver) {
    return std::thread([&queue, &udp_receiver]() {
        // + 1 for null byte - c-style strings
        char buffer[Multiplexer::MAX_DATAGRAM_SIZE + 1];

        while (running) {
            int received = udp_receiver.Receive(buffer, sizeof(buffer));
            if (received <= 0)
                continue;

            BridgeMessage msg;
            if (!Multiplexer::TryCast(buffer, received, msg)) {
                std::cerr << "udp_recv_handler: dropped malformed datagram ("
                          << received << " bytes)" << std::endl;
                continue;
            }

            queue.Enqueue(std::move(msg));
        }
    });
}
