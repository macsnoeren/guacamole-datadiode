#include "../../include/network/tcpserver.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <optional>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>

TCPServer::~TCPServer() {
    if (client_sock_fd >= 0) {
        ::shutdown(client_sock_fd, SHUT_RDWR);
        ::close(client_sock_fd);
    }

    if (recv_sock_fd >= 0) {
        ::shutdown(recv_sock_fd, SHUT_RDWR);
        ::close(recv_sock_fd);
    }
}

int TCPServer::Initialize() {
    // Initialize receiver
    recv_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (recv_sock_fd < 0) {
        perror("socket");
        return 1;
    }

    // Reuse address if it is already in use or not properly cleaned up
    int one = 1;
    ::setsockopt(recv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    std::memset(&client_sock_addr, 0, sizeof(client_sock_addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(recv_port));

    // Bind and listen to the given port
    if (::bind(recv_sock_fd, reinterpret_cast<sockaddr *>(&addr),
               sizeof(addr)) < 0) {
        perror("bind");
        ::close(recv_sock_fd);
        return 1;
    }

    if (::listen(recv_sock_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    return 0;
}

std::optional<std::tuple<sockaddr_in, socklen_t>> TCPServer::AcceptSender() {
    client_sock_fd =
        ::accept(recv_sock_fd, reinterpret_cast<sockaddr *>(&client_sock_addr),
                 &client_sock_addr_len);
    if (client_sock_fd < 0) {
        perror("accept");
        return std::nullopt;
    }

    return std::make_tuple(client_sock_addr, client_sock_addr_len);
}

int TCPServer::Receive(char *buffer, size_t len) {
    ssize_t received = ::recv(client_sock_fd, buffer, len - 1, 0);

    if (received < 0) {
        switch (errno) {
        case EAGAIN:
            // No data available, not an error
            return 0;
        default:
            perror("recv");
            return -1;
        }
    }

    buffer[received] = '\0'; // make it a C-string for printing

    return received;
}

ssize_t TCPServer::Send(const char *buffer, size_t len) {
    if (client_sock_fd < 0) {
        std::cerr << "Error: cannot send if no client is connected\n";
    }

    size_t total = 0;
    while (total < len) {
        ssize_t sent = ::send(client_sock_fd, buffer + total, len - total, 0);
        if (sent < 0 && errno != EINTR) {
            perror("sendto");
            return -1;
        }

        total += sent;
    }

    return total;
}
