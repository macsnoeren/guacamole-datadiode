#!/usr/bin/env bash
#
# Generate the TLS material for the 3node low node (into ./tls next to this file)
# and print how to bring the low node up with TLS enabled. Re-run any time; the
# private key never leaves ./tls — keep that directory out of git (.gitignore).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$HERE/../../scripts/generate-tls.sh" "$HERE/tls"

echo
echo "Start the low node with TLS enabled:"
echo "  docker compose -f docker/3node/lownode.compose.yml -f docker/include/tls/compose.yml up --build"
