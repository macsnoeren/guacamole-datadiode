"""The custom adversarial-load flow.

The operator dials in a mix of *good* and *bad* connections and watches, live,
whether the good ones keep working while the bad ones corrupt their own streams
— the guard's per-channel isolation made visible. Runs on a single asyncio event
loop so it can hold many connections at once; cumulative latency stats are
published every poll for the UI dashboard.

Parameters (from the start request body, all optional with defaults):
  good       0..65535  valid connections that ping-pong: send input, await a
                       guacd paint-back, record the round-trip; repeat.
  bad        0..65535  connections that establish, then send a corrupt packet
                       (`2.foo;`) so the guard SHUTDOWN_CHANNELs them; the good
                       ones must survive.  good + bad <= 65535 (the channel space).
  delay      0.0..10.0 seconds a connection waits between sends (0 = as fast as
                       possible).
  randomize  bool      stagger each connection's start by a random offset
                       instead of all sending in lockstep.
  duration   seconds   how long to run.

It needs the live stack (good ping-pong needs guacd's paint-back, bad corruption
needs the guard in path); against gml_only no good connection establishes and the
flow fails fast. Practical concurrency is also bounded by the container's
open-file limit — failed connects are counted, not fatal.
"""

import asyncio
import json
import random
from types import SimpleNamespace

import aguacclient
from aguacclient import ProtocolError
from runner import TestFailure

from .common import (AUDIO, ENTER_KEYSYM, GUACD_DRAW_OPS, IMAGE, NAME, SELECT,
                     SIZE, TIMEZONE, VIDEO, build_connect)
from guacclient import encode

CHANNEL_LIMIT = 65535
CONNECT_TIMEOUT_S = 5.0        # TCP connect (generous under high fan-out)
RESPONSE_TIMEOUT_S = 2.0       # await a paint instruction from guacd
SHUTDOWN_OBSERVE_S = 3.0       # await the guard's shutdown message after corrupting the stream
PUBLISH_INTERVAL_S = 0.5       # live-stats cadence

_ENTER = encode("key", ENTER_KEYSYM, "1") + encode("key", ENTER_KEYSYM, "0")
_CORRUPT = b"2.foo;"           # not valid Guacamole — corrupts the stream


def run_custom(ctx):
    """Flow entry point: run the asyncio load test to completion.

    Parameters
    ----------
    ctx : runner.TestContext
        The per-run context (params, cfg, log, stop_event, publish).

    Returns
    -------
    dict
        The result payload: ``{test, passed, params, stats}``.

    Raises
    ------
    runner.TestFailure
        On invalid parameters, or if no good connection establishes.
    """
    return asyncio.run(_main(ctx))


def _parse_params(params):
    """Validate and coerce the start-request parameters.

    Parameters
    ----------
    params : dict
        Raw parameters from the request body (any subset; defaults fill gaps).

    Returns
    -------
    types.SimpleNamespace
        ``good``, ``bad`` (int), ``delay``, ``duration`` (float), ``randomize``
        (bool).

    Raises
    ------
    runner.TestFailure
        If a value is non-numeric, out of range, ``good + bad`` exceeds the
        channel space, or both counts are zero.
    """
    def num(key, default, lo, hi, cast):
        """Coerce params[key] with `cast` and bounds-check it to [lo, hi]."""
        try:
            v = cast(params.get(key, default))
        except (TypeError, ValueError):
            raise TestFailure(f"{key}: not a number")
        if not lo <= v <= hi:
            raise TestFailure(f"{key} = {v} out of range [{lo}, {hi}]")
        return v

    good = num("good", 0, 0, CHANNEL_LIMIT, int)
    bad = num("bad", 0, 0, CHANNEL_LIMIT, int)
    if good + bad > CHANNEL_LIMIT:
        raise TestFailure(
            f"good + bad = {good + bad} exceeds the {CHANNEL_LIMIT}-channel limit")
    if good + bad == 0:
        raise TestFailure("nothing to run: set good and/or bad connections")
    return SimpleNamespace(
        good=good, bad=bad,
        delay=num("delay", 0.0, 0.0, 10.0, float),
        duration=num("duration", 10.0, 0.1, 3600.0, float),
        randomize=bool(params.get("randomize", False)))


