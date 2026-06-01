#pragma once

#include <netinet/in.h>
#include <optional>
#include <stdlib.h>
#include <string>
#include <tuple>

/**
 * @brief A bare-bones TCP client implementation
 */
class TCPClient {
  private:
    std::string server_ip;
    int server_port;
    int recv_sock_fd = -1;
    sockaddr_in recv_sock_addr;
    socklen_t recv_sock_addr_len = sizeof(recv_sock_addr);

  public:
    TCPClient(std::string recv_ip, int recv_port)
        : server_ip(recv_ip), server_port(recv_port) {}

    /**
     * @brief Closes the connection
     */
    ~TCPClient();

    /**
     * @brief Attempts to connect to the address
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Receives network traffic in a buffer (blocking)
     */
    int Receive(char buffer[], size_t len);

    /**
     * @brief Sends network traffic from a buffer
     * @return Amount of bytes sent
     */
    ssize_t Send(const char *buffer, size_t len);
};
