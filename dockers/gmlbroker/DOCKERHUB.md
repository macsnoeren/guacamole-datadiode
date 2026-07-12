# gmlbroker — Guacamole Data Diode low-side broker

Part of the **Guacamole Data Diode** project: secure, one-way remote access
built with [Apache Guacamole](https://guacamole.apache.org/) over hardware
data-diodes. `gmlbroker` is the **low-side broker**. The Guacamole web server
connects to it exactly as if it were `guacd`; the broker accepts the connection
and forwards only the allowed traffic (keystrokes and mouse movement) across the
data-diode toward the high side.

```
Guacamole Server <-> gmlbroker  --(DD)-->  gmguard  --(DD)-->  gcdbroker <-> guacd
```

The return path (screen/video updates) flows back over a second diode into
`gmlbroker`, which serves it to the Guacamole server. TLS is used between the
Guacamole server and `gmlbroker`.

## Supported tags

- `latest` — most recent build from `main`
- `<major>.<minor>` — e.g. `0.1`, tracks the latest patch of that line
- `<major>.<minor>.<patch>` — e.g. `0.1.3`, immutable specific release

## Ports

| Port | Proto | Purpose |
|------|-------|---------|
| 4823 | TCP   | Guacamole server connects here (acts as guacd) |
| 5502 | UDP   | return path from the high side |

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GUAC_LISTEN_IP`   | `0.0.0.0` | TCP bind address for the Guacamole server |
| `GUAC_LISTEN_PORT` | `4823`    | TCP port for the Guacamole server |
| `UDP_RECV_PORT`    | `5502`    | UDP port for the return path (from the high side) |
| `UDP_SEND_IP`      | *(required)* | next hop on the bridge (the guard's diode IP) |
| `UDP_SEND_PORT`    | `5500`    | UDP port on the guard |

## Example

```sh
docker run --rm \
  -e UDP_SEND_IP=10.0.0.2 \
  -p 4823:4823/tcp \
  -p 5502:5502/udp \
  macsnoeren/gmdatadiode-gmlbroker:latest
```

You normally run this image as part of a Docker Compose setup rather than by
hand. See the compose files in the project repository.

## Source & license

- Source: <https://github.com/macsnoeren/guacamole-datadiode>
- License: GPL-3.0-or-later
