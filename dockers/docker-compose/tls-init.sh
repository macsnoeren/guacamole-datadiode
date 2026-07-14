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
# tls-init - small one-shot helper, ONLY for the local test setup.
#
# gmlbroker makes its own self-signed key + cert in the shared /tls directory.
# This helper waits until that cert is there and then builds a Java truststore
# (truststore.jks) so the local Guacamole webapp trusts that self-signed cert.
# keytool lives in the guacamole image, that is why this helper runs on that
# image.
#
# In production your Guacamole runs on an other machine, so you do NOT run this
# helper. You just copy /tls/cert.pem to your Guacamole machine and import it in
# the JVM truststore over there yourself (see the README).
set -eu

CERT=/tls/cert.pem
STORE=/tls/truststore.jks
CACERTS=/opt/java/openjdk/lib/security/cacerts
STOREPASS=changeit

echo "tls-init: waiting for the gmlbroker cert ($CERT) ..."
i=0
while [ ! -f "$CERT" ]; do
  i=$((i + 1))
  if [ "$i" -gt 60 ]; then
    echo "tls-init: cert did not show up in time, giving up (is GMLBROKER_TLS on?)"
    exit 1
  fi
  sleep 1
done

if [ -f "$STORE" ]; then
  echo "tls-init: truststore is already there, reusing it"
else
  echo "tls-init: building truststore.jks from the gmlbroker cert"
  # Start from the JDK cacerts so public CAs still validate, then add our cert.
  cp "$CACERTS" "$STORE"
  keytool -importcert -noprompt -alias gmlbroker \
    -file "$CERT" -keystore "$STORE" -storepass "$STOREPASS"
fi

echo "tls-init: done"
