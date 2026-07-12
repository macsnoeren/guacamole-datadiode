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
# Translate environment variables into CLI arguments for guard.
#
#   gmguard <src_port> <dst_ip> <dst_port>
#
# Environment variables:
#   UDP_RECV_PORT   UDP port the guard receives on (from low)  [default: 5500]
#   DST_IP          high-side broker's diode IP                [required]
#   DST_PORT        UDP port on the high-side broker           [default: 5501]
#   GUARD_APPROVE   read by the binary itself; "deny" denies every request
set -eu

: "${DST_IP:?DST_IP is required (the high-side broker's diode IP)}"

set -- \
  "${UDP_RECV_PORT:-5500}" \
  "${DST_IP}" \
  "${DST_PORT:-5501}"

echo "gmguard $* (GUARD_APPROVE=${GUARD_APPROVE:-<approve>})"
exec /usr/local/bin/gmguard "$@"
