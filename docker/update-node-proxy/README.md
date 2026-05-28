# update-node-proxy

A one-way file-transfer proxy for shipping (potentially large) update files from
the "dirty" side of the data diode to the "clean" side. Operators upload a file
over HTTP on the sender container; the sender chunks it, streams the chunks over
a one-way UDP link through the data diode, and the receiver container
reassembles the file in its inbox directory.

It is intended for delivering Flatcar update payloads (or any other large blob)
to nodes that sit behind the diode and cannot be reached directly.

The same image is used on every node in the chain, switched with
`MODE=sender`, `MODE=relay`, or `MODE=receiver`. A `relay` is a pure UDP
forwarder for intermediate hops (e.g. the guard between two diode subnets).

---

## How it works

```
  operator / CI                                              consumer on clean node
        |                                                           ^
        | HTTP PUT /upload/<name>                                   | reads <name> when
        v                                                           | <name>.done appears
+---------------+   UDP   +---------------+   UDP   +-----------------+
| sender        |  ====>  | relay         |  ====>  | receiver        |
| (low side)    |  diode  | (guard)       |  diode  | (high side)     |
|               |  link 1 |               |  link 2 |                 |
| HTTP intake   |         | UDP in -> out |         | UDP in          |
| staging dir   |         | (no parsing,  |         | reassemble      |
| chunker + UDP |         |  no web UI)   |         | sha256 verify   |
| web UI        |         |               |         | inbox + sidecars|
+---------------+         +---------------+         +-----------------+
```

For a simple two-node setup (sender directly talks to receiver), the `relay`
in the middle is optional. In the Flatcar guacamole-datadiode setup the
sender and receiver live in different subnets separated by the guard, so a
`relay` runs on the guard.

- The sender writes uploads to `staging/<name>.partial`, then atomically renames
  to `staging/<name>` and writes `staging/<name>.sha256` once the body is fully
  received.
- A background thread picks up any `<name>` that has a matching `<name>.sha256`
  sidecar, marks it with `<name>.sending`, then transmits a `header`, all data
  `chunks` (each repeated `OVERSEND_DATA` times) and an `end` record over UDP.
- The receiver maintains one part-directory per in-flight `file_id`. Chunks are
  written to disk as they arrive (no large in-memory buffering). When all
  chunks for a file are present, the parts are concatenated to
  `inbox/<name>`, the sha256 is verified, and two sidecars are written:
    - `inbox/<name>.sha256` — the digest, hex
    - `inbox/<name>.done`   — a one-line "complete" marker with timestamp
- Consumers should treat `<name>` as valid **only** when `<name>.done` exists.
- Both modes expose a small Flask UI on `WEB_PORT` (default `80`) that shows the
  current queue / inbox state and auto-refreshes every 5 seconds.

### Wire format

Each UDP datagram carries one record. Multi-byte integers are big-endian.

```
header  : 'H' | file_id(16) | sha256(32) | total_chunks(4) | chunk_size(4)
              | name_len(2) | name(name_len)
chunk   : 'C' | file_id(16) | chunk_nr(4) | data(<=chunk_size)
end     : 'E' | file_id(16)
```

Header and end records are sent `OVERSEND_META` times (default 5). Each data
chunk is sent `OVERSEND_DATA` times (default 2). There is no ACK channel — the
diode is one-way.

### Why oversend instead of retransmit?

A real ACK/NAK loop is impossible over a hardware data diode. Sending each
chunk a small number of times trades bandwidth for resilience against random
packet loss. With `OVERSEND_DATA=2` the link tolerates ~50% per-packet loss
before chunks start going missing entirely; with `OVERSEND_DATA=3` it tolerates
much more. If a file fails the sha256 check on the receiver it is deleted and
the operator simply uploads it again on the sender.

---

## Configuration

The container is configured through **environment variables** which are
translated to CLI flags by [`entrypoint.sh`](entrypoint.sh). The CLI flags
themselves are also useful when running [`app.py`](app.py) directly during
development.

