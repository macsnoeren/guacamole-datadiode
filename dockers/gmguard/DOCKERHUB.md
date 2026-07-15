# gmguard — Guacamole Data Diode guard

Part of the **Guacamole Data Diode** project: secure, one-way remote access
built with [Apache Guacamole](https://guacamole.apache.org/) over hardware
data-diodes. `gmguard` is the **guard**: it inspects the Guacamole protocol
flowing between the low-side broker (`gmlbroker`) and the high-side broker
(`gcdbroker`) and forwards only what is allowed — keystrokes and mouse
movement. Everything else is rejected.

```
Guacamole Server <-> gmlbroker  --(DD)-->  gmguard  --(DD)-->  gcdbroker <-> guacd
```

The guard has **no TCP/IP connections**: it only receives and sends over UDP. In
the three-node setup it sits between the two data-diodes, physically separated
from both networks. This is also where an approval process can be implemented.

## Supported tags

- `latest` — most recent build from `main`
- `<major>.<minor>` — e.g. `0.1`, tracks the latest patch of that line
- `<major>.<minor>.<patch>` — e.g. `0.1.3`, immutable specific release

## Ports

| Port | Proto | Purpose |
|------|-------|---------|
| 5500 | UDP   | receives filtered traffic from the low side |

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `UDP_RECV_PORT` | `5500` | UDP port the guard receives on (from the low side) |
| `DST_IP`        | *(required)* | high-side broker's diode IP |
| `DST_PORT`      | `5501` | UDP port on the high-side broker |
| `GUARD_APPROVE` | *(approve)* | set to `deny` to deny every request |

## Example

```sh
docker run --rm \
  -e DST_IP=10.0.0.3 \
  -p 5500:5500/udp \
  macsnoeren/gmdatadiode-gmguard:latest
```

You normally run this image as part of a Docker Compose setup rather than by
hand. See the compose files in the project repository.

## Source & license

- Source: <https://github.com/macsnoeren/guacamole-datadiode>
- License: GPL-3.0-or-later
