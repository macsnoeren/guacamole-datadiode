# Proxies

This folder contains the proxy software that makes the remote access Guacamole solution work over the data-diodes. These are the C++ programs that sit between the Guacamole Server and guacd and move the Guacamole traffic across the diodes, while making sure only keystrokes, mouse movements and video are exchanged.

If you want the bigger picture (why data-diodes, the two-node and three-node architecture, the security idea behind it), please read the [main README](../README.md) first. This README is only about the software in this folder.

## The proxies

There are three programs that do the real work, plus a shared library they all use.

- **gmlbroker** — the low-side broker. The Guacamole Server connects to this program as if it were guacd. It accepts the connection and only sends the allowed traffic (keystrokes, mouse) over the data-diode to the other side.
- **gmguard** — the guard. It filters the traffic and rejects everything that is not allowed. Only keystrokes and mouse movements are forwarded. In the three-node setup the guard sits between the two data-diodes, so it is physically separated from both networks. This is where you would also implement an approval process if you want one.
- **gcdbroker** — the high-side broker. It connects to guacd and pretends to be the Guacamole Server. Everything it receives from guacd is sent back to the gmlbroker.
- **shared** — a small library with the networking and Guacamole protocol parsing code that the three programs share. It is not a program on its own.

The traffic flow is basically:

```
Guacamole Server <-> gmlbroker  --(DD)-->  gmguard  --(DD)-->  gcdbroker <-> guacd
```

and the video/screen updates flow back the other way over the return diode.

## Building

Each program is a separate meson project with its own `meson.build`. To build one, go into its folder and run:

```
meson setup build
meson compile -C build
```

The binary ends up in `build/`. You need `meson`, `ninja`, a C++17 compiler (`g++`) and `libssl-dev` installed. The same steps are used inside the Docker images, so if you just want to run the whole thing you probably want the Docker Compose files instead (see below).

## Running with Docker

You normally do not run the proxies by hand. The Docker images and the Docker Compose configurations live in [../dockers](../dockers). The compose files under [../dockers/docker-compose](../dockers/docker-compose) let you run the different setups:

- **1-node** — everything on a single machine, handy for testing.
- **2-node** — a low node (Guacamole Server + gmlbroker) and a high node (gcdbroker + guacd), with the guard running on the high side.
- **3-node** — a low node, a separate guard node, and a high node. The guard sits between the diodes here.

The 2-node and 3-node setups need some extra network configuration (correct IP addresses and ARP/neighbor entries), otherwise the routing over the diodes will not work.

The link between the Guacamole Server and the gmlbroker can be encrypted with TLS. This is optional and off by default. When you turn it on (`GMLBROKER_TLS=1`), the gmlbroker makes its own self-signed certificate on the first start, so you do not have to generate certificates yourself. The Guacamole side then has to trust that certificate. The full story (how to turn it on and how to make Guacamole trust it) is in the [docker-compose README](../dockers/docker-compose/README.md).

## Approval process (prepared, not implemented yet)

The idea is that an operator on the OT side can decide if connections are allowed or not. So a connection is only let through when someone actually approves it. This is a security idea: even when someone gets access on the IT side, nothing goes through the guard until a person on the trusted side says yes.

Right now this is **not implemented in the guard yet**, but it is prepared so you can see how it is meant to work. The example is in [scripts/approval.py](scripts/approval.py). It is a small, self-contained Python script (standard library only, no extra packages) that gives the operator a simple web page with two buttons: **Approve** and **Deny**.

You run it like this:

```
python scripts/approval.py
```

and then open `http://localhost:8082` in a browser. There are a few environment variables to point it at the guard:

- `GUARD_HOST` — the guard host to command (default `127.0.0.1`).
- `GUARD_CONTROL_PORT` — the guard UDP control port (default `4999`).
- `HTTP_PORT` — the port the console itself serves on (default `8082`).

How it is meant to work: when the operator clicks a button, the console sends a plaintext `approve` or `deny` UDP datagram straight to the guard's control port. So the operator talks to the guard directly, there is no broker in between and nothing on the untrusted IT side can touch this switch. The idea is:

