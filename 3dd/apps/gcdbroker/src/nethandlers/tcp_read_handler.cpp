#include "../../include/nethandlers/tcp_read_handler.h"
#include "../../../shared/include/network/multiplexer.h"
#include "../../include/running.h"
#include <iostream>
#include <sstream>
#include <string>

/*
 * @brief Reads one guacd connection and wraps each read as a NONE message
 *
 * Runs once per channel. On close it removes the channel; if it was the first
 * to remove it (guacd-initiated close), it announces SHUTDOWN to the peer. The
 * reader is the sole owner of close() for its fd.
 */
std::thread TCPReadHandler::Run(NetQueue &send_queue, TCPClient &tcp_client,
                                ChannelTable &table, uint8_t channel, int fd) {
    return std::thread([&send_queue, &tcp_client, &table, channel, fd]() {
        char buffer[Multiplexer::MAX_PAYLOAD_SIZE + 1];

        while (running) {
            int received = tcp_client.Receive(fd, buffer, sizeof(buffer));
            if (received <= 0)
                break; // 0: guacd closed, <0: error

            BridgeMessage msg;
            msg.channel = channel;
            msg.action = ChannelAction::NONE;
            msg.payload.assign(buffer, received);
            send_queue.Enqueue(std::move(msg));

            std::stringstream info;
            info << "tcp_reader: queued " << received << " bytes on channel "
                 << (int)channel << std::endl;
            std::cout << info.str();
        }

        // Only the side that initiates the close announces SHUTDOWN to the peer
        if (table.Remove(channel).has_value()) {
            BridgeMessage shutdown;
            shutdown.channel = channel;
            shutdown.action = ChannelAction::SHUTDOWN_CHANNEL;
            send_queue.Enqueue(std::move(shutdown));
            std::cout << "tcp_reader: channel " << (int)channel
                      << " closed by guacd, sent SHUTDOWN" << std::endl;
        }
        tcp_client.Close(fd);
    });
}
