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
#include <thread>

using namespace std;

constexpr int TCP_SERVER_GUACAMOLE_PORT = 4822;
constexpr int BUFFER_SIZE = 1024;//8192;
constexpr char UDP_SERVER_OUT_HOSTNAME[] = "127.0.0.1";
constexpr int UDP_SERVER_OUT_PORT = 10000;
constexpr int UDP_SERVER_IN_PORT = 20000;

void error (const char* msg ) {
  perror(msg);
  exit(1);
}

/*
  The other proxy is connected with TCP/IP with the guacd using the Guacamole
  protocol. What is received there is send over UDP to us. This data needs to
  be send to the Guacamole front-end.
*/
void udp_receive_data_guacd_in (bool* running, int clientSocketFd, struct sockaddr_in* saClient) {

  /*
  while (*running) {
    if ( sendto(clientSocketFd, "Hoi\n", strlen("Hoi\n"), MSG_NOSIGNAL, (struct sockaddr *) &saClient, sizeof(saClient) ) < 0 ) {
      perror("TCPServer::send: Socket error.");
      return;
    }
    sleep(5);
  }
  */
  
  char buffer[BUFFER_SIZE] = {0};

  // Initialize the UDP server
  int socketFdUdp;
  struct sockaddr_in serveraddrUdp;
  struct sockaddr_in clientAddr;

  if ( (socketFdUdp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
    perror("Failed to create socket");
    return;
  }

  serveraddrUdp.sin_family = AF_INET;
  serveraddrUdp.sin_addr.s_addr = INADDR_ANY;
  serveraddrUdp.sin_port = htons(UDP_SERVER_IN_PORT);

  if ( bind(socketFdUdp, (const struct sockaddr*)&serveraddrUdp, sizeof(serveraddrUdp)) < 0 ) {
    perror("Failed to bind to the UDP port");
    return;
  }

  socklen_t len = sizeof(clientAddr);
  
  while (*running) {
    cout << "Wainting on data from the UDP/IP client" << endl;
    int n = recvfrom(socketFdUdp, (char*)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&clientAddr, &len);
    buffer[n] = '\0';

    cout << "Received from proxy: " << buffer << endl;

    // Send to TCP/IP
    if ( sendto(clientSocketFd, buffer, strlen(buffer), MSG_NOSIGNAL, (struct sockaddr *) &saClient, sizeof(saClient) ) < 0 ) {
      perror("TCPServer::send: Socket error.");
      return;
    }
    
    sleep(0);
  }

  cout << "UDP/IP server for procy egress stopped." << endl;
 
}

/*
  The main application listens to the TCP/IP port for Guacamole web client and
  sends the messages over UDP to the other proxy-egress of the data-diode.
*/
int main (int argc, char *argv[]) {
  cout << argv[0] << " version 0.1 (test)" << endl;

  bool running = true;

  /* TCP/IP Server variables */
  struct sockaddr_in saServer;
  struct sockaddr_in saClient;
  socklen_t slClient;
  int socketFd;
  int opt = 1;

  if ( (socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) {
    perror("Socket failure");
    exit(0);
  }

  if ( setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("Failure setsockopt");
    exit(0);
  }

  saServer.sin_family = AF_INET;
  saServer.sin_addr.s_addr = INADDR_ANY;
  saServer.sin_port = htons(TCP_SERVER_GUACAMOLE_PORT);

  if ( bind(socketFd, (struct sockaddr*)&saServer, sizeof(saServer)) < 0) {
    perror("Bind failed");
    exit(0);
  }

  if ( listen(socketFd, 1) < 0 ) {
    perror("Failure listen to port");
    exit(0);
  }

  cout << "TCP/IP server listening on port " << TCP_SERVER_GUACAMOLE_PORT << endl;

  string hostname = "localhost";
  struct sockaddr_in saUDPServer;
  socklen_t iUDPLen = sizeof(struct sockaddr_in);
  char buffer[BUFFER_SIZE];
  struct hostent *pUDPHost;
  int socketUDPFd;
  int iUDPPort = UDP_SERVER_OUT_PORT;

  iUDPLen = sizeof(struct sockaddr_in);
  pUDPHost = gethostbyname(hostname.c_str());
  if ( pUDPHost == NULL ) {
    perror("gethostname error");
    return 1;
  }

  if ( ( socketUDPFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 ) {
    perror("UDPClient::initSocket: Socket error.");
    return 1;
  }

  saUDPServer.sin_family = AF_INET;
  saUDPServer.sin_port   = htons(iUDPPort);
  saUDPServer.sin_addr   = *((struct in_addr*) pUDPHost->h_addr);
 
  while ( running ) {
    cout << "Waiting for a client to connect" << endl;
    int clientSocketFd = accept4(socketFd, (struct sockaddr*)&saClient, &slClient, 0);
    if ( clientSocketFd < 0) {
      perror("Failure accepting new client");
      return 1;
    }
    
    cout << "Client connected to the server" << endl;
    
    char buffer[BUFFER_SIZE] = {0};
    socklen_t len;
    bool active = true;
    
    // Start the thread that forward the received UDP packets to the TCP/IP server
    thread threadUdpReceiveDataGuacdIn(udp_receive_data_guacd_in, &active, clientSocketFd, &saClient);
    
    cout << "Waiting on data from the TCP/IP client" << endl;
    while ( active ) {
      int n = recvfrom(clientSocketFd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &saClient, &len);
      if ( n < 0 ) {
	perror("TCPServer::receive: Error!");
	active = false;
      }
      
      buffer[n] = '\0';
      cout << "Received: " << buffer;
      
      // Send to UDP server
      ssize_t len = sendto(socketUDPFd, buffer, strlen(buffer), 0, (struct sockaddr *) &saUDPServer, iUDPLen);	
      if ( len < 0 ) {
	perror("UDPClient::send: Socket error.");
	active = false;
      }
    }
    
    close(clientSocketFd);
	  
    threadUdpReceiveDataGuacdIn.join();  
  }
    
  close(socketFd);
  
  return 0;
}

