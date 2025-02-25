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

// Interface to provide a client that is only able to send messages to the TCP/IP client of the server
class iTCPServerClientSendOnly {
public:
    int sendto(char* buffer, size_t bufferLength);          
};

// When a client is connected to the TCP/IP server, this class represent the client connection.
class TCPServerClient: public iTCPServerClientSendOnly {
private:
    int socketFd;
    struct sockaddr_in socketAddrClient;
    socklen_t socketLenClient;

public:
    TCPServerClient (int socketFd, struct sockaddr_in socketAddrClient, socklen_t socketLenClient) {
        this->socketFd = socketFd;
        this->socketAddrClient = socketAddrClient;
        this->socketLenClient = socketLenClient;
    }

    ~TCPServerClient () {
    }

    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

    iTCPServerClientSendOnly* getTCPServerClientSendOnly() {
        return this;
    }

    int sendto(char* buffer, size_t bufferLength) {
        return 0;
    }

    int receiveFrom (char* buffer, size_t bufferLength) {
        int n = recvfrom(this->socketFd, buffer, bufferLength, 0, (struct sockaddr *) &this->socketAddrClient, &this->socketLenClient);
        if ( n < 0 ) {
            this->error("receiveFrom: Error!");
        }
        buffer[n] = '\0';
        return 0;
    }      
    
};

// The class that actual implement the TCP/IP server and can start and stop it.
class TCPServer {
private:
    struct sockaddr_in socketAddrServer;
    int socketFd;
    int port;
    int opt;
    bool running;

    void error (const char* error) {
        std::cout << "ERROR: " << error << std::endl;
    }

public:
    TCPServer(int port): port(port), opt(1), running(false) {
    }

    ~TCPServer() {
    }

    int initialize() {
        if ( (this->socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
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
    
        if ( listen(this->socketFd, 1) < 0 ) { // TODO: total connections that can be made to the server
            this->error("start: Failure listen to port");
            return -2;
        }

        std::cout << "start: TCP/IP server listening on port " << this->port << std::endl;

        return 0;
    }

    TCPServerClient* accept () {
        std::cout << "accept: Waiting for a client to connect" << std::endl;

        struct sockaddr_in socketAddrClient;
        socklen_t socketLenClient;

        int clientSocket = accept4(this->socketFd, (struct sockaddr*)&socketAddrClient, &socketLenClient, 0);
        if ( clientSocket < 0) {
            this->error("accept: Failure accepting new client");
            return NULL;
        }

        std::cout << "accept: Client connected" << std::endl;

        return new TCPServerClient(clientSocket, socketAddrClient, socketLenClient);
    }
};
