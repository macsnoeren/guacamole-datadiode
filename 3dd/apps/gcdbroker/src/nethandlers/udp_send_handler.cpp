#include "../../include/nethandlers/udp_send_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include <iostream>
#include <sstream>
#include <string>

/*
 * @brief Serializes queued messages and sends them on the bridge
 */
std::thread UDPSendHandler::Run(NetQueue &queue, UDPSender &udp_sender) {
    return std::thread([&queue, &udp_sender]() {
        while (running) {
            std::optional<BridgeMessage> opt = queue.Dequeue();
            if (!opt)
                break; // queue closed and drained: shutting down
            BridgeMessage msg = std::move(*opt);
            std::string wire = Multiplexer::Serialize(msg);
            udp_sender.Send(wire.data(), wire.size());

            std::stringstream info;
            info << "udp_send_handler: sent " << wire.size()
                 << " bytes on channel " << (int)msg.channel << std::endl;
            std::cout << info.str();
        }
    });
}