### Shared

| Env var    | CLI flag           | Default | Description                                         |
|------------|--------------------|---------|-----------------------------------------------------|
| `MODE`     | `-m`, `--mode`     | *(req)* | `sender`, `relay`, or `receiver`.                   |
| `WEB_PORT` | `-W`, `--web-port` | `80`    | HTTP port for the web UI / upload endpoint (sender/receiver). |

### Sender (`MODE=sender`)

| Env var           | CLI flag              | Default                          | Description                                                     |
|-------------------|-----------------------|----------------------------------|-----------------------------------------------------------------|
| `UPLOAD_DIR`      | `-u`, `--upload-dir`  | `/var/lib/update-proxy/staging`  | Staging directory; uploads land here.                           |
| `SENT_DIR`        | `-S`, `--sent-dir`    | `/var/lib/update-proxy/sent`     | Files are moved here after successful UDP send.                 |
| `DD_HOST`         | `-d`, `--dd-host`     | *(required)*                     | UDP destination host (e.g. `gmproxyin` / diode input).          |
| `DD_PORT`         | `-o`, `--dd-port`     | `40000`                          | UDP destination port.                                           |
| `CHUNK_SIZE`      | `-c`, `--chunk-size`  | `1200`                           | UDP chunk payload size in bytes (keep below path MTU).          |
| `OVERSEND_DATA`   | `--oversend-data`     | `2`                              | How many times each data chunk is sent.                         |
| `OVERSEND_META`   | `--oversend-meta`     | `5`                              | How many times header / end records are sent.                   |
| `INTER_PACKET_US` | `--inter-packet-us`   | `200`                            | Sleep between packets in microseconds (0 = no pacing).          |
| `SCAN_INTERVAL`   | `--scan-interval`     | `1.0`                            | Seconds between staging-dir scans.                              |
| `MAX_UPLOAD_MB`   | `--max-upload-mb`     | `8192`                           | Maximum single upload size in MB.                               |

### Receiver (`MODE=receiver`)

| Env var        | CLI flag            | Default                       | Description                                          |
|----------------|---------------------|-------------------------------|------------------------------------------------------|
| `INBOX_DIR`    | `-i`, `--inbox-dir` | `/var/lib/update-proxy/inbox` | Directory where completed files land.                |
| `PORT`         | `-p`, `--port`      | `40000`                       | UDP port to listen on.                               |
| `BIND`         | `-b`, `--bind`      | `0.0.0.0`                     | UDP bind address.                                    |
| `IDLE_TIMEOUT` | `--idle-timeout`    | `300`                         | Seconds before incomplete files are dropped.         |

### Relay (`MODE=relay`)

A pure UDP forwarder. No parsing, no disk I/O, no web UI. Use on an
intermediate hop (e.g. the guard between two diode subnets).

| Env var   | CLI flag          | Default      | Description                                |
|-----------|-------------------|--------------|--------------------------------------------|
| `PORT`    | `-p`, `--port`    | `40000`      | UDP port to listen on.                     |
| `BIND`    | `-b`, `--bind`    | `0.0.0.0`    | UDP bind address.                          |
| `DD_HOST` | `-d`, `--dd-host` | *(required)* | UDP destination host (the next hop).       |
| `DD_PORT` | `-o`, `--dd-port` | `40000`      | UDP destination port.                      |

---

## Docker usage

### Build

```sh
cd docker/update-node-proxy
docker build -t update-node-proxy .
```

### Run — sender

Accept uploads on `http://localhost:8080/upload/<name>` and forward to the
diode input at `10.0.0.5:40000`:

```sh
docker run --rm \
    -e MODE=sender \
    -e WEB_PORT=8080 \
    -e DD_HOST=10.0.0.5 \
    -e DD_PORT=40000 \
    -v "$PWD/sender-data:/var/lib/update-proxy" \
    -p 8080:8080 \
    update-node-proxy
```

Browse to <http://localhost:8080/> for the sender UI.