class _Stats:
    """Cumulative counters and latency aggregate. Single event loop, so no
    locking: coroutines mutate it cooperatively; snapshot() is read by the
    publisher on the same loop."""

    def __init__(self, good, bad):
        """Initialise all counters to zero.

        Parameters
        ----------
        good, bad : int
            The configured connection counts (recorded as the totals).
        """
        self.open = 0
        self.peak_open = 0
        self.good_total, self.bad_total = good, bad
        self.good_established = self.good_disconnected = self.good_timeouts = 0
        self.bad_established = self.bad_corrupted = 0
        self.bad_shutdown_observed = self.bad_disconnected = 0
        self.count = self.sum = self.sumsq = 0.0
        self.lat_min = self.lat_max = None
        self.errors = []
        self.errors_dropped = 0

    def opened(self):
        """Record a connection opening (updates the open/peak gauges).

        Returns
        -------
        None
        """
        self.open += 1
        self.peak_open = max(self.peak_open, self.open)

    def closed(self):
        """Record a connection closing.

        Returns
        -------
        None
        """
        self.open -= 1

    def record_rtt(self, ms):
        """Fold one round-trip sample into the latency aggregate.

        Parameters
        ----------
        ms : float
            The measured round-trip time in milliseconds.

        Returns
        -------
        None
        """
        self.count += 1
        self.sum += ms
        self.sumsq += ms * ms
        self.lat_min = ms if self.lat_min is None else min(self.lat_min, ms)
        self.lat_max = ms if self.lat_max is None else max(self.lat_max, ms)

    def error(self, msg):
        """Record a per-connection error message (capped at 50).

        Parameters
        ----------
        msg : str
            A short description; beyond the cap, only a drop count is kept.

        Returns
        -------
        None
        """
        if len(self.errors) < 50:
            self.errors.append(msg)
        else:
            self.errors_dropped += 1

    def _latency(self):
        """Compute the latency summary from the running aggregate.

        Returns
        -------
        dict or None
            ``{count, mean_ms, min_ms, max_ms, stddev_ms}``, or None if no
            samples have been recorded.
        """
        if self.count == 0:
            return None
        mean = self.sum / self.count
        var = max(0.0, self.sumsq / self.count - mean * mean)
        return {
            "count": int(self.count),
            "mean_ms": round(mean, 2),
            "min_ms": round(self.lat_min, 2),
            "max_ms": round(self.lat_max, 2),
            "stddev_ms": round(var ** 0.5, 2),
        }

    def snapshot(self):
        """Build a JSON-serialisable snapshot for the UI / final result.

        Returns
        -------
        dict
            ``open_connections``, ``peak_open``, the ``good``/``bad`` counter
            groups, ``latency_ms`` (or None), and the ``errors`` list.
        """
        return {
            "open_connections": self.open,
            "peak_open": self.peak_open,
            "good": {
                "total": self.good_total,
                "established": self.good_established,
                "disconnected": self.good_disconnected,
                "timeouts": self.good_timeouts,
            },
            "bad": {
                "total": self.bad_total,
                "established": self.bad_established,
                "corrupted": self.bad_corrupted,
                "shutdown_observed": self.bad_shutdown_observed,
                "disconnected": self.bad_disconnected,
            },
            "latency_ms": self._latency(),
            "errors": list(self.errors),
        }


