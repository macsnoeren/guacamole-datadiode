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

constexpr int BUFFER_SIZE = 10240;
constexpr int HEARTBEAT_PUSLE = 20;
constexpr int GUACD_PORT = 4822;
constexpr char GUACD_HOST[] = "127.0.0.1";
constexpr int DATADIODE_SEND_PORT = 10000;
constexpr int DATADIODE_RECV_PORT = 20000;

using namespace std;

struct Arguments {
  string guacd_host;
  int guacd_port;
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

void thread_guacd_client_send (bool* running, TCPClientHandle* tcpClientHandle, queue<char*>* queueSend) {
  cout << "Thread Guacamole client '" << tcpClientHandle->ID << " started" << endl;
  while ( tcpClientHandle->running ) {
    while ( !tcpClientHandle->data.empty() ) {
      char* d = tcpClientHandle->data.front();
      cout << "GMC: '" << tcpClientHandle->ID << "': " << d << endl;

      ssize_t n = tcpClientHandle->tcpClient->sendTo(d, strlen(d));
      tcpClientHandle->data.pop();
      delete d; // Free allocated memory

      if ( n < 0 ) {
        cout << "thread_guacamole_client_send: Error with client during sending data" << endl;
        tcpClientHandle->running = false;
        tcpClientHandle->tcpClient->closeSocket();

        // Send the close message to the other side
        char* t = new char[50];
        sprintf(t, "9.GMS_CLOSE,%d.%s;", tcpClientHandle->ID.length(), tcpClientHandle->ID.c_str());
        queueSend->push(t);
      }
    }
    usleep(5000);
  }

  cout << "Thread guacd client send'" << tcpClientHandle->ID << "'" << endl;
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
  
  while ( tcpGuacdClientHandle->running ) {
    //cout << "Waiting on data from the guacd client '" << tcpClientHandle->ID << "'" << endl;
    ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
    if ( n  > 0 ) { // Received message from receiving data-diode
      buffer[n] = '\0';

      validator.processData(buffer, strlen(buffer));

      // Process the data that is received and put information from the connection
      queue<char*>* q = validator.getDataQueue();
      if ( q->size() > 0 ) {
        bool ready = false;
        char gmsId[50];
        char gmsEnd[50];
        sprintf(gmsId, "9.GMS_START,%d.%s;", tcpGuacdClientHandle->ID.length(), tcpGuacdClientHandle->ID.c_str());
        sprintf(gmsEnd, "7.GMS_END,%d.%s;", tcpGuacdClientHandle->ID.length(), tcpGuacdClientHandle->ID.c_str());
        strcpy(buffer, gmsId); // Re-use the buffer

        while ( !q->empty() && !ready ) {
          char* opcode = q->front();
          if ( strlen(buffer) + strlen(opcode) < BUFFER_SIZE -strlen(gmsEnd) - 1) {
            strcat(buffer, opcode);
            q->pop();
            delete opcode; // Free the memory space that has been allocated
          } else {
            ready = true;
          }
        }
        strcat(buffer, gmsEnd);
        cout << "Buffer: '" << buffer << "', length: " << strlen(buffer) << endl;

        // Push it onto the sendQueue
        char* temp = new char[strlen(buffer)+1];
        strcpy(temp, buffer);
        cout << "PUSH ON QUEUE: '" << temp << "'" << endl;
        queueSend->push(temp);
      }
      
    } else if ( n == 0 ) { // Peer properly shutted down!
      cout << "Client connection shutted down" << endl;
      tcpClient->closeSocket();
      tcpGuacdClientHandle->running = false;
      
    } else { // Problem with the client
      cout << "Error with the client connection" << endl;
      tcpClient->closeSocket();      
      tcpGuacdClientHandle->running = false;
    }
    sleep(0);
  }
  
  delete tcpGuacdClientHandle->tcpClient;

  cout << "Closing guacd client" << endl;
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

  while ( *running ) {
    cout << "thread_datadiode_send: Waiting on sending data-diode proxy client connection on port '" << args.ddout_port << "'" << endl;
    TCPServerClient* tcpClient = tcpServerSend.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "thread_datadiode_send: Sending data-diode client connected" << endl;
      while ( active ) {
        while ( active && !queueSend->empty() ) {
          cout << "thread_datadiode_send: Sending data-diode send: " << queueSend->front();
          char *d =  queueSend->front();
          ssize_t n = tcpClient->sendTo(d, strlen(d));
          if ( n >= 0 ) {
            queueSend->pop();
            delete d; // Free the allocated memory
          } else {
            cout << "thread_datadiode_send: Error with client during sending data" << endl;
            tcpClient->closeSocket();
            active = false;
          }
        }
        usleep(5000);
      }
    }
    sleep(0);
  }

