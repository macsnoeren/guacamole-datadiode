FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY include/ include/
COPY src/network/ src/network/
COPY src/lguacamole/main.cpp src/lguacamole/main.cpp
COPY src/lguacamole/meson.build src/lguacamole/meson.build

WORKDIR /app/src/lguacamole

RUN meson setup build
RUN meson compile -C build

EXPOSE 5501/udp
