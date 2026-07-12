# Guacamole Data Diode - Secure remote access using the Guacamole remote access using data-diodes.
# Copyright (C) 2020-2026  Maurice Snoeren, Simon de Cock
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

"""Guacamole wire-protocol client helpers for the nettest flows.

Plain stdlib. The instruction parser is deliberately strict: nettest exists to
test gmlbroker's output, so a malformed byte is a finding (ProtocolError), not
something to skip over. Element lengths count characters, not bytes, so the
stream is decoded as UTF-8 before slicing.
"""

import codecs
import socket
import time

# Matches the guard's element cap; anything bigger is not legitimate traffic.
MAX_ELEMENT = 8192


class ProtocolError(Exception):
    """The peer sent bytes that are not valid Guacamole wire protocol."""


def encode(*elems):
    """Encode one instruction to the Guacamole wire format.

    Parameters
    ----------
    *elems : str
        The instruction's opcode followed by its arguments.

    Returns
    -------
    bytes
        The encoded instruction, e.g. ``("select", "ssh") -> b"6.select,3.ssh;"``.
    """
    return ",".join(f"{len(e)}.{e}" for e in elems).encode() + b";"


def decode_one(raw):
    """Parse one complete, well-formed encoded instruction into its elements.

    The inverse of :func:`encode`, for constants known to be well-formed; it
    skips the defensive checks :func:`parse_instruction` makes.

    Parameters
    ----------
    raw : bytes
        One complete encoded instruction.

    Returns
    -------
    list[str]
        The decoded elements ``[opcode, *args]``.
    """
    text = raw.decode()
    elems = []
    pos = 0
    while True:
        dot = text.index(".", pos)
        length = int(text[pos:dot])
        start = dot + 1
        elems.append(text[start:start + length])
        delim = text[start + length]
        if delim == ";":
            return elems
        pos = start + length + 1


def connect(host, port, attempts=5, delay=1.0, log=None):
    """TCP-connect to a host, retrying so a still-starting stack doesn't fail
    the flow immediately.

    Parameters
    ----------
    host : str
        Target hostname or IP address.
    port : int
        Target TCP port.
    attempts : int, default 5
        Number of connection attempts before giving up.
    delay : float, default 1.0
        Seconds to wait between attempts.
    log : callable or None, optional
        Optional ``log(str)`` sink for per-attempt failure messages.

    Returns
    -------
    socket.socket
        The connected socket (with a 3 s timeout).

    Raises
    ------
    OSError
        The last connection error, if every attempt fails.
    """
    last = None
    for i in range(attempts):
        try:
            return socket.create_connection((host, port), timeout=3.0)
        except OSError as e:
            last = e
            if log:
                log(f"connect to {host}:{port} failed ({e}), "
                    f"attempt {i + 1}/{attempts}")
            if i + 1 < attempts:
                time.sleep(delay)
    raise last


class InstructionStream:
    """Incremental parser over a socket, yielding one instruction at a time."""

    def __init__(self, sock):
        """Wrap a connected socket for incremental instruction parsing.

        Parameters
        ----------
        sock : socket.socket
            An open, connected socket the stream reads from (and closes via
            :meth:`close`).
        """
        self.sock = sock
        self.buf = ""  # decoded, unconsumed characters
        self.eof = False
        self._decoder = codecs.getincrementaldecoder("utf-8")()

    def read_instruction(self, timeout):
        """Read one complete instruction, blocking up to ``timeout`` seconds.

        Parameters
        ----------
        timeout : float
            Maximum seconds to wait for a complete instruction. Each underlying
            socket read is capped at 0.5 s so callers can poll a stop flag
            between reads.

        Returns
        -------
        list[str] or None
            The instruction's elements ``[opcode, *args]``, or None on timeout
            or when the peer closed the stream (then ``self.eof`` is set True).

        Raises
        ------
        ProtocolError
            If the peer sends bytes that are not valid Guacamole wire protocol.
        """
        deadline = time.monotonic() + timeout
        while True:
            # instruction valid
            instr = self._parse_one()
            if instr is not None:
                return instr
            if self.eof:
                return None

            # reading timed out
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None

            self.sock.settimeout(min(remaining, 0.5))

            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                self.eof = True
                return None
            if not chunk:
                self.eof = True
                return None

            # Try to decode buffer, else throw ProtocolError
            try:
                self.buf += self._decoder.decode(chunk)
            except UnicodeDecodeError as e:
                raise ProtocolError(f"invalid UTF-8 from peer: {e}")

    def close(self):
        """Close the underlying socket, ignoring any error.

        Returns
        -------
        None
        """
        try:
            self.sock.close()
        except OSError:
            pass

    def _parse_one(self):
        """Consume one complete instruction from ``self.buf``.

        Returns
        -------
        list[str] or None
            The instruction's elements, or None when more data is needed (the
            buffer is left untouched until then).
        """
        instr, self.buf = parse_instruction(self.buf)
        return instr


def parse_instruction(buf):
    """Parse one complete instruction from the start of a decoded buffer.

    A pure function shared by the sync :class:`InstructionStream` and the async
    client; it never mutates its input.

    Parameters
    ----------
    buf : str
        Decoded, unconsumed characters.

    Returns
    -------
    tuple[list[str] | None, str]
        ``(elements, rest)`` on success, or ``(None, buf)`` when more data is
        needed (the buffer is returned unchanged).

    Raises
    ------
    ProtocolError
        On a malformed length prefix, an oversized element, or a bad delimiter.
    """
    pos = 0
    elems = []
    while True:
        dot = buf.find(".", pos)
        if dot < 0:
            if len(buf) - pos > len(str(MAX_ELEMENT)):
                raise ProtocolError(
                    f"length prefix too long: {buf[pos:pos + 16]!r}")
            return None, buf
        length_str = buf[pos:dot]
        if not length_str or not all(c in "0123456789" for c in length_str):
            raise ProtocolError(f"bad element length {length_str!r}")
        length = int(length_str)
        if length > MAX_ELEMENT:
            raise ProtocolError(f"element of {length} chars exceeds cap")
        start = dot + 1
        end = start + length
        if len(buf) < end + 1:  # element + its delimiter
            return None, buf
        elems.append(buf[start:end])
        delim = buf[end]
        if delim == ";":
            return elems, buf[end + 1:]
        if delim != ",":
            raise ProtocolError(f"bad delimiter {delim!r} after element")
        pos = end + 1
