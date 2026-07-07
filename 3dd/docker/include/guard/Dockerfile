FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/
COPY apps/shared/meson.build apps/shared/meson.build

COPY apps/guard/include/ apps/guard/include/
COPY apps/guard/src/ apps/guard/src/
COPY apps/guard/tests/ apps/guard/tests/
COPY apps/guard/meson.build apps/guard/meson.build

COPY docker/include/run-tests.sh /usr/local/bin/run-tests.sh

RUN meson setup apps/shared/build apps/shared
RUN meson compile -C apps/shared/build

RUN meson setup apps/guard/build apps/guard
RUN meson compile -C apps/guard/build

ARG DISABLE_TESTS=false
RUN run-tests.sh apps/shared/build apps/guard/build

WORKDIR /app/apps/guard/
