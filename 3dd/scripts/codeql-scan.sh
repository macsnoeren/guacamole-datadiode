#!/usr/bin/env bash
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
