FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/gmlbroker/src apps/gmlbroker/src

WORKDIR /app/apps/gmlbroker

RUN meson setup build src/
RUN meson compile -C build

EXPOSE 5501/udp
