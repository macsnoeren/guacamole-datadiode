/*
 * Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
 * Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/guacamole_server.h"
#include "../../shared/include/network/reader_group.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../../shared/include/util/control_channel.h"
#include "../../shared/include/util/netargs.h"
#include "../../shared/include/util/queue_monitor.h"
#include "../include/nethandlers/guacamole_accept_handler.h"
#include "../include/nethandlers/guacamole_send_handler.h"
#include "../include/nethandlers/udp_recv_handler.h"
#include "../include/nethandlers/udp_send_handler.h"
#include "../include/channel_mailbox.h"
#include "../include/running.h"
#include <atomic>
#include <iostream>
#include <optional>
#include <signal.h>
#include <thread>

std::atomic<bool> running = true;

/*
 * @brief Signals all threads to stop when an interrupt signal is received
 */
void interrupt_handler(int signum) {
    std::cout << "Stopping program..." << std::endl;
    running = false;
}

/*
 * @brief Starts the Guacamole broker that imitates the guacd program and
 * bridges Guacamole traffic.
 *
 * Accepts multiple Guacamole connections, multiplexing each onto its own
 * channel over a single UDP bridge. Handlers run on separate threads and
 * synchronize messages using thread-safe queues.
 */
int main(int argc, char *argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "\t<guac_listen_ip>: Guacamole broker's listening IP "
                     "address (for the web server)\n"
                  << "\t<guac_listen_port>: Guacamole broker's listening port\n"
                  << "\t<udp_recv_port>: port where the broker receives "
                     "traffic from\n"
                  << "\t<udp_send_ip>: address where the broker sends guacd "
                     "traffic to (the guard)\n"
                  << "\t<udp_send_port>: port where the broker sends guacd "
                     "traffic to\n"
                  << "\nExample: " << argv[0]
                  << " 127.0.0.1 4822 0.0.0.0 5501 10.0.0.2 5601" << std::endl;
        return 1;
    }

    const char *guac_listen_ip = argv[1];
    const char *udp_send_ip = argv[4];
    std::optional<int> p_listen = ParsePort(argv[2]);
    std::optional<int> p_recv = ParsePort(argv[3]);
    std::optional<int> p_send = ParsePort(argv[5]);
    if (!p_listen || !p_recv || !p_send) {
        std::cerr << "Error: ports must be integers in [1, 65535]" << std::endl;
        return 1;
    }
    int guac_listen_port = p_listen.value();
    int udp_recv_port = p_recv.value();
    int udp_send_port = p_send.value();

    // Set interrupt handler
    // TODO: better signal handling
    struct sigaction sa{};
    sa.sa_handler = interrupt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr); // `docker compose down`/`stop` send SIGTERM

    // Initialize UDP and TCP infrastructure
    int exit;
    auto gml_server = GuacamoleServer(guac_listen_ip, guac_listen_port);
    if ((exit = gml_server.Initialize()) != 0)
        return exit;

    std::cout << "Listening on TCP port " << guac_listen_port << "..."
              << std::endl;

    UDPReceiver udp_receiver(udp_recv_port);
    if ((exit = udp_receiver.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP receiver on port " << udp_recv_port
              << std::endl;

    UDPSender udp_sender(udp_send_ip, udp_send_port);
    if ((exit = udp_sender.Initialize()) != 0)
        return exit;

    std::cout << "Initialized UDP sender for " << udp_send_ip << ":"
              << udp_send_port << std::endl;

    // Demo affordance: relay the approval switch from the IT side to the guard.
    // The guard is isolated, so an operator on the low side cannot reach its
    // control port directly — but low->guard is the diode-allowed direction, so
    // gmlbroker forwards a recognised "approve"/"deny" (e.g. from nettest) on to
    // the guard's control port. NOTE: this intentionally re-opens an IT->gate
    // channel; for a hardened deployment, remove it and toggle only from the
    // OT-side approver.
    int apprv_port = ControlChannel::APPROVAL_CONTROL_PORT;
    UDPReceiver control_receiver(apprv_port);
    if ((exit = control_receiver.Initialize()) != 0)
        return exit;
    UDPSender control_sender(udp_send_ip, apprv_port);
    if ((exit = control_sender.Initialize()) != 0)
        return exit;
    std::cout << "Relaying approval toggles on UDP port " << apprv_port << " to "
              << udp_send_ip << ":" << apprv_port << std::endl;

    ChannelTable table; // Shared by accept thread and guacamole_send thread to keep track of connections
    ApprovalRegistry approvals; // Per-channel approval flags
    MailboxRegistry mailboxes; // Per-channel outbound mailbox: the reader is the sole socket writer
    ReaderGroup readers; // Tracks the per-connection reader threads for shutdown
    NetQueue recv_queue;
    NetQueue send_queue;

    // Start the handler threads. The accept and guacamole_send handlers route by
    // channel via the shared ChannelTable and ApprovalRegistry; the UDP
    // handlers ferry between the bridge and the queues.
    GuacamoleAcceptHandler accept_handler;
    GuacamoleSendHandler guacamole_send_handler;
    UDPSendHandler udp_send_handler;
    UDPRecvHandler udp_recv_handler;

    std::thread t_accept =
        accept_handler.Run(send_queue, recv_queue, gml_server, table, approvals, mailboxes, readers);
    std::thread t_guacamole_send =
        guacamole_send_handler.Run(recv_queue, mailboxes, approvals);
    std::thread t_udp_send = udp_send_handler.Run(send_queue, udp_sender);
    std::thread t_udp_recv = udp_recv_handler.Run(recv_queue, udp_receiver);

    // Validating relay: only recognised toggles are forwarded (normalised), so
    // arbitrary bytes never reach the guard's control port. The receiver's 200 ms
    // SO_RCVTIMEO lets this loop observe `running` and stop on shutdown.
    std::thread t_control([&control_receiver, &control_sender]() {
        char buf[256];
        while (running) {
            int n = control_receiver.Receive(buf, sizeof(buf));
            if (n <= 0)
                continue;
            std::optional<bool> mode =
                ControlChannel::ParseApprovalToggle(std::string(buf, n));
            if (!mode) {
                std::cerr << "gmlbroker: ignored unrecognised approval command"
                          << std::endl;
                continue;
            }
            std::string norm = *mode ? "approve" : "deny";
            control_sender.Send(norm.data(), norm.size());
            std::cout << "gmlbroker: relayed approval toggle (" << norm
                      << ") to the guard" << std::endl;
        }
    });

    // Optional diagnostic (set QUEUE_STATS_MS): watch for the return-path
    // recv_queue growing, which means the browser side can't drain the bridge.
    // std::thread t_qstats =
    //     StartQueueMonitor(recv_queue, send_queue, running, "gmlbroker");

    // Shutdown ordering (SIGINT clears `running`): the blocked accept() and
    // recvfrom() time out (SO_RCVTIMEO), so the two producer threads fall out of
    // their loops first. recv_queue then has no producer, so closing it drains
    // t_guacamole_send — once it is gone, only the detached reader threads still
    // touch the table and gml_server. We wake those readers by shutting down
    // their fds (each reader still owns its own close()) and WaitAll() for them
    // before destroying the state they capture. Finally send_queue, whose last
    // producers were those readers, is closed to drain t_udp_send.
    // if (t_qstats.joinable())
    //     t_qstats.join();
    t_accept.join();
    t_udp_recv.join();
    t_control.join();
    recv_queue.Close();
    t_guacamole_send.join();

    for (int fd : table.Fds())
        gml_server.Shutdown(fd);
    readers.WaitAll();

    send_queue.Close();
    t_udp_send.join();
}
