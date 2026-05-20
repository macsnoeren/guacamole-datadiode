#pragma once

#include <netinet/in.h>
#include <optional>
#include <stdlib.h>
#include <string>
#include <tuple>

class TCPRelay {
  private:
    std::string recv_ip;
    int recv_port;
    sockaddr_in recv_sock_addr;
    int recv_sock_fd;

    int send_sock_fd;
    sockaddr_in send_sock_addr;
    socklen_t send_sock_addr_len = sizeof(send_sock_addr);

  public:
    TCPRelay(std::string recv_ip, int recv_port)
        : recv_ip(recv_ip), recv_port(recv_port) {}
    ~TCPRelay();

    int Initialize();

    std::optional<std::tuple<sockaddr *, socklen_t>> AcceptSender();

    int Receive(char buffer[], size_t len);

    ssize_t Send(const char *buffer, size_t len);
};
