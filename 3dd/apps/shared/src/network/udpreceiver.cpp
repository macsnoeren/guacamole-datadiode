#include "../../include/network/udpreceiver.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

UDPReceiver::~UDPReceiver() {
    if (sock_fd >= 0) {
        ::shutdown(sock_fd, SHUT_RDWR);
        ::close(sock_fd);
    }
}

int UDPReceiver::Initialize() {
    sock_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    // Bind to the given port
    if (::bind(sock_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
        perror("bind");
        ::close(sock_fd);
        return 1;
    }

    // Time out a blocked recvfrom periodically so the receive loop can notice a
    // shutdown request (the `running` flag) instead of blocking forever. SIGINT
    // is delivered to one arbitrary thread, so we cannot rely on EINTR waking
    // this one.
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200 ms
    ::setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return 0;
}

int UDPReceiver::Receive(char *buffer, size_t len) {
    sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);

    ssize_t received =
        ::recvfrom(sock_fd, buffer, len - 1, 0,
                   reinterpret_cast<sockaddr *>(&src_addr), &src_len);

    if (received < 0) {
        // Timed out (no data) or interrupted: benign, let the caller re-check
        // whether it should keep running.
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return 0;
        perror("recvfrom");
        return -1;
    }

    buffer[received] = '\0'; // make it a C-string for printing

    return received;
}
