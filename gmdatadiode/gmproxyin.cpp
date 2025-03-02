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

#include <udpclient.hpp>
#include <tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int GMx_PORT = 10000; // OUT
constexpr const char GMx_HOST[] = "127.0.0.1";
constexpr int DATADIODE_SEND_PORT = 40000;
constexpr const char DATA_DIODE_SEND_HOST[] = "127.0.0.1";

using namespace std;

/*
 * Required to catch SIGPIPE signal to prevent the closing of the application.
 * The SIGPIPE signal is triggered by socket.sendto when the socket has been
 * closed by the peer.
 */
void signal_sigpipe_cb (int signum) {
  // Do nothing!
}

void thread_datadiode_send (bool* running, queue<string>* queueSend) {
  char buffer[BUFFER_SIZE];

  UDPClient udpClient(DATA_DIODE_SEND_HOST, DATADIODE_SEND_PORT);
  udpClient.initialize();

  while ( *running ) {
    while ( !queueSend->empty() ) {
      cout << "Sending over the data-diode send: " << queueSend->front();
      ssize_t n = udpClient.sendTo(queueSend->front().c_str(), queueSend->front().length());
      if ( n >= 0 ) {
        queueSend->pop();
      } else {
        cout << "Error with client during sending data" << endl;
        // TODO: What do we need to do here?!
      }
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

  queue<string> queueDataDiodeSend;

  thread threadDataDiodeSend(thread_datadiode_send, &running, &queueDataDiodeSend);

  TCPClient tcpClientGmServer(GMx_HOST, GMx_PORT);
  tcpClientGmServer.initialize();

  while ( running ) {
    cout << "Try to connect to the GMServer on host " << GMx_HOST << " on port " << GMx_PORT << endl;
    if ( tcpClientGmServer.start() == 0 ) {
      cout << "Connected with the GMServer" << endl;

      bool active = true;
      while ( active ) {
        cout << "Waiting on data from the GMServer" << endl;
        ssize_t n = tcpClientGmServer.receiveFrom(buffer, BUFFER_SIZE);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          cout << "Receive data: " << buffer;
          queueDataDiodeSend.push(string(buffer));

        } else if ( n == 0 ) { // Peer properly shutted down!
          cout << "Client connection shutted down" << endl;
          tcpClientGmServer.closeSocket();
          active = false;
          
        } else { // Problem with the client
          cout << "Error with the client connection" << endl;
          tcpClientGmServer.closeSocket();      
          active = false;
        }
        sleep(0);
      }
    }
    sleep(1);
  }
  return 0;
}

