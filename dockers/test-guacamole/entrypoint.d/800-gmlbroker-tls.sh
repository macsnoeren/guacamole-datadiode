#
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

##
## @fn 800-gmlbroker-tls.sh
##
## Extra startup step for the Guacamole webapp. When TLS to gmlbroker is used,
## the webapp has to trust the self-signed certificate that gmlbroker makes.
## This script builds a Java truststore from that certificate and points the
## JVM to it, so the webapp can set up the TLS connection to gmlbroker.
##
## This script is SOURCED by the upstream entrypoint (not executed), so the
## JAVA_OPTS we export here stays valid for Tomcat that starts later.
##
## It only does something when GMLBROKER_TLS_CERT is set. If it is not set, the
## image behaves exactly like the normal guacamole/guacamole image.
##

if [ -n "${GMLBROKER_TLS_CERT:-}" ]; then

    STORE="/opt/guacamole/tls/truststore.jks"
    PASS="${GMLBROKER_TRUSTSTORE_PASSWORD:-changeit}"

    # gmlbroker makes its cert on its own start, and that can be a bit later than
    # this webapp, so we wait a little for the cert file to show up.
    echo "guac-tls: waiting for the gmlbroker cert ($GMLBROKER_TLS_CERT) ..."
    i=0
    while [ ! -f "$GMLBROKER_TLS_CERT" ]; do
        i=$((i + 1))
        if [ "$i" -gt 60 ]; then
            echo "guac-tls: cert did not show up in time, starting without trust (is GMLBROKER_TLS on?)"
            break
        fi
        sleep 1
    done

    if [ -f "$GMLBROKER_TLS_CERT" ]; then
        echo "guac-tls: building truststore and trusting the gmlbroker cert"
        mkdir -p "$(dirname "$STORE")"
        # Start from the JDK cacerts so the normal public CAs still validate,
        # then add the gmlbroker cert to it. We rebuild it on every start, so a
        # new gmlbroker cert is picked up when you restart this container.
        rm -f "$STORE"
        cp "$JAVA_HOME/lib/security/cacerts" "$STORE"
        keytool -importcert -noprompt -alias gmlbroker \
            -file "$GMLBROKER_TLS_CERT" -keystore "$STORE" -storepass "$PASS"
        export JAVA_OPTS="${JAVA_OPTS:-} -Djavax.net.ssl.trustStore=$STORE -Djavax.net.ssl.trustStorePassword=$PASS"
        echo "guac-tls: done, JVM now trusts the gmlbroker cert via $STORE"
    fi

fi
