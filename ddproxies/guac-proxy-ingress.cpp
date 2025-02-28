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
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <iostream>
#include <thread>

#include <include/tcpserver.hpp>
#include <include/udpclient.hpp>
#include <include/udpserver.hpp>

using namespace std;

// Global defines that configures the application
constexpr char VERSION[] = "0.1 (test)";
constexpr int TCP_SERVER_GUACAMOLE_PORT = 4822;
constexpr int BUFFER_SIZE = 1024;
constexpr char UDP_SERVER_OUT_HOSTNAME[] = "127.0.0.1";
constexpr int UDP_SERVER_OUT_PORT = 10000;
constexpr int UDP_SERVER_IN_PORT = 20000;

/*
 * This function runs as a seperate thread. It create an UDP/IP server to receive data
 * from the proxy at the other side of the data-diode. If data is received this is validated
 * and send to the Guacamole web client that is connected to the TCP/IP server to this proxy.
 * 
 * @param bool running: indicate that the proxy node is still running if false the thread needs
 *                      to close and return.
 *        int clientSocketFd: the socket that can be used to send data to the connected Guacamole
 *                      client.
 *        struct sockaddr_in* saClient: the socket address assiocated with the socket that is used
 *                      to send data to the Guacamole client. 
 * @return void
*/
void udp_receive_data_guacd_in (bool* running, iTCPServerClientSendOnly* tcpServer) {
  // The buffer to receive the data from the other proxy from the guacd via UDP/IP
  char buffer[BUFFER_SIZE] = {0};

  // Setup UDP/IP server to recieve data from the proxy at the other side that is connected with the guacd
  UDPServer udpServer(UDP_SERVER_IN_PORT);
  udpServer.initialize();
  
  while (*running) {
    cout << "Wainting on data from an UDP/IP client" << endl;
    ssize_t n = udpServer.receiveFrom(buffer, BUFFER_SIZE);
    if ( n < 0 ) {
      *running = false;
      return;
    }
    buffer[n] = '\0';

    cout << "Received from proxy: '" << buffer << "'" << endl;

    // Send to TCP/IP
    if ( tcpServer->sendTo(buffer, strlen(buffer)) < 0 ) {
      perror("TCPServer::send: Socket error.");
      *running = false;
      return;
    }
    
    sleep(0);
  }

  cout << "UDP/IP server for procy egress stopped." << endl;
}

/*
 * The main application that create a TCP/IP server where the Guacamole web client is connecting
 * to. When the Guacamole client is connected, a threat is created to handle the reverse data from
 * the proxy at the other side of the data-diode. When data is received by the Guacamole client this
 * data is send to the proxy to the other side of the data-diode using the UDP/IP protocol.
 * 
 * @todo Guacomole connects to the proxy and the proxy relays this to the proxy at the other side of
 *       the data-diode that is connected with guacd. The assumption is that the plugin connections
 *       will return the same messages during the handshake. This can be learned by this proxy, so
 *       less information is required to be sent over to the otherside. Or that the data can be checked.
 * @param int argc: number of arguments that have been given at the command line.
 *        chat* argv[]: the actual pointer to the arguments that have been given by the command line.
 * @return int that indicate the exit code of the application.
*/
int main (int argc, char *argv[]) {
  cout << argv[0] << VERSION << endl;

  // Variable that controls the running state, if false everything needs to shutdown.
  bool running = true;

  // Buffer to store the information received from the TCP/IP server
  char buffer[BUFFER_SIZE] = {0};

  // Crete TCPServer where the Guacamole client can connect to
  TCPServer tcpServer(TCP_SERVER_GUACAMOLE_PORT);

  if ( tcpServer.initialize() < 0 ) {
    return -1;
  }

  if ( tcpServer.start() < 0 ) {
    return -1;
  }

  // Create the UDPClient to send data from Guacamole as UDP package to the proxy at the other side of the data-diode
  UDPClient udpClient("127.0.0.1", UDP_SERVER_OUT_PORT);
  if ( udpClient.initialize() < 0 ) {
    return -1;
  }

  while ( running ) {
    cout << "Waiting for a client to connect" << endl;
    TCPServerClient* tcpServerClient = tcpServer.accept();

    if ( tcpServerClient != NULL ) {
      cout << "Client connected to the server" << endl;

      // When not active the thread needs to shutdown.
      bool active = true;

      // Start the thread that forward the received UDP packets to the TCP/IP server
      thread threadUdpReceiveDataGuacdIn(udp_receive_data_guacd_in, &active, tcpServerClient->getTCPServerClientSendOnly());

      cout << "Waiting on data from the TCP/IP client" << endl;
      while ( active ) {
        ssize_t n = tcpServerClient->receiveFrom(buffer, BUFFER_SIZE);

        if ( n <= 0 ) {
          cout << "Problem with the TCP/IP connection with Guacamole" << endl;
          active = false;

        } else {
          buffer[n] = '\0';
          cout << "Received from TCP/IP server: " << buffer;
       
          // Send to UDP packet to the other side of the data-diode
          if ( udpClient.sendTo(buffer, strlen(buffer)) < 0 ) {
            cout << "Problem with sending UDP/IP packet" << endl;
            active = false;
          }
        }
      }
      threadUdpReceiveDataGuacdIn.join(); 
    }
    delete tcpServerClient;
  }
  
  return 0;
}

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