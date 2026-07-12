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

#include <cstdlib>
#include <iostream>
#include <sys/socket.h>

/**
 * @brief Requested size (bytes) for a bridge UDP socket buffer.
 *
 * Larger receive/send buffers absorb traffic bursts — e.g. RDP image frames,
 * doubled across two sessions — that otherwise overflow the default (~208 KB)
 * and make the kernel silently drop datagrams. Since the bridge has no
 * retransmit, a single dropped datagram corrupts a channel's stream.
 *
 * The kernel caps the effective size at net.core.rmem_max / wmem_max, so to go
 * above ~425 KB you must raise those on the *host* (they are not per-container):
 *   sudo sysctl -w net.core.rmem_max=8388608 net.core.wmem_max=8388608
 * Override the request here with BRIDGE_SOCKBUF_BYTES.
 */
inline int desired_bridge_sockbuf() {
    const char *env = std::getenv("BRIDGE_SOCKBUF_BYTES");
    int v = env ? std::atoi(env) : 0;
    return v > 0 ? v : 4 * 1024 * 1024; // 4 MB default request
}

/**
 * @brief Enlarge a UDP socket buffer (SO_RCVBUF or SO_SNDBUF) and log the result.
 *
 * The kernel doubles the requested value for bookkeeping and caps it at the
 * net.core max; logging the granted size makes a too-low host limit obvious.
 */
inline void set_bridge_sockbuf(int fd, int optname, const char *tag) {
    int want = desired_bridge_sockbuf();
    ::setsockopt(fd, SOL_SOCKET, optname, &want, sizeof(want));

    int got = 0;
    socklen_t glen = sizeof(got);
    if (::getsockopt(fd, SOL_SOCKET, optname, &got, &glen) == 0)
        std::cout << tag << ": requested " << want << " B, kernel granted "
                  << got << " B" << std::endl;
}
