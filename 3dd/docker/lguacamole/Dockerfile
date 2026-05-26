FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y build-essential meson ninja-build

COPY apps/shared/include/ apps/shared/include/
COPY apps/shared/network/ apps/shared/network/
COPY apps/lguacamole/src apps/lguacamole/src

WORKDIR /app/apps/lguacamole

RUN meson setup build src/
RUN meson compile -C build

EXPOSE 5501/udp