  cout << "Thread sending data-diode stopped" << endl;
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

  // Add GMS_NEW ed.

  while ( *running ) {
    cout << "thread_datadiode_recv: Waiting on receive data-diode proxy client connection on port '" << args.ddin_port << "'" << endl;
    TCPServerClient* tcpClient = tcpServerRecv.accept();
    if ( tcpClient != NULL ) {
      bool active = true;
      cout << "thread_datadiode_recv: Receive data-diode client connected" << endl;
      while ( active ) {
        ssize_t n = tcpClient->receiveFrom(buffer, BUFFER_SIZE);
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

              if ( findGmsOpcode(opcode, gmsOpcode, gmsValue) ) {
                if ( strcmp(gmsOpcode, "GMS_START") == 0 ) {
                  if ( guacdClients->find(string(gmsValue)) != guacdClients->end() ) { // Found
                    tcpClientHandle = guacdClients->at(string(gmsValue));

                  } else { // Send close message back!
                    char* t = new char[50];
                    sprintf(t, "9.GMS_CLOSE,%d.%s;", strlen(gmsValue), gmsValue);
                    queueRecv->push(t);
                    tcpClientHandle = NULL;
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
                  if ( guacdClients->find(string(gmsValue)) != guacdClients->end() ) { // Found and close it
                    tcpClientHandle = guacdClients->at(string(gmsValue));
                    tcpClientHandle->running = false;
                    tcpClientHandle->tcpClient->closeSocket();
                  }
                  q->pop();
                } else if ( strcmp(gmsOpcode, "GMS_NEW") == 0 ) {
                  if ( guacdClients->find(string(gmsValue)) == guacdClients->end() ) { // Not found, so create it
                    
                    cout << "Connecting to guacd on host '" << args.guacd_host << "' and port '" << args.guacd_port << "'" << endl;
                    TCPClient* tcpClient = new TCPClient(args.guacd_host, args.guacd_port);
                    tcpClient->initialize();
                    if ( tcpClient->start() == 0 ) { // Connected!
                      cout << "thread_datadiode_recv: Connected with guacd server" << endl;
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
                      perror("Not connected");
                      delete tcpClient;
          
                      // Send the close message to the other side
                      char* t = new char[50];
                      sprintf(t, "9.GMS_CLOSE,%d.%s;", strlen(gmsValue), gmsValue);
                      queueSend->push(t);
                    }
                  }
                  q->pop();

                } else {
                  cout << "ERROR: opcode not found" << endl;
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
        sleep(0);
      }
    }
    sleep(0);
  }
    
  cout << "Thread receiving data-diode stopped" << endl;
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
  cout << "  -h, --help                 show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 * Main will create two threads that create each a TCP/IP server to receive and
 * send data over the data-diodes. Main itself will create a TCP/IP server to
 * accept connections from the Guacamole web client.
 */
int main (int argc, char *argv[]) {
  /* Processing the arguments */
  Arguments arguments;
  arguments.guacd_host = GUACD_HOST;
  arguments.guacd_port = GUACD_PORT;
  arguments.ddin_port  = DATADIODE_RECV_PORT;
  arguments.ddout_port = DATADIODE_SEND_PORT;

  const char* const short_options = "g:p:i:o:h";
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
    if ( optarg != nullptr ) {
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
        default:
          help(); return 0;
      }
    } else {
      help(); return 0;
    }
  }

  /* main */
  bool running = true;

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  queue<char*> queueDataDiodeSend;
  queue<char*> queueDataDiodeRecv;
  unordered_map<string, TCPClientHandle*> guacdClientHandles;

  thread t1(thread_datadiode_send, arguments, &running, &queueDataDiodeSend);
  thread t2(thread_datadiode_recv, arguments, &running, &guacdClientHandles, &queueDataDiodeSend, &queueDataDiodeRecv);
  t1.detach();
  t2.detach();

  while ( running ) {
    //sleep(HEARTBEAT_PUSLE); // TODO?!
    //queueDataDiodeSend.push("13.GMS_HEARTBEAT;");

/*
    // Process the recv queue
    while ( !queueDataDiodeRecv.empty() ) {
      cout << "Recv from Guacamole: " << queueDataDiodeRecv.front();

      char opcode[50];
      char value[50];
      long offset = 0;

      if ( findGmsOpcode( queueDataDiodeRecv.front().c_str(), opcode, value, &offset ) ) { // Found GMS info
        if ( strcmp(opcode, "GMS_NEW") == 0 ) {
          cout << "Connecting to guacd on host '" << arguments.guacd_host << "' and port '" << arguments.guacd_port << "'" << endl;
          TCPClient* tcpClient = new TCPClient(arguments.guacd_host, arguments.guacd_port);
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
    usleep(5000);
    */
  }
  
  return 0;
}
