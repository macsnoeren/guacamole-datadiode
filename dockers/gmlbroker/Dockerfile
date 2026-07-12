# gmlbroker — the gmlbroker acts like a guacd where Guacamole will connect to.
#
# Build context is the REPO ROOT (reaches software/ + this entrypoint).
# Multi-stage: compile with meson, ship only the binary on a slim runtime.
FROM debian:bookworm AS build

WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential meson ninja-build libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy the proxies folder from the repo root into the build context.
COPY proxies /src/proxies

WORKDIR /src/proxies/gmlbroker

RUN meson setup build && meson compile -C build

# Runtime image: only ship the binary and a slim OS.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/proxies/gmlbroker/build/gmlbroker /usr/local/bin/gmlbroker
COPY dockers/gmlbroker/entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

# UDP: receives data from the low-side broker (D1).
EXPOSE 5500/udp

ENTRYPOINT ["/entrypoint.sh"]
