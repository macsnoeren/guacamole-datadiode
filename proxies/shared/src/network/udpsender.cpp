/*
 * Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
 * Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../../include/network/udpsender.h"
#include "../../include/util/sockbuf.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

UDPSender::~UDPSender() {
    if (sock_fd >= 0) {
        ::shutdown(sock_fd, SHUT_RDWR);
        ::close(sock_fd);
    }
}

int UDPSender::Initialize() {
    struct addrinfo hints{}, *results, *rp;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    // Get a linked list of possible addresses based on host, port, and hints
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &results) != 0) {
        perror("getaddrinfo");
        return -1;
    }

    int fd = -1;
    
    // Loop through list until a valid socket is found
    for (rp = results; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd >= 0)
            break;
    }

    // Copy the resolved address out of the addrinfo list *before* freeing it:
    // `rp` points into `results`, so any read of `rp->ai_addr` after
    // freeaddrinfo() would be a use-after-free.
    bool resolved = rp != nullptr;
    if (resolved) {
        struct sockaddr_in *addr_in =
            reinterpret_cast<struct sockaddr_in *>(rp->ai_addr);
        std::memset(&sock_addr, 0, sizeof(sock_addr));
        sock_addr.sin_family = addr_in->sin_family;
        sock_addr.sin_port = addr_in->sin_port;
        sock_addr.sin_addr = addr_in->sin_addr;
    }

    ::freeaddrinfo(results);

    if (!resolved) {
        std::cerr << "Could not resolve hostname or address: " << host << std::endl;
        if (fd >= 0)
            ::close(fd);
        return -1;
    }

    sock_fd = fd;

    // Enlarge the send buffer so a burst of datagrams doesn't overflow it (a
    // failed sendto silently drops the datagram, and the bridge has no
    // retransmit).
    set_bridge_sockbuf(sock_fd, SO_SNDBUF, "UDPSender SO_SNDBUF");

    return 0;
}

ssize_t UDPSender::Send(const char *buffer, size_t len) {
    size_t total = 0;

    // Keep sending while the buffer is not empty
    while (total < len) {
        ssize_t sent = ::sendto(sock_fd, buffer + total, len - total, 0,
                                reinterpret_cast<sockaddr *>(&sock_addr),
                                sizeof(sock_addr));
        if (sent < 0) {
            if (errno == EINTR)
                continue; // interrupted before anything was sent: retry
            // Leave the socket open: the destructor is the sole closer, so a
            // transient failure just drops this datagram instead of leaving a
            // stale (possibly reused) fd that breaks the next Send.
            perror("sendto");
            return -1;
        }

        total += sent;
    }

    return total;
}
