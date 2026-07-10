## nettest - A suite for stress testing Guacamole connection latency and timeouts

Plainly load-testing Guacamole connections can be a real pain. Multiple browser tabs creating and using a connection is difficult to test. Sending input on each connection at the same time, and measuring latency. This environment can manage both in an automated way using async Python coroutines. It measures round trip times (mean, min, max ms, etc.) for a variable amount of connections. Infrastructure engineers can measure how long it takes for Guacamole to react to inputs, identifying how usable remote access is under load.

The main use case of this environment is for testing how many connections Guacamole and the bridge can handle before timeouts happen (taking too long to respond).

### Setting up

Any Docker configuration with the `nettest` container included makes use of stress testing. Build and run a configuration by running `docker compose -f {Compose file name} up --build`. `nettest` should report its startup in the logs.

### The dashboard

Navigate to [http://localhost:8081] where the web server listens. A dashboard is shown with at least the following elements:

1. The custom load test, with parameters:
    - Good connections: amount of non-disruptive Guacamole connections to create at once
    - Bad connections: amount of disruptive Guacamole connections that send invalid traffic, corrupting their stream (good connections stay healthy)
    - Send delay: seconds to wait before good connections sends a 'probe' (input to guacd)
    - Randomize send offsets: unchecked makes all connections send at exactly the same time, checked adds a random send delay to each connection independently (imitating real-world connection load)
    - Duration: length of the test in seconds
    - Probe: the type of request-response to use. The key->paint option uses an ENTER key press as request, and expects a paint response (img/blob/rect instruction) back. The argv->ack option uses the argv-instruction as request, expecting an ack-instruction back. The former is more representative of real-world traffic; the latter is smaller in size and uses less bandwidth but is less representative for real communication.

2. Live run statistics with metrics:
    - Connections: how many are open, how many maximum
    - Good connections: total created, total established (handshake completed) and disconnected. Also keeps track of timeouts: increased whenever it takes too long to hear back from a request (connection stays alive)
    - Bad connections: total created, established, corrupted the stream, were disconnected
    - Latency: how many requests were sent, and mean, min, max, std deviation of round trip time

3. `nettest` print log, with a **Clear reports** button that deletes every saved
   Markdown report from the report directory (after a confirmation dialog).

### Under the hood

```
apps/nettest/
├── server.py        HTTP server + JSON API
├── runner.py        Functions for managing test state and writing test reports
├── guacclient.py    blocking Guacamole send/recv client + a Guacamole parser
├── aguacclient.py   asyncio version of guacclient.py (used by the custom flow)
├── flows/
│   ├── __init__.py  the TESTS registry  {name: callable}
│   ├── common.py    shared handshake constants + connection builder
│   └── custom.py    the custom load-testing flow implementation
└── static/
    └── index.html   UI (polls API for refreshing results)
```

1. Flows run when the browser POSTs `/api/start?test=<name>`, optionally with JSON parameters. `server.py` looks up the test name and calls `runner.start(name, params)`.
2. `TestRunner` runs one flow at a time on a daemon thread. It builds a `TestContext` and runs the actual test.
3. A test creates multiple snapshots of the values it tracks during the test. `TestRunner`'s `_publish_live` method is used to swap new with old snapshots.
4. The UI polls `/api/status` and `/api/log` every 500 ms to render the snapshot on the dashboard and append logs.
5. When a test ends, `runner.py` records the latest snapshot and writes a Markdown-report for it in `nettest-reports/`.

Adding new test flows is modular. Create a new file `flows/<name>.py`, write a custom `run_<name>(ctx)` function in it, then add it to the `TESTS` variable in `flows/__init__.py`. The UI will build it automatically.

#### Guacamole clients

`guacclient.py` and `aguacclient.py` drive the Guacamole parsing and connection establishment.
- `guacclient.py` houses the shared parser functions: `encode()`, `decode_one()`, and `parse_instruction(buf)` (the latter validates instructions). It also has the synchronous `connect()` operation for blocking connections.
- `aguacclient.py` contains the asynchronous network functions of `guacclient.py`.
