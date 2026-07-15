# gcdbroker — Guacamole Data Diode high-side broker

Part of the **Guacamole Data Diode** project: secure, one-way remote access
built with [Apache Guacamole](https://guacamole.apache.org/) over hardware
data-diodes. `gcdbroker` is the **high-side broker**. It connects to the real
`guacd` and pretends to be the Guacamole server. Everything it receives from
`guacd` is sent back over the return data-diode toward the low-side broker
(`gmlbroker`).

```
Guacamole Server <-> gmlbroker  --(DD)-->  gmguard  --(DD)-->  gcdbroker <-> guacd
```

## Supported tags

- `latest` — most recent build from `main`
- `<major>.<minor>` — e.g. `0.1`, tracks the latest patch of that line
- `<major>.<minor>.<patch>` — e.g. `0.1.3`, immutable specific release

## Ports

| Port | Proto | Purpose |
|------|-------|---------|
| 5501 | UDP   | receives filtered traffic from the guard |
| 5502 | UDP   | sends the return path to the low side |

The broker also makes an outbound TCP connection to `guacd`.

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GUACD_IP`      | `guacd-high-side` | TCP host of the real `guacd` |
| `GUACD_PORT`    | `4822` | TCP port of `guacd` |
| `UDP_RECV_PORT` | `5501` | UDP port receiving from the guard |
| `UDP_SEND_IP`   | *(required)* | low-side broker's diode IP (return path) |
| `UDP_SEND_PORT` | `5502` | UDP port on the low-side broker |

## Example

```sh
docker run --rm \
  -e GUACD_IP=guacd \
  -e UDP_SEND_IP=10.0.0.1 \
  -p 5501:5501/udp \
  macsnoeren/gmdatadiode-gcdbroker:latest
```

You normally run this image as part of a Docker Compose setup rather than by
hand. See the compose files in the project repository.

## Source & license

- Source: <https://github.com/macsnoeren/guacamole-datadiode>
- License: GPL-3.0-or-later
