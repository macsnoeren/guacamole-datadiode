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

// UDP/IP client to send data to an UDP server.
class UDPClient {
private:
    std::string host;
    int port;
    struct sockaddr_in socketAddrServer;
    int socketFd;
    socklen_t socketLen;

public:
    UDPClient (std::string host, int port): host(host), port(port) {
    }

    ~UDPClient () {
        close(this->socketFd);
    }

    int initialize() {
        if ( (this->socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
            this->error("initialize: Socket failure");
            return -1;
        }
        
        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_addr.s_addr = inet_addr(this->host.c_str());
        this->socketAddrServer.sin_port = htons(this->port);

        return 0;
    }

    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

    ssize_t sendTo(const char* buffer, size_t bufferLength) {
        return sendto(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, sizeof(this->socketAddrServer));	
    }

    // Not tested!
    ssize_t receiveFrom (char* buffer, size_t bufferLength) {
        return recvfrom(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, &this->socketLen);
    }      
    
};
