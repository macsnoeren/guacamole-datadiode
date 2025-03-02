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
#include <time.h>
#include <thread>
#include <queue>
#include <random>
#include <unordered_map>

#include <guacamole/util.h>
#include <tcpserver.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int GUACAMOLE_MAX_CLIENTS = 25;
constexpr int GUACAMOLE_PORT = 4822;
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
          cout << "Sending data-diode send: " << queueSend->front() << endl;
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
 * This thread handles the data it receives from the data-diode the
 * is connected to the guacd. It pushed the data to the queue.
 * @param bool* running: pointer to the running flag. If false the thread
 *        need to close.
 *        qeueu<string>* queueRecv: the queue that contains the string
 *        messages.
 * @return void
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
        sleep(0);
      }
    }
    sleep(0);
  }
  cout << "Thread receiving data-diode stopped" << endl;
}

/*
 * The thread handles the messages that are received from the Guacamole client connection.
 */
void thread_guacamole_client_recv (bool* running, TCPServerClientHandle* tcpGuacamoleClientHandle, queue<string>* queueSend, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];
  bool active = true;

  TCPServerClient* tcpGuacamoleClient = tcpGuacamoleClientHandle->tcpClient; 

  while ( *running && tcpGuacamoleClientHandle->running ) {
    ssize_t n = tcpGuacamoleClient->receiveFrom(buffer, BUFFER_SIZE);
    if ( tcpGuacamoleClientHandle->running && n  > 0 ) { // Received message from Guacamole client, possible that the socket has been closed
      buffer[n] = '\0';
      cout << "Received data: " << buffer;

      // Add the connection data to the data to be send.
      char gmssel[50] = "";
      sprintf(gmssel, "7.GMS_SEL,%d.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
      
      // Send the data over the data-diode
      queueSend->push(gmssel + string(buffer));

    } else if ( n == 0 ) { // Peer properly shutted down!
      cout << "thread_guacamole_client_recv: Peer Guacamole web shutted down" << endl;
      tcpGuacamoleClient->closeSocket();
      tcpGuacamoleClientHandle->running = false;
      
    } else { // Problem with the client
      cout << "thread_guacamole_client_recv: Error with the Guacamole web client connection" << endl;
      tcpGuacamoleClient->closeSocket();      
      tcpGuacamoleClientHandle->running = false;
    }
    sleep(0);
  }

  // Send the close connection over the data-diode.
  char gmsclose[50] = "";
  sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
  queueSend->push(gmsclose);

  // Delete the tcpClient and set running to false, so the main application is able to delete the handle itself.
  delete tcpGuacamoleClientHandle->tcpClient;
  cout << "Closing Guacamole web client" << endl;

  tcpGuacamoleClientHandle->tcpClient = NULL;
}

/*
 * When data is received from the data-diode, it needs to be dispatched to the correct Guacamole client.
 * This thread is responsible for this.
 */
