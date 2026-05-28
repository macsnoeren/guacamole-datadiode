#
# Copyright (C) 2025 Maurice Snoeren
#
# This program is free software: you can redistribute it and/or modify it under the terms of
# the GNU General Public License as published by the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see https://www.gnu.org/licenses/.
#
import argparse
import datetime
import html
import os
import socket
import sys
import threading
import time


def now_iso():
    return datetime.datetime.now().isoformat(timespec="milliseconds")


def log_writer(log_file):
    lock = threading.Lock()

    def write(line):
        with lock:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write(line + "\n")
            print(line, flush=True)

    return write


def heartbeat_sender(forward_host, forward_port, interval, write):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hostname = socket.gethostname()
    write(f"{now_iso()}\t[proxy]\theartbeat enabled, every {interval}s to {forward_host}:{forward_port}")
    while True:
        ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        msg = f"{hostname}: {ts}: Heartbeat"
        try:
            sock.sendto(msg.encode("utf-8"), (forward_host, forward_port))
        except OSError as e:
            write(f"{now_iso()}\t[proxy]\theartbeat error: {e}")
        time.sleep(interval)


def udp_listener(bind_host, bind_port, forward_host, forward_port, write):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((bind_host, bind_port))

    fwd_sock = None
    if forward_host:
        fwd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    write(f"{now_iso()}\t[proxy]\tlistening on udp/{bind_port}"
          + (f", forwarding to {forward_host}:{forward_port}" if forward_host else ""))

    while True:
        data, addr = sock.recvfrom(65535)
        src = f"{addr[0]}:{addr[1]}"
        msg = data.decode("utf-8", errors="replace").rstrip("\r\n")
        write(f"{now_iso()}\t{src}\t{msg}")
        if fwd_sock is not None:
            try:
                fwd_sock.sendto(data, (forward_host, forward_port))
            except OSError as e:
                write(f"{now_iso()}\t[proxy]\tforward error: {e}")


def start_webserver(web_port, log_file):
    from flask import Flask, request

    app = Flask(__name__)

    @app.route("/")
    def home():
        try:
            tail = int(request.args.get("tail", "500"))
        except ValueError:
            tail = 500

        lines = []
        if os.path.exists(log_file):
            with open(log_file, "r", encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        if tail > 0:
            lines = lines[-tail:]

        rows = []
        for line in lines:
            parts = line.rstrip("\n").split("\t", 2)
            if len(parts) == 3:
                ts, src, msg = parts
            elif len(parts) == 2:
                ts, src, msg = parts[0], "", parts[1]
            else:
                ts, src, msg = "", "", line.rstrip("\n")
            rows.append(
                f"<tr><td class='ts'>{html.escape(ts)}</td>"
                f"<td class='src'>{html.escape(src)}</td>"
                f"<td class='msg'>{html.escape(msg)}</td></tr>"
            )

        body = "\n".join(rows) if rows else "<tr><td colspan='3'><i>no log entries yet</i></td></tr>"

        return f"""
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <meta http-equiv="refresh" content="5">
    <title>Node Logging Proxy</title>
    <style>
        body {{ background:#0f172a; color:#e2e8f0; font-family:Menlo,Consolas,monospace; padding:20px; }}
        h1 {{ color:#38bdf8; margin:0 0 12px 0; }}
        .meta {{ color:#94a3b8; margin-bottom:16px; font-size:12px; }}
        table {{ width:100%; border-collapse:collapse; font-size:13px; }}
        th, td {{ text-align:left; padding:4px 8px; border-bottom:1px solid #1e293b; vertical-align:top; }}
        th {{ color:#38bdf8; position:sticky; top:0; background:#0f172a; }}
        td.ts {{ color:#94a3b8; white-space:nowrap; }}
        td.src {{ color:#facc15; white-space:nowrap; }}
        td.msg {{ color:#e2e8f0; word-break:break-word; }}
    </style>
</head>
<body>
    <h1>Node Logging Proxy</h1>
    <div class="meta">log file: {html.escape(log_file)} &middot; showing last {tail} entries &middot; auto-refresh 5s</div>
    <table>
        <thead><tr><th>timestamp</th><th>source</th><th>message</th></tr></thead>
        <tbody>
{body}
        </tbody>
    </table>
</body>
</html>
"""

    app.run(host="0.0.0.0", port=web_port, use_reloader=False)


def main():
    p = argparse.ArgumentParser(description="UDP log proxy with optional forwarding and web viewer.")
    p.add_argument("-p", "--port", type=int, default=1111,
                   help="UDP port to listen for incoming log messages [default: 1111]")
    p.add_argument("-b", "--bind", default="0.0.0.0",
                   help="UDP bind address [default: 0.0.0.0]")
    p.add_argument("-f", "--forward-host", default=None,
                   help="Forward received messages to this host (UDP). If unset, no forwarding.")
    p.add_argument("-o", "--forward-port", type=int, default=1111,
                   help="Forward UDP port [default: 1111]")
    p.add_argument("-w", "--web", action="store_true",
                   help="Enable web viewer to inspect received logs")
    p.add_argument("-W", "--web-port", type=int, default=80,
                   help="Web viewer port [default: 80]")
    p.add_argument("-l", "--log-file", default="/var/log/nodes.log",
                   help="File to append received log messages to [default: /var/log/nodes.log]")
    p.add_argument("-H", "--heartbeat-interval", type=int, default=10,
                   help="Seconds between heartbeats sent to the forward host [default: 10]. "
                        "Only active when --forward-host is set. Set to 0 to disable.")
    args = p.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.log_file)), exist_ok=True)
    write = log_writer(args.log_file)

    if args.web:
        t = threading.Thread(
            target=start_webserver,
            args=(args.web_port, args.log_file),
            daemon=True,
        )
        t.start()

    if args.forward_host and args.heartbeat_interval > 0:
        hb = threading.Thread(
            target=heartbeat_sender,
            args=(args.forward_host, args.forward_port, args.heartbeat_interval, write),
            daemon=True,
        )
        hb.start()

    try:
        udp_listener(args.bind, args.port, args.forward_host, args.forward_port, write)
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()
