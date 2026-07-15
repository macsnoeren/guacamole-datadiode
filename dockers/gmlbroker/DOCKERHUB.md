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
`gmlbroker`, which serves it to the Guacamole server. The link between the
Guacamole server and `gmlbroker` can be encrypted with TLS (optional, off by
default). See the TLS part below.

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
| `GMLBROKER_TLS`      | `0` | set to `1` (or `on`/`true`/`yes`) to turn on TLS to the Guacamole server |
| `GMLBROKER_TLS_CERT` | `/tls/cert.pem` | path to the public certificate |
| `GMLBROKER_TLS_KEY`  | `/tls/key.pem`  | path to the private key |

## TLS

TLS on the link to the Guacamole server is optional and off by default. When you
set `GMLBROKER_TLS=1`, `gmlbroker` makes its own self-signed certificate the
first time it starts and writes `cert.pem` and `key.pem` into the `/tls`
directory. It only does this when the files are not there yet, so mount a volume
on `/tls` if you want to keep the same certificate over restarts. You can also
put your own `cert.pem` / `key.pem` in `/tls`, then those are used instead.

`gmlbroker` makes its certificate on its own, it does NOT need Guacamole for
that. On the Guacamole side you have to trust that self-signed certificate
(import `cert.pem` in the JVM truststore) and let Guacamole talk TLS to
`gmlbroker` (`guacd-ssl: true`). See the project repository for the full steps.

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
