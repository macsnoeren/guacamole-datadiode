#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/guacamole_server.h"
#include "../../shared/include/network/reader_group.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../../shared/include/util/control_channel.h"
#include "../../shared/include/util/netargs.h"
#include "../include/nethandlers/guacamole_accept_handler.h"
#include "../include/nethandlers/guacamole_send_handler.h"
#include "../include/nethandlers/udp_recv_handler.h"
#include "../include/nethandlers/udp_send_handler.h"
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

    // Out-of-band approval-toggle relay: nettest (IT-side) cannot reach the
    // guard directly across the diode, so it sends a plaintext "approve"/"deny"
    // datagram here and we forward it on to the guard's control port (the
    // diode-allowed direction). gmlbroker does not decide approvals; it only
    // relays. Both sockets carry the 200 ms recv timeout so the relay loop
    // observes `running` and stops on SIGINT.
    UDPReceiver control_receiver(APPROVAL_CONTROL_PORT);
    if ((exit = control_receiver.Initialize()) != 0)
        return exit;
    UDPSender control_sender(udp_send_ip, APPROVAL_CONTROL_PORT);
    if ((exit = control_sender.Initialize()) != 0)
        return exit;
    std::cout << "Relaying approval toggles on UDP port " << APPROVAL_CONTROL_PORT
              << " to " << udp_send_ip << ":" << APPROVAL_CONTROL_PORT
              << std::endl;

    ChannelTable table; // Shared by accept thread and guacamole_send thread to keep track of connections
    ApprovalRegistry approvals; // Per-channel approval flags
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
        accept_handler.Run(send_queue, gml_server, table, approvals, readers);
    std::thread t_guacamole_send =
        guacamole_send_handler.Run(recv_queue, gml_server, table, approvals);
    std::thread t_udp_send = udp_send_handler.Run(send_queue, udp_sender);
    std::thread t_udp_recv = udp_recv_handler.Run(recv_queue, udp_receiver);

    // Validating relay: only recognised toggles are forwarded (normalised), so
    // arbitrary bytes never reach the guard's control port.
    std::thread t_control([&control_receiver, &control_sender]() {
        char buf[256];
        while (running) {
            int n = control_receiver.Receive(buf, sizeof(buf));
            if (n <= 0)
                continue;
            std::optional<bool> mode =
                ParseApprovalToggle(std::string(buf, n));
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

    // Shutdown ordering (SIGINT clears `running`): the blocked accept() and
    // recvfrom() time out (SO_RCVTIMEO), so the two producer threads fall out of
    // their loops first. recv_queue then has no producer, so closing it drains
    // t_guacamole_send — once it is gone, only the detached reader threads still
    // touch the table and gml_server. We wake those readers by shutting down
    // their fds (each reader still owns its own close()) and WaitAll() for them
    // before destroying the state they capture. Finally send_queue, whose last
    // producers were those readers, is closed to drain t_udp_send.
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
