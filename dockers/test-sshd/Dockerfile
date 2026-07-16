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

FROM ubuntu:24.04
RUN apt-get update \
    && apt-get install -y --no-install-recommends openssh-server \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /run/sshd \
    && ssh-keygen -A \
    && echo 'PasswordAuthentication yes' >> /etc/ssh/sshd_config \
    && echo 'LogLevel DEBUG3' >> /etc/ssh/sshd_config

# User tester1
RUN useradd -m -s /bin/bash tester1 \
 && echo 'tester1:testpass' | chpasswd

# User tester2
RUN useradd -m -s /bin/bash tester2 \
 && echo 'tester2:testpass' | chpasswd

# User tester3
RUN useradd -m -s /bin/bash tester3 \
 && echo 'tester3:testpass' | chpasswd

EXPOSE 22
# -D: stay in the foreground; -e: log to stderr so `docker logs sshd` shows
# guacd's auth attempts when debugging the flow.
CMD ["/usr/sbin/sshd", "-D", "-e"]
