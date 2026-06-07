#!/usr/bin/env python3

"""
Configure a Linux Ethernet interface using values from a YAML file.

Features:
- Removes all existing IPv4 addresses from the interface
- Adds a new IPv4 address
- Removes all existing neighbor entries on the interface
- Adds a permanent static neighbor entry

Requires:
    pip install pyyaml

Usage:
    sudo python3 configure_iface.py config.yml
"""

import subprocess
import sys
import yaml
from pathlib import Path

def run(cmd, check=True):
    print(f"+ {' '.join(cmd)}")
    return subprocess.run(cmd, check=check, text=True, capture_output=True)


def get_ipv4_addresses(interface):
    result = run(
        ["ip", "-4", "-o", "addr", "show", "dev", interface],
        check=False,
    )

    addrs = []

    for line in result.stdout.strip().splitlines():
        parts = line.split()
        if len(parts) >= 4:
            addrs.append(parts[3])

    return addrs


def get_neighbors(interface):
    result = run(
        ["ip", "neigh", "show", "dev", interface],
        check=False,
    )

    neighs = []

    for line in result.stdout.strip().splitlines():
        parts = line.split()
        if len(parts) >= 1:
            neighs.append(parts[0])

    return neighs


def load_config(path):
    with open(path, "r") as f:
        return yaml.safe_load(f)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: sudo {sys.argv[0]} <config.yml>")
        sys.exit(1)

    config_path = Path(sys.argv[1])

    if not config_path.exists():
        print(f"Config file not found: {config_path}")
        sys.exit(1)

    config = load_config(config_path)

    iface_in = config["iface-in"]
    in_name = iface_in["name"]
    in_addrs = iface_in["ip-addrs"]
    
    iface_out = config["iface-out"]
    out_name = iface_out["name"]
    out_addrs = iface_out["ip-addrs"]

    # Ensure interface exists
    in_result = subprocess.run(
        ["ip", "link", "show", in_name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if in_result.returncode != 0:
        print(f"Interface does not exist: {in_name}")
        sys.exit(1)

    # Ensure interface exists
    result = subprocess.run(
        ["ip", "link", "show", out_name],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        print(f"Interface does not exist: {out_name}")
        sys.exit(1)

    # Bring interface up
    run(["ip", "link", "set", in_name, "up"])
    run(["ip", "link", "set", out_name, "up"])

    # Remove existing IPv4 addresses
    print("\n[1/2] Removing existing IPv4 addresses...")
    for addr in get_ipv4_addresses(in_name):
        print(f"Removing address: {addr}")
        run(["ip", "addr", "del", addr, "dev", in_name])
    for addr in get_ipv4_addresses(out_name):
        print(f"Removing address: {addr}")
        run(["ip", "addr", "del", addr, "dev", out_name])

    if (len(sys.argv) <= 2 or sys.argv[2] != "--rm"):
        # Add new IPv4 address
        print("\n[2/2] Adding new IPv4 addresses...")
        for addr in in_addrs:
            print(f"Adding address: {addr}")
            run(["ip", "addr", "add", addr + "/24", "dev", in_name])

        for addr in out_addrs:
            print(f"Adding address: {addr}")
            run(["ip", "addr", "add", addr + "/24", "dev", out_name])


    # Show final config
    print("\n=== Final Interface Configuration ===")
    subprocess.run(["ip", "addr", "show", "dev", in_name])
    subprocess.run(["ip", "addr", "show", "dev", out_name])


if __name__ == "__main__":
    main()
