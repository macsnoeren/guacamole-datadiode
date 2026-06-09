FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/hrx_proxy/src apps/hrx_proxy/src
COPY apps/hrx_proxy/meson.build apps/hrx_proxy/meson.build

WORKDIR /app/apps/hrx_proxy

RUN meson setup build
RUN meson compile -C build
