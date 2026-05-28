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
import hashlib
import html
import os
import socket
import struct
import sys
import threading
import time
import uuid


# ---------------------------------------------------------------------------
# Wire format (one datagram = one record)
#
#   header  : b'H' | file_id(16) | sha256(32) | total_chunks(4) | chunk_size(4)
#                  | name_len(2) | name(name_len)
#   chunk   : b'C' | file_id(16) | chunk_nr(4) | data(<=chunk_size)
#   end     : b'E' | file_id(16)
#
# Multi-byte integers are network byte order (big endian).
# ---------------------------------------------------------------------------

MAGIC_HEADER = b"H"
MAGIC_CHUNK = b"C"
MAGIC_END = b"E"

HEADER_FIXED_LEN = 1 + 16 + 32 + 4 + 4 + 2
CHUNK_FIXED_LEN = 1 + 16 + 4
END_LEN = 1 + 16


def now_iso():
    return datetime.datetime.now().isoformat(timespec="milliseconds")


def log_print(line):
    print(f"{now_iso()}\t{line}", flush=True)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.digest()


# ---------------------------------------------------------------------------
# Sender side
# ---------------------------------------------------------------------------

class SenderState:
    def __init__(self, staging_dir, sent_dir):
        self.staging_dir = staging_dir
        self.sent_dir = sent_dir
        self.lock = threading.Lock()
        # name -> dict(status, total_bytes, sent_bytes, started, finished)
        self.files = {}

    def upsert(self, name, **fields):
        with self.lock:
            entry = self.files.setdefault(name, {})
            entry.update(fields)

    def snapshot(self):
        with self.lock:
            return {k: dict(v) for k, v in self.files.items()}


def start_sender_http(state, upload_dir, http_port, max_upload_mb):
    from flask import Flask, request, abort

    app = Flask(__name__)
    app.config["MAX_CONTENT_LENGTH"] = max_upload_mb * 1024 * 1024

    @app.route("/upload/<path:name>", methods=["PUT", "POST"])
    def upload(name):
        safe = os.path.basename(name)
        if not safe or safe.startswith("."):
            abort(400, "invalid filename")

        partial = os.path.join(upload_dir, safe + ".partial")
        final = os.path.join(upload_dir, safe)
        sidecar = os.path.join(upload_dir, safe + ".sha256")

        expected_sha = request.headers.get("X-Checksum-SHA256", "").strip().lower() or None

        log_print(f"[sender]\tupload start name={safe}")
        state.upsert(safe, status="uploading", sent_bytes=0, total_bytes=None,
                     started=now_iso(), finished=None)

        h = hashlib.sha256()
        total = 0
        try:
            with open(partial, "wb") as f:
                while True:
                    chunk = request.stream.read(1024 * 1024)
                    if not chunk:
                        break
                    f.write(chunk)
                    h.update(chunk)
                    total += len(chunk)
        except Exception as e:
            try:
                os.remove(partial)
            except OSError:
                pass
            log_print(f"[sender]\tupload error name={safe} err={e}")
            abort(500, f"upload error: {e}")

        digest = h.hexdigest()
        if expected_sha and expected_sha != digest:
            os.remove(partial)
            log_print(f"[sender]\tchecksum mismatch name={safe} got={digest} expected={expected_sha}")
            abort(400, "checksum mismatch")

        os.rename(partial, final)
        with open(sidecar, "w", encoding="utf-8") as f:
            f.write(digest + "\n")

        state.upsert(safe, status="staged", total_bytes=total, sent_bytes=0, sha256=digest)
        log_print(f"[sender]\tupload done name={safe} bytes={total} sha256={digest}")
        return {"name": safe, "bytes": total, "sha256": digest}, 200

    @app.route("/")
    def home():
        return render_sender_page(state)

    log_print(f"[sender]\thttp listening on tcp/{http_port}, upload dir={upload_dir}")
    app.run(host="0.0.0.0", port=http_port, use_reloader=False, threaded=True)


