#include "../../include/network/tcpclient.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int TCPClient::Connect() {
    struct addrinfo hints, *res, *p;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP

    std::string port_str = std::to_string(server_port);

    // Get socket for address
    int status =
        getaddrinfo(server_ip.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    // Loop through address infos until a valid one is found
    int fd = -1;
    for (p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1)
            continue;

        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0)
            break; // success

        perror("connect");
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    return fd;
}

int TCPClient::Receive(int fd, char *buffer, size_t len) {
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

ssize_t TCPClient::Send(int fd, const char *buffer, size_t len) {
    if (fd < 0) {
        std::cerr << "Error: cannot send to an invalid fd\n";
        return -1;
    }

    // Keep sending until no data is left in the buffer
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

void TCPClient::Shutdown(int fd) {
    if (fd >= 0)
        ::shutdown(fd, SHUT_RDWR);
}

void TCPClient::Close(int fd) {
    if (fd >= 0)
        ::close(fd);
}
