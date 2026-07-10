#!/bin/sh
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

echo "guard $* (GUARD_APPROVE=${GUARD_APPROVE:-<approve>})"
exec /usr/local/bin/gmlbroker "$@"
