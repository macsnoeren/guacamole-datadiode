FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/
COPY apps/shared/meson.build apps/shared/meson.build

COPY apps/gmlbroker/src apps/gmlbroker/src
COPY apps/gmlbroker/include apps/gmlbroker/include
COPY apps/gmlbroker/tests apps/gmlbroker/tests
COPY apps/gmlbroker/meson.build apps/gmlbroker/meson.build

COPY docker/include/run-tests.sh /usr/local/bin/run-tests.sh

RUN meson setup apps/shared/build apps/shared
RUN meson compile -C apps/shared/build

RUN meson setup apps/gmlbroker/build apps/gmlbroker
RUN meson compile -C apps/gmlbroker/build

ARG DISABLE_TESTS=false
RUN run-tests.sh apps/shared/build apps/gmlbroker/build

WORKDIR /app/apps/gmlbroker/
