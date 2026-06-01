#include "../../shared/include/network/netqueue.h"
#include "../../shared/include/network/tcpclient.h"
#include "../../shared/include/network/udpreceiver.h"
#include "../../shared/include/network/udpsender.h"
#include <arpa/inet.h>
#include <atomic>
#include <iostream>
#include <optional>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <tuple>
#include <unistd.h>

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
 * @brief Checks the network queue and sends any messages using a TCP client
 */
void tcp_send_handler(TCPClient &tcp_client, NetQueue &recv_queue) {
    while (running) {
        std::string msg = recv_queue.Dequeue();

        if (tcp_client.Send(msg.data(), msg.size()) < 0)
            break;

        std::stringstream info;
        info << "tcp_send_handler: Sent message '" << msg.c_str() << std::endl;
        std::cout << info.str() << std::endl;
    }
}

/*
 * @brief Checks the TCP client socket for data and queues it in the network queue
 */
void tcp_recv_handler(TCPClient &tcp_client, NetQueue &send_queue) {
    char buffer[1200];

    while (running) {
        int received;

        if ((received = tcp_client.Receive(buffer, sizeof(buffer))) > 0) {
            std::stringstream info;
            info << "tcp_recv_handler: Received message '" << buffer
                 << std::endl;
            std::cout << info.str() << std::endl;

            send_queue.Enqueue(std::string(buffer, received));
        } else if (received == 0) {
            std::cout << "Client disconnected" << std::endl;
            running = false;
        } else {
            running = false;
            break;
        }
    }
}

/*
 * @brief Checks the UDP receive socket for data and queues it in the network queue
 */
void udp_recv_handler(UDPReceiver &udp_receiver, NetQueue &recv_queue) {
    char buffer[1200]; // UDP max packet size

    while (running) {
        int received = udp_receiver.Receive(buffer, sizeof(buffer));
        recv_queue.Enqueue(std::string(buffer, received));

        std::stringstream info;
        info << "udp_recv_handler: Queued " << received << " bytes"
             << std::endl;
        std::cout << info.str() << std::endl;
    }
}

/*
 * @brief Checks the network queue for data and sends it on the UDP send socket
 */
void udp_send_handler(UDPSender &udp_sender, NetQueue &send_queue) {
    while (running) {
        std::string msg = send_queue.Dequeue();
        udp_sender.Send(msg.data(), msg.size());

        std::stringstream info;
        info << "udp_send_handler: Sent " << msg.size() << " bytes"
             << std::endl;
        std::cout << info.str() << std::endl;
    }
}

/*
 * @brief Starts the guacd broker that imitates the Guacamole web server and bridges Guacamole traffic.
 *
 * Starts a TCP client, UDP sender and UDP receiver for managing bridge traffic.
 * These handlers run on different threads and synchronize messages using a thread-safe queue.
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
              << udp_recv_port << std::endl;

    auto recv_queue = NetQueue();
    auto send_queue = NetQueue();

    // Before making the TCP connection, wait until a connection request is made
    char buffer[1200];
    int received = udp_receiver.Receive(buffer, sizeof(buffer));
    recv_queue.Enqueue(std::string(buffer, received));

    auto tcp_client = TCPClient(guacd_ip, guacd_port);
    if ((exit = tcp_client.Initialize()) != 0)
        return exit;

    std::cout << "Connected to address " << guacd_ip << ":" << guacd_port
              << std::endl;

    // Start threads
    std::thread t_tcp_send(tcp_send_handler, std::ref(tcp_client),
                           std::ref(recv_queue));
    std::thread t_tcp_recv(tcp_recv_handler, std::ref(tcp_client),
                           std::ref(send_queue));
    std::thread t_udp_send(udp_send_handler, std::ref(udp_sender),
                           std::ref(send_queue));
    std::thread t_udp_recv(udp_recv_handler, std::ref(udp_receiver),
                           std::ref(recv_queue));

    t_tcp_send.join();
    t_tcp_recv.join();
    t_udp_recv.join();
    t_udp_send.join();
}
