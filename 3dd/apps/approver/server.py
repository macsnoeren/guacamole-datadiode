"""approver: a minimal operator console for the guard's global approval switch.

Co-located with the guard (OT-side), it serves a single page with Approve/Deny
buttons. Clicking a button sends a plaintext "approve"/"deny" datagram straight
to the guard's control port -- the operator commands the guard directly, with no
broker in between and nothing on the IT side able to influence the decision.

Stdlib only. Configuration via environment:
    GUARD_HOST (guard)  GUARD_CONTROL_PORT (4999)  HTTP_PORT (8082)
"""

import http.server
import json
import os
import socket
import threading

STATIC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

# The approver cannot read the guard's switch back, so it remembers the last
# command it sent. `None` means "not set this session" (the guard defaults to
# DENY on startup). Guarded by `_state_lock` as the HTTP server is threaded.
_state_lock = threading.Lock()
_last_mode = None


def load_cfg():
    """Read configuration from the environment, applying defaults.

    Returns
    -------
    dict
        The guard host/port to command and the HTTP port to serve on.
    """
    env = os.environ.get
    return {
        "GUARD_HOST": env("GUARD_HOST", "guard"),
        "GUARD_CONTROL_PORT": int(env("GUARD_CONTROL_PORT", "4999")),
        "HTTP_PORT": int(env("HTTP_PORT", "8082")),
    }


def send_approval_toggle(host, port, mode):
    """Send a plaintext approval-toggle datagram to the guard's control port.

    Parameters
    ----------
    host : str
        The guard's host.
    port : int
        The guard's control port (UDP).
    mode : str
        ``"approve"`` or ``"deny"``.

    Raises
    ------
    OSError
        If the datagram cannot be sent (e.g. the host does not resolve).
    """
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(mode.encode(), (host, port))


class Handler(http.server.BaseHTTPRequestHandler):
    """Serves the operator page and the approve/deny + state endpoints."""

    cfg = load_cfg()

    def log_message(self, *args):  # quieten the default per-request stderr noise
        pass

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self._serve_index()
        elif self.path == "/api/state":
            with _state_lock:
                self._json(200, {"mode": _last_mode})
        else:
            self._json(404, {"error": "not found"})

    def _serve_index(self):
        try:
            with open(os.path.join(STATIC, "index.html"), "rb") as f:
                body = f.read()
        except OSError:
            self._json(500, {"error": "index.html missing"})
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path != "/api/approval":
            self._json(404, {"error": "not found"})
            return
        length = int(self.headers.get("Content-Length", 0))
        try:
            body = json.loads(self.rfile.read(length) or b"{}")
        except (ValueError, TypeError):
            self._json(400, {"error": "body is not valid JSON"})
            return
        mode = body.get("mode")
        if mode not in ("approve", "deny"):
            self._json(400, {"error": "mode must be 'approve' or 'deny'"})
            return
        host, port = self.cfg["GUARD_HOST"], self.cfg["GUARD_CONTROL_PORT"]
        try:
            send_approval_toggle(host, port, mode)
        except OSError as e:
            self._json(502, {"error": f"could not reach guard control "
                                      f"port {host}:{port}: {e}"})
            return
        global _last_mode
        with _state_lock:
            _last_mode = mode
        print(f"[approver] sent {mode.upper()} to {host}:{port}", flush=True)
        self._json(200, {"mode": mode})


class ThreadingHTTPServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    cfg = load_cfg()
    server = ThreadingHTTPServer(("0.0.0.0", cfg["HTTP_PORT"]), Handler)
    print(f"[approver] serving on :{cfg['HTTP_PORT']}, commanding "
          f"{cfg['GUARD_HOST']}:{cfg['GUARD_CONTROL_PORT']}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
