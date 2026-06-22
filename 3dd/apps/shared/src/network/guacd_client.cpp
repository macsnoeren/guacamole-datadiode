#include "../../include/network/guacd_client.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
// How long a write to guacd may block before it fails. A single thread
// (GuacdSendHandler) writes to every guacd connection; without a bound, one
// stalled connection (guacd not reading its input) would block that thread
// indefinitely and starve other channels' sync acks, so guacd reports them
// "not responding". On timeout the write fails and only that channel is torn
// down, leaving the others healthy. Tunable via GUACD_SEND_TIMEOUT_MS.
int guacd_send_timeout_ms() {
    const char *env = std::getenv("GUACD_SEND_TIMEOUT_MS");
    int v = env ? std::atoi(env) : 0;
    return v > 0 ? v : 2000;
}

// How long Receive() blocks before reporting an idle timeout, so the reader can
// re-send a keepalive to guacd. The browser's own periodic keepalive is swallowed
// on the forward path, so without this an idle session would trip guacd's
// "user not responding" timeout (~15 s). Must stay comfortably under it.
int guacd_recv_timeout_ms() {
    const char *env = std::getenv("GUACD_KEEPALIVE_MS");
    int v = env ? std::atoi(env) : 0;
    return v > 0 ? v : 3000;
}
} // namespace

int GuacdClient::Connect() {
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

    if (fd >= 0) {
        // Bound how long a write to this guacd connection may block, so a
        // stalled connection can't head-of-line block the shared writer thread
        // and starve other channels' sync acks.
        int sms = guacd_send_timeout_ms();
        struct timeval stv{};
        stv.tv_sec = sms / 1000;
        stv.tv_usec = (sms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &stv, sizeof(stv));

        // Receive timeout so the reader wakes periodically (when guacd is idle)
        // to re-send a keepalive — otherwise an idle session is silent toward
        // guacd and trips its "user not responding" timeout.
        int rms = guacd_recv_timeout_ms();
        struct timeval rtv{};
        rtv.tv_sec = rms / 1000;
        rtv.tv_usec = (rms % 1000) * 1000;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));
    }

    return fd;
}

int GuacdClient::Receive(int fd, char *buffer, size_t len) {
    ssize_t received = ::recv(fd, buffer, len - 1, 0);

    if (received < 0) {
        // Receive timeout: idle, but the connection is alive.
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return RECV_TIMEOUT;
        perror("recv");
        return -1;
    }

    buffer[received] = '\0'; // make it a C-string for printing

    return received;
}

ssize_t GuacdClient::Send(int fd, const char *buffer, size_t len) {
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
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cerr << "guacd_client: send to fd " << fd << " timed out "
                             "(connection stalled); tearing it down" << std::endl;
            else
                perror("send");
            return -1;
        }

        total += sent;
    }

    return total;
}

void GuacdClient::Shutdown(int fd) {
    if (fd >= 0)
        ::shutdown(fd, SHUT_RDWR);
}

void GuacdClient::Close(int fd) {
    if (fd >= 0)
        ::close(fd);
}
