#pragma once

#include <netinet/in.h>
#include <optional>
#include <stdlib.h>
#include <string>

class TCPReceiver {
  private:
    std::string host;
    int port;
    int sock_fd;

    int client_fd;
    sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

  public:
    TCPReceiver(int port) : port(port) {}
    ~TCPReceiver();

    int Initialize();

    std::optional<std::tuple<sockaddr*, socklen_t>> AcceptSender();

    int Receive(char buffer[], size_t len);
};
