/*
 * Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
 * Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <string>

class UDPReceiver {
  private:
    std::string host;
    int port;
    int sock_fd;

  public:
    UDPReceiver(int port) : port(port) {}

    /**
     * @brief Closes the socket
     */
    ~UDPReceiver();

    /**
     * @brief Opens the socket to receive on
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Receive UDP messages in buffer
     * @return How many bytes were received
     */
    int Receive(char buffer[], size_t len);
};