- **Approve** — connections are allowed through the guard.
- **Deny** — connections are blocked. A deny should not only block new requests but also disconnect the sessions that are already running.

For now the switch is meant to be global and coarse (one switch for everything, not per connection). A per-connection decision, where the operator approves each single request, is something for later.

Two more things to keep in mind. The console has **no authentication** on purpose: it is meant to run on a trusted OT-side host that only the operator can reach, so do not expose it to untrusted networks. And the console cannot read the guard's switch back, so it only remembers the last command it sent this session. The guard defaults to APPROVE on startup, so if the console says "none" it just means nothing was set yet this session.

Remember, this is the prepared example. The guard does not act on these datagrams yet, so at the moment nothing is really approved or denied. It is there so you can see on which way this is going to work later.

## Testing

### Unit tests

The programs have unit tests (partial coverage, not everything is tested). They are meson tests, so after building you can run them per program with:

```
meson test -C build
```

Coverage is not complete. For example, on the guard the main tested part is the parser logic.

### Stress testing (nettest)

There is a separate Python stress testing suite in [nettest](nettest). It creates a lot of Guacamole connections at once, sends input on all of them and measures the round trip times and timeouts. This is how you check how many connections Guacamole and the bridge can handle before things get too slow.

It also has a "bad connections" option that sends non-Guacamole traffic (like a reverse shell or a random HTTP call) towards guacd. The guard should catch this and shut those connections down, so it also partly tests the filtering on the guard.

nettest has its own [README](nettest/README.md) with the details and the dashboard description.

### Static code analysis (CodeQL)

The C++ code is scanned with [CodeQL](https://codeql.github.com/). Two helper scripts in [scripts/](scripts) drive this:

- `codeql-build-cpp.sh` does a clean (full recompile) build of every program, which CodeQL traces by watching the real compiler calls.
- `codeql-scan.sh` creates the database, runs the analysis and writes the results.

You need the CodeQL CLI on your `PATH` (or point the `CODEQL` env var at the binary; it also falls back to `$HOME/tools/codeql/codeql`), plus the usual `meson`/`ninja`/`g++` toolchain.

Installing the CodeQL CLI (the `$HOME/tools/codeql` fallback location):

```
# Pick the latest release for your platform from
# https://github.com/github/codeql-action/releases (the codeql-bundle assets
# ship the CLI together with the standard query packs).
mkdir -p "$HOME/tools"
cd "$HOME/tools"
curl -L -o codeql-bundle.tar.gz \
    https://github.com/github/codeql-action/releases/latest/download/codeql-bundle-linux64.tar.gz
tar -xzf codeql-bundle.tar.gz   # extracts a ./codeql directory
rm codeql-bundle.tar.gz

# Verify (the scripts find it here, or add it to your PATH):
"$HOME/tools/codeql/codeql" --version
export PATH="$HOME/tools/codeql:$PATH"
```

On macOS use the `codeql-bundle-osx64.tar.gz` asset instead, or install with Homebrew (`brew install codeql`).

Run the scan from this `proxies/` folder:

```
scripts/codeql-scan.sh
```

By default it runs the `cpp-security-and-quality` suite. Pick a different suite with the `SUITE` env var, for example:

```
SUITE=cpp-security-extended scripts/codeql-scan.sh
```

The output is written to `.codeql/` (gitignored):

- `cpp-db/` — the CodeQL database.
- `cpp-results.sarif` — the full result set (open it in an editor with a SARIF viewer).
- `cpp-results.csv` — a readable summary, one row per alert.

## Tips and known issues

- Make sure the IP addresses and the neighbor table (ARP) are set correctly before running, otherwise the routing over the diodes will not work.
- If you have trouble running many RDP connections at once, lower the traffic a bit with these settings:
    - Color depth 8 or 16-bit (not 24/32).
    - Disable wallpaper, theming, font smoothing, full-window drag, menu animations and desktop composition.
    - A smaller resolution if possible.
