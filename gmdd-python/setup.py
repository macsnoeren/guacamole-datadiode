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
    if len(sys.argv) != 2:
        print(f"Usage: sudo {sys.argv[0]} <config.yml>")
        sys.exit(1)

    config_path = Path(sys.argv[1])

    if not config_path.exists():
        print(f"Config file not found: {config_path}")
        sys.exit(1)

    config = load_config(config_path)

    cur_system = config[config["current-node"]]
    neigh_system = config[config["next-node"]]

    in_iface = cur_system["in"]["iface"]
    in_ip = cur_system["in"]["ip"]
    interface = cur_system["out"]["iface"]
    ip_address = cur_system["out"]["ip"]
    neighbor_ip = neigh_system["in"]["ip"]
    neighbor_mac = neigh_system["in"]["mac"]

    print(f"=== Configuring interfaces: {in_iface}, {interface} ===")

    # Ensure interface exists
    in_result = subprocess.run(
        ["ip", "link", "show", in_iface],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if in_result.returncode != 0:
        print(f"Interface does not exist: {in_iface}")
        sys.exit(1)

    # Ensure interface exists
    result = subprocess.run(
        ["ip", "link", "show", interface],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        print(f"Interface does not exist: {interface}")
        sys.exit(1)

    # Bring interface up
    run(["ip", "link", "set", in_iface, "up"])
    run(["ip", "link", "set", interface, "up"])

    # Remove existing IPv4 addresses
    print("\n[1/4] Removing existing IPv4 addresses...")
    for addr in get_ipv4_addresses(interface):
        print(f"Removing address: {addr}")
        run(["ip", "addr", "del", addr, "dev", interface])
    for addr in get_ipv4_addresses(in_iface):
        print(f"Removing address: {addr}")
        run(["ip", "addr", "del", addr, "dev", in_iface])

    # Add new IPv4 address
    print("\n[2/4] Adding new IPv4 address...")
    print(f"Adding address: {ip_address}")
    run(["ip", "addr", "add", in_ip + "/24", "dev", in_iface])
    run(["ip", "addr", "add", ip_address + "/24", "dev", interface])

    # Remove existing neighbors
    print("\n[3/4] Removing existing neighbor entries...")
    for neigh in get_neighbors(interface):
        print(f"Removing neighbor: {neigh}")
        run(
            ["ip", "neigh", "del", neigh, "dev", interface],
            check=False,
        )

    # Add static neighbor
    print("\n[4/4] Adding static neighbor entry...")
    print(f"Neighbor IP : {neighbor_ip}")
    print(f"Neighbor MAC: {neighbor_mac}")

    run([
        "ip",
        "neigh",
        "add",
        neighbor_ip,
        "lladdr",
        neighbor_mac,
        "nud",
        "permanent",
        "dev",
        interface,
    ])

    # Show final config
    print("\n=== Final Interface Configuration ===")
    subprocess.run(["ip", "addr", "show", "dev", in_iface])
    subprocess.run(["ip", "addr", "show", "dev", interface])

    print("\n=== Final Neighbor Table ===")
    subprocess.run(["ip", "neigh", "show", "dev", interface])


if __name__ == "__main__":
    main()
