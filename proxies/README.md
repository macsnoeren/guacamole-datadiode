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

TLS is used between the Guacamole Server and the gmlbroker. The [scripts/generate-tls.sh](scripts/generate-tls.sh) script generates the certificates for that.

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
