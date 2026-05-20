#pragma once

#include <string>
#include <netinet/in.h>
#include <stdlib.h>

class UDPSender {
private:
    std::string host;
    int port;
    sockaddr_in sock_addr;
    int sock_fd;

public:
    UDPSender(std::string host, int port) : host(host), port(port) {}

    int Initialize();

    ssize_t Send(const char* buffer, size_t len);
};
