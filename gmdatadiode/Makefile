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

CFLAGS = -std=c++17
INCLUDE = -Iinclude
BUILD_DIR = ../build

all: gmserver gmclient gmproxyin gmproxyout

gmserver: build_dir
	echo "gmserver"
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmserver gmserver.cpp

gmclient: build_dir
	echo "gmclient"
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmclient gmclient.cpp

gmproxyin: build_dir
	echo "gmproxyin"
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmproxyin gmproxyin.cpp

gmproxyout: build_dir
	echo "gmproxyout"
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmproxyout gmproxyout.cpp

build_dir:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
