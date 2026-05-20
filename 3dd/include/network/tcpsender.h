#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

class TCPSender {
  private:
    std::string host;
    int port;
    sockaddr_in sock_addr;
    int sock_fd;

  public:
    TCPSender(std::string host, int port) : host(host), port(port) {}
    ~TCPSender();

    int Initialize();

    ssize_t Send(const char *buffer, size_t len);
};
