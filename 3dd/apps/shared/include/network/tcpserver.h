#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

/**
 * @brief A TCP server that accepts and serves multiple simultaneous clients
 *
 * Owns only the listening socket. Accepted client sockets are handed back to the
 * caller as raw fds; the caller (via a ChannelTable) decides their lifetime.
 * Receive/Send/Shutdown/Close all operate on a caller-supplied fd, so the same
 * server instance can be used from multiple threads, one per client.
 */
class TCPServer {
  private:
    std::string recv_ip;
    int recv_port;
    int listen_fd = -1;

  public:
    TCPServer(std::string recv_ip, int recv_port)
        : recv_ip(recv_ip), recv_port(recv_port) {}

    /**
     * @brief Closes the listening socket
     */
    ~TCPServer();

    /**
     * @brief Attempts to bind to the server address and listen on it
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Waits for a client to connect (blocking)
     * @return The accepted client fd, or -1 on failure
     */
    int Accept();

    /**
     * @brief Receives traffic from a client fd into buffer (blocking)
     * @return Bytes received, 0 if the client closed, -1 on error
     */
    int Receive(int fd, char buffer[], size_t len);

    /**
     * @brief Sends all bytes in buffer to a client fd
     * @return Amount of bytes sent, or -1 on error
     */
    ssize_t Send(int fd, const char *buffer, size_t len);

    /**
     * @brief Half-closes a client fd, waking any blocking Receive on it
     */
    void Shutdown(int fd);

    /**
     * @brief Closes a client fd
     */
    void Close(int fd);
};
