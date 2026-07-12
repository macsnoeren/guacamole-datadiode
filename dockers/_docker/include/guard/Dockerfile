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

FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/
COPY apps/shared/meson.build apps/shared/meson.build

COPY apps/guard/include/ apps/guard/include/
COPY apps/guard/src/ apps/guard/src/
COPY apps/guard/tests/ apps/guard/tests/
COPY apps/guard/meson.build apps/guard/meson.build

COPY docker/include/run-tests.sh /usr/local/bin/run-tests.sh

RUN meson setup apps/shared/build apps/shared
RUN meson compile -C apps/shared/build

RUN meson setup apps/guard/build apps/guard
RUN meson compile -C apps/guard/build

ARG DISABLE_TESTS=false
RUN run-tests.sh apps/shared/build apps/guard/build

WORKDIR /app/apps/guard/
