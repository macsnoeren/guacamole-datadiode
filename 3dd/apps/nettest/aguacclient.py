"""Asyncio Guacamole client for the custom flow, which drives many concurrent
connections on one event loop. Mirrors guacclient's blocking InstructionStream
but over asyncio.open_connection, and reuses guacclient's encode/decode and the
shared ``parse_instruction`` so there is one wire parser.
"""

import asyncio
import codecs

import guacclient
from guacclient import ProtocolError  # re-exported for callers


async def connect(host, port, timeout=3.0):
    """Open a connection and wrap it in an :class:`AsyncInstructionStream`.

    Parameters
    ----------
    host : str
        Target hostname or IP address.
    port : int
        Target TCP port.
    timeout : float, default 3.0
        Maximum seconds to wait for the connection to be established.

    Returns
    -------
    AsyncInstructionStream
        A stream wrapping the open connection.

    Raises
    ------
    OSError
        If the connection cannot be made.
    asyncio.TimeoutError
        If it is not established within ``timeout``.
    """
    reader, writer = await asyncio.wait_for(
        asyncio.open_connection(host, port), timeout)
    return AsyncInstructionStream(reader, writer)


class AsyncInstructionStream:
    """Incremental parser over an asyncio stream, yielding one instruction at a
    time. Element lengths count characters, so bytes are UTF-8 decoded before
    slicing, exactly as the blocking client does."""

    def __init__(self, reader, writer):
        """Wrap an asyncio reader/writer pair.

        Parameters
        ----------
        reader : asyncio.StreamReader
            The connection's read half.
        writer : asyncio.StreamWriter
            The connection's write half (closed via :meth:`close`).
        """
        self.reader = reader
        self.writer = writer
        self.buf = ""  # decoded, unconsumed characters
        self.eof = False
        self._decoder = codecs.getincrementaldecoder("utf-8")()

    async def send(self, data):
        """Write bytes to the peer and await the drain.

        Parameters
        ----------
        data : bytes
            The bytes to send.

        Returns
        -------
        None
        """
        self.writer.write(data)
        await self.writer.drain()

    async def read_instruction(self, timeout):
        """Read one complete instruction, awaiting up to ``timeout`` seconds.

        Parameters
        ----------
        timeout : float
            Maximum seconds to wait for a complete instruction.

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
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            # Read next instruction
            instr, self.buf = guacclient.parse_instruction(self.buf)

            # Valid instruction
            if instr is not None:
                return instr

            # Connection was closed before data could be received
            if self.eof:
                return None
            remaining = deadline - loop.time()

            # Reader timeout
            if remaining <= 0:
                return None
            try:
                chunk = await asyncio.wait_for(self.reader.read(65536), remaining)
            except asyncio.TimeoutError:
                return None
            except OSError:
                # The connection was closed
                self.eof = True
                return None
            if not chunk:
                self.eof = True
                return None

            # Try decoding the instruction
            try:
                self.buf += self._decoder.decode(chunk)
            except UnicodeDecodeError as e:
                raise ProtocolError(f"invalid UTF-8 from peer: {e}")

    async def close(self):
        """Close the connection, ignoring any error.

        Returns
        -------
        None
        """
        try:
            self.writer.close()
            await self.writer.wait_closed()
        except OSError:
            pass
