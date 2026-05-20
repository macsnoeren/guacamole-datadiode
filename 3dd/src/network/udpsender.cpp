#include "../../include/network/udpsender.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int UDPSender::Initialize() {
    sock_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    std::memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(static_cast<uint16_t>(port));
    int res = ::inet_pton(AF_INET, host.c_str(), &sock_addr.sin_addr);

    if (res <= 0) {
        perror("inet_pton");
        ::close(sock_fd);
        return 1;
    }

    return 0;
}

ssize_t UDPSender::Send(const char *buffer, size_t len) {
    ssize_t sent =
        ::sendto(sock_fd, buffer, len, 0,
                 reinterpret_cast<sockaddr *>(&sock_addr), sizeof(sock_addr));
    if (sent < 0) {
        perror("sendto");
        ::close(sock_fd);
        return 1;
    }
    return 0;
}

void UDPSender::Close() {
    ::close(sock_fd);
}
