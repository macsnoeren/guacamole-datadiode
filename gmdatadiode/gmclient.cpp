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
#include <unordered_map>

#include <tcpserver.hpp>
#include <tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int GUACD_PORT = 4822;
constexpr char GUACD_HOST[] = "127.0.0.1";
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

void thread_guacd_client_send(bool* running, string gmsID, TCPClient* tcpClient, queue<string>* queueSend) {
  while ( *running ) {
    while ( !queueSend->empty() ) {
      cout << "Dispatching data to guacd client: " << queueSend->front();
      ssize_t n = tcpClient->sendTo(queueSend->front().c_str(), queueSend->front().length());
      if ( n >= 0 ) {
        queueSend->pop();
      } else {
        cout << "Error with client during sending data" << endl;
        // TODO: What do we need to do here?!
      }
    }
    sleep(0);
  }
}

void thread_guacd_client (bool* running, string gmsID, TCPClient* tcpClient, queue<string>* queueSend) {
  char buffer[BUFFER_SIZE];
  
  while ( *running ) {
    bool active = true;
    while ( active ) {
      cout << "Waiting on data from the GMServer" << endl;
      ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
      if ( n  > 0 ) { // Received message from receiving data-diode
        buffer[n] = '\0';
        cout << "Receive data: " << buffer;
        queueSend->push(string(buffer));

      } else if ( n == 0 ) { // Peer properly shutted down!
        cout << "Client connection shutted down" << endl;
        tcpClient->closeSocket();
        active = false;
        
      } else { // Problem with the client
        cout << "Error with the client connection" << endl;
        tcpClient->closeSocket();      
        active = false;
      }
    }
    sleep(1);
  }
  
  delete tcpClient;
  tcpClient = NULL;

  // TODO: How to remove the tcpClient

  cout << "Closing guacd client" << endl;
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
 * Here new clients needs to be made by the function if a new connection comes by.
 */
void thread_datadiode_recv (bool* running, unordered_map<string, TCPClient*>* tcpClients, queue<string>* queueSend, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];
  static string gmsID = "";

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

          // GMSProtocol over Guacamole protocol: d.GMS_SSS,d.VVV;
          int offset = 0;
          if ( strstr(buffer, ".GMS_") != NULL ) { // Found
            char gmsOpcode[20] = "";
            char gmsValue[20] = "";
            char* dot1 = strchr(buffer, '.'); // This exist and does not result in a NULL pointer
            char* com = strchr(dot1, ',');    // We can safely use dot1 in the other search terms.
            char* dot2 = strchr(dot1+1, '.');
            char* sem = strchr(dot1, ';');

            // Check if the elements are found, otherwise the GMS is not formulated correctly and
            // will result into a Segfault if not checked!
            if ( com != NULL && dot2 != NULL && sem != NULL ) {
              strncpy(gmsOpcode, dot1+1, com-dot1-1);
              strncpy(gmsValue, dot2+1, sem-dot2-1);
              offset = sem-buffer+1;

              cout << "FOUND GMS_OPCODE: '" << gmsOpcode << "' with value '" << gmsValue << "'" << endl;

              if ( strcmp(gmsOpcode, "GMS_NEW") == 0 ) {
                cout << "*** NEW CONNECTION ****" << endl;
                cout << "Size: " << tcpClients->size() << endl;
                if ( tcpClients->find(gmsValue) == tcpClients->end() ) { // Does not exist so create it
                  TCPClient* tcpClient = new TCPClient(GUACD_HOST, GUACD_PORT);
                  tcpClient->initialize();
                  if ( tcpClient->start() == 0 ) {
                    tcpClients->insert({gmsValue, tcpClient});
                    thread t1(thread_guacd_client, running, string(gmsValue), tcpClient, queueSend);
                    t1.detach();
                    thread t2(thread_guacd_client_send, running, string(gmsValue), tcpClient, queueSend);
                    t2.detach();
                  } else {
                    cout << "ERROR: Cannot connect to guacd server!" << endl;
                    string gmsClose = "9.GMS_CLOSE," + string(gmsValue) + ";";
                    queueSend->push(gmsClose); // Send to the other side this connection cannot connect
                  }
                } else {
                  cout << "WARNING: TCPClient does already exist!" << endl;
                }
    
              } else if ( strcmp(gmsOpcode, "GMS_SEL") == 0 ) {
                cout << "*** SELECT CONNECTION ***" << endl;
                if ( tcpClients->find(string(gmsValue)) != tcpClients->end() ) { // Yes! Exists!
                  gmsID = gmsValue;
                } else {
                  cout << "ERROR: Could not find the requested connection.";
                  string gmsClose = "9.GMS_CLOSE," + string(gmsValue) + ";"; 
                  queueSend->push(gmsClose); // Send to the other side this connection is closed
                }

              } else if ( strncmp(gmsOpcode, "GMS_CLOSE", 12) == 0 ) {
                cout << "*** CLOSE CONNECTION ****" << endl;
                if ( tcpClients->find(string(gmsValue)) != tcpClients->end() ) { // Yes! Exists!
                  gmsID = gmsValue;
                  // TODO: How to do this?
                } else {
                  cout << "ERROR: Closing connection that does not exist.";
                }
              }
            }
          }
          if ( strlen(buffer) > 0 && buffer[0] != '\n' && buffer[0] != '\0' ) {
            queueRecv->push(string(buffer + offset));
          }

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

/*
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  bool running = true;

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  unordered_map<string, TCPClient*> tcpClients;
  queue<string> queueDataDiodeSend;
  queue<string> queueDataDiodeRecv;

  thread threadDataDiodeSend(thread_datadiode_send, &running, &queueDataDiodeSend);
  thread threadDataDiodeRecv(thread_datadiode_recv, &running, &tcpClients, &queueDataDiodeSend, &queueDataDiodeRecv);

  while ( running ) {
    sleep(30);
    queueDataDiodeSend.push("13.GMS_HEARTBEAT;");
  }
  
  return 0;
}

