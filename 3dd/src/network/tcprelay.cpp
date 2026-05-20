#include "../../include/network/tcprelay.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <optional>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>

TCPRelay::~TCPRelay() {
    ::close(send_sock_fd);
    ::close(recv_sock_fd);
}

int TCPRelay::Initialize() {
    // Initialize receiver
    recv_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (recv_sock_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    std::memset(&src_addr, 0, sizeof(src_addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(recv_port));

    // Bind to the given port
    if (::bind(recv_sock_fd, reinterpret_cast<sockaddr *>(&addr),
               sizeof(addr)) < 0) {
        perror("bind");
        ::close(recv_sock_fd);
        return 1;
    }

    if (::listen(recv_sock_fd, 0) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Listening on TCP port " << recv_port << "...\n";

    return 0;
}

std::optional<std::tuple<sockaddr *, socklen_t>> TCPRelay::AcceptSender() {
    send_sock_fd = ::accept(
        recv_sock_fd, reinterpret_cast<sockaddr *>(&src_addr), &src_addr_len);
    if (send_sock_fd < 0) {
        perror("accept");
        return std::nullopt;
    }

    return std::make_tuple(reinterpret_cast<sockaddr *>(&src_addr),
                           src_addr_len);
}

int TCPRelay::Receive(char *buffer, size_t len) {
    ssize_t received = ::recv(send_sock_fd, buffer, len - 1, 0);

    if (received < 0) {
        perror("recv");
        return -1;
    }

    buffer[received] = '\0'; // make it a C-string for printing

    return received;
}

ssize_t TCPRelay::Send(const char *buffer, size_t len) {
    ssize_t sent = ::send(send_sock_fd, buffer, len, 0);
    if (sent < 0) {
        perror("sendto");
        ::close(send_sock_fd);
        return 1;
    }
    return 0;
}
