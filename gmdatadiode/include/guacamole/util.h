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
#include <stdarg.h>
#include <ctime> // Include for time-related functions
#include <queue>
#include <cstdio>  // For vprintf

#include <tcpserver.hpp>
#include <tcpclient.hpp>

// Verbosity level
static int _verbose = 0;

// Verbosity level constants
constexpr int VERBOSE_NO = 0;
constexpr int VERBOSE_INFO = 1;
constexpr int VERBOSE_WARN = 2;
constexpr int VERBOSE_DEBUG = 3;

/*
 * Struct to handle the TCPServerClients that are connected with the Guacamole web server.
 * It holds the socket connection, the running status, a unique ID, and a queue of data 
 * that needs to be sent to the Guacamole web server.
 */
struct TCPServerClientHandle {
    TCPServerClient* tcpClient;  ///< TCP client for the connection
    bool running;                ///< Status indicating if the client is still active
    std::string ID;              ///< Unique identifier for the client
    std::queue<char*> data;      ///< Data queue to hold data to be sent to the server
};

/*
 * Struct to handle the TCPServerClients that are connected with the guacd server.
 * Similar to the above structure, but for the guacd server connection.
 */
struct TCPClientHandle {
    TCPClient* tcpClient;  ///< TCP client for the connection
    bool running;          ///< Status indicating if the client is still active
    std::string ID;        ///< Unique identifier for the client
    std::queue<char*> data; ///< Data queue to hold data to be sent to the server
};

/*
 * Create a unique identifier to associate the Guacamole/guacd clients.
 * The ID is a random hexadecimal string based on time and randomness.
 * @return A unique random hexadecimal string ID.
 */
std::string createUniqueId() {
  time_t timer = time(nullptr);
  srand(static_cast<unsigned>(timer));  // Use current time for better randomness

  unsigned long long int v = 0;
  for (int i = 0; i < 16; ++i) {
    v = (v << 8) + rand() % 256;
  }

  char id[80] = "";
  std::sprintf(id, "%llX", v);  // Convert to hexadecimal format

  return std::string(id);
}

/*
 * Finds and extracts the GMS_ opcode and value from the provided data.
 * The data format should follow the pattern: 'd.GMS_XXX,d.VVV;'.
 * Example: 7.GMS_NEW,20.273948264759382736493;
 * 
 * @param data The raw data to be parsed.
 * @param gmsOpcode The extracted GMS opcode (e.g., "GMS_NEW").
 * @param gmsValue The extracted GMS value (e.g., "20.273948264759382736493").
 * @return True if the GMS opcode and value are successfully extracted, false otherwise.
 */
bool findGmsOpcode(const char* data, char* gmsOpcode, char* gmsValue) {
    if (strstr(data, ".GMS_") != nullptr) {  // Found GMS opcode
      char* dot1 = strchr((char*) data, '.');
      char* com = strchr(dot1, ',');
      char* dot2 = strchr(dot1 + 1, '.');
      char* sem = strchr(dot1, ';');
  
      // Ensure all components are found to avoid segmentation faults
      if (com != nullptr && dot2 != nullptr && sem != nullptr) {
        strncpy(gmsOpcode, dot1 + 1, com - dot1 - 1);  // Extract the opcode
        strncpy(gmsValue, dot2 + 1, sem - dot2 - 1);   // Extract the value
        gmsOpcode[com - dot1 - 1] = '\0';
        gmsValue[sem - dot2 - 1] = '\0';
        return true;
      }
    }
    return false;  // Return false if GMS pattern not found
}

/*
 * Sets the verbosity level for logging.
 * The verbosity level controls the amount of log information displayed.
 * 
 * @param level The verbosity level to set. Use the defined constants:
 *        - VERBOSE_NO: No output
 *        - VERBOSE_INFO: Informational messages
 *        - VERBOSE_WARN: Warnings
 *        - VERBOSE_DEBUG: Debug messages
 */
void setVerboseLevel(int level) {
  _verbose = level;
}

/*
 * Outputs a log message to the console based on the verbosity level.
 * The log message can include variable arguments, similar to printf statements.
 * 
 * @param level The verbosity level of the message. If the message level is
 *        higher than the current verbosity level, it will not be printed.
 * @param fmt The format string, similar to printf.
 * @param ... The variables to print in the format string.
 */
void logging(int level, const char* fmt, ...) {
  if (level <= _verbose) {
    // Get current time for timestamp in logs
    std::time_t now = std::time(nullptr);
    char timeStr[20];  // Buffer to hold formatted time string
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // Print timestamp and verbosity level
    printf("[%s] [LEVEL %d] ", timeStr, level);

    // Process the variable arguments like printf
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}
