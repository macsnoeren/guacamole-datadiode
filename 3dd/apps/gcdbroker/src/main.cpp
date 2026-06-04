#include "../../shared/include/network/channeltable.h"
#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/tcpclient.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include "../include/nethandlers/tcp_send_handler.h"
#include "../include/nethandlers/udp_recv_handler.h"
#include "../include/nethandlers/udp_send_handler.h"
#include "../include/running.h"
#include <atomic>
#include <iostream>
#include <signal.h>
#include <string>
#include <thread>

const char *guacd_ip;
int guacd_port;
const char *udp_recv_ip;
int udp_recv_port;
const char *udp_send_ip;
int udp_send_port;
std::atomic<bool> running = true;

/*
 * @brief Signals all threads to stop when an interrupt signal is received
 */
void interrupt_handler(int signum) {
    std::cout << "Stopping program..." << std::endl;
    running = false;
}

/*
 * @brief Starts the guacd broker that imitates the Guacamole web server and
 * bridges Guacamole traffic.
 *
 * Reacts to channel lifecycle messages from the bridge, opening one guacd
 * connection per channel. Handlers run on separate threads and synchronize
 * messages using thread-safe queues.
 */
int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << "\n"
                  << "\t<guacd_ip>: guacd's IP address\n"
                  << "\t<guacd_port>: guacd's listening port\n"
                  << "\t<udp_recv_ip>: address where the broker receives traffic from (hrx_proxy)\n"
                  << "\t<udp_recv_port>: port where the broker receives traffic from\n"
                  << "\t<udp_send_ip>: address where the broker sends guacd traffic to (lrx_proxy)\n"
                  << "\t<udp_send_port>: port where the broker sends guacd traffic to\n"
                  << "\nExample: " << argv[0]
                  << " 127.0.0.1 4822 0.0.0.0 5501 10.0.0.2 5601" << std::endl;
        return 1;
    }

    guacd_ip = argv[1];
    guacd_port = std::stoi(argv[2]);
    udp_recv_ip = argv[3];
    udp_recv_port = std::stoi(argv[4]);
    udp_send_ip = argv[5];
    udp_send_port = std::stoi(argv[6]);

    // Set interrupt handler
    struct sigaction sa{};
    sa.sa_handler = interrupt_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    // Initialize UDP and TCP infrastructure
    int exit;
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

    auto tcp_client = TCPClient(guacd_ip, guacd_port);
    ChannelTable table;
    NetQueue recv_queue;
    NetQueue send_queue;

    // Start the handler threads. The processor gates each channel on approval
    // and dials guacd; the UDP handlers ferry between the bridge and the queues.
    TCPSendHandler tcp_send_handler;
    UDPSendHandler udp_send_handler;
    UDPRecvHandler udp_recv_handler;

    std::thread t_tcp_send =
        tcp_send_handler.Run(recv_queue, send_queue, tcp_client, table);
    std::thread t_udp_send = udp_send_handler.Run(send_queue, udp_sender);
    std::thread t_udp_recv = udp_recv_handler.Run(recv_queue, udp_receiver);

    t_tcp_send.join();
    t_udp_recv.join();
    t_udp_send.join();
}
