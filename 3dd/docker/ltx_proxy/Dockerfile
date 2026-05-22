FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY include/ include/
COPY src/network/ src/network/
COPY src/ltx_proxy/ src/ltx_proxy/

WORKDIR /app/src/ltx_proxy

RUN meson setup build
RUN meson compile -C build

EXPOSE 5501/udp
