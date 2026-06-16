"""nettest: a web UI for exercising 3DD remote access (CLAUDE.md's "Network
testing" challenge). Serves a single page on HTTP_PORT with one button per
flow in flows.TESTS plus a live log view; the flows drive gmlbroker over TCP
as if nettest were the Guacamole web server.

Stdlib only. Configuration via environment:
    GML_HOST (gmlbroker)  GML_PORT (4823)  HTTP_PORT (8081)
    E2E_SSH_HOST (sshd)  E2E_SSH_PORT (22)  E2E_SSH_USER (tester)
    E2E_SSH_PASS (testpass)  E2E_ESTABLISH_TIMEOUT_S (15)
    REPORT_DIR (/var/log/nettest)
The E2E_SSH_* / E2E_ESTABLISH_TIMEOUT_S settings are the SSH backend the custom
flow drives guacd toward.
"""

import http.server
import json
import os
import signal
import socket
import threading
import urllib.parse

import flows
import runner

STATIC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")


def send_approval_toggle(host, port, mode):
    """Send a plaintext approval-toggle datagram to gmlbroker's control port.

    gmlbroker relays it on to the guard, which applies it as a global
    approve/deny switch. nettest cannot reach the guard directly across the
    diode, so the toggle always goes via gmlbroker.

    Parameters
    ----------
    host : str
        gmlbroker's host.
    port : int
        gmlbroker's control port (UDP).
    mode : str
        ``"approve"`` or ``"deny"``.

    Returns
    -------
    None

    Raises
    ------
    OSError
        If the datagram cannot be sent (e.g. the host does not resolve).
    """
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(mode.encode(), (host, port))


