#!/bin/bash
set -e

# Clean any stale runtime state so a restarted container comes up cleanly.
mkdir -p /var/run/xrdp /run/dbus
rm -f /var/run/xrdp/xrdp*.pid 2>/dev/null || true

# A system bus keeps the xfce session from stalling on dbus.
dbus-daemon --system --fork 2>/dev/null || true

# Session manager in the background; the RDP listener stays in the foreground so
# it is PID 1 and receives SIGTERM on `docker compose down`/`stop`.
/usr/sbin/xrdp-sesman --nodaemon &
exec /usr/sbin/xrdp --nodaemon