async def _main(ctx):
    """Orchestrate the run: spawn connections, run for the duration, summarise.

    Parameters
    ----------
    ctx : runner.TestContext
        The per-run context.

    Returns
    -------
    dict
        The result payload ``{test, passed, params, stats}``.

    Raises
    ------
    runner.TestFailure
        On invalid params, or if good connections were requested but none
        established (the live stack is not up).
    """
    p = _parse_params(ctx.params)
    log = ctx.log
    host, port = ctx.cfg["GML_HOST"], ctx.cfg["GML_PORT"]
    establish_timeout = ctx.cfg["E2E_ESTABLISH_TIMEOUT_S"]
    connect_instr = build_connect({
        "hostname": ctx.cfg["E2E_SSH_HOST"], "port": ctx.cfg["E2E_SSH_PORT"],
        "username": ctx.cfg["E2E_SSH_USER"], "password": ctx.cfg["E2E_SSH_PASS"],
    })
    log(f"[custom] {p.good} good + {p.bad} bad connection(s), {p.delay:.1f}s "
        f"send delay, {'randomized' if p.randomize else 'lockstep'} offsets, "
        f"{p.duration:.0f}s — guacd SSH target {ctx.cfg['E2E_SSH_HOST']}")

    stats = _Stats(p.good, p.bad)
    stop = asyncio.Event()
    cfg = SimpleNamespace(host=host, port=port, connect_instr=connect_instr,
                          establish_timeout=establish_timeout, p=p)

    conns = [asyncio.ensure_future(_good_conn(stats, stop, cfg))
             for _ in range(p.good)]
    conns += [asyncio.ensure_future(_bad_conn(stats, stop, cfg))
              for _ in range(p.bad)]
    publisher = asyncio.ensure_future(_publisher(ctx, stats, stop))

    try:
        await _until_done(ctx, stop, p.duration)
    finally:
        stop.set()
        await asyncio.sleep(0.2)            # let connections wind down cleanly
        for c in conns:
            c.cancel()
        await asyncio.gather(*conns, return_exceptions=True)
        publisher.cancel()
        await asyncio.gather(publisher, return_exceptions=True)

    snap = stats.snapshot()
    ctx.publish(snap)  # final snapshot = whole-test cumulative

    if p.good and stats.good_established == 0:
        raise TestFailure(
            "no good connection established — is the full stack up (guard "
            "approving, guacd able to reach the SSH backend, credentials valid)?")

    lat = snap["latency_ms"]
    if lat:
        log(f"[custom] latency over {lat['count']} pings: mean {lat['mean_ms']}ms "
            f"min {lat['min_ms']}ms max {lat['max_ms']}ms stddev {lat['stddev_ms']}ms")
    log(f"[custom] good {stats.good_established} established / "
        f"{stats.good_disconnected} disconnected; bad {stats.bad_corrupted} "
        f"corrupted / {stats.bad_shutdown_observed} shut down by the guard")

    result = {"test": "custom",
              "passed": stats.good_disconnected == 0,
              "params": vars(p),
              "stats": snap}
    log("RESULT " + json.dumps(result, separators=(",", ":")))
    return result


async def _until_done(ctx, stop, duration):
    """Block until the duration elapses or the operator requests a stop.

    Parameters
    ----------
    ctx : runner.TestContext
        Watched for ``ctx.stop_event``.
    stop : asyncio.Event
        The flow-internal stop signal (not awaited here, set by the caller).
    duration : float
        How long to run, in seconds.

    Returns
    -------
    None
    """
    loop = asyncio.get_running_loop()
    end = loop.time() + duration
    while loop.time() < end and not ctx.stop_event.is_set():
        await asyncio.sleep(0.1)


async def _publisher(ctx, stats, stop):
    """Publish the live snapshot every PUBLISH_INTERVAL_S until stop.

    Parameters
    ----------
    ctx : runner.TestContext
        Provides ``ctx.publish``.
    stats : _Stats
        The aggregate to snapshot.
    stop : asyncio.Event
        Ends the loop when set.

    Returns
    -------
    None
    """
    while not stop.is_set():
        ctx.publish(stats.snapshot())
        try:
            await asyncio.wait_for(stop.wait(), timeout=PUBLISH_INTERVAL_S)
        except asyncio.TimeoutError:
            pass


