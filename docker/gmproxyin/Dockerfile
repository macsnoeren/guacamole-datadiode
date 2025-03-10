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

# Building the binaries first to create a lean image file
FROM alpine:latest AS build

RUN apk update
RUN apk upgrade
RUN apk add gcompat
RUN apk add libc6-compat
RUN apk add build-base
RUN apk add make
RUN apk add git

RUN git clone https://github.com/macsnoeren/guacamole-datadiode.git

WORKDIR /guacamole-datadiode/gmdatadiode
RUN make gmproxyin

# Building the final image
FROM alpine:latest

WORKDIR /bin

COPY entrypoint.sh .
COPY --from=build /guacamole-datadiode/build/gmproxyin  .

RUN chmod +x entrypoint.sh
RUN chmod +x gmproxyin

ENTRYPOINT ["sh", "entrypoint.sh"]
