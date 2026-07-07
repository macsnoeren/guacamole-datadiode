#!/usr/bin/env bash
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
