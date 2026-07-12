#!/usr/bin/env python3

# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

import socket
import threading
import argparse
import sys

DATA_PREVIEW_CUTOFF = 4096

def forward_tcp_to_udp(tcp_sock, udp_sock, udp_dst):
    try:
        while True:
            data = tcp_sock.recv(4096)
            if not data:
                break

            data_preview = data.decode()
            if len(data_preview) > DATA_PREVIEW_CUTOFF * 2:
                data_preview = data_preview[:20] + "~~~" + data_preview[len(data_preview)-20:len(data_preview)]

            print(f"[TCP->UDP] {len(data)} bytes to {udp_dst}:\n\t{data_preview}")
            udp_sock.sendto(data, udp_dst)
    except Exception as e:
        print(f"[TCP->UDP] Error: {e}", file=sys.stderr)
    finally:
        try:
            tcp_sock.shutdown(socket.SHUT_RD)
        except Exception:
            pass
        print("[TCP->UDP] Stopped")

def forward_udp_to_tcp(udp_sock, tcp_sock):
    try:
        while True:
            data, addr = udp_sock.recvfrom(65535)
            if not data:
                continue

            data_preview = data.decode()
            if len(data_preview) > DATA_PREVIEW_CUTOFF * 2:
                data_preview = data_preview[:20] + "~~~" + data_preview[len(data_preview)-20:len(data_preview)]

            print(f"[UDP->TCP] {len(data)} bytes to {addr}:\n\t{data_preview}")
            tcp_sock.sendall(data)
    except Exception as e:
        print(f"[UDP->TCP] Error: {e}", file=sys.stderr)
    finally:
        try:
            tcp_sock.shutdown(socket.SHUT_WR)
        except Exception:
            pass
        print("[UDP->TCP] Stopped")

# Bind sockets and start handler threads
def handle_client(client_sock, udp_out_ip, udp_out_port, udp_in_ip, udp_in_port):
    print(f"[*] New TCP client from {client_sock.getpeername()}")

    udp_out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    udp_in_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_in_sock.bind((udp_in_ip, udp_in_port))
    print(f"[*] Bound UDP-in (from B) on {(udp_in_ip, udp_in_port)}")

    udp_dst = (udp_out_ip, udp_out_port)

    t1 = threading.Thread(target=forward_tcp_to_udp,
                          args=(client_sock, udp_out_sock, udp_dst),
                          daemon=True)
    t2 = threading.Thread(target=forward_udp_to_tcp,
                          args=(udp_in_sock, client_sock),
                          daemon=True)

    t1.start()
    t2.start()

    t1.join()
    udp_in_sock.close()
    t2.join()

    client_sock.close()
    udp_out_sock.close()
    print("[*] TCP client closed")

def main():
    parser = argparse.ArgumentParser(
        description="TCP<->UDP bridge acting as guacd for Guacamole (Side A)."
    )
    parser.add_argument("--listen-ip", default="0.0.0.0",
                        help="IP to listen for Guacamole TCP (default: 0.0.0.0)")
    parser.add_argument("--listen-port", type=int, default=4822,
                        help="TCP port to listen for Guacamole (default: 4822)")
    parser.add_argument("--udp-out-ip", required=True,
                        help="Destination IP for UDP towards guacd side (Side B)")
    parser.add_argument("--udp-out-port", type=int, default=5501,
                        help="Destination UDP port towards Side B (default: 5501)")
    parser.add_argument("--udp-in-ip", default="0.0.0.0",
                        help="Local IP to bind for UDP responses from Side B (default: 0.0.0.0)")
    parser.add_argument("--udp-in-port", type=int, default=5601,
                        help="Local UDP port to bind for responses from Side B (default: 5601)")
    args = parser.parse_args()

    srv_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv_sock.bind((args.listen_ip, args.listen_port))
    srv_sock.listen(1)
    print(f"[*] Listening for Guacamole TCP on {(args.listen_ip, args.listen_port)}")
    print(f"[*] UDP out to {(args.udp_out_ip, args.udp_out_port)}; UDP in on {(args.udp_in_ip, args.udp_in_port)}")

    try:
        while True:
            client_sock, addr = srv_sock.accept()
            handle_client(client_sock,
                          args.udp_out_ip, args.udp_out_port,
                          args.udp_in_ip, args.udp_in_port)
    except KeyboardInterrupt:
        print("\n[!] Shutting down bridge...")
    finally:
        srv_sock.close()

if __name__ == "__main__":
    main()
