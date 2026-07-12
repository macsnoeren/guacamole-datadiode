#!/usr/bin/env bash

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
# Generate the TLS material for the 5node low-tx node (into ./tls next to this
# file) and print how to bring the low-tx node up with TLS enabled. Re-run any
# time; the private key never leaves ./tls — keep that directory out of git
# (.gitignore).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$HERE/../../scripts/generate-tls.sh" "$HERE/tls"

echo
echo "Start the low-tx node with TLS enabled:"
echo "  docker compose -f docker/5node/lowtxnode.compose.yml -f docker/include/tls/compose.yml up --build"
