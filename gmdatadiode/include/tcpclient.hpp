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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/stat.h>
#include <thread>

/*
 * Class that implements a TCP client that can be used to connect to a TCP server.
 */
class TCPClient {
private:
    // Host to connect to
    std::string host;

    // Port to connect to
    int port;

    // Contains the address information of the server
    struct sockaddr_in socketAddrServer;

    // Holds the assiocated socket for the TCP server
    int socketFd;
    socklen_t socketLen;

public:
    /*
     * Constructs the TCPClient class.
     * @param host is the host to connect to.
     * @param port to connect to.
     */
    TCPClient (std::string host, int port): host(host), port(port) {
    }

    ~TCPClient () {
        close(this->socketFd);
    }

    /*
     * Initialize the class to setup the object.
     */
    int initialize () {
        return 0;
    }

    /*
     * Creates the socket and connects to the host on the given port.
     * @return zero when connected, otherwise -1.
     */
    int start () {
        if ( (this->socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
            std::cout << "initialize: Socket failure" << std::endl;
            return -1;
        }

        memset(&this->socketAddrServer, '\0', sizeof(this->socketAddrServer));
        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_port = htons(this->port);

        if ( inet_aton(this->host.c_str(), &(this->socketAddrServer.sin_addr)) == 0 ) {
            std::cout << "tcpClient: Error: Address '" << this->host << "' is an invalid address" << std::endl;
            return -1;
        } 

        return connect(this->socketFd, (struct sockaddr*) &this->socketAddrServer, sizeof(this->socketAddrServer));
    }

    /*
     * Sends data to the connected peer.
     * @param buffer is a pointer to the data.
     * @param bufferLength is the total data that needs to be send.
     * @return total amount of bytes that have been send or <0 error.
     */
    ssize_t sendTo(const char* buffer, size_t bufferLength) {
        return send(this->socketFd, buffer, bufferLength, 0);	
    }

    /*
     * Receive data from the connected peer.
     * @param buffer to receive the data.
     * @param bufferLength that shows how big the buffer is.
     * @return total amount of bytes that have been send or <0 error.
     */
    ssize_t receiveFrom (char* buffer, size_t bufferLength) {
        return recv(this->socketFd, buffer, bufferLength, 0);
    }

    /*
     * Close the socket and the connection with the peer will be closed.
     */
    int closeSocket () {
        return close(this->socketFd);
    }
    
};
