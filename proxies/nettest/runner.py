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

"""Test execution for nettest: one flow at a time, driven from the web API.

A flow is a callable taking a TestContext and returning a result dict (pass),
raising TestFailure (the system under test misbehaved), or raising
StopRequested (it noticed ctx.stop_event). Anything else is an internal error.
"""

import collections
import json
import os
import threading
import time


class TestFailure(AssertionError):
    """The system under test did not behave as expected."""


class StopRequested(Exception):
    """The operator pressed stop; the flow aborted between phases."""


class LogBuffer:
    """Thread-safe ring buffer of log lines with a monotonic cursor, so the
    web UI can poll for only-new lines. Also mirrors to stdout for the
    container logs."""

    def __init__(self, maxlen=2000):
        """Create the buffer.

        Parameters
        ----------
        maxlen : int, default 2000
            Maximum number of lines retained (oldest are dropped).
        """
        self._lock = threading.Lock()
        self._lines = collections.deque(maxlen=maxlen)
        self._seq = 0  # never reset, so stale client cursors stay valid

    def append(self, line):
        """Append a line and mirror it to stdout.

        Parameters
        ----------
        line : str
            The log line to store.

        Returns
        -------
        None
        """
        with self._lock:
            self._seq += 1
            self._lines.append((self._seq, line))
        print(line, flush=True)

    def since(self, seq):
        """Return the lines newer than a cursor.

        Parameters
        ----------
        seq : int
            The cursor returned by a previous call (0 for all lines).

        Returns
        -------
        tuple[list[str], int]
            ``(lines newer than seq, the new cursor)``.
        """
        with self._lock:
            return [l for s, l in self._lines if s > seq], self._seq

    def clear(self):
        """Drop all buffered lines (the cursor keeps advancing).

        Returns
        -------
        None
        """
        with self._lock:
            self._lines.clear()


class TestContext:
    """Everything a flow needs: a log sink, a stop signal, the static config
    (GML_HOST etc.), the per-run parameters from the start request, and a
    publish sink for a live stats snapshot the UI can poll mid-run."""

    def __init__(self, log, stop_event, cfg, params, publish):
        """Bundle the per-run state handed to a flow.

        Parameters
        ----------
        log : callable
            ``log(str) -> None`` sink for log lines.
        stop_event : threading.Event
            Set when the operator requests a stop.
        cfg : dict
            The static configuration (see ``server.load_cfg``).
        params : dict
            Per-run parameters from the ``/api/start`` request body.
        publish : callable
            ``publish(dict) -> None`` sink that replaces the live snapshot.
        """
        self.log = log
        self.stop_event = stop_event
        self.cfg = cfg
        self.params = params
        self.publish = publish  # publish(dict): replace the live snapshot

    def check_stop(self):
        """Raise if a stop was requested; a flow's cooperative cancel point.

        Returns
        -------
        None

        Raises
        ------
        StopRequested
            If ``stop_event`` is set.
        """
        if self.stop_event.is_set():
            raise StopRequested()


