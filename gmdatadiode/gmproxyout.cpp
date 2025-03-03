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

#include <udpserver.hpp>
#include <tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr const char GMx_HOST[] = "127.0.0.1";
constexpr int GMx_PORT = 20000; // IN
constexpr int DATADIODE_RECV_PORT = 40000;

using namespace std;

struct Arguments {
  string gmx_host;
  int gmx_port;
  int ddin_port;
};

/*
 * Required to catch SIGPIPE signal to prevent the closing of the application.
 * The SIGPIPE signal is triggered by socket.sendto when the socket has been
 * closed by the peer.
 */
void signal_sigpipe_cb (int signum) {
  // Do nothing!
}

void thread_datadiode_recv (Arguments args, bool* running, queue<string>* queueRecv) {
  char buffer[BUFFER_SIZE];

  UDPServer udpServer(args.ddin_port);
  udpServer.initialize();
  udpServer.start();

  while ( *running ) {
    cout << "Waiting on data from data-diode on port '" << args.ddin_port << "'" << endl;
    ssize_t n = udpServer.receiveFrom(buffer, BUFFER_SIZE);
    if ( n  > 0 ) { // Received message from receiving data-diode
      buffer[n] = '\0';
      cout << "Received data-diode data: " << buffer;
      queueRecv->push(string(buffer));
      
    } else { // Problem with the client
      cout << "Error with the client connection" << endl;
      // What to do?!
    }
    usleep(5000);
  }
  cout << "Thread sending data-diode stopped" << endl;
}

/*
 * Print the help of all the options to the console
 */
void help() {
  cout << "Usage: gmproxyout [OPTION]" << endl << endl;
  cout << "Options and their default values" << endl;
  cout << "  -g host, --gmx-host=host  host where it needs to connect to send data from gmserver or gmclient [default: " << GMx_HOST << "]" << endl;
  cout << "  -p port, --gmx-port=port  port where it need to connect to the gmserver ot gmclient             [default: " << GMx_PORT << "]" << endl;
  cout << "  -i port, --ddin-port=port port that the data is received from gmproxyin on UDP port             [default: " << DATADIODE_RECV_PORT << "]" << endl;
  cout << "  -h, --help                show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 */
int main (int argc, char *argv[]) {
  // Parse command-line options
  Arguments arguments;
  arguments.gmx_host = GMx_HOST;
  arguments.gmx_port = GMx_PORT;
  arguments.ddin_port = DATADIODE_RECV_PORT;

  const char* const short_options = "g:p:i:h";
  static struct option long_options[] = {
    {"gmx-host", optional_argument, nullptr, 'g'},
    {"gmx-port", optional_argument, nullptr, 'p'},
    {"ddin-port", optional_argument, nullptr, 'i'},
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
          arguments.gmx_host = string(optarg);
          break;
        case 'p':
          arguments.gmx_port = stoi(optarg);
          break;
        case 'i':
          arguments.ddin_port = stoi(optarg);
          break;
        default:
          help(); return 0;
      }
    } else {
      help(); return 0;
    }
  }

  // Main
  bool running = true;
  char buffer[BUFFER_SIZE];

  signal(SIGPIPE, signal_sigpipe_cb); // SIGPIPE closes application and is issued when sendto is called when peer is closed.

  queue<string> queueDataDiodeRecv;

  thread threadDataDiodeRecv(thread_datadiode_recv, arguments, &running, &queueDataDiodeRecv);

  TCPClient tcpClientGmServer(arguments.gmx_host, arguments.gmx_port);
  tcpClientGmServer.initialize();

  while ( running ) {
    cout << "Try to connect to the GMServer on host " << arguments.gmx_host << " on port " << arguments.gmx_port << endl;
    if ( tcpClientGmServer.start() == 0 ) {
      cout << "Connected with the GMServer" << endl;

      bool active = true;
      while ( active ) {
        while ( active && !queueDataDiodeRecv.empty() ) {
          cout << "Sending over the data-diode send: " << queueDataDiodeRecv.front();
          ssize_t n = tcpClientGmServer.sendTo(queueDataDiodeRecv.front().c_str(), queueDataDiodeRecv.front().length());
          if ( n >= 0 ) {
            queueDataDiodeRecv.pop();
          } else {
            cout << "Error with client during sending data" << endl;
            tcpClientGmServer.closeSocket();
            active = false;
          }
        }
        sleep(0);
      }
    }
    sleep(1);
  }
  return 0;
}

