#include "../../include/network/tcpsender.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

TCPSender::~TCPSender() {
    ::close(sock_fd);
}

int TCPSender::Initialize() {
    sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    std::memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(static_cast<uint16_t>(port));
    int addr_res = ::inet_pton(AF_INET, host.c_str(), &sock_addr.sin_addr);

    if (addr_res <= 0) {
        perror("inet_pton");
        ::close(sock_fd);
        return 1;
    }

    int conn_res = ::connect(sock_fd, reinterpret_cast<sockaddr*>(&sock_addr), sizeof(sock_addr));
    if (conn_res < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connection with " << host << " established\n";

    return 0;
}

ssize_t TCPSender::Send(const char *buffer, size_t len) {
    ssize_t sent = ::send(sock_fd, buffer, len, 0);
    if (sent < 0) {
        perror("sendto");
        ::close(sock_fd);
        return 1;
    }
    return 0;
}
