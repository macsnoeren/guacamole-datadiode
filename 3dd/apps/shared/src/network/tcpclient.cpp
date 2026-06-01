#include "../../include/network/tcpclient.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

TCPClient::~TCPClient() {
    if (recv_sock_fd >= 0) {
        ::shutdown(recv_sock_fd, SHUT_RDWR);
        ::close(recv_sock_fd);
    }
}

int TCPClient::Initialize() {
    // int one = 1;
    // ::setsockopt(recv_sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct addrinfo hints, *res, *p;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; // TCP

    std::string port_str = std::to_string(server_port);

    // Get socket for address
    int status = getaddrinfo(server_ip.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    // Loop through address infos until a valid one is found
    int ret = -1;
    for (p = res; p != nullptr; p = p->ai_next) {
        recv_sock_fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (recv_sock_fd == -1)
            continue;

        if (::connect(recv_sock_fd, p->ai_addr, p->ai_addrlen) == 0) {
            ret = 0; // success
            break;
        }

        perror("connect");
        ::close(recv_sock_fd);
    }
    freeaddrinfo(res);

    return ret;
}

int TCPClient::Receive(char *buffer, size_t len) {
    ssize_t received = ::recv(recv_sock_fd, buffer, len - 1, 0);

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

ssize_t TCPClient::Send(const char *buffer, size_t len) {
    if (recv_sock_fd < 0) {
        std::cerr << "Error: cannot send if no client is connected\n";
    }

    // Keep sending until no data is left in the buffer
    size_t total = 0;
    while (total < len) {
        ssize_t sent = ::send(recv_sock_fd, buffer + total, len - total, 0);
        if (sent < 0 && errno != EINTR) {
            perror("sendto");
            return -1;
        }

        total += sent;
    }

    return total;
}
