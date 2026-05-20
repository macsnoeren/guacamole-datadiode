#pragma once

#include <string>
#include <netinet/in.h>
#include <stdlib.h>

class UDPReceiver {
  private:
    std::string host;
    int port;
    int sock_fd;

  public:
    UDPReceiver(int port) : port(port) {}

    int Initialize();

    int Receive(char buffer[], size_t len);

    void Close();
};
