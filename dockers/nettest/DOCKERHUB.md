# nettest — Guacamole Data Diode stress-test suite

Part of the **Guacamole Data Diode** project: secure, one-way remote access
built with [Apache Guacamole](https://guacamole.apache.org/) over hardware
data-diodes. `nettest` is a small, stdlib-only Python web application for
**stress-testing** the bridge. It opens many Guacamole connections at once,
sends input on all of them and measures round-trip times and timeouts, so you
can see how many connections Guacamole and the diode bridge handle before
things get too slow.

It also has a "bad connections" mode that sends non-Guacamole traffic toward
`guacd`; the guard should catch and drop those, so it partly exercises the
filtering as well.

## Dashboard

Open <http://localhost:8081> for the dashboard: configure a custom load test
(good/bad connections, send delay, duration, probe type), watch live run
statistics (connections, timeouts, latency mean/min/max/std), and read the log.
Each finished run is written as a Markdown report.

## Supported tags

- `latest` — most recent build from `main`
- `<major>.<minor>` — e.g. `0.1`, tracks the latest patch of that line
- `<major>.<minor>.<patch>` — e.g. `0.1.3`, immutable specific release

## Ports

| Port | Proto | Purpose |
|------|-------|---------|
| 8081 | TCP   | web dashboard + JSON API |

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GML_HOST`   | `gmlbroker` | host of the gmlbroker to drive |
| `GML_PORT`   | `4823` | TCP port of the gmlbroker |
| `HTTP_PORT`  | `8081` | port the dashboard listens on |
| `E2E_SSH_HOST` | `sshd` | SSH backend guacd is driven toward |
| `E2E_SSH_PORT` | `22` | SSH backend port |
| `E2E_SSH_USER` | `tester` | SSH backend user |
| `E2E_SSH_PASS` | `testpass` | SSH backend password |
| `E2E_ESTABLISH_TIMEOUT_S` | `15` | connection-establish timeout (seconds) |
| `REPORT_DIR` | `/var/log/nettest` | where Markdown reports are written |

## Example

```sh
docker run --rm \
  -e GML_HOST=gmlbroker \
  -p 8081:8081/tcp \
  macsnoeren/gmdatadiode-nettest:latest
```

You normally run this image as part of a Docker Compose setup that includes the
rest of the bridge. See the compose files in the project repository.

## Source & license

- Source: <https://github.com/macsnoeren/guacamole-datadiode>
- License: GPL-3.0-or-later
