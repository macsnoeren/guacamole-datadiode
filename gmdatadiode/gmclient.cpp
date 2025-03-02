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

#include <guacamole/util.h>
#include <tcpserver.hpp>
#include <tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr int HEARTBEAT_PUSLE = 20;
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

void thread_guacd_client_recv (bool* running, TCPClientHandle* tcpClientHandle, queue<string>* queueSend) {
  char buffer[BUFFER_SIZE];
  
  TCPClient* tcpClient = tcpClientHandle->tcpClient;

  while ( tcpClientHandle->running ) {
    cout << "Waiting on data from the GMServer" << endl;
    ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
    if ( n  > 0 ) { // Received message from receiving data-diode
      buffer[n] = '\0';
      cout << "Receive data: " << buffer;

      // Add the connection data to the data to be send.
      char gmssel[50] = "";
      sprintf(gmssel, "7.GMS_SEL,%d.%s;", tcpClientHandle->ID.length(), tcpClientHandle->ID.c_str());
    
      // Send the data over the data-diode
      queueSend->push(gmssel + string(buffer));
      
    } else if ( n == 0 ) { // Peer properly shutted down!
      cout << "Client connection shutted down" << endl;
      tcpClient->closeSocket();
      tcpClientHandle->running = false;
      
    } else { // Problem with the client
      cout << "Error with the client connection" << endl;
      tcpClient->closeSocket();      
      tcpClientHandle->running = false;
    }
    sleep(1);
  }
  
  delete tcpClientHandle->tcpClient;

  cout << "Closing guacd client" << endl;
  tcpClientHandle->tcpClient = NULL;
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
    cout << "thread_datadiode_send: Waiting on sending data-diode proxy client connection" << endl;
    TCPServerClient* tcpClient = tcpServerSend.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "thread_datadiode_send: Sending data-diode client connected" << endl;
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          cout << "thread_datadiode_send: Sending data-diode send: " << queueSend->front();
          ssize_t n = tcpClient->sendTo(queueSend->front().c_str(), queueSend->front().length());
          if ( n >= 0 ) {
            queueSend->pop();
          } else {
            cout << "thread_datadiode_send: Error with client during sending data" << endl;
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
void thread_datadiode_recv (bool* running, unordered_map<string, TCPClientHandle*>* guacdClients, queue<string>* queueSend, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];

  TCPServer tcpServerRecv(DATADIODE_RECV_PORT, 1);
  tcpServerRecv.initialize();
  tcpServerRecv.start();

  while ( *running ) {
    cout << "thread_datadiode_recv: Waiting on receive data-diode proxy client connection" << endl;
    TCPServerClient* tcpClient = tcpServerRecv.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "thread_datadiode_recv: Receive data-diode client connected" << endl;
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          cout << "thread_datadiode_recv: Receive data-diode data: " << buffer;
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
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  bool running = true;

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  queue<string> queueDataDiodeSend;
  queue<string> queueDataDiodeRecv;
  unordered_map<string, TCPClientHandle*> guacdClientHandles;

  thread t1(thread_datadiode_send, &running, &queueDataDiodeSend);
  thread t2(thread_datadiode_recv, &running, &guacdClientHandles, &queueDataDiodeSend, &queueDataDiodeRecv);
  t1.detach();
  t2.detach();

  while ( running ) {
    //sleep(HEARTBEAT_PUSLE);
    //queueDataDiodeSend.push("13.GMS_HEARTBEAT;");

    // Process the recv queue
    while ( !queueDataDiodeRecv.empty() ) {
      cout << "Dispatching data to Guacamole client: " << queueDataDiodeRecv.front();

      char opcode[50];
      char value[50];
      long offset = 0;

      if ( findGmsOpcode( queueDataDiodeRecv.front().c_str(), opcode, value, &offset ) ) { // Found GMS info
        if ( strcmp(opcode, "GMS_NEW") == 0 ) {
          TCPClient* tcpClient = new TCPClient(GUACD_HOST, GUACD_PORT);
          tcpClient->initialize();
          if ( tcpClient->start() == 0 ) { // Connected!
            cout << "thread_datadiode_recv: Connected with guacd server" << endl;
            TCPClientHandle *tcpClientHandle = new TCPClientHandle;
            *tcpClientHandle = {
              tcpClient,
              true,
              string(value)
            };
            guacdClientHandles.insert({string(value), tcpClientHandle});
            thread t(thread_guacd_client_recv, &running, tcpClientHandle, &queueDataDiodeSend);
            t.detach();

          } else { // Not connected
            delete tcpClient;

            // Send the close message to the other side
            char gmsclose[50] = "";
            sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(value), value);
            queueDataDiodeSend.push(string(gmsclose));
          }

        } else if ( strcmp(opcode, "GMS_SEL") == 0 ) {
          if ( guacdClientHandles.find(string(value)) != guacdClientHandles.end() ) { // Found assiocated 
            TCPClientHandle* tcpClientHandle = guacdClientHandles.at(string(value));
            ssize_t n = tcpClientHandle->tcpClient->sendTo(queueDataDiodeRecv.front().c_str()+offset, queueDataDiodeRecv.front().length()-offset);
            if ( n < 0 ) {
              cout << "thread_datadiode_recv: Error with client during sending data" << endl;
              tcpClientHandle->tcpClient->closeSocket();
              tcpClientHandle->running = false;

              // Send the close message to the other side
              char gmsclose[50] = "";
              sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(value), value);
              queueDataDiodeSend.push(string(gmsclose));
            }

          } else { // Not found, send the close message to the other side
            char gmsclose[50] = "";
            sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(value), value);
            queueDataDiodeSend.push(string(gmsclose));
          }

        } else if ( strcmp(opcode, "GMS_CLOSE") == 0 ) {
          if ( guacdClientHandles.find(string(value)) != guacdClientHandles.end() ) { // Found assiocated 
            TCPClientHandle* tcpClientHandle = guacdClientHandles.at(string(value));
            tcpClientHandle->tcpClient->closeSocket();
            tcpClientHandle->running = false;
          }
        }
      }
      queueDataDiodeRecv.pop();
    }

    // Clean up the handles
    unordered_map<string, TCPClientHandle*>::iterator it = guacdClientHandles.begin();
    while ( it!=guacdClientHandles.end() ) {
      if ( it->second == NULL ) {
        cout << "thread_guacamole_client_send: Final cleanup" << endl;
        it = guacdClientHandles.erase(it);

      } else if ( it->second->running == false && it->second->tcpClient == NULL ) {
        cout << "thread_guacamole_client_send: Trash Guacamole client '" << it->first << "'" << endl;
        delete it->second;
        guacdClientHandles[it->first] = NULL;

      } else {
        ++it;
      }
    }
    sleep(0);
  }
  
  return 0;
}

