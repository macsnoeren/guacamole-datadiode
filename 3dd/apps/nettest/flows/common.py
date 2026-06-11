"""Shared Guacamole protocol constants and helpers for the nettest flows.

A flow module imports the canned client handshake and the connect-builder from
here so it is just its own logic; keeping these in one place is also what makes
adding a flow cheap. Extend it as new flows need shared pieces.
"""

import guacclient
from guacclient import encode

# Canned ssh `args` as gmlbroker must forge them — verbatim from guacd 1.6.0
# (SSH_ARGS_1_6_0 in apps/gmlbroker/src/handshake_forger.cpp). 43 elements
# after the opcode: the VERSION token plus 42 parameter names.
EXPECTED_SSH_ARGS = (
    b"4.args,13.VERSION_1_5_0,8.hostname,8.host-key,4.port,7.timeout,8.username,"
    b"8.password,9.font-name,9.font-size,11.enable-sftp,19.sftp-root-directory,"
    b"21.sftp-disable-download,19.sftp-disable-upload,11.private-key,"
    b"10.passphrase,10.public-key,12.color-scheme,7.command,15.typescript-path,"
    b"15.typescript-name,22.create-typescript-path,25.typescript-write-existing,"
    b"14.recording-path,14.recording-name,24.recording-exclude-output,"
    b"23.recording-exclude-mouse,22.recording-include-keys,"
    b"21.create-recording-path,24.recording-write-existing,9.read-only,"
    b"21.server-alive-interval,9.backspace,13.terminal-type,10.scrollback,"
    b"6.locale,8.timezone,12.disable-copy,13.disable-paste,15.wol-send-packet,"
    b"12.wol-mac-addr,18.wol-broadcast-addr,12.wol-udp-port,13.wol-wait-time;"
)

# The client side of the handshake, matching what the Guacamole 1.6.0 web
# server sends (see CLAUDE.md's captured sample and tests/integration).
SELECT = encode("select", "ssh")
SIZE = encode("size", "1680", "933", "96")
AUDIO = encode("audio", "audio/L8", "audio/L16")
VIDEO = encode("video")
IMAGE = encode("image", "image/jpeg", "image/png", "image/webp")
TIMEZONE = encode("timezone", "Europe/Berlin")
NAME = encode("name", "guacadmin")

# `connect` carries one value per parameter in the args list, positionally.
# After auto-approval gmlbroker replays this to the real guacd, which validates
# the count against its own 1.6.0 list — so build it from EXPECTED_SSH_ARGS.
_ARGS_ELEMS = guacclient.decode_one(EXPECTED_SSH_ARGS)


def build_connect(values):
    """Build a positional ``connect`` instruction for the 1.6.0 args list.

    Each named parameter in ``values`` is placed at its slot in the args list;
    every other slot is left empty.

    Parameters
    ----------
    values : dict[str, str]
        Parameter name -> value (e.g. ``{"hostname": "sshd", "port": "22"}``).
        Names not in the 1.6.0 args list are ignored.

    Returns
    -------
    bytes
        The encoded ``connect`` instruction.
    """
    return encode("connect", _ARGS_ELEMS[1],  # the VERSION token
                  *[values.get(p, "") for p in _ARGS_ELEMS[2:]])


# The forged waiting screen and gmlbroker's heartbeat only ever emit
# size/rect/cfill/sync, so the first opcode outside that set on the return
# stream means a real guacd session has taken over the channel.
GUACD_DRAW_OPS = {"img", "blob", "cursor", "dispose", "copy", "name", "end"}
ENTER_KEYSYM = "65293"  # X11 Return keysym; pressing it re-prompts the shell
