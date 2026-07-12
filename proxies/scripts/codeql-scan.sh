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

# Build a CodeQL C++ database for the 3DD components and analyse it.
#
# Prereqs: the CodeQL CLI on PATH (or set CODEQL), plus meson/ninja/g++.
# Outputs go to 3dd/.codeql/ (gitignored): the database and a SARIF result file.
#
# Usage:
#   scripts/codeql-scan.sh                 # security-and-quality suite
#   SUITE=cpp-security-extended scripts/codeql-scan.sh
set -euo pipefail

cd "$(dirname "$0")/.."  # the 3dd/ directory
ROOT="$(pwd)"

CODEQL="${CODEQL:-codeql}"
command -v "$CODEQL" >/dev/null || CODEQL="$HOME/tools/codeql/codeql"

SUITE="${SUITE:-cpp-security-and-quality}"
OUT="$ROOT/.codeql"
DB="$OUT/cpp-db"
SARIF="$OUT/cpp-results.sarif"

mkdir -p "$OUT"
rm -rf "$DB"

echo ">>> creating database ($DB)"
"$CODEQL" database create "$DB" \
  --language=cpp \
  --source-root="$ROOT" \
  --command="$ROOT/scripts/codeql-build-cpp.sh"

echo ">>> analysing with codeql/cpp-queries:codeql-suites/$SUITE.qls"
"$CODEQL" database analyze "$DB" \
  "codeql/cpp-queries:codeql-suites/$SUITE.qls" \
  --format=sarif-latest \
  --output="$SARIF" \
  --sarif-add-snippets

echo ">>> done. SARIF: $SARIF"

# A human-readable CSV alongside the SARIF (one row per alert).
"$CODEQL" database interpret-results "$DB" \
  "codeql/cpp-queries:codeql-suites/$SUITE.qls" \
  --format=csv --output="$OUT/cpp-results.csv"
echo ">>> CSV: $OUT/cpp-results.csv"
