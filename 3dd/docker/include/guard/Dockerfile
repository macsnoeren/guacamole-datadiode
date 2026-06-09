FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/

COPY apps/guard/include/ apps/guard/include/
COPY apps/guard/src/ apps/guard/src/
COPY apps/guard/tests/ apps/guard/tests/
COPY apps/guard/meson.build apps/guard/meson.build

WORKDIR /app/apps/guard

RUN meson setup build
RUN meson compile -C build
