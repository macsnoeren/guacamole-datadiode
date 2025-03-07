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
make gmproxyin
cd ../docker/gmproxyin

# Copy the executable
cp ../../build/gmproxyin .

# Build the docker image
docker build --tag gmproxyin .

# remove the executable
rm ./gmproxyin

# Example: docker run -d gmproxyin
