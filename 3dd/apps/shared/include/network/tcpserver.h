#pragma once

#include <netinet/in.h>
#include <optional>
#include <stdlib.h>
#include <string>
#include <tuple>

/**
 * @brief A bare-bones TCP server implementation
 */
class TCPServer {
  private:
    std::string recv_ip;
    int recv_port;
    sockaddr_in recv_sock_addr;
    int recv_sock_fd = -1;

    int client_sock_fd = -1;
    sockaddr_in client_sock_addr;
    socklen_t client_sock_addr_len = sizeof(client_sock_addr);

  public:
    TCPServer(std::string recv_ip, int recv_port)
        : recv_ip(recv_ip), recv_port(recv_port) {}

    /**
     * @brief Closes the client and server connections
     */
    ~TCPServer();

    /**
     * @brief Attempts to bind to the server address and listen on it
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Waits for a client to connect and stores its socket FD
     * @return The socket FD on success, else std::nullopt
     */
    std::optional<std::tuple<sockaddr_in, socklen_t>> AcceptSender();

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
