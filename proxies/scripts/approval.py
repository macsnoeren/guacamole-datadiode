"""approval.py -- a self-contained operator console for the guard's approval switch.

This is a standalone, single-file version of the ``approver`` app. The
Approve/Deny page is embedded directly in this file, so the whole console is
one drop-in Python script with no other files required. Stdlib only.

================================ MANUAL ================================

WHAT IT IS
    A bare-bones web UI, co-located with the guard on the OT side, that lets an
    operator drive the guard's global approve/deny switch. Clicking a button
    sends a plaintext ``approve``/``deny`` UDP datagram straight to the guard's
    control port -- the operator commands the guard directly, with no broker in
    between and nothing on the untrusted IT side able to influence the gate.

    A global DENY also disconnects live sessions, not just future requests
    (handled at the guard). The switch is deliberately global and coarse -- a
    per-request operator decision is future work.

RUNNING
    Requires only Python 3 (standard library, no dependencies):

        python approval.py

    Then open http://localhost:8082 in a browser.

CONFIGURATION (environment variables)
    GUARD_HOST           guard host to command          (default: 127.0.0.1)
    GUARD_CONTROL_PORT   guard UDP control port         (default: 4999)
    HTTP_PORT            port this console serves on    (default: 8082)

    Example:

        GUARD_HOST=guard GUARD_CONTROL_PORT=4999 HTTP_PORT=8082 python approval.py

HTTP API
    GET  /              -> the operator page (embedded, no static files)
    GET  /api/state     -> {"mode": "approve"|"deny"|null}
                           the last command sent this session. The console
                           cannot read the guard's switch back; the guard
                           defaults to APPROVE on startup, so null means
                           "not set this session".
    POST /api/approval  body {"mode": "approve"|"deny"}
                           sends the datagram to the guard and remembers the
                           command for the session. Returns {"mode": ...} on
                           success, or an error object with status:
                             400  body not valid JSON / bad mode
                             404  unknown path
                             502  could not reach the guard control port

NOTES
    - Threaded HTTP server; the remembered last-mode is guarded by a lock.
    - Deliberately no authentication: it is meant to run on a trusted OT-side
      host reachable only by the operator. Do not expose it to untrusted
      networks.

=======================================================================
"""

import http.server
import json
import os
import socket
import threading

# The console cannot read the guard's switch back, so it remembers the last
# command it sent. `None` means "not set this session" (the guard defaults to
# APPROVE on startup). Guarded by `_state_lock` as the HTTP server is threaded.
_state_lock = threading.Lock()
_last_mode = None

# The operator page, embedded so no static/ directory is needed.
INDEX_HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>3DD — Guard approver</title>
<style>
  :root { --muted: #6b7280; --bg: #f8fafc; --card: #fff; --line: #e5e7eb; }
  * { box-sizing: border-box; }
  body {
    font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
    background: var(--bg); color: #111827; margin: 0;
    min-height: 100vh; display: grid; place-items: center;
  }
  .card {
    background: var(--card); border: 1px solid var(--line); border-radius: 12px;
    padding: 2rem 2.25rem; width: min(92vw, 420px);
    box-shadow: 0 1px 3px rgba(0,0,0,.06);
  }
  h1 { font-size: 1.15rem; margin: 0 0 .25rem; }
  .sub { color: var(--muted); font-size: .85rem; margin: 0 0 1.5rem; }
  .state {
    display: flex; align-items: center; gap: .6rem;
    font-size: .95rem; margin-bottom: 1.5rem;
  }
  .dot { width: .7rem; height: .7rem; border-radius: 50%; background: #9ca3af; }
  .dot.approve { background: #16a34a; }
  .dot.deny { background: #dc2626; }
  .buttons { display: flex; gap: .75rem; }
  button {
    flex: 1; padding: .85rem 1rem; font-size: 1rem; font-weight: 600;
    border-radius: 10px; border: 2px solid var(--line); background: #fff;
    cursor: pointer; transition: transform .05s;
  }
  button:active { transform: translateY(1px); }
  #approve { border-color: #16a34a; color: #166534; }
  #deny { border-color: #dc2626; color: #991b1b; }
  #msg { font-size: .8rem; color: var(--muted); margin-top: 1rem; min-height: 1em; }
</style>
</head>
<body>
  <div class="card">
    <h1>Guard approval</h1>
    <p class="sub">Operator console — toggles the guard's global approval switch.</p>
    <div class="state">
      <span id="dot" class="dot"></span>
      <span>Last command this session: <strong id="state-text">none (guard default: APPROVE)</strong></span>
    </div>
    <div class="buttons">
      <button id="approve">Approve</button>
      <button id="deny">Deny</button>
    </div>
    <div id="msg"></div>
  </div>
<script>
const $ = (id) => document.getElementById(id);

function render(mode) {
  const dot = $("dot"), text = $("state-text");
  dot.className = "dot" + (mode ? " " + mode : "");
  text.textContent = mode ? mode.toUpperCase()
                          : "none (guard default: APPROVE)";
}

async function setMode(mode) {
  $("msg").textContent = "sending…";
  try {
    const r = await fetch("/api/approval", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mode }),
    });
    const data = await r.json();
    if (!r.ok) throw new Error(data.error || r.statusText);
    render(data.mode);
    $("msg").textContent = "Sent " + data.mode.toUpperCase() + " to the guard."
      + (mode === "deny" ? " Live sessions are also disconnected." : "");
  } catch (e) {
    $("msg").textContent = "Error: " + e.message;
  }
}

$("approve").onclick = () => setMode("approve");
$("deny").onclick = () => setMode("deny");

// Reflect the last command sent (survives a page refresh within the session).
fetch("/api/state").then(r => r.json()).then(d => render(d.mode)).catch(() => {});
</script>
</body>
</html>
"""

def load_cfg():
    """Read configuration from the environment, applying defaults.

    Returns
    -------
    dict
        The guard host/port to command and the HTTP port to serve on.
    """
    env = os.environ.get
    return {
        "GUARD_HOST": env("GUARD_HOST", "127.0.0.1"),
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
        body = INDEX_HTML.encode("utf-8")
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
        print(f"[approval] sent {mode.upper()} to {host}:{port}", flush=True)
        self._json(200, {"mode": mode})

class ThreadingHTTPServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

def main():
    cfg = load_cfg()
    server = ThreadingHTTPServer(("0.0.0.0", cfg["HTTP_PORT"]), Handler)
    print(f"[approval] serving on :{cfg['HTTP_PORT']}, commanding "
          f"{cfg['GUARD_HOST']}:{cfg['GUARD_CONTROL_PORT']}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
