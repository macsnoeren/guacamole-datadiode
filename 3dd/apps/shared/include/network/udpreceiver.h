#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

class UDPReceiver {
  private:
    std::string host;
    int port;
    int sock_fd;

  public:
    UDPReceiver(int port) : port(port) {}

    /**
     * @brief Closes the socket
     */
    ~UDPReceiver();

    /**
     * @brief Opens the socket to receive on
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Receive UDP messages in buffer
     * @return How many bytes were received
     */
    int Receive(char buffer[], size_t len);
};
