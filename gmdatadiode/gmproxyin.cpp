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

#include <udpclient.hpp>
#include <tcpclient.hpp>

constexpr int BUFFER_SIZE = 1024;
constexpr const char GMx_HOST[] = "127.0.0.1";
constexpr int GMx_PORT = 10000; // OUT
constexpr const char DATA_DIODE_SEND_HOST[] = "127.0.0.1";
constexpr int DATADIODE_SEND_PORT = 40000;

using namespace std;

struct Arguments {
  string gmx_host;
  int gmx_port;
  string ddout_host;
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

void thread_datadiode_send (Arguments args, bool* running, queue<string>* queueSend) {
  char buffer[BUFFER_SIZE];

  UDPClient udpClient(args.ddout_host, args.ddout_port);
  udpClient.initialize();

  cout << "thread_datadiode_send: UDP client sending data to host '" << args.ddout_host << "' and port '" << args.ddout_port << "'" << endl;
  while ( *running ) {
    while ( !queueSend->empty() ) {
      cout << "thread_datadiode_send: Sending over the data-diode send: " << queueSend->front();
      ssize_t n = udpClient.sendTo(queueSend->front().c_str(), queueSend->front().length());
      if ( n >= 0 ) {
        queueSend->pop();
      } else {
        cout << "thread_datadiode_send: Error with client during sending data" << endl;
        // TODO: What do we need to do here?!
      }
    }
    usleep(5000);
  }
  cout << "thread_datadiode_send: Thread sending data-diode stopped" << endl;
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
  cout << "  -o port, --ddout-port=port port that the gmproxyout is using                                    [default: " << DATADIODE_SEND_PORT << "]" << endl;
  cout << "  -h, --help                 show this help page." << endl << endl;
  cout << "More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode." << endl;
}

/*
 */
int main (int argc, char *argv[]) {
  // Parse the command-line options
  Arguments arguments;
  arguments.gmx_host = GMx_HOST;
  arguments.gmx_port = GMx_PORT;
  arguments.ddout_host = DATA_DIODE_SEND_HOST;
  arguments.ddout_port = DATADIODE_SEND_PORT;

  const char* const short_options = "g:p:d:o:h";
  static struct option long_options[] = {
    {"gmx-host", optional_argument, nullptr, 'g'},
    {"gmx-port", optional_argument, nullptr, 'p'},
    {"ddout-host", optional_argument, nullptr, 'd'},
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

  queue<string> queueDataDiodeSend;

  thread threadDataDiodeSend(thread_datadiode_send, arguments, &running, &queueDataDiodeSend);

  TCPClient tcpClientGmServer(arguments.gmx_host, arguments.gmx_port);
  tcpClientGmServer.initialize();

  while ( running ) {
    cout << "Try to connect to the GMServer on host " << arguments.gmx_host << " on port " << arguments.gmx_port << endl;
    if ( tcpClientGmServer.start() == 0 ) {
      cout << "Connected with the GMServer" << endl;

      bool active = true;
      while ( active ) {
        cout << "Waiting on data from the GMServer" << endl;
        ssize_t n = tcpClientGmServer.receiveFrom(buffer, BUFFER_SIZE);
        if ( n  > 0 ) { // Received message from receiving data-diode
          buffer[n] = '\0';
          cout << "Receive data: " << buffer;
          queueDataDiodeSend.push(string(buffer));

        } else if ( n == 0 ) { // Peer properly shutted down!
          cout << "Client connection shutted down" << endl;
          tcpClientGmServer.closeSocket();
          active = false;
          
        } else { // Problem with the client
          cout << "Error with the client connection" << endl;
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

