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
#include <getopt.h>
#include <thread>
#include <queue>
#include <unordered_map>

#include <guacamole/util.h>
#include <guacamole/validator.hpp>
#include <tcpserver.hpp>
#include <tcpclient.hpp>

using namespace std;

// Buffer size used to read the messages from gmserver or gmclient.
constexpr int BUFFER_SIZE = 10240;

// Default heartbeat pulse configuration
constexpr int HEARTBEAT_PUSLE = 20;

// The default host that is used to connect to the guacd server. 
constexpr char GUACD_HOST[] = "127.0.0.1";

// The default port that is used to connect to the guacd server
constexpr int GUACD_PORT = 4822;

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
  string guacd_host;
  int guacd_port;
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

void thread_guacd_client_send (bool* running, TCPClientHandle* tcpClientHandle, queue<char*>* queueSend) {
  logging(VERBOSE_INFO, "Thread thread_guacd_client_send with client id '%s' started\n", tcpClientHandle->ID.c_str());
  while ( tcpClientHandle->running ) {
    while ( !tcpClientHandle->data.empty() ) {
      char* d = tcpClientHandle->data.front();

      logging(VERBOSE_DEBUG, "Send data to guacd client %s: %d\n", tcpClientHandle->ID.c_str(), d);
      ssize_t n = tcpClientHandle->tcpClient->sendTo(d, strlen(d));
      delete d; // Free allocated memory
      tcpClientHandle->data.pop();

      if ( n < 0 ) {
        logging(VERBOSE_NO, "Error connection during sending data with guacd client %s\n", tcpClientHandle->ID.c_str());
        tcpClientHandle->running = false;
        tcpClientHandle->tcpClient->closeSocket();

        // Send the close message to the other side
        char* t = new char[50];
        sprintf(t, "9.GMS_CLOSE,%ld.%s;", tcpClientHandle->ID.length(), tcpClientHandle->ID.c_str());
        queueSend->push(t);
      }
    }
    usleep(5000);
  }

  logging(VERBOSE_INFO, "Thread thread_guacd_client_send with guacd client id '%s' stoppen\n", tcpClientHandle->ID.c_str());
}

/*
 * The thread that is responsible to handle the data that is received by the
 * guacd server. Data that is validated and put on the queue with connection
 * information.
 * Ready for test
 */
void thread_guacd_client_recv (bool* running, TCPClientHandle* tcpGuacdClientHandle, queue<char*>* queueSend) {
  char buffer[BUFFER_SIZE];
  
  ProtocolValidator validator;
  TCPClient* tcpClient = tcpGuacdClientHandle->tcpClient;
  
    logging(VERBOSE_DEBUG, "Thread thread_guacd_client_recv to receive data from the guacd client '%s'\n", tcpGuacdClientHandle->ID.c_str());
    while ( tcpGuacdClientHandle->running ) {
    ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
    logging(VERBOSE_DEBUG, "Received data from guacd client %s: %s\n", tcpGuacdClientHandle->ID.c_str(), buffer);
    if ( n  > 0 ) { // Received message from receiving data-diode
      buffer[n] = '\0';

      validator.processData(buffer, strlen(buffer));

      // Process the data that is received and put information from the connection
      queue<char*>* q = validator.getDataQueue();
      if ( q->size() > 0 ) {
        bool ready = false;
        char gmsId[50];
        char gmsEnd[50];
        sprintf(gmsId, "9.GMS_START,%ld.%s;", tcpGuacdClientHandle->ID.length(), tcpGuacdClientHandle->ID.c_str());
        sprintf(gmsEnd, "7.GMS_END,%ld.%s;", tcpGuacdClientHandle->ID.length(), tcpGuacdClientHandle->ID.c_str());
        strcpy(buffer, gmsId); // Re-use the buffer

        while ( !q->empty() && !ready ) {
          logging(VERBOSE_DEBUG, "Validator guacd receive queue: %s\n", q->front());
          char* opcode = q->front();
          if ( strlen(buffer) + strlen(opcode) < BUFFER_SIZE -strlen(gmsEnd) - 1) {
            strcat(buffer, opcode);
            delete opcode; // Free the memory space that has been allocated
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
      logging(VERBOSE_NO, "Peer shutted down of guacd clien: %s\n", tcpGuacdClientHandle->ID.c_str());
      tcpClient->closeSocket();
      tcpGuacdClientHandle->running = false;
      
    } else { // Problem with the client
      logging(VERBOSE_NO, "Error with the guacd client connection\n");
      tcpClient->closeSocket();      
      tcpGuacdClientHandle->running = false;
    }
    usleep(5000);
  }
  
  delete tcpGuacdClientHandle->tcpClient;

  logging(VERBOSE_INFO, "Closing guacd client\n");
  tcpGuacdClientHandle->tcpClient = NULL;
}

/*
 * This thread handles the data that is required to be send to the sending 
 * data-diode. The data is eventually send to the guacd server. A queue is
 * used and when there are messages, this is send to the data-diode proxy.
 * The queue is filled by the messages that are send by the Guacamole web
 * client. The data-diode proxy connect to the TCP/IP server that is created
 * by this thread.
 * Ready for test
 * @param bool* running: pointer to the running flag. If false the thread
 *        need to close.
 *        qeueu<string>* queueSend: the queue that contains the string
 *        messages.
 * @return void
 */
void thread_datadiode_send (Arguments args, bool* running, queue<char*>* queueSend) {
  char buffer[BUFFER_SIZE];

  TCPServer tcpServerSend(args.ddout_port, 1);
  tcpServerSend.initialize();
  tcpServerSend.start();

  logging(VERBOSE_INFO, "Listening for the gm proxyin on port %d\n", args.ddout_port);
  while ( *running ) {
    logging(VERBOSE_DEBUG, "Waiting on the gmproxyin to connect...\n");
    TCPServerClient* tcpClient = tcpServerSend.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      logging(VERBOSE_DEBUG, "gmproxyin connected\n");
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          char *d =  queueSend->front();
          logging(VERBOSE_DEBUG, "Send data to gmproxyin: %s\n", d);
          ssize_t n = tcpClient->sendTo(d, strlen(d));
          if ( n >= 0 ) {
            delete d; // Free the allocated memory
            queueSend->pop();
          } else {
            logging(VERBOSE_NO, "Error with gmproxyin client during sending data\n");
            tcpClient->closeSocket();
            active = false;
          }
        }
        usleep(5000);
      }
    }
    usleep(5000);
  }

  logging(VERBOSE_NO, "Thread sending data-diode stopped");
}