class TestRunner:
    """Runs at most one flow at a time on a worker thread."""

    def __init__(self, tests, cfg, log_buffer):
        """Create the runner.

        Parameters
        ----------
        tests : dict[str, callable]
            The flow registry, name -> ``run(ctx)`` callable.
        cfg : dict
            The static configuration shared with every flow.
        log_buffer : LogBuffer
            Where flows' log lines go and reports read from.
        """
        self._tests = tests
        self._cfg = cfg
        self._log = log_buffer
        self._lock = threading.Lock()
        self._thread = None
        self._stop = threading.Event()
        self._state = "idle"
        self._test = None
        self._started = None
        self._result = None
        self._live = None  # latest stats snapshot published by the running flow
        self._report = None  # path of the last written report

    def start(self, name, params):
        """Start a flow on a worker thread, if none is running.

        Parameters
        ----------
        name : str
            A key into the flow registry (assumed valid; the caller checks).
        params : dict
            Per-run parameters passed through to the flow via ``ctx.params``.

        Returns
        -------
        bool
            True if the flow was started, False if one was already running.
        """
        with self._lock:
            if self._thread is not None and self._thread.is_alive():
                return False
            self._log.clear()
            self._stop = threading.Event()
            self._state = "running"
            self._test = name
            self._started = time.time()
            self._result = None
            self._live = None
            ctx = TestContext(self._log.append, self._stop, self._cfg, params,
                              self._publish_live)
            self._thread = threading.Thread(
                target=self._run, args=(name, ctx), daemon=True)
            self._thread.start()
            return True

    def stop(self):
        """Request the running flow to stop (sets the stop event).

        Returns
        -------
        None
        """
        self._stop.set()

    def _publish_live(self, snapshot):
        """Replace the live stats snapshot; called from the flow's thread.

        Parameters
        ----------
        snapshot : dict
            The latest cumulative stats to expose on ``/api/status``.

        Returns
        -------
        None
        """
        with self._lock:
            self._live = snapshot

    def status(self):
        """Snapshot the runner state for ``/api/status``.

        Returns
        -------
        dict
            ``{state, test, started, result, live, report, report_dir}``.
        """
        with self._lock:
            return {
                "state": self._state,
                "test": self._test,
                "started": self._started,
                "result": self._result,
                "live": self._live,
                "report": self._report,
                "report_dir": self._cfg.get("REPORT_DIR"),
            }

    def clear_reports(self):
        """Delete all saved Markdown reports from REPORT_DIR.

        Only files matching the ``nettest-*.md`` report naming are removed, so
        nothing else in the directory is touched. The last-report pointer is
        cleared too, so ``status()`` stops referencing a deleted file.

        Returns
        -------
        tuple[int, list[str]]
            The number of reports deleted, and any per-file error messages.
        """
        report_dir = self._cfg.get("REPORT_DIR")
        deleted = 0
        errors = []
        if report_dir:
            try:
                names = os.listdir(report_dir)
            except OSError as e:
                return 0, [f"could not read {report_dir}: {e}"]
            for name in names:
                if name.startswith("nettest-") and name.endswith(".md"):
                    try:
                        os.remove(os.path.join(report_dir, name))
                        deleted += 1
                    except OSError as e:
                        errors.append(f"{name}: {e}")
        with self._lock:
            self._report = None
        self._log.append(f"[reports] cleared {deleted} report(s)")
        return deleted, errors

    def _run(self, name, ctx):
        """Worker-thread body: run the flow and record its terminal state.

        Parameters
        ----------
        name : str
            The flow name (for logging and the report filename).
        ctx : TestContext
            The context handed to the flow.

        Returns
        -------
        None
        """
        try:
            result = self._tests[name](ctx)
        except StopRequested:
            ctx.log(f"[{name}] stopped by user")
            self._finish("stopped", None)
        except TestFailure as e:
            ctx.log(f"[{name}] FAILED: {e}")
            self._finish("failed", {"error": str(e)})
        except Exception as e:  # internal error, not a test verdict
            ctx.log(f"[{name}] ERROR: {e.__class__.__name__}: {e}")
            self._finish("error", {"error": str(e)})
        else:
            self._finish("passed", result)

    def _finish(self, state, result):
        """Record the terminal state and write the run's report.

        Parameters
        ----------
        state : str
            One of ``"passed"``, ``"failed"``, ``"stopped"``, ``"error"``.
        result : dict or None
            The flow's result (pass) or ``{"error": ...}`` (fail/error); None
            for a stopped run.

        Returns
        -------
        None
        """
        with self._lock:
            self._state = state
            self._result = result
            test, started, live = self._test, self._started, self._live
        # Write the report outside the lock (file I/O) so status() never blocks.
        path = self._write_report(test, state, started, result, live)
        with self._lock:
            self._report = path

    def _write_report(self, test, state, started, result, live):
        """Save a timestamped Markdown report of the finished run.

        Parameters
        ----------
        test : str
            The flow name.
        state : str
            The terminal state.
        started : float or None
            Start time as a Unix timestamp.
        result : dict or None
            The flow's result (or ``{"error": ...}``), or None when stopped.
        live : dict or None
            The last live snapshot, used as the summary when ``result`` is None.

        Returns
        -------
        str or None
            The path written, or None if reports are disabled or the write
            failed.
        """
        report_dir = self._cfg.get("REPORT_DIR")
        if not report_dir:
            return None
        ended = time.time()
        stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime(ended))
        path = os.path.join(report_dir, f"nettest-{stamp}-{test}-{state}.md")
        # For a stopped flow there is no return value, but a live snapshot (the
        # custom flow) still carries the cumulative stats worth recording.
        data = result if result is not None else (
            {"final_stats": live} if live else {})
        log_lines, _ = self._log.since(0)
        body = _render_report(test, state, started, ended, data, log_lines)
        try:
            os.makedirs(report_dir, exist_ok=True)
            with open(path, "w") as f:
                f.write(body)
        except OSError as e:
            print(f"runner: could not write report {path}: {e}", flush=True)
            return None
        print(f"runner: wrote report {path}", flush=True)
        return path


