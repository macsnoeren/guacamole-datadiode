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

# test-guacamole - the normal Guacamole webapp image with one extra startup step
# so it can trust the self-signed certificate that gmlbroker makes for the TLS
# leg between the webapp and gmlbroker. See the README in this folder.
#
# The upstream entrypoint runs every script in /opt/guacamole/entrypoint.d in
# alphabetical order, so we drop our step in there. The 800- prefix makes it run
# after the normal config steps (700-) and before Tomcat starts (999-).
FROM guacamole/guacamole:1.6.0

COPY entrypoint.d/800-gmlbroker-tls.sh /opt/guacamole/entrypoint.d/800-gmlbroker-tls.sh

# The image runs as the non-root "guacamole" user, so make a directory it may
# write the truststore into at startup (the rest of /opt/guacamole is read-only
# for that user).
USER root
RUN mkdir -p /opt/guacamole/tls && chown guacamole:guacamole /opt/guacamole/tls
USER guacamole
