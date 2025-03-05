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
#include <getopt.h>
#include <thread>
#include <queue>
#include <random>
#include <unordered_map>
#include <mutex>

#include <guacamole/util.h>
#include <guacamole/validator.hpp>
#include <tcpserver.hpp>

using namespace std;

// Buffer size used to read the messages from Guacamole.
constexpr int BUFFER_SIZE = 10240;

// Maximum connections that the Guacamole can make.
constexpr int GUACAMOLE_MAX_CLIENTS = 25;

// The port where the Guacamole can connect to.
constexpr int GUACAMOLE_PORT = 4822;

// The port to which the gmproxin can connect to.
constexpr int DATADIODE_SEND_PORT = 10000;

// The port to which the gmproxyout can connect to.
constexpr int DATADIODE_RECV_PORT = 20000;

/*
 * Struct that holds the arguments of the application and is used to
 * provide a centralized configuration to the different parts of the
 * application.
 */
struct Arguments {
  int guacamole_max_clients;
  int guacamole_port;
  int ddin_port;
  int ddout_port;
};

/*
 * Required to catch SIGPIPE signal to prevent the closing of the application.
 * The SIGPIPE signal is triggered by socket.sendto when the socket has been
 * closed by the peer.
 */
void signal_sigpipe_cb (int signum) {
  // Do nothing!
}

/*
 * This method sends the data in the queueSend to the data-diode.
 * Ready for test
 * @param bool* running: pointer to the running flag. If false the thread
 *        need to close.
 * @param qeueu<string>* queueSend: the queue that contains the string
 *        messages.
 * @return void
 */
void thread_datadiode_send (Arguments args, bool* running, queue<char*>* queueSend) {
  char buffer[BUFFER_SIZE];

  TCPServer tcpServerSend(args.ddout_port, 1);
  tcpServerSend.initialize();
  tcpServerSend.start();

  cout << "Waiting on sending data-diode proxy client connection on port '" << args.ddout_port << "'" << endl;
  while ( *running ) {
    TCPServerClient* tcpClient = tcpServerSend.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "proxyout client is connected" << endl;
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          char* d = queueSend->front();
          ssize_t n = tcpClient->sendTo(d, strlen(d)); // Send data
          if ( n >= 0 ) {
            delete d; // Free the allocated memory
            queueSend->pop();
          } else {
            cout << "Error with client during sending data" << endl;
            tcpClient->closeSocket();
            active = false;
          }
        }
        usleep(5000);
      }
    }
    usleep(5000);
  }
  cout << "Thread sending data-diode stopped" << endl;
}

/*
 * This thread handles the data it receives from the data-diode the
 * is connected to the guacd. It pushed the data to the queue.
 * Ready for test
 * @param bool* running: pointer to the running flag. If false the thread
 *        need to close.
 * @param qeueu<string>* queueRecv: the queue that contains the string
 *        messages.
 * @return void
 */
void thread_datadiode_recv (Arguments args, bool* running, unordered_map<string, TCPServerClientHandle*>* gmClientHandles, queue<char*>* queueRecv, queue<char*>* queueSend) {
  char buffer[BUFFER_SIZE];

  TCPServerClientHandle* tcpClientHandle = NULL;
  ProtocolValidator validator;
  TCPServer tcpServerRecv(args.ddin_port, 1);
  tcpServerRecv.initialize();
  tcpServerRecv.start();

  cout << "Waiting on receive data-diode proxy client connection on port '" << args.ddin_port << "'" << endl;
  while ( *running ) {
    TCPServerClient* tcpClient = tcpServerRecv.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "proxyin client connected" << endl;
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          
          validator.processData(buffer, strlen(buffer));

          // Process the data that is received and put it on the send Queue to be send over the data-diode
          queue<char*>* q = validator.getDataQueue();
          if ( q->size() > 0 ) {
            strcpy(buffer, "\0");            
            while ( !q->empty() ) {
              char* opcode = q->front();
              char gmsOpcode[50] = "";
              char gmsValue[50] = "";

              if ( findGmsOpcode(opcode, gmsOpcode, gmsValue) ) {
                if ( strcmp(gmsOpcode, "GMS_START") == 0 ) {
                  if ( gmClientHandles->find(string(gmsValue)) != gmClientHandles->end() ) { // Found
                    tcpClientHandle = gmClientHandles->at(string(gmsValue));

                  } else { // Not found
                    // Send the close connection over the data-diode.
                    char* gmsclose = new char[50];
                    sprintf(gmsclose, "9.GMS_CLOSE,%d.%s;", strlen(gmsValue), gmsValue);
                    queueSend->push(gmsclose);
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_END") == 0 ) {
                  if ( tcpClientHandle != NULL ) {
                    if ( strcmp(gmsValue, tcpClientHandle->ID.c_str()) != 0 ) {
                      cout << "thread_datadiode_recv: ERROR GMS protocol (1)" << endl;
                    }
                  } else {
                    cout << "thread_datadiode_recv: ERROR GMS protocol (2)" << endl;
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_CLOSE") == 0 ) {
                  if ( gmClientHandles->find(string(gmsValue)) != gmClientHandles->end() ) { // Found and close it
                    tcpClientHandle = gmClientHandles->at(string(gmsValue));
                    tcpClientHandle->running = false;
                    tcpClientHandle->tcpClient->closeSocket();
                  }
                  q->pop();
                }

              } else {
                if ( tcpClientHandle != NULL ) {
                  tcpClientHandle->data.push(q->front()); // Move the to the client using pointer!
                  q->pop();
                }
              }
            }
          }

        } else if ( n == 0 ) { // Peer properly shutted down!
          tcpClient->closeSocket();
          active = false;
          
        } else { // Problem with the client
          cout << "Error with the client connection" << endl;
          tcpClient->closeSocket();      
          active = false;
        }
        usleep(5000);
      }
    }
    usleep(5000);
  }
  cout << "Thread receiving data-diode stopped" << endl;
}

