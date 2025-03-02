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

// A struct to maintain the state of a TCPServerClient
struct TCPServerClientHandle {
    TCPServerClient* tcpClient;
    bool running;
    std::string ID;
  };

// GMSProtocol over Guacamole protocol: d.GMS_SSS,d.VVV;
bool findGmsOpcode (const char* data, char* gmsOpcode, char* gmsValue, long* offset) {
    *offset = 0;
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
        *offset = sem-data+1;
        std::cout << "FOUND GMS_OPCODE: '" << gmsOpcode << "' with value '" << gmsValue << "'" << std::endl;
        return true;
      }
    }
    return false;
}
  