def render_sender_page(state):
    rows = []
    for name, info in sorted(state.snapshot().items()):
        total = info.get("total_bytes")
        sent = info.get("sent_bytes", 0)
        pct = ""
        if total:
            pct = f"{(sent / total) * 100:.1f}%"
        rows.append(
            f"<tr><td>{html.escape(name)}</td>"
            f"<td>{html.escape(info.get('status', ''))}</td>"
            f"<td class='num'>{total if total is not None else ''}</td>"
            f"<td class='num'>{sent}</td>"
            f"<td class='num'>{pct}</td>"
            f"<td>{html.escape(info.get('started', ''))}</td>"
            f"<td>{html.escape(info.get('finished', ''))}</td>"
            f"<td class='sha'>{html.escape(info.get('sha256', ''))}</td></tr>"
        )
    body = "\n".join(rows) if rows else "<tr><td colspan='8'><i>no files yet</i></td></tr>"

    return f"""<!doctype html>
<html><head><meta charset="utf-8"><meta http-equiv="refresh" content="5">
<title>Update Node Proxy (sender)</title>
<style>
body {{ background:#0f172a; color:#e2e8f0; font-family:Menlo,Consolas,monospace; padding:20px; }}
h1 {{ color:#38bdf8; margin:0 0 12px 0; }}
.meta {{ color:#94a3b8; margin-bottom:16px; font-size:12px; }}
table {{ width:100%; border-collapse:collapse; font-size:13px; }}
th, td {{ text-align:left; padding:4px 8px; border-bottom:1px solid #1e293b; vertical-align:top; }}
th {{ color:#38bdf8; }}
td.num {{ text-align:right; color:#facc15; }}
td.sha {{ color:#64748b; font-size:11px; word-break:break-all; }}
</style></head>
<body>
<h1>Update Node Proxy &mdash; sender</h1>
<div class="meta">auto-refresh 5s</div>
<table>
<thead><tr><th>name</th><th>status</th><th>total</th><th>sent</th><th>%</th>
<th>started</th><th>finished</th><th>sha256</th></tr></thead>
<tbody>{body}</tbody>
</table></body></html>"""


def sender_loop(state, upload_dir, sent_dir, dd_host, dd_port,
                chunk_size, oversend_data, oversend_meta, inter_packet_us, scan_interval):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    log_print(f"[sender]\tudp sending to {dd_host}:{dd_port} chunk_size={chunk_size} "
              f"oversend_data={oversend_data} oversend_meta={oversend_meta} "
              f"inter_packet_us={inter_packet_us}")

    while True:
        try:
            send_pending_files(state, sock, upload_dir, sent_dir, dd_host, dd_port,
                               chunk_size, oversend_data, oversend_meta, inter_packet_us)
        except Exception as e:
            log_print(f"[sender]\tscan error: {e}")
        time.sleep(scan_interval)