/*
 * The thread handles the messages that are received from the Guacamole client connection.
 * Data that is validated and put on the queue with information of this connection.
 * Ready for test!
 */
void thread_guacamole_client_recv (bool* running, TCPServerClientHandle* tcpGuacamoleClientHandle, queue<char*>* queueSend, queue<char*>* queueRecv) {
  char buffer[BUFFER_SIZE];
  bool active = true;

  ProtocolValidator validator;
  TCPServerClient* tcpGuacamoleClient = tcpGuacamoleClientHandle->tcpClient; 

  while ( *running && tcpGuacamoleClientHandle->running ) {
    ssize_t n = tcpGuacamoleClient->receiveFrom(buffer, BUFFER_SIZE);
    if ( tcpGuacamoleClientHandle->running && n  > 0 ) { // Received message from Guacamole client, possible that the socket has been closed
      buffer[n] = '\0';

      validator.processData(buffer, strlen(buffer)); // Validates the protocol AND get each opcode seperately

      // Process the data that is received and put it on the send Queue to be send over the data-diode
      queue<char*>* q = validator.getDataQueue();
      if ( q->size() > 0 ) {
        bool ready = false;
        char gmsId[50];
        char gmsEnd[50];
        sprintf(gmsId, "9.GMS_START,%d.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
        sprintf(gmsEnd, "7.GMS_END,%d.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
        strcpy(buffer, gmsId); // Re-use the buffer
        while ( !q->empty() && !ready ) { // Process the received data
          char* opcode = q->front();
          if ( strlen(buffer) + strlen(opcode) < BUFFER_SIZE - strlen(gmsEnd) - 1) { // It still fits!
            strcat(buffer, opcode);
            delete opcode; // Free the memory space that was created
            q->pop();
          } else {
            ready = true;
          }
        }
        strcat(buffer, gmsEnd);
        
        // Push it onto the sendQueue
        char* temp = new char[strlen(buffer)+1];
        strcpy(temp, buffer);
        queueSend->push(temp);
      }

    } else if ( n == 0 ) { // Peer properly shutted down!
      cout << "Guacamole web shutted down '" << tcpGuacamoleClientHandle->ID << "'" << endl;
      tcpGuacamoleClient->closeSocket();
      tcpGuacamoleClientHandle->running = false;
      
    } else { // Problem with the client
      cout << "Error Guacamole web '" << tcpGuacamoleClientHandle->ID << "'" << endl;
      tcpGuacamoleClient->closeSocket();      
      tcpGuacamoleClientHandle->running = false;
    }
    sleep(0);
  }

  // Send the close connection over the data-diode.
  char* gmsclose = new char[50];
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
void thread_guacamole_client_send (bool* running, TCPServerClientHandle* guacamoleClient, queue<char*>* queueSend) {
  while ( guacamoleClient->running ) {
    while ( !guacamoleClient->data.empty() ) {
      char* d = guacamoleClient->data.front();
      
      ssize_t n = guacamoleClient->tcpClient->sendTo(d, strlen(d));
      delete d; // Free allocated memory
      guacamoleClient->data.pop();

      if ( n < 0 ) {
        cout << "thread_guacamole_client_send: Error with client during sending data" << endl;
        guacamoleClient->running = false;
        guacamoleClient->tcpClient->closeSocket();

        // Send the close message to the other side
        char* t = new char[50];
        sprintf(t, "9.GMS_CLOSE,%d.%s;", guacamoleClient->ID.length(), guacamoleClient->ID.c_str());
        queueSend->push(t);
      }
    }
    usleep(5000);
  }

  cout << "Thread guacamole client '" << guacamoleClient->ID << "'" << endl;
}

string createUniqueId () {
  time_t timer = time(nullptr);
  char id[80] = "";
  
  sprintf(id, "%ld%ld", timer, random() + random() + random());

  return string(id);
}

/*
 * Print the help of all the options to the console
 */
void help() {
  cout << "Usage: gmserver [OPTION]" << endl << endl;
  cout << "Options and their default values" << endl;
  cout << "  -c num, --max-clients=num  maximal connections that the Guacamole web client can make [default: " << GUACAMOLE_MAX_CLIENTS << "]" << endl;
  cout << "  -p port, --port=port       port where the Guacamole wev client is connecting to       [default: " << GUACAMOLE_PORT << "]" << endl;
  cout << "  -i port, --ddin-port=port  port that the gmproxyout needs to connect to               [default: " << DATADIODE_RECV_PORT << "]" << endl;
  cout << "  -o port, --ddout-port=port port that the gmproxyin needs to connect to                [default: " << DATADIODE_SEND_PORT << "]" << endl;
  cout << "  -h, --help                 show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  // Create the default configuration.
  Arguments arguments;
  arguments.guacamole_max_clients = GUACAMOLE_MAX_CLIENTS;
  arguments.guacamole_port = GUACAMOLE_PORT;
  arguments.ddin_port = DATADIODE_RECV_PORT;
  arguments.ddout_port = DATADIODE_SEND_PORT;

  // Create the short and long options of the application.
  const char* const short_options = "c:p:i:o:h";
  static struct option long_options[] = {
    {"max-clients", optional_argument, nullptr, 'c'},
    {"port", optional_argument, nullptr, 'p'},
    {"ddin-port", optional_argument, nullptr, 'i'},
    {"ddout-port", optional_argument, nullptr, 'o'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  }; 

  int opt;
  while ( (opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1 ) { 
    if ( optarg != nullptr ) {
      switch(opt) {
        case 'h':
          help(); return 0;
          break;
        case 'c':
          arguments.guacamole_max_clients = stoi(optarg);
          break;
        case 'p':
          arguments.guacamole_port = stoi(optarg);
          break;
        case 'i':
          arguments.ddin_port = stoi(optarg);
          break;
        case 'o':
          arguments.ddout_port = stoi(optarg);
          break;
        default:
          help(); return 0;
      }
    } else {
      help(); return 0;
    }
  }

  // Create the running variable, buffer and queue.
  bool running = true;
  queue<char*> queueDataDiodeSend;
  queue<char*> queueDataDiodeRecv;
  unordered_map<string, TCPServerClientHandle*> guacamoleClientHandles;

  // Create the necessary threads
  thread t1(thread_datadiode_send, arguments, &running, &queueDataDiodeSend);
  thread t2(thread_datadiode_recv, arguments, &running, &guacamoleClientHandles, &queueDataDiodeRecv, &queueDataDiodeSend);
  t1.detach();
  t2.detach();
  
  // Create the TCP/IP server to accept connections from the Guacamole web client
  TCPServer tcpServerGuacamole(arguments.guacamole_port, arguments.guacamole_max_clients);
  tcpServerGuacamole.initialize();
  tcpServerGuacamole.start();

  cout << "Waiting on a Guacamole client connection on port '" << arguments.guacamole_port << "' (max: " << arguments.guacamole_max_clients << ")" << endl;
  while ( running ) {
    TCPServerClient* tcpGuacamoleClient = tcpServerGuacamole.accept();
    if ( tcpGuacamoleClient != NULL ) {
      cout << "Guacamole client connected" << endl;
      
      // Create unique id to be assiocated to this connection
      string id = createUniqueId();
      TCPServerClientHandle *tcpServerClientHandle = new TCPServerClientHandle;
      *tcpServerClientHandle = {
        tcpGuacamoleClient,
        true,
        id
      };

      guacamoleClientHandles.insert({id, tcpServerClientHandle}); // Add the client handle
      
      // Send the new connection to the other side.
      char* t = new char[50];
      sprintf(t, "7.GMS_NEW,%d.%s;", id.length(), id.c_str());
      queueDataDiodeSend.push(t);
      
      thread t1(thread_guacamole_client_recv, &running, tcpServerClientHandle, &queueDataDiodeSend, &queueDataDiodeRecv);
      thread t2(thread_guacamole_client_send, &running, tcpServerClientHandle, &queueDataDiodeSend);
      t1.detach();
      t2.detach();
    }
  }
  return 0;
}
