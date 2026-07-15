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

#
# Translate environment variables into CLI arguments for gmlbroker.
#
#   gmlbroker <guac_listen_ip> <guac_listen_port> <udp_recv_port> \
#             <udp_send_ip> <udp_send_port>
#
# Environment variables:
#   GUAC_LISTEN_IP    TCP bind for the Guacamole web server   [default: 0.0.0.0]
#   GUAC_LISTEN_PORT  TCP port for the Guacamole web server   [default: 4823]
#   UDP_RECV_PORT     UDP port for the return path (from high) [default: 5502]
#   UDP_SEND_IP       next hop on the bridge (the guard)       [required]
#   UDP_SEND_PORT     UDP port on the guard                    [default: 5500]
set -eu

: "${UDP_SEND_IP:?UDP_SEND_IP is required (the guard's diode IP)}"

# -----------------------------------------------------------------------------
# TLS material for the gmlbroker <-> Guacamole web server leg.
#
# When TLS is enabled, gmlbroker makes its OWN self-signed key + cert into the
# /tls directory. It does this only when the files are not there yet, so the
# first start creates them and after that we reuse the same cert. If you put
# your own cert.pem / key.pem in /tls, we keep those and do not overwrite them.
#
# This does NOT need Guacamole at all, gmlbroker makes its cert on its own. In
# production your Guacamole runs somewhere else, you then copy /tls/cert.pem to
# that machine and import it in its truststore yourself (see the README).
# -----------------------------------------------------------------------------
is_true() {
  case "$(printf '%s' "${1:-}" | tr '[:upper:]' '[:lower:]')" in
    1|on|true|yes) return 0 ;;
    *) return 1 ;;
  esac
}

if is_true "${GMLBROKER_TLS:-0}"; then
  CERT="${GMLBROKER_TLS_CERT:-/tls/cert.pem}"
  KEY="${GMLBROKER_TLS_KEY:-/tls/key.pem}"
  # Export the resolved paths so the gmlbroker binary loads the cert/key from the
  # exact place we generated them. Without this the binary falls back to its own
  # default (/etc/gmlbroker/tls/...) which does not match our /tls default.
  export GMLBROKER_TLS_CERT="$CERT"
  export GMLBROKER_TLS_KEY="$KEY"
  if [ ! -f "$CERT" ] || [ ! -f "$KEY" ]; then
    echo "gmlbroker: TLS is on, generating self-signed cert -> $CERT / $KEY"
    mkdir -p "$(dirname "$CERT")" "$(dirname "$KEY")"
    # CN/SAN are cosmetic here: Guacamole validates the chain, not the hostname.
    openssl req -x509 -newkey rsa:2048 -nodes \
      -keyout "$KEY" -out "$CERT" -days 3650 \
      -subj "/CN=gmlbroker" \
      -addext "subjectAltName=DNS:gmlbroker"
    chmod 600 "$KEY"
  else
    echo "gmlbroker: TLS is on, reusing existing cert -> $CERT"
  fi
fi

set -- \
  "${GUAC_LISTEN_IP:-0.0.0.0}" \
  "${GUAC_LISTEN_PORT:-4823}" \
  "${UDP_RECV_PORT:-5502}" \
  "${UDP_SEND_IP}" \
  "${UDP_SEND_PORT:-5500}"

echo "gmlbroker $*"
exec /usr/local/bin/gmlbroker "$@"
