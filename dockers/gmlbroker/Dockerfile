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

# gmlbroker — the gmlbroker acts like a guacd where Guacamole will connect to.
#
# Build context is the REPO ROOT (reaches software/ + this entrypoint).
# Multi-stage: compile with meson, ship only the binary on a slim runtime.
FROM debian:bookworm AS build

WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential meson ninja-build libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy the proxies folder from the repo root into the build context.
COPY proxies /src/proxies

WORKDIR /src/proxies/gmlbroker

RUN meson setup build && meson compile -C build

# Runtime image: only ship the binary and a slim OS.
FROM debian:bookworm-slim

# openssl (the CLI) is used by entrypoint.sh to generate the self-signed TLS
# cert when GMLBROKER_TLS is enabled. libssl3 is the runtime lib for gmlbroker.
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 libssl3 openssl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/proxies/gmlbroker/build/gmlbroker /usr/local/bin/gmlbroker
COPY dockers/gmlbroker/entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

# UDP: receives data from the low-side broker (D1).
EXPOSE 5500/udp

ENTRYPOINT ["/entrypoint.sh"]
