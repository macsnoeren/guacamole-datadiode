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

// The class that actual implement the TCP/IP server and can start and stop it.
class UDPServer {
private:
    struct sockaddr_in socketAddrServer;
    socklen_t socketLen;
    int socketFd;
    int port;
    int opt;
    bool running;

    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

public:
    UDPServer(int port): port(port), opt(1), running(false) {
    }

    ~UDPServer() {
        close(this->socketFd);
    }

    int initialize() {
        if ( (this->socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
            this->error("initialize: Socket failure");
            return -1;
        }
        
        if ( setsockopt(this->socketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
            this->error("initialize: Failure setsockopt");
            return -2;
        }

        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_addr.s_addr = INADDR_ANY; // TODO: configurable?
        this->socketAddrServer.sin_port = htons(this->port);

        return 0;
    }

    int start () {
        if ( bind(this->socketFd, (struct sockaddr*)&this->socketAddrServer, sizeof(this->socketAddrServer)) < 0) {
            this->error("start: Bind failed");
            return -1;
        }

        return 0;
    }

    ssize_t sendTo(char* buffer, size_t bufferLength) {
        return sendto(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, this->socketLen);	
    }

    ssize_t receiveFrom (char* buffer, size_t bufferLength) {
        return recvfrom(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrServer, &this->socketLen);
    }   

};