async def _good_conn(stats, stop, cfg):
    """One good connection: establish, then ping-pong, recording each RTT.

    Parameters
    ----------
    stats : _Stats
        The shared aggregate to update.
    stop : asyncio.Event
        Ends the ping loop when set.
    cfg : types.SimpleNamespace
        Run config (``host``, ``port``, ``connect_instr``, ``establish_timeout``,
        ``p``).

    Returns
    -------
    None
    """
    loop = asyncio.get_running_loop()
    stream = await _open(stats, stop, cfg, "good")
    if stream is None:
        return
    try:
        await _initial_offset(stop, cfg.p)
        await _forge(stream, cfg.connect_instr)
        if await _read_until(stream, _is_paint, cfg.establish_timeout) is None:
            stats.error("good: no session established (no paint-back)")
            return
        stats.good_established += 1

        while not stop.is_set():
            t0 = loop.time()
            try:
                # Send input: pressing and immediately letting go of the ENTER key
                await stream.send(_ENTER)
            except OSError:
                stats.good_disconnected += 1
                return

            # Wait for next paint frame (e.g. rect, cfill)
            frame = await _read_until(stream, _is_paint, RESPONSE_TIMEOUT_S)
            if frame is None:
                # If channel is closed, no reply back from guacd
                if stream.eof:
                    stats.good_disconnected += 1
                    return
                stats.good_timeouts += 1
            else:
                stats.record_rtt((loop.time() - t0) * 1000)

            # Wait a delay before sending again
            await _interruptible_sleep(cfg.p.delay, stop)
    except ProtocolError as e:
        stats.error(f"good: non-Guacamole bytes: {e}")
    except asyncio.CancelledError:
        pass
    finally:
        # Mark this connection closed
        stats.closed()
        await stream.close()


async def _bad_conn(stats, stop, cfg):
    """One bad connection: establish, then corrupt the stream and watch it die.

    Parameters
    ----------
    stats : _Stats
        The shared aggregate to update.
    stop : asyncio.Event
        Aborts the run when set.
    cfg : types.SimpleNamespace
        Run config (see :func:`_good_conn`).

    Returns
    -------
    None
    """
    stream = await _open(stats, stop, cfg, "bad")
    if stream is None:
        return
    try:
        await _initial_offset(stop, cfg.p)
        await _forge(stream, cfg.connect_instr)
        if await _read_until(stream, _is_paint, cfg.establish_timeout) is None:
            stats.error("bad: no session established (no paint-back)")
            return
        stats.bad_established += 1

        # Run as an approved channel for a randomized slice of the test, then
        # corrupt — staggering corruptions and leaving time to watch the good
        # connections survive the aftermath.
        await _interruptible_sleep(random.uniform(0, cfg.p.duration * 0.5), stop)
        if stop.is_set():
            return
        try:
            await stream.send(_CORRUPT)
        except OSError:
            stats.bad_disconnected += 1
            return
        stats.bad_corrupted += 1

        # The guard detects the corrupt stream and SHUTDOWN_CHANNELs it; that
        # echo tears our connection down, which we see as EOF. Drain until the
        # close rather than reading once — an established session still has
        # guacd traffic in flight (keepalive syncs, redraws), and a single read
        # would return one of those instead of the EOF.
        if await _drain_until_eof(stream, SHUTDOWN_OBSERVE_S):
            stats.bad_shutdown_observed += 1
    except ProtocolError as e:
        stats.error(f"bad: non-Guacamole bytes: {e}")
    except asyncio.CancelledError:
        pass
    finally:
        stats.closed()
        await stream.close()


