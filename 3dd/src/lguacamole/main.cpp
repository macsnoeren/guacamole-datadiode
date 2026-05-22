#include "../../include/network/netqueue.h"
#include "../../include/network/tcpserver.h"
#include "../../include/network/udpreceiver.h"
#include "../../include/network/udpsender.h"
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

const char *guac_listen_ip;
int guac_listen_port;
const char *udp_recv_ip;
int udp_recv_port;
const char *udp_send_ip;
int udp_send_port;
std::atomic<bool> running = true;

void interrupt_handler(int signum) {
    std::cout << "Stopping program..." << std::endl;
    running = false;
}

void tcp_send_handler(TCPServer &tcp_server, NetQueue &recv_queue) {
    while (running) {
        std::string msg = recv_queue.Dequeue();

        if (tcp_server.Send(msg.data(), msg.size()) < 0)
            break;

        std::stringstream info;
        info << "tcp_send_handler: Sent message '" << msg.c_str() << std::endl;
        std::cout << info.str() << std::endl;
    }
}

void tcp_recv_handler(TCPServer &tcp_server, NetQueue &send_queue) {
    char buffer[65535];

    while (running) {
        int received;

        if ((received = tcp_server.Receive(buffer, sizeof(buffer))) > 0) {
            std::stringstream info;
            info << "tcp_recv_handler: Received message '" << buffer
                 << std::endl;
            std::cout << info.str() << std::endl;

            send_queue.Enqueue(std::string(buffer, received));
        } else if (received == 0) {
            std::cout << "Client disconnected";
        } else {
            running = false;
            break;
        }
    }
}

void udp_recv_handler(UDPReceiver &udp_receiver, NetQueue &recv_queue) {
    char buffer[65535]; // UDP max packet size

    while (running) {
        int received = udp_receiver.Receive(buffer, sizeof(buffer));
        recv_queue.Enqueue(std::string(buffer, received));

        std::stringstream info;
        info << "udp_recv_handler: Queued " << received << " bytes"
             << std::endl;
        std::cout << info.str() << std::endl;
    }
}

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

int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <guac_listen_ip> <guac_listen_port> <udp_recv_ip> "
                     "<udp_recv_port> <udp_send_ip> <udp_send_port>\n"
                  << "Example: " << argv[0]
                  << " 127.0.0.1 4822 0.0.0.0 5501 10.0.0.2 5601\n";
        return 1;
    }

    guac_listen_ip = argv[1];
    guac_listen_port = std::stoi(argv[2]);
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
    auto tcp_server = TCPServer(guac_listen_ip, guac_listen_port);
    if ((exit = tcp_server.Initialize()) != 0)
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
              << udp_recv_port << std::endl;

    auto recv_queue = NetQueue();
    auto send_queue = NetQueue();

    std::optional<std::tuple<sockaddr_in, socklen_t>> client_result;

    // Accept the first client trying to connect (Guacamole)
    if ((client_result = tcp_server.AcceptSender()) != std::nullopt) {
        sockaddr_in client_addr = std::get<0>(client_result.value());
        char ip_str[INET_ADDRSTRLEN];

        ::inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        uint16_t port = ntohs(client_addr.sin_port);

        std::cout << "Client connected from " << ip_str << ":" << port
                  << std::endl;
    } else {
        return 1;
    }

    // Start threads
    std::thread t_tcp_send(tcp_send_handler, std::ref(tcp_server),
                           std::ref(recv_queue));
    std::thread t_tcp_recv(tcp_recv_handler, std::ref(tcp_server),
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
