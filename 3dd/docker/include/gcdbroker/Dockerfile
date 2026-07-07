FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/
COPY apps/shared/meson.build apps/shared/meson.build

COPY apps/gcdbroker/src apps/gcdbroker/src
COPY apps/gcdbroker/include apps/gcdbroker/include
COPY apps/gcdbroker/tests apps/gcdbroker/tests
COPY apps/gcdbroker/meson.build apps/gcdbroker/meson.build

COPY docker/include/run-tests.sh /usr/local/bin/run-tests.sh

RUN meson setup apps/shared/build apps/shared
RUN meson compile -C apps/shared/build

RUN meson setup apps/gcdbroker/build apps/gcdbroker
RUN meson compile -C apps/gcdbroker/build

ARG DISABLE_TESTS=false
RUN run-tests.sh apps/shared/build apps/gcdbroker/build

WORKDIR /app/apps/gcdbroker/
