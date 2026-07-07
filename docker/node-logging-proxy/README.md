# node-logging-proxy

A lightweight UDP log proxy for collecting log messages from multiple nodes in the
guacamole-datadiode setup. It accepts UDP log messages on a configurable port,
appends them to a local log file (one line per packet, prefixed with a timestamp
and the source `ip:port`), and can optionally:

- **forward** every received packet to another UDP destination, so multiple
  collectors can be chained;
- **serve a web viewer** so you can browse the captured log lines from a
  browser.

It is intended for diagnostic use during deployment of multiple Flatcar nodes
behind the data diode, where there is no other shared logging infrastructure.

---

## How it works

```
                     +-----------------+
   node-A  --UDP-->  |                 |  --UDP--> forward-host
   node-B  --UDP-->  | node-logging-   |
   node-C  --UDP-->  | proxy           |  --append--> /var/log/nodes.log
                     |                 |
                     |  (optional      |
                     |   webserver)    |  <--HTTP-- browser
                     +-----------------+
```

- Each incoming UDP datagram is treated as **one log line**. There is no
  reassembly across datagrams; if a sender splits a message it will arrive as
  separate lines. Keep messages under the path MTU (~1400 bytes is safe).
- Lines are written tab-separated as
  `<iso-timestamp>\t<source-ip:port>\t<message>` to the log file and to
  stdout.
- If `--forward-host` is set, the **raw** datagram is re-sent unmodified to
  `<forward-host>:<forward-port>`. The proxy does not add or strip anything on
  the forwarded path, so chaining multiple proxies works.
- If `--web` is enabled, a Flask HTTP server is started in a background thread
  that reads the log file on each request and renders the last N entries as a
  table that auto-refreshes every 5 seconds.
- If `--forward-host` is set, the proxy also sends a **heartbeat** UDP packet
  to that host every `--heartbeat-interval` seconds (default `10`). The
  payload is `<hostname>: <yyyy-mm-dd hh:mm:ss>: Heartbeat`, where `<hostname>`
  is the container's hostname. Set the interval to `0` to disable. Heartbeats
  are not written to the local log file, only the startup announcement is.

---

## Configuration

