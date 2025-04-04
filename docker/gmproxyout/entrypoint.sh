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
#!/bin/sh

# Usage: gmproxyout [OPTION]
#
# Options and their default values
#   -g host, --gmx-host=host  host where it needs to connect to send data from gmserver or gmclient [default: 127.0.0.1]
#   -p port, --gmx-port=port  port where it need to connect to the gmserver or gmclient             [default: 20000]
#   -i port, --ddin-port=port port that the data is received from gmproxyin on UDP port             [default: 40000]
#   -n, --no-check             disable the validation check on the protocol when it passes
#   -t, --test                 testing mode will send UDP messages to gmproxyout
#   -v                         verbose add v's to increase level
#   -h, --help                show this help page.
#
# More documentation can be found on https://github.com/macsnoeren/guacamole-datadiode.

# The arguments list to pass to the gm application
GM_ARGS=

if [ "$INFO" == "true" ];
then
    GM_ARGS="$GM_ARGS -v"
fi

if [ "$WARN" == "true" ];
then
    GM_ARGS="$GM_ARGS -vv"
fi

if [ "$DEBUG" == "true" ];
then
    GM_ARGS="$GM_ARGS -vvv"
fi

if [ "$GMX_HOST" != "" ];
then
    GM_ARGS="$GM_ARGS -g $GMX_HOST"
fi

if [ "$GMX_PORT" != "" ];
then
    GM_ARGS="$GM_ARGS -p $GMX_PORT"
fi

if [ "$DDIN_PORT" != "" ];
then
    GM_ARGS="$GM_ARGS -i $DDIN_PORT"
fi

if [ "$TEST" == "true" ];
then
    GM_ARGS="$GM_ARGS -t"
fi

if [ "$NOCHECK" == "true" ];
then
    GM_ARGS="$GM_ARGS -n"
fi

echo gmproxyout $GM_ARGS
gmproxyout $GM_ARGS
