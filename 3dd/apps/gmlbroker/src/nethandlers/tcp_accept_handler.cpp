#include "../../include/nethandlers/tcp_accept_handler.h"
#include "../../include/nethandlers/tcp_read_handler.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>

/*
 * @brief Accepts Guacamole connections, allocating a channel for each
 */
std::thread TCPAcceptHandler::Run(NetQueue &queue, TCPServer &tcp_server,
                                  ChannelTable &table,
                                  ApprovalRegistry &approvals) {
    return std::thread([&queue, &tcp_server, &table, &approvals]() {
        while (running) {
            int fd = tcp_server.Accept();
            if (fd < 0) {
                if (running)
                    continue;
                break;
            }

            // Try to allocate the lowest channel not yet taken
            std::optional<uint8_t> channel = table.Allocate(fd);
            if (!channel) {
                std::cerr
                    << "accept_handler: channel table full, rejecting client"
                    << std::endl;
                tcp_server.Close(fd);
                continue;
            }

            // Register the channel's approval state before its reader can run.
            // The CREATE (approval request) is sent later, once the forged
            // handshake is done — nothing crosses the bridge for an incomplete
            // handshake.
            approvals.Create(channel.value());
            std::cout << "accept_handler: new channel " << (int)channel.value()
                      << " (fd " << fd << ")" << std::endl;

            // Hand the connection to its own reader thread and detach it, so the
            // accept loop can keep accepting connections. The reader's thread
            // body captures only the shared refs (not the handler), so the
            // temporary handler going out of scope here is safe.
            TCPReadHandler reader;
            reader.Run(queue, tcp_server, table, approvals, channel.value(), fd)
                .detach();
        }
    });
}
