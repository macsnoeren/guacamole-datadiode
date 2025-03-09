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
#include <list>

#include <guacamole/util.h>
#include <udpclient.hpp>
#include <tcpclient.hpp>

using namespace std;

// Buffer size used to read the messages from gmserver or gmclient.
constexpr int BUFFER_SIZE = 10240;

// Default host configuration to connect to the gmserver or gmclient.
constexpr const char GMx_HOST[] = "127.0.0.1";

// Default port configuration to connect to the mserver or gmclient.
constexpr int GMx_PORT = 10000; // Get data GMserver out

// Default host configuration to send UDP/IP traffic to gmproxyout.
constexpr const char DATA_DIODE_SEND_HOST[] = "127.0.0.1";

// Default port configuration to send UDP/IP traffic to gmproxyout.
constexpr int DATA_DIODE_SEND_PORT = 40000;

/*
 * Struct that holds the arguments of the application and is used to
 * provide a centralized configuration to the different parts of the
 * application.
 */
struct Arguments {
  string gmx_host;
  int gmx_port;
  string ddout_host;
  int ddout_port;
  bool test;
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
 * The thread that is responsible to process the queue and send the data to the gmproxyou.
 * 
 * @param[in] args contains the arguments that are configured by the main application.
 * @param[in/out] running is used to check if the program is stil running, can also be set.
 * @param[in] queueSend is used to push the data to the gmproxyout.
 */
void thread_datadiode_send (Arguments args, bool* running, queue<char*>* queueSend) {
  char buffer[BUFFER_SIZE];

  UDPClient udpClient(args.ddout_host, args.ddout_port);
  udpClient.initialize();

  logging(VERBOSE_INFO, "Starting UDP client to connect to %s:%d\n", args.ddout_host.c_str(), args.ddout_port);
  while ( *running ) {
    while ( !queueSend->empty() ) {
      if ( args.test ) {
        logging(VERBOSE_NO, "UDP client send message: %s\n", queueSend->front());
      }
      logging(VERBOSE_DEBUG, "UDP send: %s\n", queueSend->front());
      ssize_t n = udpClient.sendTo(queueSend->front(), strlen(queueSend->front()));
      if ( n >= 0 ) {
        delete queueSend->front(); // Free memory that has been allocated
        queueSend->pop();
      } else {
        logging(VERBOSE_NO, "Error with client during sending data\n");
        // TODO: What do we need to do here?!
      }
    }
    usleep(5000);
  }
}

/*
 * Print the help of all the options to the console
 */
void help() {
  cout << "Usage: gmproxyin [OPTION]" << endl << endl;
  cout << "Options and their default values" << endl;
  cout << "  -g host, --gmx-host=host   host where it needs to connect to get data from gmserver or gmclient [default: " << GMx_HOST << "]" << endl;
  cout << "  -p port, --gmx-port=port   port where it need to connect to the gmserver ot gmclient            [default: " << GMx_PORT << "]" << endl;
  cout << "  -d host, --ddout-host=host host that the UDP data needs to send to the gmproxyout               [default: " << DATA_DIODE_SEND_HOST << "]" << endl;
  cout << "  -o port, --ddout-port=port port that the gmproxyout is using                                    [default: " << DATA_DIODE_SEND_PORT << "]" << endl;
  cout << "  -t, --test                 testing mode will send UDP messages to gmproxyout" << endl;
  cout << "  -v                         verbose add v's to increase level" << endl;
  cout << "  -h, --help                 show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 * Main application where it all starts. It create the thread to process the queue to
 * send the UDP/IP data to gmproxy out. Furthermore, it processes the queue to send the
 * data to the gmserver or gmclient.
 * @param[in] argc is the number of arguments that are available.
 * @param[in] argv[] is the list of arguments that are given by the command line.
 * @return exit status is returned.
 */
int main (int argc, char *argv[]) {
  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  // Create the default configuration.
  Arguments arguments;
  arguments.gmx_host = GMx_HOST;
  arguments.gmx_port = GMx_PORT;
  arguments.ddout_host = DATA_DIODE_SEND_HOST;
  arguments.ddout_port = DATA_DIODE_SEND_PORT;
  arguments.test = false;
  arguments.verbosity = VERBOSE_NO;

  // Create the short and long options of the application.
  const char* const short_options = "vthg:p:d:o:";
  static struct option long_options[] = {
    {"test", no_argument, nullptr, 't'},
    {"gmx-host", optional_argument, nullptr, 'g'},
    {"gmx-port", optional_argument, nullptr, 'p'},
    {"ddout-host", optional_argument, nullptr, 'd'},
    {"ddout-port", optional_argument, nullptr, 'o'},
    {"help", no_argument, nullptr, 'h'},
    {0, 0, 0, 0}
  };

  int opt;
  while ( (opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1 ) { 
    switch(opt) {
      case 'h':
        help(); return 0;
        break;
      case 't':
        arguments.test = true;
        break;
      case 'g':
        arguments.gmx_host = string(optarg);
        break;
      case 'p':
        arguments.gmx_port = stoi(optarg);
        break;
      case 'd':
        arguments.ddout_host = string(optarg);
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
  char buffer[BUFFER_SIZE];
  queue<char*> queueDataDiodeSend;

  // Create the thread to send the data-diode data to gmproxyout.
  thread t(thread_datadiode_send, arguments, &running, &queueDataDiodeSend);
  t.detach();

  // Create the connection with the gmserver of gmclient (gmx) and process the queue.
  TCPClient tcpClientGmServer(arguments.gmx_host, arguments.gmx_port);
  tcpClientGmServer.initialize();
  ssize_t n = 0;

  if ( arguments.test ) logging(VERBOSE_NO, "Testing mode!\n");
  while ( arguments.test ) {
    char* m = new char[100];
    logging(VERBOSE_NO, m, "TESTING-GMPROXYIN-MESSAGE-%010ld\n", ++n);
    queueDataDiodeSend.push(m);
    sleep(1);
  }

  logging(VERBOSE_INFO, "Connecting to the gmserver or gmclient %s:%d\n", arguments.gmx_host.c_str(), arguments.gmx_port);
  while ( running ) {
    logging(VERBOSE_DEBUG, "Trying to connect to the gmserver ot gmclient...\n");
    if ( tcpClientGmServer.start() == 0 ) {
      logging(VERBOSE_DEBUG, "Connected with the gmserver or gmclient\n");

      bool active = true;
      while ( active ) {
        n = tcpClientGmServer.receiveFrom(buffer, BUFFER_SIZE);
        logging(VERBOSE_DEBUG, "GMx received: %s\n", buffer);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          if ( n < BUFFER_SIZE ) {
            char* temp = new char[n+1];
            strcpy(temp, buffer);
            queueDataDiodeSend.push(temp);
          } else {
            logging(VERBOSE_NO, "ERROR: buffer size larger than maximum of %d\n", BUFFER_SIZE);
          }

        } else if ( n == 0 ) { // Peer properly shutted down!
          logging(VERBOSE_DEBUG, "GMx connection peer closed connection\n");
          tcpClientGmServer.closeSocket();
          active = false;
          
        } else { // Problem with the client
          logging(VERBOSE_DEBUG, "GMx connection error\n");
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
