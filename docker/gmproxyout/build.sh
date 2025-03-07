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
make gmproxyout
cd ../docker/gmproxyout

# Copy the executable
cp ../../build/gmproxyout .

# Build the docker image
docker build --tag gmdatadiode-gmproxyout .

# remove the executable
rm ./gmproxyout

# Example: docker run -d -p 20000:20000 -p 40000:40000 gmproxyout