def send_pending_files(state, sock, upload_dir, sent_dir, dd_host, dd_port,
                       chunk_size, oversend_data, oversend_meta, inter_packet_us):
    # A staged file is one that has a .sha256 sidecar but no .partial and no .sending marker.
    for entry in sorted(os.listdir(upload_dir)):
        if entry.endswith(".partial") or entry.endswith(".sha256") or entry.endswith(".sending"):
            continue
        full = os.path.join(upload_dir, entry)
        if not os.path.isfile(full):
            continue
        sidecar = full + ".sha256"
        if not os.path.exists(sidecar):
            # upload still in progress, no sidecar yet
            continue
        marker = full + ".sending"
        try:
            # mark it; if marker already exists we skip
            fd = os.open(marker, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.close(fd)
        except FileExistsError:
            continue
        try:
            send_one_file(state, sock, full, sidecar, dd_host, dd_port,
                          chunk_size, oversend_data, oversend_meta, inter_packet_us)
            os.makedirs(sent_dir, exist_ok=True)
            os.rename(full, os.path.join(sent_dir, entry))
            os.rename(sidecar, os.path.join(sent_dir, entry + ".sha256"))
        finally:
            try:
                os.remove(marker)
            except OSError:
                pass


def send_one_file(state, sock, path, sidecar_path, dd_host, dd_port,
                  chunk_size, oversend_data, oversend_meta, inter_packet_us):
    name = os.path.basename(path)
    size = os.path.getsize(path)
    with open(sidecar_path, "r", encoding="utf-8") as f:
        sha_hex = f.read().strip().split()[0]
    sha = bytes.fromhex(sha_hex)
    total_chunks = max(1, (size + chunk_size - 1) // chunk_size)
    file_id = uuid.uuid4().bytes
    name_bytes = name.encode("utf-8")
    if len(name_bytes) > 0xFFFF:
        raise ValueError("filename too long")

    header = (MAGIC_HEADER + file_id + sha +
              struct.pack("!II", total_chunks, chunk_size) +
              struct.pack("!H", len(name_bytes)) + name_bytes)
    end = MAGIC_END + file_id

    state.upsert(name, status="sending", total_bytes=size, sent_bytes=0,
                 sha256=sha_hex, started=now_iso(), finished=None)
    log_print(f"[sender]\tsend start name={name} bytes={size} chunks={total_chunks} "
              f"file_id={file_id.hex()}")

    sleep_s = inter_packet_us / 1_000_000.0 if inter_packet_us > 0 else 0

    for _ in range(oversend_meta):
        sock.sendto(header, (dd_host, dd_port))
        if sleep_s:
            time.sleep(sleep_s)

    with open(path, "rb") as f:
        for chunk_nr in range(total_chunks):
            data = f.read(chunk_size)
            packet = MAGIC_CHUNK + file_id + struct.pack("!I", chunk_nr) + data
            for _ in range(oversend_data):
                sock.sendto(packet, (dd_host, dd_port))
                if sleep_s:
                    time.sleep(sleep_s)
            state.upsert(name, sent_bytes=(chunk_nr + 1) * chunk_size
                         if (chunk_nr + 1) * chunk_size < size else size)

    for _ in range(oversend_meta):
        sock.sendto(end, (dd_host, dd_port))
        if sleep_s:
            time.sleep(sleep_s)

    state.upsert(name, status="sent", sent_bytes=size, finished=now_iso())
    log_print(f"[sender]\tsend done name={name} bytes={size}")


# ---------------------------------------------------------------------------
# Receiver side
# ---------------------------------------------------------------------------

class ReceiverState:
    def __init__(self, inbox_dir):
        self.inbox_dir = inbox_dir
        self.lock = threading.Lock()
        # file_id -> dict(name, total_chunks, chunk_size, sha256_hex, received_chunks(set),
        #                 last_packet_ts, status)
        self.files = {}

    def get_or_init(self, file_id, **fields):
        with self.lock:
            entry = self.files.get(file_id)
            if entry is None:
                entry = {"received_chunks": set(), "status": "receiving",
                         "first_packet_ts": time.time()}
                self.files[file_id] = entry
            entry.update(fields)
            entry["last_packet_ts"] = time.time()
            return entry

    def update(self, file_id, **fields):
        with self.lock:
            entry = self.files.get(file_id)
            if entry is not None:
                entry.update(fields)
                entry["last_packet_ts"] = time.time()
            return entry

    def remove(self, file_id):
        with self.lock:
            return self.files.pop(file_id, None)

    def snapshot(self):
        with self.lock:
            out = {}
            for fid, info in self.files.items():
                cp = dict(info)
                cp["received_count"] = len(info.get("received_chunks", ()))
                cp.pop("received_chunks", None)
                out[fid.hex()] = cp
            return out


def receiver_loop(state, bind_host, port, idle_timeout):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16 * 1024 * 1024)
    except OSError:
        pass
    sock.bind((bind_host, port))
    log_print(f"[receiver]\tudp listening on {bind_host}:{port}, inbox={state.inbox_dir}")

    sock.settimeout(1.0)
    while True:
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            sweep_idle(state, idle_timeout)
            continue
        try:
            handle_packet(state, data)
        except Exception as e:
            log_print(f"[receiver]\tpacket error: {e}")


def handle_packet(state, data):
    if not data:
        return
    kind = data[0:1]

    if kind == MAGIC_HEADER:
        if len(data) < HEADER_FIXED_LEN:
            return
        file_id = data[1:17]
        sha = data[17:49]
        total_chunks, chunk_size = struct.unpack("!II", data[49:57])
        name_len, = struct.unpack("!H", data[57:59])
        if len(data) < HEADER_FIXED_LEN + name_len:
            return
        name = data[59:59 + name_len].decode("utf-8", errors="replace")
        safe = os.path.basename(name) or file_id.hex()
        part_dir = os.path.join(state.inbox_dir, "." + file_id.hex() + ".parts")
        os.makedirs(part_dir, exist_ok=True)
        entry = state.get_or_init(file_id, name=safe, total_chunks=total_chunks,
                                  chunk_size=chunk_size, sha256_hex=sha.hex(),
                                  part_dir=part_dir)
        if entry.get("status") == "receiving" and "announced" not in entry:
            entry["announced"] = True
            log_print(f"[receiver]\thdr name={safe} file_id={file_id.hex()} "
                      f"chunks={total_chunks} chunk_size={chunk_size} sha256={sha.hex()}")
        return

    if kind == MAGIC_CHUNK:
        if len(data) < CHUNK_FIXED_LEN:
            return
        file_id = data[1:17]
        chunk_nr, = struct.unpack("!I", data[17:21])
        payload = data[21:]
        entry = state.files.get(file_id)
        if entry is None:
            # chunk arrived before header; stash bytes on disk so the header can pick them up
            part_dir = os.path.join(state.inbox_dir, "." + file_id.hex() + ".parts")
            os.makedirs(part_dir, exist_ok=True)
            entry = state.get_or_init(file_id, name=None, total_chunks=None,
                                      chunk_size=None, sha256_hex=None,
                                      part_dir=part_dir)
        part_path = os.path.join(entry["part_dir"], f"{chunk_nr:08d}")
        if chunk_nr not in entry["received_chunks"]:
            with open(part_path, "wb") as f:
                f.write(payload)
            entry["received_chunks"].add(chunk_nr)
        state.update(file_id)
        try_finalize(state, file_id)
        return

    if kind == MAGIC_END:
        if len(data) < END_LEN:
            return
        file_id = data[1:17]
        try_finalize(state, file_id)
        return


def try_finalize(state, file_id):
    entry = state.files.get(file_id)
    if entry is None:
        return
    if entry.get("status") != "receiving":
        return
    total = entry.get("total_chunks")
    if total is None:
        return
    if len(entry["received_chunks"]) < total:
        return
    name = entry.get("name") or file_id.hex()
    final = os.path.join(state.inbox_dir, name)
    sha_path = final + ".sha256"
    done_path = final + ".done"

    h = hashlib.sha256()
    try:
        with open(final, "wb") as out:
            for nr in range(total):
                part_path = os.path.join(entry["part_dir"], f"{nr:08d}")
                with open(part_path, "rb") as pf:
                    block = pf.read()
                out.write(block)
                h.update(block)
    except OSError as e:
        log_print(f"[receiver]\tassemble error name={name}: {e}")
        entry["status"] = "error"
        return

    digest = h.hexdigest()
    expected = entry.get("sha256_hex")
    if expected and digest != expected:
        log_print(f"[receiver]\tsha mismatch name={name} got={digest} expected={expected}")
        try:
            os.remove(final)
        except OSError:
            pass
        entry["status"] = "sha_mismatch"
        cleanup_parts(entry)
        return

    with open(sha_path, "w", encoding="utf-8") as f:
        f.write(digest + "\n")
    with open(done_path, "w", encoding="utf-8") as f:
        f.write(now_iso() + "\n")

    entry["status"] = "complete"
    entry["finished_ts"] = time.time()
    log_print(f"[receiver]\tcomplete name={name} bytes={os.path.getsize(final)} sha256={digest}")
    cleanup_parts(entry)


def cleanup_parts(entry):
    pd = entry.get("part_dir")
    if not pd or not os.path.isdir(pd):
        return
    try:
        for f in os.listdir(pd):
            try:
                os.remove(os.path.join(pd, f))
            except OSError:
                pass
        os.rmdir(pd)
    except OSError:
        pass


def sweep_idle(state, idle_timeout):
    now = time.time()
    drop = []
    with state.lock:
        for fid, entry in state.files.items():
            if entry.get("status") == "complete":
                if now - entry.get("finished_ts", now) > 3600:
                    drop.append(fid)
                continue
            if now - entry.get("last_packet_ts", now) > idle_timeout:
                if entry.get("total_chunks") and len(entry["received_chunks"]) >= entry["total_chunks"]:
                    continue
                drop.append(fid)
    for fid in drop:
        entry = state.remove(fid)
        if entry is None:
            continue
        log_print(f"[receiver]\tdrop incomplete file_id={fid.hex()} "
                  f"name={entry.get('name')} received={len(entry.get('received_chunks', ()))}"
                  f"/{entry.get('total_chunks')}")
        cleanup_parts(entry)


def start_receiver_http(state, http_port):
    from flask import Flask

    app = Flask(__name__)

    @app.route("/")
    def home():
        return render_receiver_page(state)

    log_print(f"[receiver]\thttp viewer on tcp/{http_port}")
    app.run(host="0.0.0.0", port=http_port, use_reloader=False, threaded=True)


def render_receiver_page(state):
    in_progress_rows = []
    for fid, info in sorted(state.snapshot().items()):
        if info.get("status") == "complete":
            continue
        total = info.get("total_chunks")
        recv = info.get("received_count", 0)
        pct = f"{(recv / total) * 100:.1f}%" if total else ""
        in_progress_rows.append(
            f"<tr><td>{html.escape(info.get('name') or '')}</td>"
            f"<td>{html.escape(info.get('status', ''))}</td>"
            f"<td class='num'>{recv}</td>"
            f"<td class='num'>{total if total is not None else ''}</td>"
            f"<td class='num'>{pct}</td>"
            f"<td class='sha'>{html.escape(fid)}</td></tr>"
        )

    completed_rows = []
    try:
        entries = os.listdir(state.inbox_dir)
    except OSError:
        entries = []
    done_files = [e[:-5] for e in entries if e.endswith(".done")]
    for name in sorted(done_files):
        path = os.path.join(state.inbox_dir, name)
        size = os.path.getsize(path) if os.path.exists(path) else 0
        sha = ""
        sha_path = path + ".sha256"
        if os.path.exists(sha_path):
            with open(sha_path, "r", encoding="utf-8") as f:
                sha = f.read().strip().split()[0]
        completed_rows.append(
            f"<tr><td>{html.escape(name)}</td>"
            f"<td class='num'>{size}</td>"
            f"<td class='sha'>{html.escape(sha)}</td></tr>"
        )

    body_in = "\n".join(in_progress_rows) if in_progress_rows else \
        "<tr><td colspan='6'><i>nothing in flight</i></td></tr>"
    body_done = "\n".join(completed_rows) if completed_rows else \
        "<tr><td colspan='3'><i>no completed files</i></td></tr>"

    return f"""<!doctype html>
<html><head><meta charset="utf-8"><meta http-equiv="refresh" content="5">
<title>Update Node Proxy (receiver)</title>
<style>
body {{ background:#0f172a; color:#e2e8f0; font-family:Menlo,Consolas,monospace; padding:20px; }}
h1, h2 {{ color:#38bdf8; margin:0 0 12px 0; }}
.meta {{ color:#94a3b8; margin-bottom:16px; font-size:12px; }}
table {{ width:100%; border-collapse:collapse; font-size:13px; margin-bottom:24px; }}
th, td {{ text-align:left; padding:4px 8px; border-bottom:1px solid #1e293b; vertical-align:top; }}
th {{ color:#38bdf8; }}
td.num {{ text-align:right; color:#facc15; }}
td.sha {{ color:#64748b; font-size:11px; word-break:break-all; }}
</style></head>
<body>
<h1>Update Node Proxy &mdash; receiver</h1>
<div class="meta">inbox: {html.escape(state.inbox_dir)} &middot; auto-refresh 5s</div>
<h2>in flight</h2>
<table><thead><tr><th>name</th><th>status</th><th>received</th><th>total</th><th>%</th>
<th>file_id</th></tr></thead><tbody>{body_in}</tbody></table>
<h2>completed</h2>
<table><thead><tr><th>name</th><th>bytes</th><th>sha256</th></tr></thead>
<tbody>{body_done}</tbody></table>
</body></html>"""


# ---------------------------------------------------------------------------
# Relay side (pure UDP forwarder, no parsing)
# ---------------------------------------------------------------------------

def relay_loop(bind_host, listen_port, forward_host, forward_port):
    in_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    in_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        in_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16 * 1024 * 1024)
    except OSError:
        pass
    in_sock.bind((bind_host, listen_port))
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    log_print(f"[relay]\tudp {bind_host}:{listen_port} -> {forward_host}:{forward_port}")

    pkt_count = 0
    byte_count = 0
    next_report = time.time() + 10.0
    while True:
        data, _ = in_sock.recvfrom(65535)
        try:
            out_sock.sendto(data, (forward_host, forward_port))
        except OSError as e:
            log_print(f"[relay]\tforward error: {e}")
            continue
        pkt_count += 1
        byte_count += len(data)
        now = time.time()
        if now >= next_report:
            log_print(f"[relay]\tforwarded {pkt_count} packets, {byte_count} bytes total")
            next_report = now + 10.0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="Update node proxy: HTTP upload -> UDP -> reassemble.")
    p.add_argument("-m", "--mode", choices=["sender", "receiver", "relay"], required=True,
                   help="Run as sender (HTTP intake + UDP out), receiver (UDP in + assemble), "
                        "or relay (UDP in + UDP out, no parsing).")

    # shared
    p.add_argument("-W", "--web-port", type=int, default=80,
                   help="HTTP port for the web UI / upload endpoint [default: 80]")

    # sender
    p.add_argument("-u", "--upload-dir", default="/var/lib/update-proxy/staging",
                   help="Staging directory for uploaded files [sender] [default: /var/lib/update-proxy/staging]")
    p.add_argument("-S", "--sent-dir", default="/var/lib/update-proxy/sent",
                   help="Directory to move files to after successful UDP send [sender] [default: /var/lib/update-proxy/sent]")
    p.add_argument("-d", "--dd-host", default=None,
                   help="UDP destination host (gmproxyin / data-diode in) [sender]")
    p.add_argument("-o", "--dd-port", type=int, default=40000,
                   help="UDP destination port [sender] [default: 40000]")
    p.add_argument("-c", "--chunk-size", type=int, default=1200,
                   help="UDP chunk payload size in bytes [sender] [default: 1200]")
    p.add_argument("--oversend-data", type=int, default=2,
                   help="How many times each data chunk is sent [sender] [default: 2]")
    p.add_argument("--oversend-meta", type=int, default=5,
                   help="How many times header/end records are sent [sender] [default: 5]")
    p.add_argument("--inter-packet-us", type=int, default=200,
                   help="Sleep between packets in microseconds, 0 disables [sender] [default: 200]")
    p.add_argument("--scan-interval", type=float, default=1.0,
                   help="Seconds between staging-dir scans [sender] [default: 1.0]")
    p.add_argument("--max-upload-mb", type=int, default=8192,
                   help="Max upload size in MB [sender] [default: 8192]")

    # receiver
    p.add_argument("-i", "--inbox-dir", default="/var/lib/update-proxy/inbox",
                   help="Directory where completed files land [receiver] [default: /var/lib/update-proxy/inbox]")
    p.add_argument("-p", "--port", type=int, default=40000,
                   help="UDP port to listen on [receiver] [default: 40000]")
    p.add_argument("-b", "--bind", default="0.0.0.0",
                   help="UDP bind address [receiver] [default: 0.0.0.0]")
    p.add_argument("--idle-timeout", type=int, default=300,
                   help="Seconds before incomplete files are dropped [receiver] [default: 300]")

    args = p.parse_args()

    if args.mode == "sender":
        if not args.dd_host:
            print("error: --dd-host is required in sender mode", file=sys.stderr)
            sys.exit(2)
        os.makedirs(args.upload_dir, exist_ok=True)
        os.makedirs(args.sent_dir, exist_ok=True)
        state = SenderState(args.upload_dir, args.sent_dir)

        t_http = threading.Thread(
            target=start_sender_http,
            args=(state, args.upload_dir, args.web_port, args.max_upload_mb),
            daemon=True,
        )
        t_http.start()

        try:
            sender_loop(state, args.upload_dir, args.sent_dir,
                        args.dd_host, args.dd_port,
                        args.chunk_size, args.oversend_data, args.oversend_meta,
                        args.inter_packet_us, args.scan_interval)
        except KeyboardInterrupt:
            sys.exit(0)

    elif args.mode == "receiver":
        os.makedirs(args.inbox_dir, exist_ok=True)
        state = ReceiverState(args.inbox_dir)

        t_http = threading.Thread(
            target=start_receiver_http,
            args=(state, args.web_port),
            daemon=True,
        )
        t_http.start()

        try:
            receiver_loop(state, args.bind, args.port, args.idle_timeout)
        except KeyboardInterrupt:
            sys.exit(0)

    else:  # relay
        if not args.dd_host:
            print("error: --dd-host is required in relay mode", file=sys.stderr)
            sys.exit(2)
        try:
            relay_loop(args.bind, args.port, args.dd_host, args.dd_port)
        except KeyboardInterrupt:
            sys.exit(0)


if __name__ == "__main__":
    main()
