FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/network/ apps/shared/network/
COPY apps/gcdbroker/src apps/gcdbroker/src

WORKDIR /app/apps/gcdbroker/

RUN meson setup build src
RUN meson compile -C build

EXPOSE 5502/udp
