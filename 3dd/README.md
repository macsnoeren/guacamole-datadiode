Iron Bridge: foundation of the 'remote access over a Triple Data Diode architecture' use case.

Project Iron Bridge created the initial PoC for a secure system that regulates the flow of Guacamole remote access traffic between networks. It was created to give control and security over data streams to the OT (DCS) side. This increases plant operator control over inbound requests. It provides strict filtering on inbound traffic.

![Iron Bridge server rack](docs/assets/rack.webp)

## Architecture

In total, five C++ CLI programs ('apps') were developed to make the use case possible (excluding Guacamole and remote access apps). Three of these are necessary to run:
- **gmlbroker**, Guacamole broker. The interface through which the web server receives and sends remote access traffic. It sits on the non-critical side of the network (plant DMZ) and routes remote access traffic over the Triple Data Diode (3DD). Runs on: low side send proxy.
- **guard**. This application filters out any traffic not strictly required for remote access. This includes file transfer operations. It also serves as the approving/denying barrier for remote connections. Runs on: guard proxy.
- **gcdbroker**, guacd broker. The interface through which the guacd program receives and sends remote access traffic. Runs on: high side send proxy.

On the hardware topology level, there are two network paths, one inbound (into the plant) and one outbound. It is physically impossible to reverse the network stream direction on these paths, due to data diodes. Simplified, the paths visit:

Inbound: web server <--> Guacamole broker -->(DD) guard -->(DD) guacd broker <--> guacd

and

Outbound: guacd <--> guacd broker -->(DD) Guacamole broker <--> web server

where DD is one data diode.

Two receiver proxies were created (low side receive proxy & high side receive proxy) to route traffic from guacd broker and from the guard respectively. We currently do not see much use in these proxies, as their presence does not contribute to security much, and can be left out.

The full architecture:

![Full 3DD architecture](docs/assets/5node-arch.webp)

## Environment

Each app runs inside its own Docker container. Some Docker Compose configurations were made for convenient testing and running the system. Currently, the following configurations exist inside 3dd/docker:
- 1node: Run all applications on a single machine (including the optional proxies)
- 3node: Run necessary applications on three nodes: the low node (web server + broker), the guard node (guard proxy), and the high node (guacd + broker). Needs additional network (IP address + ARP entry) configuration to work
- 5node: Same as 3node, but also runs the optional proxies on two different nodes. Also needs additional network configuration to work.
- bridgeless: a pure web server-to-guacd configuration with no 3DD apps involved.

## Installation & running

Guides on how to install & run on several configurations can be found in [this file](docs/install-and-run.md).

## Testing

Iron Bridge tested the code using the following methods:
- Unit testing (partial coverage);
- Stress testing (checking potential network capacity of the 3DD);
- Static code analysis (CodeQL);
- User Acceptance Test.

### Unit testing

Unit tests are run by default when a configuration runs. Not all code is fully covered. For example, currently, the guard's only tested unit is the parser logic.

To disable testing on configuration run, add the following to a configuration's compose file (watch the quotation marks surrounding "true"):
```
  gmlbroker:
    build:
      args:
        DISABLE_TESTS: "true"
```

### Stress testing

As part of Iron Bridge, a standalone stress testing environment was built in Python. Although it is not the most representative simulation method for testing real-world use, it can put high stress on the 3DD and measure network statistics.

![Stress testing dashboard](docs/assets/stress-test-dashboard.webp)

Test the amount of connections that can be active at once by setting good connections. Set bad connections to 0, and play around with some other parameters. Hovering over a parameter shows its description. When running a test, pay special attention to the timeouts field. During the test, the active connections attempt to simultaneously send key strokes and wait for a screen update back from the remote host. Any time such a reply is not received before a timeout happens (2 seconds), the timeout field increases by one. Divided by the n statistic (amount of key strokes sent), a rough metric of loss can be interpreted.

The Bad connections parameter simulates non-Guacamole traffic (reverse shell, HTTP call, etc.) to guacd. The guard should catch this bad traffic in flight and shut down the connections. This partially tests the traffic filtering feature on the guard.

### Static code analysis

The C++ components are scanned with [CodeQL](https://codeql.github.com/). Two helper scripts in `3dd/scripts/` drive this:

- `codeql-build-cpp.sh` does a clean (full recompile) build of every component, which CodeQL traces by observing the real compiler invocations.
- `codeql-scan.sh` creates the database, runs the analysis suite, and writes the results.

Prerequisites: the CodeQL CLI on your `PATH` (or point the `CODEQL` env var at the binary; it also falls back to `$HOME/tools/codeql/codeql`), plus the usual `meson`/`ninja`/`g++` toolchain.

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
On macOS use the `codeql-bundle-osx64.tar.gz` asset instead. Alternatively install via Homebrew (`brew install codeql`).

Run the scan from the `3dd/` directory:
```
scripts/codeql-scan.sh
```

By default it runs the `cpp-security-and-quality` suite. Pick a different suite with the `SUITE` env var, for example:
```
SUITE=cpp-security-extended scripts/codeql-scan.sh
```

Outputs are written to `3dd/.codeql/` (gitignored):
- `cpp-db/` — the CodeQL database;
- `cpp-results.sarif` — the full result set (open in an editor's SARIF viewer);
- `cpp-results.csv` — a human-readable summary, one row per alert.

## Issues

- Make sure the IP addresses and neighbor table (ARP) are set correctly before running, or the routing will not work.
- If you are having trouble with running many RDP connections at once, set the following settings to reduce traffic bandwidth: 
    - Color depth 8 or 16-bit (not 24/32)
    - Disable wallpaper, theming, font smoothing, full-window drag, menu animations, and desktop composition
    - A smaller resolution if possible
