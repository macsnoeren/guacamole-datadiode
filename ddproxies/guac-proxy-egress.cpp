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

using namespace std;

constexpr int TCP_SERVER_GUACAMOLE_PORT = 4823;
constexpr int BUFFER_SIZE = 1024;//8192;
constexpr char UDP_SERVER_OUT_HOSTNAME[] = "127.0.0.1";
constexpr int UDP_SERVER_OUT_PORT = 20000;
constexpr int UDP_SERVER_IN_PORT = 10000;

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
  The main application sets up a TCP/IP connection with the guacd server.
  When it receives data it is send to the UDP server of the ingress proxy.
*/
int main (int argc, char *argv[]) {
  cout << argv[0] << " version 0.1 (test)" << endl;

  // Deamonize
  /*
  pid_t pid, sid;

   pid = fork();
   if (pid > 0) {
      exit(EXIT_SUCCESS);
   } else if (pid < 0) {
      exit(EXIT_FAILURE);
   }

   umask(0);

   openlog("proxy-ingress", LOG_NOWAIT | LOG_PID, LOG_USER);
   syslog(LOG_NOTICE, "Successfully started proxe-ingress");

   sid = setsid();
   if(sid < 0) {
      syslog(LOG_ERR, "Could not generate session ID for child process");
      exit(EXIT_FAILURE);
   }

   if((chdir("/")) < 0) {
      syslog(LOG_ERR, "Could not change working directory to /");
      exit(EXIT_FAILURE);
   }

   close(STDIN_FILENO);
   close(STDOUT_FILENO);
   close(STDERR_FILENO);
  */
  // End Deamonize
  
  bool running = true;

  /* TCP/IP Client variables */
  string hostnameS = "localhost";
  struct sockaddr_in saServer;
  struct sockaddr_in saClient;
  socklen_t slClient;
  int socketFd;
  socklen_t iLen = sizeof(struct sockaddr_in);
  
  if ( (socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("Socket failure");
    exit(0);
  }

  saServer.sin_family = AF_INET;
  saServer.sin_port   = htons(TCP_SERVER_GUACAMOLE_PORT);
  saServer.sin_addr.s_addr = inet_addr("127.0.0.1");//*((struct in_addr*) pHost->h_addr);

  // Setup the UDP client
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
    cout << "Trying to connect to server on port " << TCP_SERVER_GUACAMOLE_PORT << endl;
    if ( connect(socketFd,(struct sockaddr *) &saServer, sizeof(struct sockaddr_in)) < 0 ) {
      perror("TCPClient::initSocket: Connection error.");
      return 2;
    }

    cout << "Connected with client" << endl;
    
    char buffer[BUFFER_SIZE] = {0};
    socklen_t len;
    bool active = true;
    
    // Start the thread that forward the received UDP packets to the TCP/IP server
    thread threadUdpReceiveDataGuacdIn(udp_receive_data_guacd_in, &active, socketFd, &saClient);
    
    cout << "Waiting on data from the TCP/IP client" << endl;
    while ( active ) {
      int n = recvfrom(socketFd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &saClient, &len);
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
    
    close(socketFd);
	  
    threadUdpReceiveDataGuacdIn.join();  
  }
    
  close(socketFd);
  
  return 0;
}