async def _open(stats, stop, cfg, kind):
    """Connect and count it open, or record the failure and return None.

    Parameters
    ----------
    stats : _Stats
        Updated with the open count or a connect-failure error.
    stop : asyncio.Event
        Unused here; kept for a uniform connection-coroutine signature.
    cfg : types.SimpleNamespace
        Provides ``host`` and ``port``.
    kind : str
        ``"good"`` or ``"bad"``, for the error message.

    Returns
    -------
    aguacclient.AsyncInstructionStream or None
        The open stream, or None if the connection failed.
    """
    try:
        stream = await aguacclient.connect(cfg.host, cfg.port, CONNECT_TIMEOUT_S)
    except (OSError, asyncio.TimeoutError) as e:
        stats.error(f"{kind}: connect failed: {e}")
        return None
    stats.opened()
    return stream


async def _forge(stream, connect_instr):
    """Drive gmlbroker's forged handshake (select->args, connect->ready).

    Parameters
    ----------
    stream : aguacclient.AsyncInstructionStream
        The connection to handshake over.
    connect_instr : bytes
        The encoded ``connect`` instruction carrying the SSH backend params.

    Returns
    -------
    None

    Raises
    ------
    runner.TestFailure
        If gmlbroker does not answer ``args`` or ``ready``.
    """
    await stream.send(SELECT)
    if await _read_until(stream, lambda i: i[0] == "args", 5.0) is None:
        raise TestFailure("handshake: no args reply")
    await stream.send(SIZE + AUDIO + VIDEO + IMAGE + TIMEZONE + NAME + connect_instr)
    if await _read_until(stream, lambda i: i[0] == "ready", 5.0) is None:
        raise TestFailure("handshake: no ready reply")


async def _read_until(stream, predicate, timeout):
    """Read instructions until one satisfies a predicate.

    Parameters
    ----------
    stream : aguacclient.AsyncInstructionStream
        The connection to read from.
    predicate : callable
        ``predicate(instr: list[str]) -> bool``; the first match is returned.
    timeout : float
        Overall budget in seconds across all reads.

    Returns
    -------
    list[str] or None
        The matching instruction, or None on timeout or peer close.
    """
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while True:
        remaining = deadline - loop.time()
        if remaining <= 0:
            return None
        instr = await stream.read_instruction(remaining)
        if instr is None:
            return None
        if predicate(instr):
            return instr


def _is_paint(instr):
    """Whether an instruction is a guacd drawing op (a "real session" signal).

    Parameters
    ----------
    instr : list[str]
        A parsed instruction ``[opcode, *args]``.

    Returns
    -------
    bool
        True if the opcode is in ``GUACD_DRAW_OPS``.
    """
    return instr[0] in GUACD_DRAW_OPS


async def _drain_until_eof(stream, timeout):
    """Read and discard instructions until the peer closes the stream.

    Parameters
    ----------
    stream : aguacclient.AsyncInstructionStream
        The connection to drain.
    timeout : float
        Maximum seconds to wait for the close.

    Returns
    -------
    bool
        True if EOF was observed within ``timeout``, else False.
    """
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while not stream.eof:
        remaining = deadline - loop.time()
        if remaining <= 0:
            return False
        await stream.read_instruction(remaining)
    return True


async def _initial_offset(stop, p):
    """Optionally stagger a connection's start by a random offset.

    Parameters
    ----------
    stop : asyncio.Event
        Cuts the wait short if set.
    p : types.SimpleNamespace
        The parsed params; the offset is drawn from ``[0, delay]`` (or
        ``[0, 0.1]`` when ``delay`` is 0) only if ``p.randomize``.

    Returns
    -------
    None
    """
    if p.randomize:
        await _interruptible_sleep(random.uniform(0, p.delay or 0.1), stop)


async def _interruptible_sleep(seconds, stop):
    """Sleep up to ``seconds``, returning early if ``stop`` is set.

    Parameters
    ----------
    seconds : float
        Maximum seconds to sleep (<= 0 returns immediately).
    stop : asyncio.Event
        Wakes the sleep early when set.

    Returns
    -------
    None
    """
    if seconds <= 0:
        return
    try:
        await asyncio.wait_for(stop.wait(), timeout=seconds)
    except asyncio.TimeoutError:
        pass
