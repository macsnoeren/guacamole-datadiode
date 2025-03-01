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

#include <tcpserver.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int GUACAMOLE_PORT = 4822;
constexpr int GUACAMOLE_MAX_CLIENTS = 25;
constexpr int DATADIODE_SEND_PORT = 10000;
constexpr int DATADIODE_RECV_PORT = 20000;

using namespace std;

/*
 * Required to catch SIGPIPE signal to prevent the closing of the application.
 * The SIGPIPE signal is triggered by socket.sendto when the socket has been
 * closed by the peer.
 */
void signal_sigpipe_cb (int signum) {
  // Do nothing!
}

/*
 * This thread handles the data that is required to be send to the sending 
 * data-diode. The data is eventually send to the guacd server. A queue is
 * used and when there are messages, this is send to the data-diode proxy.
 * The queue is filled by the messages that are send by the Guacamole web
 * client. The data-diode proxy connect to the TCP/IP server that is created
 * by this thread.
 * @param bool* running: pointer to the running flag. If false the thread
 *        need to close.
 *        qeueu<string>* queueSend: the queue that contains the string
 *        messages.
 * @return void
 */
void thread_datadiode_send (bool* running, queue<string>* queueSend) {
  char buffer[BUFFER_SIZE];

  TCPServer tcpServerSend(DATADIODE_SEND_PORT, 1);
  tcpServerSend.initialize();
  tcpServerSend.start();

  while ( *running ) {
    cout << "Waiting on sending data-diode proxy client connection" << endl;
    TCPServerClient* tcpClient = tcpServerSend.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "Sending data-diode client connected" << endl;
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          cout << "Sending data-diode send: " << queueSend->front();
          ssize_t n = tcpClient->sendTo(queueSend->front().c_str(), queueSend->front().length());
          if ( n >= 0 ) {
            queueSend->pop();
          } else {
            cout << "Error with client during sending data" << endl;
            tcpClient->closeSocket();
            active = false;
          }
        }
        sleep(0);
      }
    }
    sleep(0);
  }
  cout << "Thread sending data-diode stopped" << endl;
}

/*
 * 
 */
void thread_datadiode_recv (bool* running, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];

  TCPServer tcpServerRecv(DATADIODE_RECV_PORT, 1);
  tcpServerRecv.initialize();
  tcpServerRecv.start();

  while ( *running ) {
    cout << "Waiting on receive data-diode proxy client connection" << endl;
    TCPServerClient* tcpClient = tcpServerRecv.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "Receive data-diode client connected" << endl;
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          cout << "Receive data-diode data: " << buffer;
          queueRecv->push(string(buffer));

        } else if ( n == 0 ) { // Peer properly shutted down!
          tcpClient->closeSocket();
          active = false;
          
        } else { // Problem with the client
          cout << "Error with the client connection" << endl;
          tcpClient->closeSocket();      
          active = false;
        }
      }
    }
    sleep(0);
  }
  cout << "Thread receiving data-diode stopped" << endl;
}

void thread_guacamole_client (bool* running, TCPServerClient* tcpGuacamoleClient, queue<string>* queueSend, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];
  bool active = true;

  while ( *running && active ) {
    ssize_t n = tcpGuacamoleClient->receiveFrom(buffer, BUFFER_SIZE);
    if ( n  > 0 ) { // Received message from Guacamole client
      buffer[n] = '\0';
      cout << "Received data: " << buffer;
      // TODO: Add assiocation of the client to the data
      queueSend->push("7.GMS_SEL,2.ID;"); // TODO: Real ID
      queueSend->push(string(buffer));

    } else if ( n == 0 ) { // Peer properly shutted down!
      tcpGuacamoleClient->closeSocket();
      active = false;
      
    } else { // Problem with the client
      cout << "Error with the client connection" << endl;
      tcpGuacamoleClient->closeSocket();      
      active = false;
    }
    sleep(0);
  }

  delete tcpGuacamoleClient;
  tcpGuacamoleClient = NULL;
  cout << "Closing Guacamole client" << endl;
}

void thread_dispatch_guacamole_client (bool* running, queue<string>* queueSend, queue<string>* queueRecv, list<TCPServerClient*>* tcpServerClients) {

  cout << "Thread Guacamole client dispatcher started" << endl;
  while ( *running ) {
    while ( !queueRecv->empty() ) {
      cout << "Dispatching data to Guacamole client: " << queueRecv->front();
      // TODO: Do the real dispatching
      // TODO: Get assiocation of the client to the data
      queueRecv->pop();
    }
    sleep(0);
  }
  queueSend->push("9.GMS_CLOSE,2.ID;"); // TODO: Real ID
  cout << "Thread dispatch guacamole clients stopped" << endl;
}

/*
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  bool running = true;

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  list<TCPServerClient*> tcpServerClients;
  list<thread> threadServerClients;
  queue<string> queueDataDiodeSend;
  queue<string> queueDataDiodeRecv;

  thread threadDataDiodeSend(thread_datadiode_send, &running, &queueDataDiodeSend);
  thread threadDataDiodeRecv(thread_datadiode_recv, &running, &queueDataDiodeRecv);
  thread threadDispatchGuacamoleClient(thread_dispatch_guacamole_client, &running, &queueDataDiodeSend, &queueDataDiodeRecv, &tcpServerClients);

  TCPServer tcpServerGuacamole(GUACAMOLE_PORT, GUACAMOLE_MAX_CLIENTS);
  tcpServerGuacamole.initialize();
  tcpServerGuacamole.start();

  while ( running ) {
    cout << "Waiting on Guacamole client connection" << endl;
    TCPServerClient* tcpGuacamoleClient = tcpServerGuacamole.accept();
    if ( tcpGuacamoleClient != NULL ) {
      tcpServerClients.push_back(tcpGuacamoleClient);
      cout << "Guacamole client connected" << endl;
      queueDataDiodeSend.push("7.GMS_NEW,2.ID;"); // TODO: Real ID
      
      //threadServerClients.push_back(thread(thread_guacamole_client, &running, tcpGuacamoleClient, &queueDataDiodeSend, &queueDataDiodeRecv));
      thread t(thread_guacamole_client, &running, tcpGuacamoleClient, &queueDataDiodeSend, &queueDataDiodeRecv);
      t.detach();
    }

    // Remove clients that stopped working from the list.
    for (list<TCPServerClient*>::iterator i=tcpServerClients.begin(); i != tcpServerClients.end(); i++) {
      if ( (*i) == NULL ) {
        tcpServerClients.erase(i);
      }
    }
  }
  
  for (list<thread>::iterator i=threadServerClients.begin(); i != threadServerClients.end(); i++) {
		i->join();
	}

  return 0;
}

