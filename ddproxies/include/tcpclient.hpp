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

// TCP/IP client to send data to an TCP/IP server.
class TCPClient {
private:
    std::string host;
    int port;
    struct sockaddr_in socketAddrServer;
    int socketFd;
    socklen_t socketLen;

public:
    TCPClient (std::string host, int port): host(host), port(port) {
    }

    ~TCPClient () {
        close(this->socketFd);
    }

    int initialize () {
        return 0;
    }

    int start () {
        if ( (this->socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
//        if ( (this->socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
            this->error("initialize: Socket failure");
            return -1;
        }

        bzero(&this->socketAddrServer, sizeof(struct sockaddr_in));

        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_addr.s_addr = inet_addr(this->host.c_str());
        this->socketAddrServer.sin_port = htons(this->port);

        return connect(this->socketFd, (struct sockaddr*) &this->socketAddrServer, sizeof(struct sockaddr_in));
    }

    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

    ssize_t sendTo(const char* buffer, size_t bufferLength) {
        return sendto(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, sizeof(this->socketAddrServer));	
    }

    ssize_t receiveFrom (char* buffer, size_t bufferLength) {
        return recvfrom(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, &this->socketLen);
    }

    int closeSocket () {
        return close(this->socketFd);
    }
    
};
