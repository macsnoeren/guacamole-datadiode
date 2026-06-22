#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

/**
 * @brief A TCP client that can open multiple simultaneous connections
 *
 * Each Connect() dials the configured server and returns a fresh fd, so one
 * client instance can hold many concurrent connections (one per channel).
 * Receive/Send/Shutdown/Close all operate on a caller-supplied fd, so the same
 * instance can be used from multiple threads, one per connection.
 */
class GuacdClient {
  private:
    std::string server_ip;
    int server_port;

  public:
    GuacdClient(std::string server_ip, int server_port)
        : server_ip(server_ip), server_port(server_port) {}

    ~GuacdClient() = default;

    /**
     * @brief Opens a new connection to the configured server
     * @return The connected fd, or -1 on failure
     */
    int Connect();

    /**
     * @brief Receives traffic from a connection fd into buffer (blocking)
     * @return Bytes received, 0 if the server closed, -1 on error
     */
    int Receive(int fd, char buffer[], size_t len);

    /**
     * @brief Sends all bytes in buffer to a connection fd
     * @return Amount of bytes sent, or -1 on error
     */
    ssize_t Send(int fd, const char *buffer, size_t len);

    /**
     * @brief Half-closes a connection fd, waking any blocking Receive on it
     */
    void Shutdown(int fd);

    /**
     * @brief Closes a connection fd
     */
    void Close(int fd);
};
