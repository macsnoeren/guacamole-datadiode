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
 * This class implements an UDP server.
 */
class UDPServer {
private:
     // Contains the address information of the server
    struct sockaddr_in socketAddrServer;

    // Holds the assiocated socket for the UDP server
    socklen_t socketLen;
    int socketFd;

    // Port that the server will listen to
    int port;

    // Used to set options of the socket during creation.
    int opt;

     // Determine whether the TCP server is still running.
    bool running;

    /*
     * Generic error method to show errors.
     * @param error that contains the error message.
     */
    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

public:
    /*
     * Constructs the UDPServer class.
     * @param port to listen to.
     */
    UDPServer(int port): port(port), opt(1), running(false) {
    }

    ~UDPServer() {
        close(this->socketFd);
    }

    /*
     * Initialize the class to setup the object.
     */
    int initialize() {
        if ( (this->socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
            this->error("initialize: Socket failure");
            return -1;
        }
        
        if ( setsockopt(this->socketFd, SOL_SOCKET, SO_REUSEADDR, &this->opt, sizeof(this->opt))) {
            this->error("initialize: Failure setsockopt");
            return -2;
        }

#ifdef SO_REUSEPORT
        setsockopt(this->socketFd, SOL_SOCKET, SO_REUSEPORT, &this->opt, sizeof(this->opt));
#endif

        memset(&this->socketAddrServer, 0, sizeof(this->socketAddrServer));
        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_addr.s_addr = htonl(INADDR_ANY); // TODO: configurable?
        this->socketAddrServer.sin_port = htons(this->port);

        return 0;
    }

    /*
     * Creates the socket and start listening to the port.
     * @return zero when succesfull, otherwise <0.
     */
    int start () {
        if ( bind(this->socketFd, (struct sockaddr*)&this->socketAddrServer, sizeof(this->socketAddrServer)) < 0) {
            this->error("start: Bind failed");
            return -1;
        }

        return 0;
    }

    /*
     * Sends data to the connected peer.
     * @param buffer is a pointer to the data.
     * @param bufferLength is the total data that needs to be send.
     * @return total amount of bytes that have been send or <0 error.
     */
    ssize_t sendTo(char* buffer, size_t bufferLength) {
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

};
