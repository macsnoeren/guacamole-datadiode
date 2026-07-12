#!/bin/sh
#
# Translate environment variables into CLI arguments for gcdbroker.
#
#   gcdbroker <guacd_ip> <guacd_port> <udp_recv_port> <udp_send_ip> <udp_send_port>
#
# Environment variables:
#   GUACD_IP        TCP host of the real guacd            [default: guacd-high-side]
#   GUACD_PORT      TCP port of guacd                     [default: 4822]
#   UDP_RECV_PORT   UDP port receiving from the guard     [default: 5501]
#   UDP_SEND_IP     low-side broker's diode IP (return)   [required]
#   UDP_SEND_PORT   UDP port on the low-side broker       [default: 5502]
set -eu

: "${UDP_SEND_IP:?UDP_SEND_IP is required (the low-side broker's return IP)}"

set -- \
  "${GUACD_IP:-guacd-high-side}" \
  "${GUACD_PORT:-4822}" \
  "${UDP_RECV_PORT:-5501}" \
  "${UDP_SEND_IP}" \
  "${UDP_SEND_PORT:-5502}"

echo "gcdbroker $*"
exec /usr/local/bin/gcdbroker "$@"