def load_cfg():
    """Read configuration from the environment, applying defaults.

    Returns
    -------
    dict
        The config consumed by the server, runner, and flows.
    """
    env = os.environ.get
    return {
        "GML_HOST": env("GML_HOST", "gmlbroker"),
        "GML_PORT": int(env("GML_PORT", "4823")),
        "GML_CONTROL_PORT": int(env("GML_CONTROL_PORT", "4999")),
        "HTTP_PORT": int(env("HTTP_PORT", "8081")),
        "E2E_SSH_HOST": env("E2E_SSH_HOST", "sshd"),
        "E2E_SSH_PORT": env("E2E_SSH_PORT", "22"),
        "E2E_SSH_USER": env("E2E_SSH_USER", "tester"),
        "E2E_SSH_PASS": env("E2E_SSH_PASS", "testpass"),
        "E2E_ESTABLISH_TIMEOUT_S": float(env("E2E_ESTABLISH_TIMEOUT_S", "15")),
        "REPORT_DIR": env("REPORT_DIR", "/var/log/nettest"),
    }


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    # Set once in main() before the server starts.
    runner = None
    log_buffer = None
    cfg = None

    def log_message(self, *args):
        pass  # the UI polls every 500 ms; default logging would flood stdout

    # Parse GET requests
    def do_GET(self):
        """Route a GET request: the page and the read-only API endpoints.

        Returns
        -------
        None
        """
        url = urllib.parse.urlparse(self.path)
        if url.path == "/":
            with open(os.path.join(STATIC, "index.html"), "rb") as f:
                body = f.read()
            self._reply(200, "text/html; charset=utf-8", body)
        elif url.path == "/api/tests":
            self._json(200, {"tests": sorted(flows.TESTS)})
        elif url.path == "/api/status":
            self._json(200, self.runner.status())
        elif url.path == "/api/log":
            query = urllib.parse.parse_qs(url.query)
            since = int(query.get("since", ["0"])[0])
            lines, cursor = self.log_buffer.since(since)
            self._json(200, {"next": cursor, "lines": lines})
        else:
            self._json(404, {"error": "not found"})

    # Parse POST requests
    def do_POST(self):
        """Route a POST request: starting and stopping a flow.

        Returns
        -------
        None
        """
        url = urllib.parse.urlparse(self.path)
        if url.path == "/api/start":
            test = urllib.parse.parse_qs(url.query).get("test", [None])[0]
            params = self._read_json_body()
            if params is None:
                self._json(400, {"error": "request body is not valid JSON"})
            elif test not in flows.TESTS:
                self._json(404, {"error": f"unknown test: {test}"})
            elif not self.runner.start(test, params):
                self._json(409, {"error": "a test is already running"})
            else:
                self._json(200, {"started": test})
        elif url.path == "/api/stop":
            self.runner.stop()
            self._json(200, {"stopping": True})
        elif url.path == "/api/approval":
            self._set_approval()
        else:
            self._json(404, {"error": "not found"})

    def _set_approval(self):
        """Flip the guard's global approval switch via gmlbroker's control port.

        Reads ``{"mode": "approve"|"deny"}`` from the body and relays it as a
        UDP datagram. The switch is global (all connections) and applies to
        subsequent approval requests.

        Returns
        -------
        None
        """
        body = self._read_json_body()
        mode = body.get("mode") if body is not None else None
        if mode not in ("approve", "deny"):
            self._json(400, {"error": "mode must be 'approve' or 'deny'"})
            return
        host, port = self.cfg["GML_HOST"], self.cfg["GML_CONTROL_PORT"]
        try:
            send_approval_toggle(host, port, mode)
        except OSError as e:
            self._json(502, {"error": f"could not reach gmlbroker control "
                                      f"port {host}:{port}: {e}"})
            return
        self.log_buffer.append(
            f"[approval] global switch set to {mode.upper()} "
            f"(sent to {host}:{port})")
        self._json(200, {"mode": mode})

    # Parse JSON body
    def _read_json_body(self):
        """Parse the request body as a JSON object.

        Returns
        -------
        dict or None
            The parsed object; ``{}`` for an empty body (flows that take no
            params); None if the body is present but not a valid JSON object.
        """
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return {}
        raw = self.rfile.read(length)
        try:
            obj = json.loads(raw)
        except (json.JSONDecodeError, UnicodeDecodeError):
            return None
        return obj if isinstance(obj, dict) else None

    def _json(self, code, obj):
        """Send a JSON response.

        Parameters
        ----------
        code : int
            HTTP status code.
        obj : Any
            A JSON-serialisable object for the body.

        Returns
        -------
        None
        """
        self._reply(code, "application/json", json.dumps(obj).encode())

    def _reply(self, code, ctype, body):
        """Send a response with an explicit content type and length.

        Parameters
        ----------
        code : int
            HTTP status code.
        ctype : str
            The Content-Type header value.
        body : bytes
            The response body.

        Returns
        -------
        None
        """
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    """Build the server, install signal handlers, and serve until shutdown.

    Returns
    -------
    None
    """
    cfg = load_cfg()
    try:
        os.makedirs(cfg["REPORT_DIR"], exist_ok=True)
    except OSError as e:
        print(f"nettest: report dir {cfg['REPORT_DIR']} unavailable, reports "
              f"will be skipped: {e}", flush=True)
    Handler.log_buffer = runner.LogBuffer()
    Handler.runner = runner.TestRunner(flows.TESTS, cfg, Handler.log_buffer)
    Handler.cfg = cfg

    srv = http.server.ThreadingHTTPServer(("0.0.0.0", cfg["HTTP_PORT"]), Handler)
    srv.daemon_threads = True

    def shut_down(signum, frame):
        Handler.runner.stop()
        # shutdown() must not run on the serve_forever() thread (deadlock).
        threading.Thread(target=srv.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, shut_down)
    signal.signal(signal.SIGINT, shut_down)

    print(f"nettest: listening on HTTP port {cfg['HTTP_PORT']} "
          f"(gmlbroker at {cfg['GML_HOST']}:{cfg['GML_PORT']})", flush=True)
    srv.serve_forever()
    print("nettest: shut down", flush=True)


if __name__ == "__main__":
    main()
