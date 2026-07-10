# gmlbroker — the gmlbroker acts like a guacd where Guacamole will connect to.
#
# Build context is the REPO ROOT (reaches software/ + this entrypoint).
# Multi-stage: compile with meson, ship only the binary on a slim runtime.
FROM debian:bookworm AS build

WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential meson ninja-build git ca-certificates libssl-dev \
    && rm -rf /var/lib/apt/lists/*

#RUN git clone https://github.com/macsnoeren/guacamole-datadiode.git
RUN git clone --single-branch --branch dev/refactoring-repo https://github.com/macsnoeren/guacamole-datadiode.git

WORKDIR guacamole-datadiode/proxies/gmlbroker

RUN meson setup build && meson compile -C build

# Runtime image: only ship the binary and a slim OS.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/guacamole-datadiode/proxies/gmlbroker/build/gmlbroker /usr/local/bin/gmlbroker
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

# UDP: receives data from the low-side broker (D1).
EXPOSE 5500/udp

ENTRYPOINT ["/entrypoint.sh"]
