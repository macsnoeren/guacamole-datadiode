#!/bin/sh
#
# Copyright (C) 2025 Maurice Snoeren
#
# This program is free software: you can redistribute it and/or modify it under the terms of
# the GNU General Public License as published by the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see https://www.gnu.org/licenses/.
#
# Usage: node-logging-proxy [OPTION]
#
# Environment variables (all optional):
#   PORT          UDP port to listen on                            [default: 1111]
#   BIND          UDP bind address                                 [default: 0.0.0.0]
#   FORWARD_HOST  Forward received UDP messages to this host       [default: none]
#   FORWARD_PORT  Forward UDP port                                 [default: 1111]
#   WEB           Set to "true" to enable the web viewer           [default: off]
#   WEB_PORT      Web viewer port                                  [default: 80]
#   LOG_FILE            Path to log file                           [default: /var/log/nodes.log]
#   HEARTBEAT_INTERVAL  Seconds between heartbeats to forward host [default: 10, 0 disables]
#                       (only active when FORWARD_HOST is set)
#
# More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode.

ARGS=

if [ -n "$PORT" ];               then ARGS="$ARGS --port $PORT";                             fi
if [ -n "$BIND" ];               then ARGS="$ARGS --bind $BIND";                             fi
if [ -n "$FORWARD_HOST" ];       then ARGS="$ARGS --forward-host $FORWARD_HOST";             fi
if [ -n "$FORWARD_PORT" ];       then ARGS="$ARGS --forward-port $FORWARD_PORT";             fi
if [ -n "$WEB_PORT" ];           then ARGS="$ARGS --web-port $WEB_PORT";                     fi
if [ -n "$LOG_FILE" ];           then ARGS="$ARGS --log-file $LOG_FILE";                     fi
if [ -n "$HEARTBEAT_INTERVAL" ]; then ARGS="$ARGS --heartbeat-interval $HEARTBEAT_INTERVAL"; fi
if [ "$WEB" = "true" ];          then ARGS="$ARGS --web";                                    fi

echo "node-logging-proxy $ARGS"
exec python /app/app.py $ARGS