void thread_guacamole_client_send (bool* running, unordered_map<string, TCPServerClientHandle*>* guacamoleClients, queue<string>* queueSend, queue<string>* queueRecv) {
  cout << "Thread Guacamole client dispatcher started" << endl;
  while ( *running ) {
    while ( !queueRecv->empty() ) {
      cout << "Dispatching data to Guacamole client: " << queueRecv->front();

      char opcode[50];
      char value[50];
      long offset = 0;

      if ( findGmsOpcode( queueRecv->front().c_str(), opcode, value, &offset ) ) { // Found GMS info
        if ( guacamoleClients->find(string(value)) != guacamoleClients->end() ) { // Found assiocated 
          TCPServerClientHandle* tcpServerClientHandle = guacamoleClients->at(string(value));
          if ( strcmp(opcode, "GMS_SEL") == 0 ) {
            ssize_t n = tcpServerClientHandle->tcpClient->sendTo(queueRecv->front().c_str()+offset, queueRecv->front().length()-offset);
            if ( n < 0 ) {
              cout << "thread_guacamole_client_send: Error with client during sending data" << endl;
              tcpServerClientHandle->running = false;
              tcpServerClientHandle->tcpClient->closeSocket();

              // Send the close message to the other side
              char gmsclose[50] = "";
              sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(value), value);
              queueSend->push(string(gmsclose));
            }

          } else if ( strcmp(opcode, "GMS_CLOSE") == 0 ) {
            cout << "thread_guacamole_client_send: Close request of Guacamole client from guacd" << endl;
            tcpServerClientHandle->tcpClient->closeSocket(); // Close the socket
            tcpServerClientHandle->running = false; // Stop the thread!

          } else {
            cout << "thread_guacamole_client_send: Error opcode '" << opcode << "' not found" << endl;
          }

        } else { // Connection does not exist, send a close message back
          char gmsclose[50] = "";
          sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(value), value);
          queueSend->push(string(gmsclose));
        }
      }
      queueRecv->pop();
    }

    unordered_map<string, TCPServerClientHandle*>::iterator it = guacamoleClients->begin();
    while ( it!=guacamoleClients->end() ) {
      if ( it->second == NULL ) {
        cout << "thread_guacamole_client_send: Final cleanup" << endl;
        it = guacamoleClients->erase(it);

      } else if ( it->second->running == false && it->second->tcpClient == NULL ) {
        cout << "thread_guacamole_client_send: Trash Guacamole client '" << it->first << "'" << endl;
        delete it->second;
        (*guacamoleClients)[it->first] = NULL;

      } else {
        ++it;
      }
    }

    sleep(0);
  }
  
  cout << "Thread dispatch guacamole clients stopped" << endl;
}

string createUniqueId () {
  time_t timer = time(nullptr);
  char id[80] = "";
  
  sprintf(id, "%ld%ld", timer, random() + random() + random());

  return string(id);
}

/*
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  bool running = true;

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  queue<string> queueDataDiodeSend;
  queue<string> queueDataDiodeRecv;
  unordered_map<string, TCPServerClientHandle*> guacamoleClientHandles;

  // Create the necessary threads
  thread t1(thread_datadiode_send, &running, &queueDataDiodeSend);
  thread t2(thread_datadiode_recv, &running, &queueDataDiodeRecv);
  thread t3(thread_guacamole_client_send, &running, &guacamoleClientHandles, &queueDataDiodeSend, &queueDataDiodeRecv);
  t1.detach();
  t2.detach();
  t3.detach();
  
  // Create the TCP/IP server to accept connections from the Guacamole web client
  TCPServer tcpServerGuacamole(GUACAMOLE_PORT, GUACAMOLE_MAX_CLIENTS);
  tcpServerGuacamole.initialize();
  tcpServerGuacamole.start();

  sleep(5); // Let the data-diode connect first

  while ( running ) {
    cout << "Waiting on a Guacamole client connection" << endl;
    TCPServerClient* tcpGuacamoleClient = tcpServerGuacamole.accept();
    if ( tcpGuacamoleClient != NULL ) {
      cout << "Guacamole client connected" << endl;
      
      string id = createUniqueId();
      TCPServerClientHandle *tcpServerClientHandle = new TCPServerClientHandle;
      *tcpServerClientHandle = {
        tcpGuacamoleClient,
        true,
        id
      };

      guacamoleClientHandles.insert({id, tcpServerClientHandle}); // Add the client handle
      
      // Send the new connection to the other side.
      char gmsnew[50] = "";
      sprintf(gmsnew, "7.GMS_NEW,%d.%s;", id.length(), id.c_str());
      queueDataDiodeSend.push(string(gmsnew));
      
      thread t1(thread_guacamole_client_recv, &running, tcpServerClientHandle, &queueDataDiodeSend, &queueDataDiodeRecv);
      t1.detach();
    }
  }
  return 0;
}

