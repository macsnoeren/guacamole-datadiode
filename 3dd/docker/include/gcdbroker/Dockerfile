FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/

COPY apps/gcdbroker/src apps/gcdbroker/src
COPY apps/gcdbroker/include apps/gcdbroker/include
COPY apps/gcdbroker/tests apps/gcdbroker/tests
COPY apps/gcdbroker/meson.build apps/gcdbroker/meson.build

WORKDIR /app/apps/gcdbroker/

RUN meson setup build
RUN meson compile -C build
