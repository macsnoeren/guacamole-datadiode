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
#include <udpserver.hpp>
#include <tcpclient.hpp>

using namespace std;

// Buffer size used to read the messages from gmserver or gmclient.
constexpr int BUFFER_SIZE = 10240;

// Default host configuration to connect to the gmserver or gmclient.
constexpr const char GMx_HOST[] = "127.0.0.1";

// Default port configuration to connect to the gmserver or gmclient.
constexpr int GMx_PORT = 20000;

// Default port configuration to accept UDP/IP traffic from gmproxyin.
constexpr int DATA_DIODE_RECV_PORT = 40000;

/*
 * Struct that holds the arguments of the application and is used to
 * provide a centralized configuration to the different parts of the
 * application.
 */
struct Arguments {
  string gmx_host;
  int gmx_port;
  int ddin_port;
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
 * The thread that is responsible to create an UDP/IP server to receive the UDP/IP
 * messages from gmproxyin via the data-diode.
 * 
 * @param[in] args contains the arguments that are configured by the main application.
 * @param[in/out] running is used to check if the program is stil running, can also be set.
 * @param[in] queueRecv is used to push the data that is received to.
 */
void thread_datadiode_recv (Arguments args, bool* running, queue<char*>* queueRecv) {
  char buffer[BUFFER_SIZE];

  UDPServer udpServer(args.ddin_port);
  udpServer.initialize();
  udpServer.start();

  logging(VERBOSE_INFO, "UDP Server started listening to port %d\n", args.ddin_port);
  while ( *running ) {
    ssize_t n = udpServer.receiveFrom(buffer, BUFFER_SIZE);
    logging(VERBOSE_DEBUG, "UDP received: %s\n", buffer);
    if ( n  > 0 ) {
      buffer[n] = '\0';
      if ( args.test ) {
        logging(VERBOSE_NO, "Received from gmproxyin: %s\n", buffer);
      }
      if ( !args.test ) {
        if ( n < BUFFER_SIZE ) {
          char* temp = new char[n+1];
          strcpy(temp, buffer);
          queueRecv->push(temp);
        } else {
          logging(VERBOSE_NO, "ERROR: buffer size larger than maximum of %d\n", BUFFER_SIZE);
        }
      }

    } else { // Problem with the client
      logging(VERBOSE_NO, "Error with the client connection\n");
      // What to do?!
    }
    usleep(5000);
  }
  logging(VERBOSE_INFO, "Thread 'thread_datadiode_recv' stopped\n");
}

/*
 * Print the help of all the options to the console
 */
void help() {
  cout << "Usage: gmproxyout [OPTION]" << endl << endl;
  cout << "Options and their default values" << endl;
  cout << "  -g host, --gmx-host=host  host where it needs to connect to send data from gmserver or gmclient [default: " << GMx_HOST << "]" << endl;
  cout << "  -p port, --gmx-port=port  port where it need to connect to the gmserver or gmclient             [default: " << GMx_PORT << "]" << endl;
  cout << "  -i port, --ddin-port=port port that the data is received from gmproxyin on UDP port             [default: " << DATA_DIODE_RECV_PORT << "]" << endl;
  cout << "  -t, --test                 testing mode will send UDP messages to gmproxyout" << endl;
  cout << "  -v                         verbose add v's to increase level" << endl;
  cout << "  -h, --help                show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 * Main application where it all starts. It create the thread to receive the UDP/IP data from
 * the gmproxyin. It checks the queue and will send the queue to the gmserver or gmclient.
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
  arguments.ddin_port = DATA_DIODE_RECV_PORT;
  arguments.test = false;
  arguments.verbosity = VERBOSE_NO;

  // Create the short and long options of the application.
  const char* const short_options = "vthg:p:i:";
  static struct option long_options[] = {
    {"gmx-host", optional_argument, nullptr, 'g'},
    {"gmx-port", optional_argument, nullptr, 'p'},
    {"ddin-port", optional_argument, nullptr, 'i'},
    {"test", no_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
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
      case 'i':
        arguments.ddin_port = stoi(optarg);
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
  queue<char*> queueDataDiodeRecv;

  // Create the thread to receive the data-diode data from gmproxyin.
  thread t(thread_datadiode_recv, arguments, &running, &queueDataDiodeRecv);
  t.detach();

  // Create the connection with the gmserver of gmclient (gmx) and process the queue.
  TCPClient tcpClientGmx(arguments.gmx_host, arguments.gmx_port);
  tcpClientGmx.initialize();

  if ( arguments.test ) logging(VERBOSE_NO, "Testing mode!\n");
  logging(VERBOSE_INFO, "Connecting to the gmserver or gmclient %s:%d\n", arguments.gmx_host.c_str(), arguments.gmx_port);
  while ( running ) {
    logging(VERBOSE_DEBUG, "Trying to connect to the gmserver or gmclient...\n");
    if ( tcpClientGmx.start() == 0 ) {
      logging(VERBOSE_INFO, "Connected with the gmserver or gmclient\n");

      bool active = true;
      while ( active ) {
        while ( active && !queueDataDiodeRecv.empty() ) {
          ssize_t n = tcpClientGmx.sendTo(queueDataDiodeRecv.front(), strlen(queueDataDiodeRecv.front()));
          logging(VERBOSE_DEBUG, "Send to gmx: %s\n", queueDataDiodeRecv.front());
          if ( n >= 0 ) {
            delete queueDataDiodeRecv.front(); // Free the memory that has been allocated
            queueDataDiodeRecv.pop();
          } else {
            logging(VERBOSE_NO, "Error with client during sending data\n");
            tcpClientGmx.closeSocket();
            active = false;
          }
        }
        usleep(5000);
      }
    }
    sleep(1);
  }
  return 0;
}

