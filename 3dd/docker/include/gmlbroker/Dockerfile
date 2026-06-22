FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/

COPY apps/gmlbroker/src apps/gmlbroker/src
COPY apps/gmlbroker/include apps/gmlbroker/include
COPY apps/gmlbroker/tests apps/gmlbroker/tests
COPY apps/gmlbroker/meson.build apps/gmlbroker/meson.build

WORKDIR /app/apps/gmlbroker

RUN meson setup build
RUN meson compile -C build
