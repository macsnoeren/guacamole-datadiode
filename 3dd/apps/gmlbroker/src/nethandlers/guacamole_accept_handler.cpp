#include "../../include/nethandlers/guacamole_accept_handler.h"
#include "../../include/nethandlers/guacamole_read_handler.h"
#include "../../include/running.h"
#include <iostream>
#include <optional>

// [ISSUE] MS: Also in this case there is no cap on the created threads which opens up DoS possibilties again.
//             A client that opens many connections (up to 65536) and can exhaust memory.

/*
 * @brief Accepts Guacamole connections, allocating a channel for each
 */
std::thread GuacamoleAcceptHandler::Run(NetQueue &queue, NetQueue &recv_queue,
                                  GuacamoleServer &guacamole_server,
                                  ChannelTable &table,
                                  ApprovalRegistry &approvals,
                                  ReaderGroup &readers) {
    return std::thread([&queue, &recv_queue, &guacamole_server, &table, &approvals, &readers]() {
        while (running) {
            int fd = guacamole_server.Accept();
            if (fd < 0) {
                if (running)
                    continue;
                break;
            }

            // Try to allocate the lowest channel not yet taken
            std::optional<uint16_t> channel = table.Allocate(fd);
            if (!channel) {
                std::cerr
                    << "accept_handler: channel table full, rejecting client"
                    << std::endl;
                guacamole_server.Close(fd);
                continue;
            }

            // Register the channel's approval state before its reader can run.
            // The CREATE (approval request) is sent later, once the forged
            // handshake is done — nothing crosses the bridge for an incomplete
            // handshake.
            approvals.Create(channel.value());
            std::cout << "accept_handler: new channel " << (int)channel.value()
                      << std::endl;

            // Hand the connection to its own reader thread and detach it, so the
            // accept loop can keep accepting connections. The reader's thread
            // body captures only the shared refs (not the handler), so the
            // temporary handler going out of scope here is safe. Count the reader
            // in before launching it (this thread is joined on shutdown before
            // WaitAll runs, so the count is final by then).
            readers.Enter();
            GuacamoleReadHandler reader;
            reader.Run(queue, recv_queue, guacamole_server, table, approvals, readers, channel.value(), fd)
                .detach();
        }
    });
}
