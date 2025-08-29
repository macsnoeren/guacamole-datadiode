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
 * This class implements a UDPClient to connect to UDP servers.
 */
class UDPClient {
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
     * Constructs the UDPClient class.
     * @param host is the host to connect to.
     * @param port to connect to.
     */
    UDPClient (std::string host, int port): host(host), port(port) {
    }

    ~UDPClient () {
        close(this->socketFd);
    }

    /*
     * Initialize the class to setup the object.
     */
    int initialize() {
        if ((this->socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
            this->error("initialize: Socket failure");
            return -1;
        }

        struct hostent* server = gethostbyname(this->host.c_str());
        if (server == nullptr) {
            this->error("initialize: no such host");
            return -1;
        }

        memset(&this->socketAddrServer, 0, sizeof(this->socketAddrServer));
        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_port = htons(this->port);
        memcpy(&this->socketAddrServer.sin_addr.s_addr,
            server->h_addr,
            server->h_length);

        this->socketLen = sizeof(this->socketAddrServer);

        return 0;
    }

    /*
     * Generic error method to show errors.
     * @param error that contains the error message.
     */
    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

    /*
     * Sends data to the connected peer.
     * @param buffer is a pointer to the data.
     * @param bufferLength is the total data that needs to be send.
     * @return total amount of bytes that have been send or <0 error.
     */
    ssize_t sendTo(const char* buffer, size_t bufferLength) {
        return sendto(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, sizeof(this->socketAddrServer));	
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
    
};
