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

# Usage: gmserver [OPTION]
#
# Options and their default values
#   -c num, --max-clients=num  maximal connections that the Guacamole web client can make [default: 25]
#   -p port, --port=port       port where the Guacamole wev client is connecting to       [default: 4822]
#   -i port, --ddin-port=port  port that the gmproxyout needs to connect to               [default: 20000]
#   -o port, --ddout-port=port port that the gmproxyin needs to connect to                [default: 10000]
#   -v                         verbose add v's to increase level
#   -h, --help                 show this help page.
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

if [ "$GUACAMOLE_PORT" != "" ];
then
    GM_ARGS="$GM_ARGS -p $GUACAMOLE_PORT"
fi

if [ "$DDIN_PORT" != "" ];
then
    GM_ARGS="$GM_ARGS -i $DDIN_PORT"
fi

if [ "$DDOUT_PORT" != "" ];
then
    GM_ARGS="$GM_ARGS -o $DDOUT_PORT"
fi

echo gmserver $GM_ARGS
gmserver $GM_ARGS
