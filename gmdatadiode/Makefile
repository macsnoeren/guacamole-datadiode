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

CFLAGS = -std=c++17 -static
INCLUDE = -Iinclude
BUILD_DIR = ../build
LIBS =

all: gmserver gmclient gmproxyin gmproxyout

gmserver: build_dir
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmserver gmserver.cpp ${LIBS}

gmclient: build_dir
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmclient gmclient.cpp ${LIBS}

gmproxyin: build_dir
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmproxyin gmproxyin.cpp ${LIBS}

gmproxyout: build_dir
	g++ $(CFLAGS) $(INCLUDE) -o $(BUILD_DIR)/gmproxyout gmproxyout.cpp ${LIBS}

build_dir:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
