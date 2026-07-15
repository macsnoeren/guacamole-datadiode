#!/usr/bin/env bash

# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
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

# Clean build of every C++ component, for CodeQL to trace.
#
# CodeQL builds a C++ database by observing real compiler invocations, so this
# must force a full recompile (hence the `rm -rf build`) — if Meson skips
# already-built targets, CodeQL sees no source and the database is empty.
#
# Run it via `codeql database create ... --command=scripts/codeql-build-cpp.sh`
# (see scripts/codeql-scan.sh), not on its own.
set -euo pipefail

cd "$(dirname "$0")/.."  # the 3dd/ directory

APPS=(
  apps/shared
  apps/guard
  apps/gmlbroker
  apps/gcdbroker
  apps/hrx_proxy
  apps/lrx_proxy
)

for app in "${APPS[@]}"; do
  echo ">>> building $app"
  rm -rf "$app/build"
  meson setup "$app/build" "$app"
  meson compile -C "$app/build"
done
