#!/usr/bin/env python3
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
# Test helper: stuur een (of meer) UDP log-bericht(en) naar een node-logging-proxy
# endpoint. Geen externe dependencies.
#
# Voorbeelden:
#   python send-log.py "hello"
#       => stuurt "hello" naar 127.0.0.1:1111 (TX/guard local listener)
#
#   python send-log.py -H 10.10.20.2 "hello from outside"
#       => stuurt naar de high-side-rx-proxy
#
#   python send-log.py -c 5 -i 0.5 "burst test"
#       => stuurt 5 berichten met 0.5s ertussen, elk genummerd
#
#   echo "uit stdin" | python send-log.py -
#       => leest het bericht van stdin
#
import argparse
import socket
import sys
import time


def main():
    p = argparse.ArgumentParser(description="Stuur UDP log-bericht(en) naar een node-logging-proxy endpoint.")
    p.add_argument("message", help="Bericht om te sturen. Gebruik '-' om van stdin te lezen.")
    p.add_argument("-H", "--host", default="127.0.0.1",
                   help="Doel-host [default: 127.0.0.1]")
    p.add_argument("-p", "--port", type=int, default=1111,
                   help="Doel-UDP-poort [default: 1111]")
    p.add_argument("-c", "--count", type=int, default=1,
                   help="Aantal berichten om te sturen [default: 1]. "
                        "Bij >1 wordt de teller [n/total] toegevoegd.")
    p.add_argument("-i", "--interval", type=float, default=0.0,
                   help="Seconden tussen berichten [default: 0]")
    p.add_argument("-q", "--quiet", action="store_true",
                   help="Geen bevestiging op stdout")
    args = p.parse_args()

    if args.message == "-":
        message = sys.stdin.read().rstrip("\n")
    else:
        message = args.message

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (args.host, args.port)

    for n in range(1, args.count + 1):
        payload = message if args.count == 1 else f"{message} [{n}/{args.count}]"
        sock.sendto(payload.encode("utf-8"), target)
        if not args.quiet:
            print(f"sent -> {args.host}:{args.port}  {payload}")
        if n < args.count and args.interval > 0:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
