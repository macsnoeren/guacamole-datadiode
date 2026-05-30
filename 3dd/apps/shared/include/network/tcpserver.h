#pragma once

#include <netinet/in.h>
#include <optional>
#include <stdlib.h>
#include <string>
#include <tuple>

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
    ~TCPServer();

    int Initialize();

    std::optional<std::tuple<sockaddr_in, socklen_t>> AcceptSender();

    int Receive(char buffer[], size_t len);

    ssize_t Send(const char *buffer, size_t len);
};
