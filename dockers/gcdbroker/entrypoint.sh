#!/bin/sh

# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

#
# Translate environment variables into CLI arguments for gcdbroker.
#
#   gcdbroker <guacd_ip> <guacd_port> <udp_recv_port> <udp_send_ip> <udp_send_port>
#
# Environment variables:
#   GUACD_IP        TCP host of the real guacd            [default: guacd-high-side]
#   GUACD_PORT      TCP port of guacd                     [default: 4822]
#   UDP_RECV_PORT   UDP port receiving from the guard     [default: 5501]
#   UDP_SEND_IP     low-side broker's diode IP (return)   [required]
#   UDP_SEND_PORT   UDP port on the low-side broker       [default: 5502]
set -eu

: "${UDP_SEND_IP:?UDP_SEND_IP is required (the low-side broker's return IP)}"

set -- \
  "${GUACD_IP:-guacd-high-side}" \
  "${GUACD_PORT:-4822}" \
  "${UDP_RECV_PORT:-5501}" \
  "${UDP_SEND_IP}" \
  "${UDP_SEND_PORT:-5502}"

echo "gcdbroker $*"
exec /usr/local/bin/gcdbroker "$@"
