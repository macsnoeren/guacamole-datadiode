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
# Translate environment variables into CLI arguments for gmlbroker.
#
#   gmlbroker <guac_listen_ip> <guac_listen_port> <udp_recv_port> \
#             <udp_send_ip> <udp_send_port>
#
# Environment variables:
#   GUAC_LISTEN_IP    TCP bind for the Guacamole web server   [default: 0.0.0.0]
#   GUAC_LISTEN_PORT  TCP port for the Guacamole web server   [default: 4823]
#   UDP_RECV_PORT     UDP port for the return path (from high) [default: 5502]
#   UDP_SEND_IP       next hop on the bridge (the guard)       [required]
#   UDP_SEND_PORT     UDP port on the guard                    [default: 5500]
set -eu

: "${UDP_SEND_IP:?UDP_SEND_IP is required (the guard's diode IP)}"

set -- \
  "${GUAC_LISTEN_IP:-0.0.0.0}" \
  "${GUAC_LISTEN_PORT:-4823}" \
  "${UDP_RECV_PORT:-5502}" \
  "${UDP_SEND_IP}" \
  "${UDP_SEND_PORT:-5500}"

echo "gmlbroker $*"
exec /usr/local/bin/gmlbroker "$@"