### Run — receiver

Listen for UDP chunks on port 40000 and assemble completed files into
`./inbox/`:

```sh
docker run --rm \
    -e MODE=receiver \
    -e WEB_PORT=8080 \
    -e PORT=40000 \
    -v "$PWD/receiver-data:/var/lib/update-proxy" \
    -p 8080:8080 \
    -p 40000:40000/udp \
    update-node-proxy
```

Browse to <http://localhost:8080/> for the receiver UI. Completed files land in
the mounted `receiver-data/inbox/` directory, together with `<name>.sha256` and
`<name>.done`.

### Run — relay

Forward every UDP datagram from `:40000` to the receiver on the next subnet:

```sh
docker run --rm \
    -e MODE=relay \
    -e PORT=40000 \
    -e DD_HOST=10.10.20.2 \
    -e DD_PORT=40000 \
    --network host \
    update-node-proxy
```

`--network host` is recommended on the guard so the relay can bind to a
specific NIC (set `BIND=<nic-ip>`) and reach the next-hop subnet without
docker NAT.

---

## Sending an update file

### `curl` — push a file

```sh
curl --upload-file ./flatcar-image.bin \
     http://<sender-host>:8080/upload/flatcar-image.bin
```

### `curl` — push a file with explicit checksum verification

```sh
SHA=$(sha256sum ./flatcar-image.bin | cut -d' ' -f1)
curl --upload-file ./flatcar-image.bin \
     -H "X-Checksum-SHA256: $SHA" \
     http://<sender-host>:8080/upload/flatcar-image.bin
```

If the body does not match `X-Checksum-SHA256` the sender returns `400` and
deletes the partial file; nothing is sent over the diode.

### Python — push a file

```python
import requests
with open("flatcar-image.bin", "rb") as f:
    r = requests.put("http://<sender-host>:8080/upload/flatcar-image.bin", data=f)
print(r.status_code, r.json())
```

---

## Consuming files on the receiver side

Wait for the `.done` sidecar to appear before reading `<name>`. A minimal poll:

```sh
while [ ! -f /var/lib/update-proxy/inbox/flatcar-image.bin.done ]; do
    sleep 1
done
# safe to use:
cp /var/lib/update-proxy/inbox/flatcar-image.bin /opt/flatcar/update.bin
```

The companion `<name>.sha256` holds the verified digest (also embedded in the
header during transfer) and matches what the sender stored.

---

## Running without Docker

```sh
pip install -r requirements.txt

# terminal 1 — receiver
python app.py --mode receiver --web-port 8081 --port 40000 \
    --inbox-dir ./inbox

# terminal 2 — sender
python app.py --mode sender --web-port 8080 \
    --dd-host 127.0.0.1 --dd-port 40000 \
    --upload-dir ./staging --sent-dir ./sent

# terminal 3 — push a test file
curl --upload-file ./somefile.bin http://127.0.0.1:8080/upload/somefile.bin
```

After the sender finishes transmitting, `./inbox/somefile.bin` and
`./inbox/somefile.bin.done` should appear.

---

## Limitations

- **No ACKs.** Loss-recovery is purely oversend-based. A file that fails the
  sha256 check on the receiver is discarded; re-upload it on the sender.
- **No authentication.** Anyone able to reach the upload endpoint can push a
  file, and anyone able to reach the diode UDP port can inject chunks. Run
  these containers on trusted network segments only.
- **In-memory file-id tracking on the receiver.** A restart of the receiver
  while a file is in flight loses the partial parts directory.
- **No retention policy.** Files accumulate in `SENT_DIR` and `INBOX_DIR`
  forever unless cleaned externally.
- **Header-after-chunks race.** If chunks arrive before the header, they are
  staged on disk under the file_id but the receiver only knows the final name
  once the header is seen. With `OVERSEND_META=5` and headers sent first this
  is rare in practice.

---

## License

GPL-3.0, see the headers in [`app.py`](app.py) and [`entrypoint.sh`](entrypoint.sh).
