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
#pragma once

#include <string.h>

#include <tcpserver.hpp>
#include <tcpclient.hpp>

/*
 * A struct to handle the TCPServerClients that are connected with the Guacamole web server.
 * It holds the socket connection, whether it is stil running, an unique ID and the queue
 * with data that has to be send to the Guacamole web server.
 */
struct TCPServerClientHandle {
    TCPServerClient* tcpClient;
    bool running;
    std::string ID;
    std::queue<char*> data;
};

/*
 * A struct to handle the TCPServerClients that are connected with the guacd server.
 * It holds the socket connection, whether it is stil running, an unique ID and the queue
 * with data that has to be send to the guacd server.
 */
struct TCPClientHandle {
    TCPClient* tcpClient;
    bool running;
    std::string ID;
    std::queue<char*> data;
};

/*
 * Create a unique ID that can be used to assiocate the Guacamole/guacd clients.
 * @return a random heximal string.
 */
std::string createUniqueId () {
  time_t timer = time(nullptr);
  srand(time(0));

  unsigned long long int v = 0;
  for (int i=0; i < 16; i++ ) {
    v = (v << 8) + rand() % 256;
  }
  
  char id[80] = "";
  std::sprintf(id, "%lX", v);

  return std::string(id);
}

/*
 * In order to provide information concerning the different connections over the data-diode
 * channel, a GMS_ protocol is created on top on the Guacamole protocol. This method finds
 * this opcode and value to make it easier to extract the GMS_ protocol. The GMS_ protocol
 * is constructed like 'd.GMS_XXX,d.VVV;'. Example: 7.GMS_NEW,20.273948264759382736493;
 */
bool findGmsOpcode (const char* data, char* gmsOpcode, char* gmsValue) {
    if ( strstr(data, ".GMS_") != NULL ) { // Found GMS opcode
      char* dot1 = strchr((char*) data, '.'); // This exist and does not result in a NULL pointer
      char* com = strchr(dot1, ',');    // We can safely use dot1 in the other search terms.
      char* dot2 = strchr(dot1+1, '.');
      char* sem = strchr(dot1, ';');
  
      // Check if the elements are found, otherwise the GMS is not formulated correctly and
      // will result into a Segfault if not checked!
      if ( com != NULL && dot2 != NULL && sem != NULL ) {
        strncpy(gmsOpcode, dot1+1, com-dot1-1);
        strncpy(gmsValue, dot2+1, sem-dot2-1);
        gmsOpcode[com-dot1-1] = '\0';
        gmsValue[sem-dot2-1] = '\0';
        //*offset = sem-data+1;
        return true;
      }
    }
    return false;
}
  