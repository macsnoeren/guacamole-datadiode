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

# Rebuild the application
cd ../../gmdatadiode
make gmserver
cd ../docker/gmserver

# Copy the executable
cp ../../build/gmserver .

# Build the docker image
docker build --tag gmserver .

# remove the executable
rm ./gmserver

# Example: docker run -d -p 4822:4822 -p 10000:10000 -p 20000:20000 gmserver