/*
 * Here new clients needs to be made by the function if a new connection comes by.
 * Ready for test
 */
void thread_datadiode_recv (Arguments args, bool* running, unordered_map<string, TCPClientHandle*>* guacdClients, queue<char*>* queueSend, queue<char*>* queueRecv) {
  char buffer[BUFFER_SIZE];

  TCPClientHandle* tcpClientHandle = NULL;
  ProtocolValidator validator;
  TCPServer tcpServerRecv(args.ddin_port, 1);
  tcpServerRecv.initialize();
  tcpServerRecv.start();

  logging(VERBOSE_INFO, "Listening for gmproxou on port %d\n", args.ddin_port);
  while ( *running ) {
    logging(VERBOSE_DEBUG, "Waining on gmproxyout to connect...\n");
    TCPServerClient* tcpClient = tcpServerRecv.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      logging(VERBOSE_DEBUG, "proxyout client connected\n");
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
        logging(VERBOSE_DEBUG, "Received data from gmproxyou: %s\n", buffer);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';

          // Process the data that is received and push it to the correct queue
          validator.processData(buffer, strlen(buffer));

          queue<char*>* q = validator.getDataQueue();
          if ( q->size() > 0 ) {
            strcpy(buffer, "\0");
            while ( !q->empty() ) {
              char* opcode = q->front();
              char gmsOpcode[50] = "";
              char gmsValue[50] = "";

              logging(VERBOSE_DEBUG, "Read gmproxyout validation queue: %s\n", q->front());
              if ( findGmsOpcode(opcode, gmsOpcode, gmsValue) ) {
                logging(VERBOSE_DEBUG, "Received GMS opcode %s with value %s\n", gmsOpcode, gmsValue);
                if ( strcmp(gmsOpcode, "GMS_START") == 0 ) {
                  if ( guacdClients->find(string(gmsValue)) != guacdClients->end() ) { // Found
                    tcpClientHandle = guacdClients->at(string(gmsValue));

                  } else { // Send close message back!
                    logging(VERBOSE_DEBUG, "guacs client not found, closing connection.\n");
                    char* t = new char[50];
                    sprintf(t, "9.GMS_CLOSE,%ld.%s;", strlen(gmsValue), gmsValue);
                    queueRecv->push(t);
                    tcpClientHandle = NULL;
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_END") == 0 ) {
                  if ( tcpClientHandle != NULL ) {
                    if ( strcmp(gmsValue, tcpClientHandle->ID.c_str()) != 0 ) {
                      logging(VERBOSE_NO, "thread_datadiode_recv: ERROR GMS protocol (1)\n");
                    }
                  } else {
                    logging(VERBOSE_NO, "thread_datadiode_recv: ERROR GMS protocol (2)\n");
                  }
                  q->pop();

                } else if ( strcmp(gmsOpcode, "GMS_CLOSE") == 0 ) {
                  if ( guacdClients->find(string(gmsValue)) != guacdClients->end() ) { // Found and close it
                    logging(VERBOSE_DEBUG, "Closing the guacd socket %s\n", gmsValue);
                    tcpClientHandle = guacdClients->at(string(gmsValue));
                    tcpClientHandle->running = false;
                    tcpClientHandle->tcpClient->closeSocket();
                  }
                  q->pop();
                } else if ( strcmp(gmsOpcode, "GMS_NEW") == 0 ) {
                  if ( guacdClients->find(string(gmsValue)) == guacdClients->end() ) { // Not found, so create it
                    
                    logging(VERBOSE_INFO, "Connecting to guacd on host '%s' and port '%s'\n", args.guacd_host, args.guacd_port);
                    TCPClient* tcpClient = new TCPClient(args.guacd_host, args.guacd_port);
                    tcpClient->initialize();
                    if ( tcpClient->start() == 0 ) { // Connected!
                      logging(VERBOSE_DEBUG, "Connected with guacd server\n");
                      TCPClientHandle *tcpClientHandle = new TCPClientHandle;
                      *tcpClientHandle = {
                        tcpClient,
                        true,
                        string(gmsValue)
                      };
                      guacdClients->insert({string(gmsValue), tcpClientHandle});
                      thread t1(thread_guacd_client_recv, running, tcpClientHandle, queueSend);
                      thread t2(thread_guacd_client_send, running, tcpClientHandle, queueSend);
                      t1.detach();
                      t2.detach();
          
                    } else { // Not connected
                      logging(VERBOSE_NO, "Could not connect to the guacd server\n");
                      delete tcpClient;
          
                      // Send the close message to the other side
                      char* t = new char[50];
                      sprintf(t, "9.GMS_CLOSE,%ld.%s;", strlen(gmsValue), gmsValue);
                      queueSend->push(t);
                    }
                  }
                  q->pop();

                } else {
                  logging(VERBOSE_NO, "ERROR: opcode %s not found\n", gmsOpcode);
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
          logging(VERBOSE_NO, "Error with the guacd client connection\n");
          tcpClient->closeSocket();      
          active = false;
        }
        usleep(5000);
      }
    }
    usleep(5000);
  }
    
  logging(VERBOSE_INFO, "Thread thread_datadiode_recv stopped\n");
}

/*
 * Print the help of all the options to the console
 */
void help() {
  cout << "Usage: gmclient [OPTION]" << endl << endl;
  cout << "Options and their default values" << endl;
  cout << "  -g host, --guacd-host=host host where guacd is running to connect with  [default: " << GUACD_HOST << "]" << endl;
  cout << "  -p port, --guacd-port=port port where the guacd service is running      [default: " << GUACD_PORT << "]" << endl;
  cout << "  -i port, --ddin-port=port  port that the gmproxyout needs to connect to [default: " << DATADIODE_RECV_PORT << "]" << endl;
  cout << "  -o port, --ddout-port=port port that the gmproxyin needs to connect to  [default: " << DATADIODE_SEND_PORT << "]" << endl;
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
  arguments.guacd_host = GUACD_HOST;
  arguments.guacd_port = GUACD_PORT;
  arguments.ddin_port  = DATADIODE_RECV_PORT;
  arguments.ddout_port = DATADIODE_SEND_PORT;
  arguments.verbosity = VERBOSE_NO;

  // Create the short and long options of the application.
  const char* const short_options = "vhg:p:i:o:";
  static struct option long_options[] = {
    {"guacd-host", optional_argument, nullptr, 'g'},
    {"guacd-port", optional_argument, nullptr, 'p'},
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
      case 'g':
        arguments.guacd_host = string(optarg);
        break;
      case 'p':
        arguments.guacd_port = stoi(optarg);
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
  unordered_map<string, TCPClientHandle*> guacdClientHandles;

  // Create the necessary threads.
  thread t1(thread_datadiode_send, arguments, &running, &queueDataDiodeSend);
  thread t2(thread_datadiode_recv, arguments, &running, &guacdClientHandles, &queueDataDiodeSend, &queueDataDiodeRecv);
  t1.detach();
  t2.detach();

  while ( running ) {
    //sleep(HEARTBEAT_PUSLE); // TODO?!
    //queueDataDiodeSend.push("13.GMS_HEARTBEAT;");
    sleep(10);
  }
  
  return 0;
}
