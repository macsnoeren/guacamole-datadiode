#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/guacamole_server.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../include/nethandlers/tcp_accept_handler.h"
#include "../include/nethandlers/tcp_send_handler.h"
#include "../include/nethandlers/udp_recv_handler.h"
#include "../include/nethandlers/udp_send_handler.h"
#include "../include/running.h"
#include <atomic>
#include <iostream>
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
    if (argc != 7) {
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
    int guac_listen_port = std::stoi(argv[2]);
    int udp_recv_port = std::stoi(argv[4]);
    const char *udp_send_ip = argv[5];
    int udp_send_port = std::stoi(argv[6]);

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

    ChannelTable table; // Shared by accept thread and tcp_send thread to keep track of connections
    ApprovalRegistry approvals; // Per-channel approval flags
    NetQueue recv_queue;
    NetQueue send_queue;

    // Start the handler threads. The accept and tcp_send handlers route by
    // channel via the shared ChannelTable and ApprovalRegistry; the UDP
    // handlers ferry between the bridge and the queues.
    TCPAcceptHandler accept_handler;
    TCPSendHandler tcp_send_handler;
    UDPSendHandler udp_send_handler;
    UDPRecvHandler udp_recv_handler;

    std::thread t_accept =
        accept_handler.Run(send_queue, gml_server, table, approvals);
    std::thread t_tcp_send =
        tcp_send_handler.Run(recv_queue, gml_server, table, approvals);
    std::thread t_udp_send = udp_send_handler.Run(send_queue, udp_sender);
    std::thread t_udp_recv = udp_recv_handler.Run(recv_queue, udp_receiver);

    t_accept.join();
    t_tcp_send.join();
    t_udp_recv.join();
    t_udp_send.join();
}
