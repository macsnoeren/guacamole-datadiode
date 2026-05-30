#!/usr/bin/env python3
import socket
import threading
import argparse
import sys
import time

DATA_PREVIEW_CUTOFF = 4096

def forward_udp_to_tcp(udp_sock, tcp_sock):
    try:
        while True:
            data, addr = udp_sock.recvfrom(65535)
            if not data:
                continue

            data_preview = data.decode()
            if len(data_preview) > DATA_PREVIEW_CUTOFF * 2:
                data_preview = data_preview[:20] + "~~~" + data_preview[len(data_preview)-20:len(data_preview)]

            print(f"[UDP->TCP] {len(data)} bytes from {addr}:\n\t{data_preview}")
            tcp_sock.sendall(data)
    except Exception as e:
        print(f"[UDP->TCP] Error: {e}", file=sys.stderr)
    finally:
        try:
            tcp_sock.shutdown(socket.SHUT_WR)
        except Exception:
            pass
        print("[UDP->TCP] Stopped")

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
        print("[TCP->UDP] Stopped")

def main():
    parser = argparse.ArgumentParser(
        description="UDP<->TCP bridge acting as Guacamole client-side for guacd (Side B)."
    )
    parser.add_argument("--udp-in-ip", default="0.0.0.0",
                        help="IP to bind for incoming UDP from Side A (default: 0.0.0.0)")
    parser.add_argument("--udp-in-port", type=int, default=5501,
                        help="UDP port to listen for data from Side A (default: 5501)")
    parser.add_argument("--udp-out-ip", required=True,
                        help="Destination IP for UDP back to Side A")
    parser.add_argument("--udp-out-port", type=int, default=5601,
                        help="Destination UDP port back to Side A (default: 5601)")
    parser.add_argument("--guacd-ip", default="guacd",
                        help="Hostname/IP of guacd (default: guacd in docker)")
    parser.add_argument("--guacd-port", type=int, default=4822,
                        help="TCP port of guacd (default: 4822)")
    args = parser.parse_args()

    # Connect to guacd
    while True:
        try:
            tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            tcp_sock.connect((args.guacd_ip, args.guacd_port))
            print(f"[*] Connected to guacd at {(args.guacd_ip, args.guacd_port)}")
            break
        except Exception as e:
            print(f"[!] Failed to connect to guacd: {e}. Retrying in 2s...", file=sys.stderr)
            time.sleep(2)

    udp_in_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_in_sock.bind((args.udp_in_ip, args.udp_in_port))
    print(f"[*] Listening for UDP from Side A on {(args.udp_in_ip, args.udp_in_port)}")

    udp_out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_dst = (args.udp_out_ip, args.udp_out_port)
    print(f"[*] UDP out back to Side A at {udp_dst}")

    t1 = threading.Thread(target=forward_udp_to_tcp,
                          args=(udp_in_sock, tcp_sock),
                          daemon=True)
    t2 = threading.Thread(target=forward_tcp_to_udp,
                          args=(tcp_sock, udp_out_sock, udp_dst),
                          daemon=True)

    t1.start()
    t2.start()

    try:
        t1.join()
        t2.join()
    except KeyboardInterrupt:
        print("\n[!] Bridge stopping...")
    finally:
        udp_in_sock.close()
        udp_out_sock.close()
        tcp_sock.close()

if __name__ == "__main__":
    main()
