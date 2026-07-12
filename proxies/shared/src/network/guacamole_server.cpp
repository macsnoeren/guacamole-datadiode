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

#include "../../include/network/guacamole_server.h"
#include "../../include/util/tls.h"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

GuacamoleServer::~GuacamoleServer() {
    if (listen_fd >= 0) {
        ::shutdown(listen_fd, SHUT_RDWR);
        ::close(listen_fd);
    }
    if (ssl_ctx)
        SSL_CTX_free(ssl_ctx);
}

ssl_st *GuacamoleServer::SslFor(int fd) {
    std::lock_guard<std::mutex> lock(ssl_mtx);
    auto it = ssl_by_fd.find(fd);
    return it == ssl_by_fd.end() ? nullptr : it->second;
}

bool GuacamoleServer::HasPending(int fd) {
    if (!tls_on)
        return false;
    SSL *ssl = SslFor(fd);
    return ssl && SSL_pending(ssl) > 0;
}

int GuacamoleServer::InitializeTls() {
    // A write to a peer that has gone away raises SIGPIPE, which defaults to
    // terminating the process. SSL_write (and the plaintext Send loop) can both
    // hit that on an abrupt browser disconnect, so ignore it and rely on the
    // EPIPE return instead.
    ::signal(SIGPIPE, SIG_IGN);

    // OpenSSL 1.1.0+ initializes itself on first use, so no explicit
    // library/algorithm setup is needed here.
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        std::cerr << "TLS: SSL_CTX_new failed" << std::endl;
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // Refuse anything below TLS 1.2; the web server (TLS client) negotiates up.
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);

    const std::string cert = tls_cert_path();
    const std::string key = tls_key_path();

    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert.c_str()) != 1) {
        std::cerr << "TLS: failed to load certificate chain from " << cert
                  << std::endl;
        ERR_print_errors_fp(stderr);
        return -1;
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key.c_str(), SSL_FILETYPE_PEM) !=
        1) {
        std::cerr << "TLS: failed to load private key from " << key << std::endl;
        ERR_print_errors_fp(stderr);
        return -1;
    }
    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
        std::cerr << "TLS: private key does not match certificate" << std::endl;
        ERR_print_errors_fp(stderr);
        return -1;
    }

    std::cout << "TLS enabled: loaded certificate " << cert << " and key " << key
              << std::endl;
    return 0;
}

int GuacamoleServer::Initialize() {
    struct addrinfo hints{}, *results, *rp;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Get a linked list of possible addresses based on host, port, and hints
    if (::getaddrinfo(host.c_str(), std::to_string(recv_port).c_str(), &hints, &results) != 0) {
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
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    bool resolved = rp != nullptr;
    if (resolved) {
        struct sockaddr_in *addr_in =
            reinterpret_cast<struct sockaddr_in *>(rp->ai_addr);
        addr.sin_family = addr_in->sin_family;
        addr.sin_port = addr_in->sin_port;
        addr.sin_addr = addr_in->sin_addr;
    }

    ::freeaddrinfo(results);

    if (!resolved) {
        std::cerr << "Could not resolve hostname or address: " << host << std::endl;
        if (fd >= 0)
            ::close(fd);
        return -1;
    }

    // Reuse the socket opened in the resolve loop above rather than opening a
    // second one (which would leak the first fd).
    listen_fd = fd;

    // Reuse address if it is already in use or not properly cleaned up
    int one = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Bind and listen to the given port
    if (::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
        perror("bind");
        if (listen_fd >= 0)
            ::close(listen_fd);
        return 1;
    }

    if (::listen(listen_fd, 16) < 0) {
        perror("listen");
        if (listen_fd >= 0)
            ::close(listen_fd);
        return 1;
    }

    // Time out a blocked accept periodically so the accept loop can notice a
    // shutdown request (the `running` flag) instead of blocking forever; SIGINT
    // may be delivered to a different thread, so EINTR can't be relied on.
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200 ms
    ::setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Maintainer toggle: stand up the TLS context only when explicitly enabled.
    // A configured-but-broken TLS setup must abort startup rather than silently
    // serving plaintext, which would be a security downgrade.
    tls_on = tls_enabled();
    if (tls_on) {
        if (InitializeTls() != 0) {
            std::cerr << "TLS: initialization failed, refusing to start"
                      << std::endl;
            return 1;
        }
    } else {
        std::cout << "TLS disabled: serving the web server in plaintext"
                  << std::endl;
    }

    return 0;
}

int GuacamoleServer::Accept() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int fd = ::accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr),
                      &client_len);
    if (fd < 0) {
        // EINVAL: listen socket was shut down. EAGAIN/EWOULDBLOCK: accept timed
        // out (no pending connection). EINTR: interrupted. All benign — the
        // caller re-checks `running` and either retries or stops.
        if (errno != EINVAL && errno != EAGAIN && errno != EWOULDBLOCK &&
            errno != EINTR)
            perror("accept");
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << "Client connected from " << ip_str << ":"
              << ntohs(client_addr.sin_port) << " (fd " << fd << ")"
              << std::endl;

    // In TLS mode, wrap the connection now but don't drive the handshake here:
    // SSL_set_accept_state() lets the first SSL_read/SSL_write on the owning
    // thread negotiate it, so a slow client can't stall the accept loop.
    if (tls_on) {
        SSL *ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            std::cerr << "TLS: SSL_new failed for fd " << fd << std::endl;
            ERR_print_errors_fp(stderr);
            ::close(fd);
            return -1;
        }
        SSL_set_fd(ssl, fd);
        SSL_set_accept_state(ssl);
        std::lock_guard<std::mutex> lock(ssl_mtx);
        ssl_by_fd[fd] = ssl;
    }

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
    if (tls_on) {
        SSL *ssl = SslFor(fd);
        if (!ssl)
            return -1;
        int n = SSL_read(ssl, buffer, static_cast<int>(len - 1));
        if (n > 0) {
            buffer[n] = '\0'; // make it a C-string for printing
            return n;
        }
        switch (SSL_get_error(ssl, n)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // Handshake in progress or a partial record: nothing to deliver yet.
            return RETRY;
        case SSL_ERROR_ZERO_RETURN:
            return 0; // peer sent close_notify: orderly TLS shutdown
        default:
            return -1;
        }
    }

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

    if (tls_on) {
        if (len == 0)
            return 0;
        SSL *ssl = SslFor(fd);
        if (!ssl)
            return -1;
        // The fd is blocking and partial writes aren't enabled, so SSL_write
        // either writes everything or fails. (Renegotiation, which could ask to
        // read mid-write, doesn't occur in TLS 1.2/1.3 here.)
        int n = SSL_write(ssl, buffer, static_cast<int>(len));
        if (n <= 0)
            return -1;
        return n;
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
    // Socket-level half-close only: this is the cross-thread wake for a reader
    // blocked in poll/recv, so it must NOT touch the SSL object (which belongs
    // to the reader thread). The reader does the SSL teardown in Close().
    if (fd >= 0)
        ::shutdown(fd, SHUT_RDWR);
}

void GuacamoleServer::Close(int fd) {
    if (tls_on) {
        SSL *ssl = nullptr;
        {
            std::lock_guard<std::mutex> lock(ssl_mtx);
            auto it = ssl_by_fd.find(fd);
            if (it != ssl_by_fd.end()) {
                ssl = it->second;
                ssl_by_fd.erase(it);
            }
        }
        if (ssl) {
            // Best-effort close_notify; don't wait for the peer's. The socket may
            // already be shut down (EPIPE), which is fine — we free regardless.
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
    }

    if (fd >= 0)
        ::close(fd);
}
