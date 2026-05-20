#include "../../include/network/tcpreceiver.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <optional>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>

TCPReceiver::~TCPReceiver() {
    ::close(sock_fd);
}

int TCPReceiver::Initialize() {
    sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    std::memset(&src_addr, 0, sizeof(src_addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    // Bind to the given port
    if (::bind(sock_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        ::close(sock_fd);
        return 1;
    }

    if (::listen(sock_fd, 0) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Listening on TCP port " << port << "...\n";

    return 0;
}

std::optional<std::tuple<sockaddr*, socklen_t>> TCPReceiver::AcceptSender() {
    client_fd = ::accept(sock_fd, reinterpret_cast<sockaddr*>(&src_addr), &src_addr_len);
    if (client_fd < 0) {
        perror("accept");
        return std::nullopt;
    }

    return std::make_tuple(reinterpret_cast<sockaddr*>(&src_addr), src_addr_len);
}

int TCPReceiver::Receive(char *buffer, size_t len) {
    ssize_t received = ::recv(client_fd, buffer, len - 1, 0);

    if (received < 0) {
        perror("recv");
        return -1;
    }

    buffer[received] = '\0'; // make it a C-string for printing

    return received;
}
