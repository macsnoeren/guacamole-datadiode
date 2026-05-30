#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

class UDPReceiver {
  private:
    std::string host;
    int port;
    int sock_fd;

  public:
    UDPReceiver(int port) : port(port) {}
    ~UDPReceiver();

    int Initialize();

    int Receive(char buffer[], size_t len);
};
