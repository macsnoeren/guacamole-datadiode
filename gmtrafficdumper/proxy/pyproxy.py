"""
The code forwards traffic between ports 4823 and 4822, and logs the TCP message payloads as text. Note: This file was generated with an LLM.
"""

import socket
import threading
import signal
import sys
import base64
from datetime import datetime

LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 4823

GUACD_HOST = "guacd"
GUACD_PORT = 4822

FILE_TIMESTAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
LOG_FILE = f"/logs/trafficlog-{FILE_TIMESTAMP}.log"

shutdown_event = threading.Event()
threads = []


def log(direction, data):
    with open(LOG_FILE, "a") as f:
        timestamp = datetime.now().isoformat()

        text = data.decode(errors='ignore')
        if "blob" in text:
            try:
                parts = text.split(',')
                if len(parts) >= 3:
                    decoded = base64.b64decode(parts[2]).decode(errors='ignore')
                    f.write(f"{direction} (Text decoded): {decoded}\n")
            except:
                pass

        f.write(f"[{timestamp}] {direction}: {data.decode(errors='ignore')}\n")

def forward(source, destination, direction):
    try:
        while not shutdown_event.is_set():
            data = source.recv(4096)
            if not data:
                break
            log(direction, data)
            destination.sendall(data)
    except Exception:
        pass
    finally:
        try:
            source.close()
        except:
            pass
        try:
            destination.close()
        except:
            pass


def handle_client(client_socket):
    try:
        guacd_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        guacd_socket.connect((GUACD_HOST, GUACD_PORT))

        t1 = threading.Thread(
            target=forward,
            args=(client_socket, guacd_socket, "client -> guacd"),
            daemon=True
        )
        t2 = threading.Thread(
            target=forward,
            args=(guacd_socket, client_socket, "guacd -> client"),
            daemon=True
        )

        t1.start()
        t2.start()

        threads.extend([t1, t2])

    except Exception as e:
        print(f"Failed to connect to guacd: {e}")
        client_socket.close()


def signal_handler(signum, frame):
    print(f"\n[!] Received signal {signum}, shutting down...")
    shutdown_event.set()


def main():
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    server.bind((LISTEN_HOST, LISTEN_PORT))
    server.listen(5)
    server.settimeout(1.0)  # allows periodic shutdown check

    print(f"[*] Listening on {LISTEN_HOST}:{LISTEN_PORT}")

    try:
        while not shutdown_event.is_set():
            try:
                client_socket, addr = server.accept()
                print(f"[+] Connection from {addr}")

                t = threading.Thread(
                    target=handle_client,
                    args=(client_socket,),
                    daemon=True
                )
                t.start()
                threads.append(t)

            except socket.timeout:
                continue

    finally:
        print("[*] Closing server socket...")
        server.close()

        print("[*] Waiting for threads to finish...")
        for t in threads:
            t.join(timeout=2)

        print("[*] Shutdown complete.")
        sys.exit(0)


if __name__ == "__main__":
    main()
