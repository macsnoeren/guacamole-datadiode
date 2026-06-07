#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

/**
 * @brief Simple UDP sender implementation
 */
class UDPSender {
  private:
    std::string host;
    int port;
    sockaddr_in sock_addr;
    int sock_fd = -1;

  public:
    UDPSender(std::string host, int port) : host(host), port(port) {}

    /**
     * @brief Closes the socket
     */
    ~UDPSender();

    /**
     * @brief Opens the socket to send to
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Sends all bytes in buffer
     * @return How many bytes were sent
     */
    ssize_t Send(const char *buffer, size_t len);
};
