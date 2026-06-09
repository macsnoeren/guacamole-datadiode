FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/src/ apps/shared/src/
COPY apps/shared/tests/ apps/shared/tests/

COPY apps/gmlbroker/src apps/gmlbroker/src
COPY apps/gmlbroker/include apps/gmlbroker/include

WORKDIR /app/apps/gmlbroker

RUN meson setup build src/
RUN meson compile -C build

EXPOSE 5505/udp
EXPOSE 4823/udp
