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

# Run the Meson tests for a build directory, honouring the DISABLE_TESTS build
# arg (exported into the RUN environment). Used by the broker Dockerfiles so the
# skip logic lives in one place instead of being duplicated per project.
#
# Usage: run-tests.sh <build-dir> [<build-dir> ...]
set -e

for build_dir in "$@"; do
    if [ "$DISABLE_TESTS" = "true" ]; then
        echo "Warning: Skipping tests for $build_dir!"
    else
        meson test -C "$build_dir" || (cat "$build_dir/meson-logs/testlog.txt" ; exit 1)
    fi
done
