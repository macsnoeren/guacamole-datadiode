/*
Copyright (C) 2025 Maurice Snoeren

This program is free software: you can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation, version 3.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see https://www.gnu.org/licenses/.
*/
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

/*
 * Class implementing a TCP client that connects to a TCP server.
 * Provides methods to start the connection, send data, receive data, and close the connection.
 */
class TCPClient {
private:
    std::string host;                ///< Host to connect to (IP address or hostname)
    int port;                        ///< Port to connect to
    struct sockaddr_in socketAddrServer;  ///< Server address information
    int socketFd;                    ///< Socket file descriptor
    socklen_t socketLen;             ///< Length of the socket address structure


public:
    /*
     * Constructor for TCPClient class.
     * Initializes the host and port to which the client will connect.
     * @param host The host to connect to (IP address or hostname).
     * @param port The port to connect to on the host.
     */
    TCPClient (std::string host, int port): host(host), port(port) {
    }

    /*
     * Destructor to ensure the socket is properly closed when the object is destroyed.
     */
    ~TCPClient () {
        close(this->socketFd);
    }

    /*
     * Initialize the TCPClient object. Currently does nothing but can be expanded.
     * @return 0 on success, or an error code on failure.
     */
    int initialize () {
        return 0;
    }

    /*
     * Creates a socket and attempts to connect to the specified host and port.
     * @return 0 if the connection was successful, -1 if there was an error.
     */
    int start () {
        if ( (this->socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
            return -1;
        }

        memset(&this->socketAddrServer, '\0', sizeof(this->socketAddrServer));
        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_port = htons(this->port);

        if ( inet_aton(this->host.c_str(), &(this->socketAddrServer.sin_addr)) == 0 ) {
            return -1;
        } 

        return connect(this->socketFd, (struct sockaddr*) &this->socketAddrServer, sizeof(this->socketAddrServer));
    }

    /*
     * Sends data to the connected peer.
     * @param buffer A pointer to the data to be sent.
     * @param bufferLength The length of the data to send.
     * @return The number of bytes sent, or a negative value in case of error.
     */
    ssize_t sendTo(const char* buffer, size_t bufferLength) {
        return send(this->socketFd, buffer, bufferLength, 0);	
    }

    /*
     * Receives data from the connected peer.
     * @param buffer A pointer to the buffer to receive data into.
     * @param bufferLength The size of the buffer.
     * @return The number of bytes received, or a negative value in case of error.
     */
    ssize_t receiveFrom (char* buffer, size_t bufferLength) {
        return recv(this->socketFd, buffer, bufferLength, 0);
    }

    /*
     * Closes the socket and the connection with the peer.
     * @return 0 if successful, or a negative value in case of error.
     */
    int closeSocket () {
        return close(this->socketFd);
    }
    
};
