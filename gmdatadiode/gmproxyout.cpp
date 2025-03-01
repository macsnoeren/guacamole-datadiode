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
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <thread>
#include <queue>
#include <list>

#include <include/udpserver.hpp>
#include <include/tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int GMx_PORT = 20000; // IN
constexpr const char GMx_HOST[] = "127.0.0.1";
constexpr int DATADIODE_RECV_PORT = 40000;

using namespace std;

/*
 * Required to catch SIGPIPE signal to prevent the closing of the application.
 * The SIGPIPE signal is triggered by socket.sendto when the socket has been
 * closed by the peer.
 */
void signal_sigpipe_cb (int signum) {
  // Do nothing!
}

void thread_datadiode_recv (bool* running, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];

  UDPServer udpServer(DATADIODE_RECV_PORT);
  udpServer.initialize();
  udpServer.start();

  while ( *running ) {
    cout << "Waiting on data from data-diode" << endl;
    ssize_t n = udpServer.receiveFrom(buffer, BUFFER_SIZE);
    if ( n  > 0 ) { // Received message from receiving data-diode
      buffer[n] = '\0';
      cout << "Received data-diode data: " << buffer;
      queueRecv->push(string(buffer));
      
    } else { // Problem with the client
      cout << "Error with the client connection" << endl;
      // What to do?!
    }
    sleep(0);
  }
  cout << "Thread sending data-diode stopped" << endl;
}

/*
 */
int main (int argc, char *argv[]) {
  bool running = true;
  char buffer[BUFFER_SIZE];

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  queue<string> queueDataDiodeRecv;

  thread threadDataDiodeRecv(thread_datadiode_recv, &running, &queueDataDiodeRecv);

  TCPClient tcpClientGmServer(GMx_HOST, GMx_PORT);
  tcpClientGmServer.initialize();

  while ( running ) {
    cout << "Try to connect to the GMServer on host " << GMx_HOST << " on port " << GMx_PORT << endl;
    if ( tcpClientGmServer.start() == 0 ) {
      cout << "Connected with the GMServer" << endl;

      bool active = true;
      while ( active ) {
        while ( active && !queueDataDiodeRecv.empty() ) {
          cout << "Sending over the data-diode send: " << queueDataDiodeRecv.front();
          ssize_t n = tcpClientGmServer.sendTo(queueDataDiodeRecv.front().c_str(), queueDataDiodeRecv.front().length());
          if ( n >= 0 ) {
            queueDataDiodeRecv.pop();
          } else {
            cout << "Error with client during sending data" << endl;
            tcpClientGmServer.closeSocket();
            active = false;
            perror("MM");
            sleep(5);
          }
        }
        sleep(0);
      }
    }
    sleep(1);
  }
  return 0;
}

