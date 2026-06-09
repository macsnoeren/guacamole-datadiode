FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/lrx_proxy/src/ apps/lrx_proxy/src/
COPY apps/lrx_proxy/meson.build apps/lrx_proxy/meson.build

WORKDIR /app/apps/lrx_proxy

RUN meson setup build
RUN meson compile -C build

EXPOSE 5503/udp
