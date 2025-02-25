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
    sendto(char* buffer, size_t bufferLength) = 0;          
};

// The class that actual implement the TCP/IP server and can start and stop it.
class TCPServer {
private:
    struct sockaddr_in socketAddrServer;
    int socket;
    int port;
    int opt;
    bool running;

    void error (string error) {
        std::cout << "ERROR: " << error << std::endl;
    }

public:
    TCPServer(int port): port(port), opt(1), running(false) {
    }

    ~TCPServer() {
    }

    int initialize() {
        if ( (this->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
            this->error("initialize: Socket failure");
            return -1;
        }
        
        if ( setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &this->opt, sizeof(this->opt))) {
            this->error("initialize: Failure setsockopt");
            return -2;
        }

        this->socketAddrServer.sin_family = AF_INET;
        this->socketAddrServer.sin_addr.s_addr = INADDR_ANY; // TODO: configurable?
        this->socketAddrServer.sin_port = htons(this->port);

        return 0;
    }

    int start () {
        if ( bind(this->socket, (struct sockaddr*)&this->socketAddrServer, sizeof(this->socketAddrServer)) < 0) {
            this->error("start: Bind failed");
            return -1;
        }
    
        if ( listen(this->socket, 1) < 0 ) { // TODO: total connections that can be made to the server
            this->error("start: Failure listen to port");
            return -2;
        }

        cout << "start: TCP/IP server listening on port " << this->port << endl;

        return 0;
    }

    TCPServerClient accept () {
        cout << "accept: Waiting for a client to connect" << endl;

        struct sockaddr_in socketAddrClient;
        socklen_t socketLenClient;

        int clientSocket = accept4(this->socket, (struct sockaddr*)&socketAddrClient, &socketLenClient, 0);
        if ( clientSocket < 0) {
            perror("Failure accepting new client");
            return 1;
        }



    

};

// When a client is connected to the TCP/IP server, this class represent the client connection.
class TCPServerClient {
private:
    struct sockaddr_in socketAddrClient;


public:
    TCPServerClient () {
    }

    ~TCPServerClient () {
    }

int receiveFrom (char* buffer, size_t bufferLength) {
    int n = recvfrom(this->socket, buffer, bufferLength, 0, (struct sockaddr *) &this->sockerAddrClient, &len);
  if ( n < 0 ) {
perror("TCPServer::receive: Error!");
active = false;
  }
  
  buffer[n] = '\0';
  cout << "Received: " << buffer;
  
}

};