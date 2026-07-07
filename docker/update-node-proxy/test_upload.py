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
# Test client for update-node-proxy (sender mode).
#
# Uploads a file to PUT http://<host>:<port>/upload/<name>, streaming the body
# (so it can handle large files without loading them into memory) and computing
# a sha256 on the fly that it sends as X-Checksum-SHA256 so the proxy verifies
# the upload end-to-end.
#
# Example:
#   python test_upload.py ./flatcar-image.bin
#   python test_upload.py ./flatcar-image.bin --host 10.0.0.10 --port 8080
#   python test_upload.py --generate 100M /tmp/blob.bin
#
import argparse
import hashlib
import http.client
import os
import sys
import time
import urllib.parse


CHUNK = 1024 * 1024  # 1 MiB read/upload block


def parse_size(s):
    s = s.strip().upper()
    mult = 1
    if s.endswith("K"):
        mult, s = 1024, s[:-1]
    elif s.endswith("M"):
        mult, s = 1024 * 1024, s[:-1]
    elif s.endswith("G"):
        mult, s = 1024 * 1024 * 1024, s[:-1]
    return int(float(s) * mult)


def generate_file(path, size_bytes):
    print(f"generating {size_bytes} bytes of random data into {path}")
    written = 0
    with open(path, "wb") as f:
        while written < size_bytes:
            block = os.urandom(min(CHUNK, size_bytes - written))
            f.write(block)
            written += len(block)
    print(f"generated {written} bytes")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(CHUNK), b""):
            h.update(block)
    return h.hexdigest()


def human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} TB"


class ProgressReader:
    """File-like wrapper that prints upload progress and counts bytes."""

    def __init__(self, fp, total):
        self.fp = fp
        self.total = total
        self.sent = 0
        self.start = time.time()
        self.last_print = 0.0

    def read(self, n=-1):
        data = self.fp.read(n if n and n > 0 else CHUNK)
        if data:
            self.sent += len(data)
            now = time.time()
            if now - self.last_print >= 0.5 or self.sent == self.total:
                elapsed = max(now - self.start, 1e-6)
                rate = self.sent / elapsed
                pct = (self.sent / self.total) * 100 if self.total else 0
                sys.stdout.write(
                    f"\r  uploaded {human(self.sent)} / {human(self.total)} "
                    f"({pct:5.1f}%)  {human(rate)}/s"
                )
                sys.stdout.flush()
                self.last_print = now
        return data


def upload(host, port, name, path, send_checksum, method):
    size = os.path.getsize(path)
    print(f"target: http://{host}:{port}/upload/{name}")
    print(f"file:   {path}  ({human(size)})")

    sha = None
    if send_checksum:
        print("computing sha256...")
        t0 = time.time()
        sha = sha256_file(path)
        print(f"  sha256 = {sha}  ({time.time() - t0:.2f}s)")

    conn = http.client.HTTPConnection(host, port, timeout=600)
    headers = {
        "Content-Length": str(size),
        "Content-Type": "application/octet-stream",
    }
    if sha:
        headers["X-Checksum-SHA256"] = sha

    print("uploading...")
    t0 = time.time()
    with open(path, "rb") as f:
        body = ProgressReader(f, size)
        conn.request(method, "/upload/" + urllib.parse.quote(name), body=body, headers=headers)
        resp = conn.getresponse()
        resp_body = resp.read().decode("utf-8", errors="replace")
    elapsed = time.time() - t0
    print()
    print(f"http {resp.status} {resp.reason}  ({elapsed:.2f}s, "
          f"avg {human(size / max(elapsed, 1e-6))}/s)")
    if resp_body:
        print(f"response: {resp_body}")
    return resp.status


def main():
    p = argparse.ArgumentParser(description="Upload a file to update-node-proxy (sender mode).")
    p.add_argument("file", help="Path to the file to upload (or to generate, with --generate).")
    p.add_argument("--host", default="127.0.0.1", help="Sender host [default: 127.0.0.1]")
    p.add_argument("--port", type=int, default=8080, help="Sender HTTP port [default: 8080]")
    p.add_argument("--name", default=None,
                   help="Upload name on the proxy [default: basename of file]")
    p.add_argument("--method", choices=["PUT", "POST"], default="PUT",
                   help="HTTP method [default: PUT]")
    p.add_argument("--no-checksum", action="store_true",
                   help="Don't send X-Checksum-SHA256 header.")
    p.add_argument("--generate", default=None,
                   help="Generate a random file of this size at <file> before uploading "
                        "(e.g. 1K, 50M, 2G).")
    args = p.parse_args()

    if args.generate:
        generate_file(args.file, parse_size(args.generate))

    if not os.path.isfile(args.file):
        print(f"error: not a file: {args.file}", file=sys.stderr)
        sys.exit(2)

    name = args.name or os.path.basename(args.file)
    status = upload(args.host, args.port, name, args.file,
                    send_checksum=not args.no_checksum, method=args.method)
    sys.exit(0 if 200 <= status < 300 else 1)


if __name__ == "__main__":
    main()