def _render_report(test, state, started, ended, data, log_lines):
    """Build the Markdown report body.

    Parameters
    ----------
    test : str
        The flow name.
    state : str
        The terminal state.
    started, ended : float or None
        Start/end times as Unix timestamps (``started`` may be None).
    data : dict
        The structured result/summary to render.
    log_lines : list[str]
        The full buffered log to embed.

    Returns
    -------
    str
        The complete Markdown document (metadata, summary bullets, raw JSON,
        log).
    """
    fmt = "%Y-%m-%d %H:%M:%S"
    s = time.strftime(fmt, time.localtime(started)) if started else "—"
    e = time.strftime(fmt, time.localtime(ended))
    dur = f"{ended - started:.1f}s" if started else "—"
    md = [
        f"# nettest report — {test} — {state}",
        "",
        f"- **Test:** {test}",
        f"- **Outcome:** {state}",
        f"- **Started:** {s}",
        f"- **Ended:** {e}",
        f"- **Duration:** {dur}",
        "",
        "## Summary",
        "",
    ]
    md += _md_bullets(data) if data else ["_no structured result_"]
    md += [
        "",
        "## Raw result",
        "",
        "```json",
        json.dumps(data, indent=2, default=str),
        "```",
        "",
        "## Log",
        "",
        "```",
        *log_lines,
        "```",
        "",
    ]
    return "\n".join(md) + "\n"


def _md_bullets(data, indent=0):
    """Render a (possibly nested) dict/list as Markdown bullet lists.

    Parameters
    ----------
    data : dict or list or scalar
        The value to render.
    indent : int, default 0
        Current nesting depth (two spaces per level).

    Returns
    -------
    list[str]
        The rendered Markdown lines.
    """
    pad = "  " * indent
    lines = []
    if isinstance(data, dict):
        for k, v in data.items():
            if isinstance(v, (dict, list)) and v:
                lines.append(f"{pad}- **{k}:**")
                lines += _md_bullets(v, indent + 1)
            else:
                lines.append(f"{pad}- **{k}:** {_md_scalar(v)}")
    elif isinstance(data, list):
        for i, item in enumerate(data):
            if isinstance(item, (dict, list)) and item:
                lines.append(f"{pad}- [{i}]")
                lines += _md_bullets(item, indent + 1)
            else:
                lines.append(f"{pad}- {_md_scalar(item)}")
    else:
        lines.append(f"{pad}- {_md_scalar(data)}")
    return lines


def _md_scalar(v):
    """Format a scalar for a Markdown bullet.

    Parameters
    ----------
    v : Any
        The value (None, bool, or anything ``str()``-able).

    Returns
    -------
    str
        ``"—"`` for None, ``"yes"``/``"no"`` for bools, else ``str(v)``.
    """
    if v is None:
        return "—"
    if isinstance(v, bool):
        return "yes" if v else "no"
    return str(v)
