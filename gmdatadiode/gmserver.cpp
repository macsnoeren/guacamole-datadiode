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

// Application version
constexpr char VERSION[] = "1.0";

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
  int verbosity;
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
  TCPServer tcpServerSend(args.ddout_port, 1);
  tcpServerSend.initialize();

  if ( tcpServerSend.start() < 0 ) {
    logging(VERBOSE_NO, "Could not bind to the port %d\n", args.ddout_port);
    *running = false;
    return;
  }

  logging(VERBOSE_INFO, "Listening for gmproxyout to connect on port %d\n", args.ddout_port);
  while ( *running ) {
    logging(VERBOSE_DEBUG, "Waiting on gmproxyout connection...\n");
    TCPServerClient* tcpClient = tcpServerSend.waitOnClient();
    if ( tcpClient != NULL ) {
      bool active = true;
      logging(VERBOSE_DEBUG, "gmproxyout client connected\n");
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          char* d = queueSend->front();
          logging(VERBOSE_DEBUG, "Send to gmproxyout: %s\n", d);
          ssize_t n = tcpClient->sendTo(d, strlen(d)); // Send data
          if ( n >= 0 ) {
            delete[] d; // Free the allocated memory
            queueSend->pop();
          } else {
            logging(VERBOSE_NO, "gmproxyout connection error during sending data\n");
            tcpClient->closeSocket();
            active = false;
          }
        }
        usleep(5000);
      }
    } else {
      logging(VERBOSE_NO, "Could not initialize server to listen for gmproxyout clients, port taken?\n");
      *running = false;
    }
    usleep(5000);
  }
  logging(VERBOSE_INFO, "Thread sending data-diode stopped\n");
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
  if ( tcpServerRecv.start() < 0 ) {
    logging(VERBOSE_NO, "Could not bind to the port %d\n", args.ddin_port);
    *running = false;
    return;
  }

  logging(VERBOSE_INFO, "Listening for gmproxyout to connect on port %d\n", args.ddin_port);
  while ( *running ) {
    logging(VERBOSE_DEBUG, "Waiting on gmproxyin connection...\n");
    TCPServerClient* tcpClient = tcpServerRecv.waitOnClient();
    if ( tcpClient != NULL ) {
      bool active = true;
      logging(VERBOSE_DEBUG, "gmproxyin client connected\n");
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
        logging(VERBOSE_DEBUG, "Received from gmproxyin: %s\n", buffer);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          
          validator.processData(buffer, strlen(buffer));

          // Process the data that is received and put it on the send Queue to be send over the data-diode
          queue<char*>* q = validator.getDataQueue();
          if ( q->size() > 0 ) {
            strcpy(buffer, "\0");            
            while ( !q->empty() ) {
              logging(VERBOSE_DEBUG, "Validator gmproxyin queue: %s\n", q->front());
              char* opcode = q->front();
              char gmsOpcode[50] = "";
              char gmsValue[50] = "";

              if ( findGmsOpcode(opcode, gmsOpcode, gmsValue) ) {
                logging(VERBOSE_DEBUG, "Found opcode %s with value %s\n", gmsOpcode, gmsValue);
                if ( strcmp(gmsOpcode, "GMS_START") == 0 ) {
                  if ( gmClientHandles->find(string(gmsValue)) != gmClientHandles->end() ) { // Found
                    logging(VERBOSE_DEBUG, "TCP client Guacamole is found\n");
                    tcpClientHandle = gmClientHandles->at(string(gmsValue));

                  } else { // Not found
                    logging(VERBOSE_DEBUG, "TCP client Guacamole is NOT found, send GMS_CLOSE message\n");
                    char* gmsclose = new char[50];
                    sprintf(gmsclose, "9.GMS_CLOSE,%ld.%s;", strlen(gmsValue), gmsValue);
                    queueSend->push(gmsclose);
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_END") == 0 ) {
                  if ( tcpClientHandle != NULL ) {
                    if ( strcmp(gmsValue, tcpClientHandle->ID.c_str()) != 0 ) {
                      logging(VERBOSE_NO, "thread_datadiode_recv: ERROR GMS protocol (1)\n");
                    }
                  } else {
                    logging(VERBOSE_NO, "thread_datadiode_recv: ERROR GMS protocol (2)");
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_CLOSE") == 0 ) {
                  if ( gmClientHandles->find(string(gmsValue)) != gmClientHandles->end() ) { // Found and close it
                    logging(VERBOSE_DEBUG, "Closing Guacamole client %s\n", gmsValue);
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
          logging(VERBOSE_DEBUG, "gmproxyin peer connection closed\n");
          tcpClient->closeSocket();
          active = false;
          
        } else { // Problem with the client
          logging(VERBOSE_NO, "gmproxyin connection error\n");
          tcpClient->closeSocket();      
          active = false;
        }
        usleep(5000);
      }
    } else {
      logging(VERBOSE_NO, "Could not initialize server to listen for gmproxyin clients, port taken?\n");
      *running = false;
    }
    usleep(5000);
  }
  logging(VERBOSE_INFO, "Thread thread_datadiode_recv stopped\n");
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

  logging(VERBOSE_DEBUG, "Thread started to handle data from Guacamole %s\n", tcpGuacamoleClientHandle->ID.c_str());
  while ( *running && tcpGuacamoleClientHandle->running ) {
    ssize_t n = tcpGuacamoleClient->receiveFrom(buffer, BUFFER_SIZE);
    logging(VERBOSE_DEBUG, "Received from Guacamole %s: %s\n", tcpGuacamoleClientHandle->ID.c_str(), buffer);
    if ( tcpGuacamoleClientHandle->running && n  > 0 ) { // Received message from Guacamole client, possible that the socket has been closed
      buffer[n] = '\0';

      validator.processData(buffer, strlen(buffer)); // Validates the protocol AND get each opcode seperately

      // Process the data that is received and put it on the send Queue to be send over the data-diode
      queue<char*>* q = validator.getDataQueue();
      if ( q->size() > 0 ) {
        bool ready = false;
        char gmsId[50];
        char gmsEnd[50];
        sprintf(gmsId, "9.GMS_START,%ld.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
        sprintf(gmsEnd, "7.GMS_END,%ld.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
        strcpy(buffer, gmsId); // Re-use the buffer
        while ( !q->empty() && !ready ) { // Process the received data
          logging(VERBOSE_DEBUG, "Validator Guacamole %s queue: %s\n", tcpGuacamoleClientHandle->ID.c_str(), q->front());
          char* opcode = q->front();
          if ( strlen(buffer) + strlen(opcode) < BUFFER_SIZE - strlen(gmsEnd) - 1) { // It still fits!
            strcat(buffer, opcode);
            delete[] opcode; // Free the memory space that was created
            q->pop();
          } else {
            ready = true;
          }
        }
        strcat(buffer, gmsEnd);
        
        // Push it onto the sendQueue
        if ( strlen(buffer) < BUFFER_SIZE ) {
          char* temp = new char[strlen(buffer)+1];
          strcpy(temp, buffer);
          queueSend->push(temp);
        } else {
          logging(VERBOSE_NO, "ERROR: buffer size larger than maximum of %d\n", BUFFER_SIZE);
        }
      }

    } else if ( n == 0 ) { // Peer properly shutted down!
      logging(VERBOSE_DEBUG, "Guacamole client %s peer stopped\n", tcpGuacamoleClientHandle->ID.c_str());
      tcpGuacamoleClient->closeSocket();
      tcpGuacamoleClientHandle->running = false;
      
    } else { // Problem with the client
      logging(VERBOSE_NO, "Error Guacamole client %s\n", tcpGuacamoleClientHandle->ID.c_str());
      tcpGuacamoleClient->closeSocket();      
      tcpGuacamoleClientHandle->running = false;
    }
    sleep(0);
  }

  // Send the close connection over the data-diode.
  char* gmsclose = new char[50];
  sprintf(gmsclose, "9.GMS_CLOSE,%ld.%s;", tcpGuacamoleClientHandle->ID.length(), tcpGuacamoleClientHandle->ID.c_str());
  queueSend->push(gmsclose);

  // Delete the tcpClient and set running to false, so the main application is able to delete the handle itself.
  logging(VERBOSE_DEBUG, "Closing Guacamole client %s\n", tcpGuacamoleClientHandle->ID.c_str());
  delete tcpGuacamoleClientHandle->tcpClient;
  tcpGuacamoleClientHandle->tcpClient = NULL;
}

/*
 * When data is received from the data-diode, it needs to be dispatched to the correct Guacamole client.
 * This thread is responsible for this.
 */
void thread_guacamole_client_send (bool* running, TCPServerClientHandle* guacamoleClient, queue<char*>* queueSend) {
  logging(VERBOSE_DEBUG, "Thread started to handle data to Guacamole %s\n", guacamoleClient->ID.c_str());
  while ( guacamoleClient->running ) {
    while ( !guacamoleClient->data.empty() ) {
      char* d = guacamoleClient->data.front();
      logging(VERBOSE_DEBUG, "Guacamole queue: %s\n", d);

      ssize_t n = guacamoleClient->tcpClient->sendTo(d, strlen(d));
      delete[] d; // Free allocated memory
      guacamoleClient->data.pop();

      if ( n < 0 ) {
        logging(VERBOSE_NO, "thread_guacamole_client_send: Error with client during sending data\n");
        guacamoleClient->running = false;
        guacamoleClient->tcpClient->closeSocket();

        // Send the close message to the other side
        char* t = new char[50];
        sprintf(t, "9.GMS_CLOSE,%ld.%s;", guacamoleClient->ID.length(), guacamoleClient->ID.c_str());
        queueSend->push(t);
      }
    }
    usleep(5000);
  }

  logging(VERBOSE_INFO, "Thread guacamole client %s\n", guacamoleClient->ID.c_str());
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
  cout << "  -v                         verbose add v's to increase level" << endl;
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
  arguments.verbosity = VERBOSE_NO;

  // Create the short and long options of the application.
  const char* const short_options = "vhc:p:i:o:";
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
      case 'v':
        arguments.verbosity++;
        if ( arguments.verbosity > VERBOSE_DEBUG ) arguments.verbosity = VERBOSE_DEBUG;
        break;
      default:
        help(); return 0;
    }
  }

  // Set verbose level
  setVerboseLevel(arguments.verbosity);

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

  if ( tcpServerGuacamole.start() < 0 ) {
    logging(VERBOSE_NO, "Could not bind to the port %d\n", arguments.guacamole_port);
    running = false;
    return 0;
  }

  logging(VERBOSE_INFO, "Listening for Guacamole to connect on port %d (max connections: %d)\n", arguments.guacamole_port, arguments.guacamole_max_clients);
  while ( running ) {
    logging(VERBOSE_DEBUG, "Waiting on Guacamole connection...\n");
    TCPServerClient* tcpGuacamoleClient = tcpServerGuacamole.waitOnClient();
    if ( tcpGuacamoleClient != NULL ) {
      logging(VERBOSE_DEBUG, "Guacamole client connected\n");
      
      // Create unique id to be assiocated to this connection
      string id = createUniqueId();
      while ( guacamoleClientHandles.find({id}) != guacamoleClientHandles.end() ) {
        id = createUniqueId();
      }

      // Create the new handle with unique ID.
      TCPServerClientHandle *tcpServerClientHandle = new TCPServerClientHandle;
      *tcpServerClientHandle = {
        tcpGuacamoleClient,
        true,
        id
      };

      logging(VERBOSE_DEBUG, "New Guacamole client added %s\n", id.c_str());
      guacamoleClientHandles.insert({id, tcpServerClientHandle}); // Add the client handle
      
      // Send the new connection to the other side.
      char* t = new char[50];
      sprintf(t, "7.GMS_NEW,%ld.%s;", id.length(), id.c_str());
      queueDataDiodeSend.push(t);
      
      thread t1(thread_guacamole_client_recv, &running, tcpServerClientHandle, &queueDataDiodeSend, &queueDataDiodeRecv);
      thread t2(thread_guacamole_client_send, &running, tcpServerClientHandle, &queueDataDiodeSend);
      t1.detach();
      t2.detach();

    } else {
      logging(VERBOSE_NO, "Could not initialize server to listen for Guacamole clients, port taken?\n");
      running = false;
    }
  }
  return 0;
}