The container is configured through **environment variables** (preferred, see
[Docker usage](#docker-usage)) which are translated to CLI flags by
[`entrypoint.sh`](entrypoint.sh). The CLI flags themselves are also useful when
running [`app.py`](app.py) directly during development.

| Env var        | CLI flag             | Default              | Description                                                  |
|----------------|----------------------|----------------------|--------------------------------------------------------------|
| `PORT`         | `-p`, `--port`       | `1111`               | UDP port to listen on for incoming log messages.             |
| `BIND`         | `-b`, `--bind`       | `0.0.0.0`            | UDP bind address. Set to a specific IP to listen on only one interface. |
| `FORWARD_HOST` | `-f`, `--forward-host` | *(unset)*          | If set, every received datagram is forwarded to this host.   |
| `FORWARD_PORT` | `-o`, `--forward-port` | `1111`             | UDP port to forward to.                                      |
| `WEB`          | `-w`, `--web`        | *(off)*              | Set to `true` to enable the web viewer.                      |
| `WEB_PORT`     | `-W`, `--web-port`   | `80`                 | TCP port the web viewer listens on (always on `0.0.0.0`).    |
| `LOG_FILE`     | `-l`, `--log-file`   | `/var/log/nodes.log` | File to append received messages to.                         |
| `HEARTBEAT_INTERVAL` | `-H`, `--heartbeat-interval` | `10`           | Seconds between heartbeats to the forward host. `0` disables. Only active when `FORWARD_HOST` is set. |

Notes:

- All env vars are optional; omitting one means the default is used.
- `WEB` is only enabled when the value is exactly the string `true`.
- The log file's parent directory is created on startup if it does not exist.

---

## Docker usage

### Build

```sh
cd docker/node-logging-proxy
docker build -t node-logging-proxy .
```

### Run — minimal collector

Listen on UDP/1111, no forwarding, no webview:

```sh
docker run --rm -p 1111:1111/udp node-logging-proxy
```

### Run — with web viewer

Listen on UDP/1111 and expose the webview on port 8080:

```sh
docker run --rm \
    -e WEB=true \
    -e WEB_PORT=8080 \
    -p 1111:1111/udp \
    -p 8080:8080 \
    node-logging-proxy
```

Browse to <http://localhost:8080/> to see the live log. Use the `?tail=N`
query parameter to change how many lines are shown
(e.g. <http://localhost:8080/?tail=2000>).

### Run — chained / forwarding

Receive on UDP/1111 and forward every datagram unchanged to a central collector
at `10.0.0.5:1111`. While forwarding is active, the proxy also sends a
heartbeat every 10 seconds so the collector knows it is alive:

```sh
docker run --rm \
    -e FORWARD_HOST=10.0.0.5 \
    -e FORWARD_PORT=1111 \
    -p 1111:1111/udp \
    node-logging-proxy
```

Override the heartbeat interval (e.g. every 30 seconds) or disable it entirely
with `HEARTBEAT_INTERVAL=0`:

```sh
docker run --rm \
    -e FORWARD_HOST=10.0.0.5 \
    -e HEARTBEAT_INTERVAL=30 \
    -p 1111:1111/udp \
    node-logging-proxy
```

### Run — persistent log file

Mount a host directory so the log survives container restarts:

```sh
docker run --rm \
    -e WEB=true \
    -v "$PWD/logs:/var/log" \
    -p 1111:1111/udp \
    -p 80:80 \
    node-logging-proxy
```

The default `LOG_FILE=/var/log/nodes.log` lands inside the mounted volume.

### Run — bind to a specific host interface

When the host has multiple network interfaces and you only want to accept logs
on one of them, set `BIND`:

```sh
docker run --rm \
    -e BIND=192.168.1.10 \
    -p 192.168.1.10:1111:1111/udp \
    node-logging-proxy
```

---

## Sending log messages

Any tool that can send UDP works. A few common ones:

### `nc` (BSD or OpenBSD netcat)

```sh
echo "hello from node-A" | nc -u -w1 <proxy-host> 1111
```

### `logger` (util-linux)

```sh
logger --server <proxy-host> --udp --port 1111 "service-X: started"
```

### Python one-liner

```python
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.sendto(b"hello from node-B\n", ("<proxy-host>", 1111))
```

### Bash `/dev/udp`

```sh
echo "hello from node-C" > /dev/udp/<proxy-host>/1111
```

---

## Web viewer

When enabled with `--web` / `WEB=true`, a minimal HTML page is served on
`--web-port` (default `80`). It:

- displays the **last 500** log entries by default (override with `?tail=N`);
- auto-refreshes every **5 seconds** (HTML `meta refresh`, so no JS required);
- renders each line as `timestamp | source | message` in a dark, monospaced
  table.

The page is read from the log file on every request, so it always reflects the
current contents of `LOG_FILE`. There is no buffering, no filtering, and no
authentication — keep the web port on a trusted network.

---

## Running without Docker

For local development you can run the script directly:

```sh
pip install -r requirements.txt
python app.py --port 1111 --web --web-port 8080 --log-file ./nodes.log
```

Then send a test message in another terminal:

```sh
echo "test" | nc -u -w1 127.0.0.1 1111
```

---

## Limitations

- **No UDP reassembly.** One datagram = one log line. Messages that don't fit
  in a single datagram will appear as multiple lines.
- **No authentication.** Anyone able to reach the UDP port can inject log
  lines, and anyone able to reach the web port can read them. This proxy is
  intended for diagnostic use on a trusted network segment.
- **No log rotation.** The log file grows without bound. Either mount a tmpfs
  for ephemeral use, or pair it with `logrotate` / a periodic truncate on the
  host if it runs long-term.
- **In-order, single-process.** Forwarding uses a single UDP socket from the
  same thread that receives, so very high message rates may drop packets at
  the kernel receive buffer.

---

## License

GPL-3.0, see the headers in [`app.py`](app.py) and [`entrypoint.sh`](entrypoint.sh).
