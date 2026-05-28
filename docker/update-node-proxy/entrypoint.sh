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
# Usage: update-node-proxy [OPTION]
#
# Required:
#   MODE              "sender", "receiver", or "relay"
#
# Shared (sender + receiver):
#   WEB_PORT          HTTP port for the web UI / upload endpoint       [default: 80]
#
# Sender:
#   UPLOAD_DIR        Staging directory for uploaded files             [default: /var/lib/update-proxy/staging]
#   SENT_DIR          Directory for files after successful UDP send    [default: /var/lib/update-proxy/sent]
#   DD_HOST           UDP destination host (required in sender mode)   [no default]
#   DD_PORT           UDP destination port                             [default: 40000]
#   CHUNK_SIZE        UDP chunk payload size                           [default: 1200]
#   OVERSEND_DATA     How many times each data chunk is sent           [default: 2]
#   OVERSEND_META     How many times header/end records are sent       [default: 5]
#   INTER_PACKET_US   Sleep between packets in microseconds            [default: 200]
#   SCAN_INTERVAL     Seconds between staging-dir scans                [default: 1.0]
#   MAX_UPLOAD_MB     Max single upload size in MB                     [default: 8192]
#
# Receiver:
#   INBOX_DIR         Directory where completed files land             [default: /var/lib/update-proxy/inbox]
#   PORT              UDP port to listen on                            [default: 40000]
#   BIND              UDP bind address                                 [default: 0.0.0.0]
#   IDLE_TIMEOUT      Seconds before incomplete files are dropped      [default: 300]
#
# Relay (UDP-in -> UDP-out, no parsing, no web UI):
#   PORT              UDP port to listen on                            [default: 40000]
#   BIND              UDP bind address                                 [default: 0.0.0.0]
#   DD_HOST           UDP destination host (required in relay mode)    [no default]
#   DD_PORT           UDP destination port                             [default: 40000]
#
# More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode.

if [ -z "$MODE" ]; then
    echo "update-node-proxy: MODE must be set to 'sender' or 'receiver'" >&2
    exit 2
fi

ARGS="--mode $MODE"

if [ -n "$WEB_PORT" ];        then ARGS="$ARGS --web-port $WEB_PORT";               fi

# sender args
if [ -n "$UPLOAD_DIR" ];      then ARGS="$ARGS --upload-dir $UPLOAD_DIR";           fi
if [ -n "$SENT_DIR" ];        then ARGS="$ARGS --sent-dir $SENT_DIR";               fi
if [ -n "$DD_HOST" ];         then ARGS="$ARGS --dd-host $DD_HOST";                 fi
if [ -n "$DD_PORT" ];         then ARGS="$ARGS --dd-port $DD_PORT";                 fi
if [ -n "$CHUNK_SIZE" ];      then ARGS="$ARGS --chunk-size $CHUNK_SIZE";           fi
if [ -n "$OVERSEND_DATA" ];   then ARGS="$ARGS --oversend-data $OVERSEND_DATA";     fi
if [ -n "$OVERSEND_META" ];   then ARGS="$ARGS --oversend-meta $OVERSEND_META";     fi
if [ -n "$INTER_PACKET_US" ]; then ARGS="$ARGS --inter-packet-us $INTER_PACKET_US"; fi
if [ -n "$SCAN_INTERVAL" ];   then ARGS="$ARGS --scan-interval $SCAN_INTERVAL";     fi
if [ -n "$MAX_UPLOAD_MB" ];   then ARGS="$ARGS --max-upload-mb $MAX_UPLOAD_MB";     fi

# receiver args
if [ -n "$INBOX_DIR" ];       then ARGS="$ARGS --inbox-dir $INBOX_DIR";             fi
if [ -n "$PORT" ];            then ARGS="$ARGS --port $PORT";                       fi
if [ -n "$BIND" ];            then ARGS="$ARGS --bind $BIND";                       fi
if [ -n "$IDLE_TIMEOUT" ];    then ARGS="$ARGS --idle-timeout $IDLE_TIMEOUT";       fi

echo "update-node-proxy $ARGS"
exec python /app/app.py $ARGS
