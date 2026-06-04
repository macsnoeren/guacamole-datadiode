FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/

COPY apps/gcdbroker/src apps/gcdbroker/src
COPY apps/gcdbroker/include apps/gcdbroker/include

WORKDIR /app/apps/gcdbroker/

RUN meson setup build src
RUN meson compile -C build

EXPOSE 5502/udp
