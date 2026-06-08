#include "../../include/network/guacamole_server.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

GuacamoleServer::~GuacamoleServer() {
    if (listen_fd >= 0) {
        ::shutdown(listen_fd, SHUT_RDWR);
        ::close(listen_fd);
    }
}

int GuacamoleServer::Initialize() {
    listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // Reuse address if it is already in use or not properly cleaned up
    int one = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(recv_port));

    // Bind and listen to the given port
    if (::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
        perror("bind");
        ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    return 0;
}

int GuacamoleServer::Accept() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int fd = ::accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr),
                      &client_len);
    if (fd < 0) {
        if (errno != EINVAL) // EINVAL: listen socket was shut down
            perror("accept");
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << "Client connected from " << ip_str << ":"
              << ntohs(client_addr.sin_port) << " (fd " << fd << ")"
              << std::endl;

    return fd;
}

int GuacamoleServer::WaitReadable(int fd, int timeout_ms) {
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    int r = ::poll(&pfd, 1, timeout_ms);
    if (r < 0) {
        if (errno == EINTR)
            return 0;
        perror("poll");
        return -1;
    }
    return r > 0 ? 1 : 0;
}

int GuacamoleServer::Receive(int fd, char *buffer, size_t len) {
    ssize_t received = ::recv(fd, buffer, len - 1, 0);

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

ssize_t GuacamoleServer::Send(int fd, const char *buffer, size_t len) {
    if (fd < 0) {
        std::cerr << "Error: cannot send to an invalid fd\n";
        return -1;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t sent = ::send(fd, buffer + total, len - total, 0);
        if (sent < 0) {
            if (errno == EINTR)
                continue;
            perror("send");
            return -1;
        }

        total += sent;
    }

    return total;
}

void GuacamoleServer::Shutdown(int fd) {
    if (fd >= 0)
        ::shutdown(fd, SHUT_RDWR);
}

void GuacamoleServer::Close(int fd) {
    if (fd >= 0)
        ::close(fd);
}
