# gmguard — the guard that filters the Guacamole protocol between the gmlbroker
#           and the gcdbroker. Only allowing keystrokes and mouse information.
#           The guard does not have any TCP/IP connections and only uses UDP
#           to receive and send data.
#
# Build context is the REPO ROOT (reaches software/ + this entrypoint).
# Multi-stage: compile with meson, ship only the binary on a slim runtime.
FROM debian:bookworm AS build

WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential meson ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Copy the proxies folder from the repo root into the build context.
COPY proxies /src/proxies

WORKDIR /src/proxies/gmguard

RUN meson setup build && meson compile -C build

# Runtime image: only ship the binary and a slim OS.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/proxies/gmguard/build/gmguard /usr/local/bin/gmguard
COPY dockers/gmguard/entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

# UDP: receives data from the low-side broker (D1).
EXPOSE 5500/udp

ENTRYPOINT ["/entrypoint.sh"]
