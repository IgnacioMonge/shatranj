#!/usr/bin/env python3
"""Small black-box DIRECT peer for SPCX/FuseX and real hardware.

The probe deliberately knows only the DIRECT wire framing and the liveness
messages needed for a transport smoke test.  It does not drive a game/session
reducer and it never starts or stops an emulator process.  The protocol core
is usable without sockets (``DirectPeer.feed`` and ``DirectPeer.tick``), which
keeps the unit tests deterministic and makes the same peer useful in a small
hardware harness.
"""

from __future__ import annotations

import argparse
import datetime as _datetime
import hashlib
import json
import math
import os
from pathlib import Path
import select
import socket
import sys
import tempfile
import time
from collections import deque
from dataclasses import dataclass, field, replace
from typing import Any, Callable, Deque, Iterable, Mapping, MutableMapping, Optional


DEFAULT_PORT = 5000
DEFAULT_MAX_FRAME_SIZE = 47
MACH_CODES = frozenset(("ZX", "NXT", "MAC", "LNX", "PC"))
HELLO_GUEST = "HELLO DIRECT GUEST"
MAX_ACK_DELAY = 60.0


class ConformanceError(RuntimeError):
    """Base class for a probe failure which is safe to report in JSON."""

    code = "conformance_error"

    def __init__(self, message: str, code: Optional[str] = None) -> None:
        super().__init__(message)
        if code:
            self.code = code


class FrameError(ConformanceError, ValueError):
    """A DIRECT line violates the framing contract."""


class MalformedFrameError(FrameError):
    code = "malformed_frame"


class NulFrameError(FrameError):
    code = "nul_frame"


class OversizeFrameError(FrameError):
    code = "oversize_frame"


class TruncatedFrameError(FrameError):
    code = "truncated_frame"


class ProtocolError(ConformanceError):
    code = "protocol_error"


class SendError(ConformanceError):
    code = "send_error"


def _monotonic(clock: Any) -> float:
    value = getattr(clock, "monotonic", None)
    if value is None:
        return time.monotonic()
    return float(value() if callable(value) else value)


def _sleep(clock: Any, seconds: float) -> None:
    if seconds <= 0:
        return
    value = getattr(clock, "sleep", None)
    if value is None:
        time.sleep(seconds)
    else:
        value(seconds)


def _utc_timestamp() -> str:
    return _datetime.datetime.now(_datetime.timezone.utc).isoformat().replace(
        "+00:00", "Z"
    )


def _as_bytes(value: bytes | bytearray | memoryview) -> bytes:
    try:
        return memoryview(value).tobytes()
    except TypeError as exc:
        raise TypeError("frame data must be bytes-like") from exc


def decode_frame(frame: bytes | bytearray | memoryview) -> str:
    """Decode one already-framed payload as strict wire ASCII."""

    try:
        return _as_bytes(frame).decode("ascii")
    except UnicodeDecodeError as exc:
        raise MalformedFrameError("DIRECT payload is not ASCII", "non_ascii_frame") from exc


def encode_frame(payload: str | bytes, max_frame_size: int = DEFAULT_MAX_FRAME_SIZE) -> bytes:
    """Return one newline-terminated DIRECT payload after strict validation."""

    if isinstance(payload, str):
        try:
            raw = payload.encode("ascii")
        except UnicodeEncodeError as exc:
            raise MalformedFrameError("DIRECT payload is not ASCII", "non_ascii_frame") from exc
    else:
        raw = _as_bytes(payload)
    if b"\x00" in raw:
        raise NulFrameError("DIRECT payload contains NUL")
    if b"\n" in raw or b"\r" in raw:
        raise MalformedFrameError("DIRECT payload contains a line separator")
    if not raw:
        raise MalformedFrameError("DIRECT payload is empty")
    if len(raw) > max_frame_size:
        raise OversizeFrameError(
            f"DIRECT payload is {len(raw)} bytes; limit is {max_frame_size}"
        )
    return raw + b"\n"


class DirectFramer:
    """Fragmentation-safe newline parser for DIRECT TCP payloads."""

    def __init__(self, max_frame_size: int = DEFAULT_MAX_FRAME_SIZE) -> None:
        if max_frame_size <= 0:
            raise ValueError("max_frame_size must be positive")
        self.max_frame_size = int(max_frame_size)
        self._buffer = bytearray()

    @property
    def buffered_bytes(self) -> int:
        return len(self._buffer)

    @property
    def buffer(self) -> bytes:
        """A read-only snapshot useful to deterministic tests and diagnostics."""

        return bytes(self._buffer)

    def reset(self) -> None:
        self._buffer.clear()

    def _reject_partial(self) -> None:
        if b"\x00" in self._buffer:
            self.reset()
            raise NulFrameError("DIRECT payload contains NUL")
        # A CR immediately before LF is accepted, so one extra byte is
        # allowed while a fragmented CRLF line is incomplete.
        if len(self._buffer) > self.max_frame_size + 1:
            self.reset()
            raise OversizeFrameError(
                f"DIRECT payload exceeds {self.max_frame_size} bytes"
            )

    def _validate_line(self, raw: bytes) -> bytes:
        if b"\x00" in raw:
            raise NulFrameError("DIRECT payload contains NUL")
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if b"\r" in raw:
            raise MalformedFrameError("DIRECT payload contains carriage return")
        if not raw:
            raise MalformedFrameError("DIRECT payload is empty")
        if len(raw) > self.max_frame_size:
            raise OversizeFrameError(
                f"DIRECT payload is {len(raw)} bytes; limit is {self.max_frame_size}"
            )
        try:
            raw.decode("ascii")
        except UnicodeDecodeError as exc:
            raise MalformedFrameError(
                "DIRECT payload is not ASCII", "non_ascii_frame"
            ) from exc
        return raw

    def feed(self, data: bytes | bytearray | memoryview) -> list[bytes]:
        """Consume arbitrary bytes and return every complete payload in order."""

        chunk = _as_bytes(data)
        if not chunk:
            return []
        self._buffer.extend(chunk)
        frames: list[bytes] = []
        try:
            while True:
                newline = self._buffer.find(b"\n")
                if newline < 0:
                    self._reject_partial()
                    break
                raw = bytes(self._buffer[:newline])
                del self._buffer[: newline + 1]
                frames.append(self._validate_line(raw))
        except FrameError:
            self.reset()
            raise
        return frames

    def finish(self) -> None:
        """Reject a stream ending in a partial line."""

        if self._buffer:
            self._reject_partial()
            self.reset()
            raise TruncatedFrameError("DIRECT stream ended before newline")

def parse_hello(payload: str | bytes) -> dict[str, str]:
    """Parse exactly one of the two contract-defined DIRECT HELLO lines."""

    text = payload if isinstance(payload, str) else decode_frame(payload)
    if text == HELLO_GUEST:
        return {"role": "guest"}
    prefix = "HELLO DIRECT HOST WHITE="
    if text.startswith(prefix) and text[len(prefix) :] in ("HOST", "GUEST"):
        return {"role": "host", "white_owner": text[len(prefix) :]}
    raise ProtocolError(f"unexpected DIRECT HELLO: {text!r}", "unexpected_hello")


def host_hello(white_owner: str = "HOST") -> str:
    owner = str(white_owner).upper()
    if owner not in ("HOST", "GUEST"):
        raise ValueError("white_owner must be HOST or GUEST")
    return f"HELLO DIRECT HOST WHITE={owner}"


def parse_mach(payload: str | bytes) -> str:
    """Parse an exact ``MACH`` announcement and return its machine code."""

    text = payload if isinstance(payload, str) else decode_frame(payload)
    if text.startswith("MACH ") and text[5:] in MACH_CODES:
        return text[5:]
    raise ProtocolError(f"unexpected MACH announcement: {text!r}", "invalid_mach")


def sha256_file(path: str | os.PathLike[str], chunk_size: int = 131072) -> str:
    """Hash one artifact without loading it all into memory."""

    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while True:
            block = stream.read(chunk_size)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def hash_artifacts(paths: Iterable[str | os.PathLike[str]]) -> list[dict[str, Any]]:
    """Return machine-readable hashes, retaining missing-artifact failures."""

    records: list[dict[str, Any]] = []
    for value in paths:
        path = Path(value)
        record: dict[str, Any] = {"path": str(value)}
        try:
            record["sha256"] = sha256_file(path)
            record["bytes"] = path.stat().st_size
            record["ok"] = True
        except OSError as exc:
            record.update({"ok": False, "error": str(exc)})
        records.append(record)
    return records


def atomic_write_json(
    path: str | os.PathLike[str], value: Mapping[str, Any], *, indent: int = 2
) -> None:
    """Write JSON through a same-directory temporary file and os.replace."""

    destination = Path(path)
    parent = destination.parent if destination.parent != Path("") else Path(".")
    parent.mkdir(parents=True, exist_ok=True)
    temporary: Optional[Path] = None
    fd, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=str(parent)
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=indent, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
        temporary = None
    finally:
        if temporary is not None:
            try:
                temporary.unlink()
            except FileNotFoundError:
                pass


class Evidence:
    """Mutable, JSON-safe counters shared by one or more peer sessions."""

    def __init__(self, mode: str, config: Optional[Mapping[str, Any]] = None) -> None:
        self._started_monotonic = 0.0
        self.data: dict[str, Any] = {
            "schema": "spcx-direct-conformance/v1",
            "schema_version": 1,
            "result": "FAIL",
            "ok": False,
            "mode": mode,
            "role": mode.upper(),
            "started_at": _utc_timestamp(),
            "ended_at": None,
            "duration_seconds": 0.0,
            "config": dict(config or {}),
            "requirements": {
                "handshake": True,
                "duration_seconds": 0.0,
                "reconnects": 0,
            },
            "handshake": {
                "required": True,
                "ok": False,
                "attempted": 0,
                "completed": 0,
                "peer_machine": None,
            },
            "frames": {
                "inbound": 0,
                "outbound": 0,
                "received": 0,
                "sent": 0,
                "bytes_in": 0,
                "bytes_out": 0,
                "malformed": 0,
                "oversize": 0,
                "nul": 0,
                "send_failures": 0,
            },
            "ping": {
                "inbound": 0,
                "outbound": 0,
                "ack_sent": 0,
                "ack_delayed": 0,
                "ack_delay_seconds": 0.0,
                "ack_delay_max_seconds": 0.0,
                "ack_delays_seconds": [],
                "ack_received": 0,
                "ack_latencies_seconds": [],
                "unanswered": 0,
            },
            "ack": {"inbound": 0, "outbound": 0},
            "close": {
                "peer": False,
                "local": False,
                "peer_count": 0,
                "local_count": 0,
                "clean": False,
                "reason": None,
                "events": [],
            },
            "cleanup": {"sockets_closed": False, "listener_closed": False},
            "reconnect": {
                "configured": 0,
                "attempts": 0,
                "succeeded": 0,
                "failures": 0,
                "required": 0,
                "required_met": True,
            },
            "sessions": [],
            "failures": [],
            "artifacts": [],
        }

    def start_session(self, now: float) -> int:
        sessions = self.data["sessions"]
        index = len(sessions)
        sessions.append(
            {
                "index": index,
                "started_at": _utc_timestamp(),
                "duration_seconds": 0.0,
                "state": "handshake",
                "local_hello": None,
                "peer_hello": None,
                "local_mach": None,
                "peer_mach": None,
                "frames": {"inbound": 0, "outbound": 0},
                "started_monotonic": now,
            }
        )
        self.data["handshake"]["attempted"] += 1
        return index

    def _session(self, index: int) -> MutableMapping[str, Any]:
        return self.data["sessions"][index]

    def frame_in(self, payload: bytes, session_index: Optional[int] = None) -> None:
        frames = self.data["frames"]
        frames["inbound"] += 1
        frames["received"] += 1
        frames["bytes_in"] += len(payload) + 1
        if session_index is not None:
            self._session(session_index)["frames"]["inbound"] += 1

    def frame_out(self, payload: bytes, session_index: Optional[int] = None) -> None:
        frames = self.data["frames"]
        frames["outbound"] += 1
        frames["sent"] += 1
        frames["bytes_out"] += len(payload)
        if session_index is not None:
            self._session(session_index)["frames"]["outbound"] += 1

    def frame_error(self, error: FrameError) -> None:
        frames = self.data["frames"]
        if error.code == "nul_frame":
            frames["nul"] += 1
        elif error.code == "oversize_frame":
            frames["oversize"] += 1
        else:
            frames["malformed"] += 1
        self.failure(error.code, str(error))

    def failure(self, code: str, message: str) -> None:
        failure = {"code": code, "message": message}
        if failure not in self.data["failures"]:
            self.data["failures"].append(failure)

    def close(self, *, peer: bool, reason: str, clean: bool = False) -> None:
        close = self.data["close"]
        event = {"side": "peer" if peer else "local", "reason": reason, "at": _utc_timestamp()}
        close["events"].append(event)
        if peer:
            close["peer"] = True
            close["peer_count"] += 1
        else:
            close["local"] = True
            close["local_count"] += 1
        close["reason"] = reason
        close["clean"] = bool(close["clean"] or clean)

    def finish_session(self, index: int, now: float, state: str) -> None:
        session = self._session(index)
        session["duration_seconds"] = max(0.0, now - session.pop("started_monotonic", now))
        session["state"] = state

    def finalize(self, now: float, *, ok: bool, reason: Optional[str] = None) -> dict[str, Any]:
        self.data["duration_seconds"] = max(0.0, now - self._started_monotonic)
        self.data["ended_at"] = _utc_timestamp()
        self.data["handshake"]["ok"] = self.data["handshake"]["completed"] > 0
        if reason:
            self.failure("requirement", reason)
        self.data["ok"] = bool(ok)
        self.data["result"] = "PASS" if self.data["ok"] else "FAIL"
        return self.data

    def set_started(self, now: float) -> None:
        self._started_monotonic = now

    def to_dict(self) -> dict[str, Any]:
        return self.data


class DirectPeer:
    """Protocol-only DIRECT peer with optional socket output."""

    def __init__(
        self,
        mode: str = "guest",
        *,
        machine: Optional[str] = "PC",
        white_owner: str = "HOST",
        require_mach: bool = False,
        max_frame_size: int = DEFAULT_MAX_FRAME_SIZE,
        ack_delay: float = 0.0,
        max_ack_delay: float = MAX_ACK_DELAY,
        ping_interval: float = 0.0,
        handshake_timeout: float = 10.0,
        sock: Any = None,
        clock: Any = None,
        evidence: Optional[Evidence] = None,
        send_hook: Optional[Callable[[bytes], Any]] = None,
    ) -> None:
        self.mode = mode.lower()
        if self.mode not in ("host", "guest"):
            raise ValueError("mode must be host or guest")
        if machine is not None:
            machine = str(machine).upper()
            if machine not in MACH_CODES:
                raise ValueError("machine must be ZX, NXT, MAC, LNX, or PC")
        self.machine = machine
        self.white_owner = str(white_owner).upper()
        if self.white_owner not in ("HOST", "GUEST"):
            raise ValueError("white_owner must be HOST or GUEST")
        if max_frame_size <= 0:
            raise ValueError("max_frame_size must be positive")
        if (
            not math.isfinite(float(ack_delay))
            or not math.isfinite(float(max_ack_delay))
            or ack_delay < 0
            or max_ack_delay < 0
            or ack_delay > max_ack_delay
        ):
            raise ValueError("ack_delay must be bounded by max_ack_delay")
        if ack_delay > MAX_ACK_DELAY:
            raise ValueError(f"ack_delay cannot exceed {MAX_ACK_DELAY:g} seconds")
        if (
            not math.isfinite(float(ping_interval))
            or not math.isfinite(float(handshake_timeout))
            or ping_interval < 0
            or handshake_timeout <= 0
        ):
            raise ValueError("ping_interval must be non-negative and handshake_timeout positive")
        self.require_mach = bool(require_mach)
        self.max_frame_size = int(max_frame_size)
        self.ack_delay = float(ack_delay)
        self.max_ack_delay = float(max_ack_delay)
        self.ping_interval = float(ping_interval)
        self.handshake_timeout = float(handshake_timeout)
        self.clock = clock or _RealClock()
        self.sock = sock
        self.send_hook = send_hook
        config = {
            "machine": machine,
            "white_owner": self.white_owner,
            "require_mach": self.require_mach,
            "max_frame_size": self.max_frame_size,
            "ack_delay": self.ack_delay,
            "ping_interval": self.ping_interval,
        }
        own_evidence = evidence is None
        self.evidence = evidence or Evidence(self.mode, config)
        if own_evidence:
            self.evidence.set_started(_monotonic(self.clock))
        self.session_index = self.evidence.start_session(_monotonic(self.clock))
        self.framer = DirectFramer(self.max_frame_size)
        self.state = "new"
        self.had_ready = False
        self.hello_sent = False
        self.hello_received = False
        self.mach_sent = False
        self.mach_received = False
        self.peer_hello: Optional[str] = None
        self.peer_machine: Optional[str] = None
        self.outbox: Deque[bytes] = deque()
        self.pending_acks: Deque[tuple[float, float]] = deque()
        self.pending_pings: Deque[float] = deque()
        self.next_ping_at: Optional[float] = None
        self._closed = False
        self._failure: Optional[ConformanceError] = None
        self._started = _monotonic(self.clock)

    @property
    def ready(self) -> bool:
        return self.state == "ready"

    @property
    def closed(self) -> bool:
        return self._closed

    @property
    def failed(self) -> bool:
        return self._failure is not None

    @property
    def failure(self) -> Optional[ConformanceError]:
        return self._failure

    def attach(self, sock: Any) -> None:
        self.sock = sock

    def local_hello(self) -> str:
        return HELLO_GUEST if self.mode == "guest" else host_hello(self.white_owner)

    def local_mach(self) -> Optional[str]:
        return None if self.machine is None else f"MACH {self.machine}"

    def start(self) -> None:
        if self.state != "new":
            return
        self._started = _monotonic(self.clock)
        self.state = "handshake"
        self.hello_sent = self._send(self.local_hello())
        self.evidence._session(self.session_index)["local_hello"] = self.local_hello()

    def _send(self, payload: str | bytes) -> bool:
        if self._closed:
            return False
        wire = encode_frame(payload, self.max_frame_size)
        try:
            if self.send_hook is not None:
                result = self.send_hook(wire)
                if result is False:
                    raise SendError("send hook rejected frame")
            elif self.sock is None:
                self.outbox.append(wire)
            else:
                send_all(self.sock, wire, clock=self.clock)
        except (ConformanceError, OSError) as exc:
            error = exc if isinstance(exc, ConformanceError) else SendError(str(exc))
            self._fail(error)
            return False
        self.evidence.frame_out(wire, self.session_index)
        text = decode_frame(wire[:-1])
        session = self.evidence._session(self.session_index)
        if text.startswith("MACH "):
            session["local_mach"] = text[5:]
        if text == "PING":
            self.evidence.data["ping"]["outbound"] += 1
        elif text == "ACK PING":
            ping = self.evidence.data["ping"]
            ack = self.evidence.data["ack"]
            ping["ack_sent"] += 1
            ack["outbound"] += 1
        elif text.startswith("ACK"):
            self.evidence.data["ack"]["outbound"] += 1
        return True

    def send(self, payload: str | bytes) -> bool:
        """Send one validated application frame."""

        return self._send(payload)

    def drain_outbox(self) -> list[bytes]:
        values = list(self.outbox)
        self.outbox.clear()
        return values

    def _maybe_send_mach(self, now: float) -> None:
        if self.hello_received and self.hello_sent and not self.mach_sent:
            if self.machine is None:
                self.mach_sent = True
            else:
                self.mach_sent = self._send(self.local_mach() or "")
        self._maybe_ready(now)

    def _maybe_ready(self, now: float) -> None:
        if self.state != "handshake" or not self.hello_received or not self.hello_sent:
            return
        if self.require_mach and not self.mach_received:
            return
        self.state = "ready"
        self.had_ready = True
        self.evidence.data["handshake"]["completed"] += 1
        self.next_ping_at = now + self.ping_interval if self.ping_interval else None
        self.evidence._session(self.session_index)["state"] = "ready"

    def _fail(self, error: ConformanceError) -> None:
        if self._failure is None:
            self._failure = error
            self.evidence.failure(error.code, str(error))
        self.close(reason=error.code, clean=False)

    def receive_eof(self, reason: str = "peer_eof") -> None:
        if not self._closed:
            self.evidence.close(peer=True, reason=reason, clean=False)
            self.close(reason=reason, clean=False, _record=False)

    def _handle(self, raw: bytes, now: float) -> None:
        text = decode_frame(raw)
        self.evidence.frame_in(raw, self.session_index)
        session = self.evidence._session(self.session_index)
        if text == "PING":
            ping = self.evidence.data["ping"]
            ping["inbound"] += 1
            self.pending_acks.append((now + self.ack_delay, now))
            self.tick(now)
            return
        if text == "ACK PING":
            ping = self.evidence.data["ping"]
            ping["ack_received"] += 1
            if self.pending_pings:
                latency = max(0.0, now - self.pending_pings.popleft())
                ping["ack_latencies_seconds"].append(latency)
            else:
                ping["unanswered"] += 1
            self.evidence.data["ack"]["inbound"] += 1
            return
        if text == "BYE":
            self.evidence.close(peer=True, reason="peer_bye", clean=True)
            self.close(reason="peer_bye", clean=True, _record=False)
            return
        if self.state == "new":
            self.start()
        if not self.hello_received:
            expected_role = "host" if self.mode == "guest" else "guest"
            try:
                parsed = parse_hello(text)
            except ProtocolError as exc:
                self._fail(exc)
                raise
            if parsed["role"] != expected_role:
                error = ProtocolError(
                    f"expected {expected_role} HELLO, got {text!r}", "unexpected_hello"
                )
                self._fail(error)
                raise error
            self.hello_received = True
            self.peer_hello = text
            session["peer_hello"] = text
            self._maybe_send_mach(now)
            return
        if text.startswith("HELLO DIRECT "):
            if text == self.peer_hello:
                return
            error = ProtocolError(
                "conflicting DIRECT HELLO", "conflicting_hello"
            )
            self._fail(error)
            raise error
        if text.startswith("MACH "):
            try:
                machine = parse_mach(text)
            except ProtocolError as exc:
                self._fail(exc)
                raise
            self.mach_received = True
            self.peer_machine = machine
            session["peer_mach"] = machine
            self.evidence.data["handshake"]["peer_machine"] = machine
            self._maybe_ready(now)
            return
        if self.state != "ready":
            error = ProtocolError(
                f"unexpected pre-handshake frame: {text!r}", "unexpected_handshake_frame"
            )
            self._fail(error)
            raise error
        if text.startswith("ACK "):
            self.evidence.data["ack"]["inbound"] += 1
            return
        # Other application payloads are intentionally observed, not judged:
        # this tool is a transport peer and must not become session policy.

    def feed(self, data: bytes | bytearray | memoryview, now: Optional[float] = None) -> list[bytes]:
        """Process arbitrary received bytes and return complete payloads."""

        if self._closed:
            return []
        current = _monotonic(self.clock) if now is None else float(now)
        try:
            frames = self.framer.feed(data)
        except FrameError as exc:
            self.evidence.frame_error(exc)
            self._fail(exc)
            raise
        for frame in frames:
            self._handle(frame, current)
        self.tick(current)
        return frames

    def tick(self, now: Optional[float] = None) -> None:
        """Flush delayed ACKs and emit due keepalive PINGs."""

        if self._closed:
            return
        current = _monotonic(self.clock) if now is None else float(now)
        while self.pending_acks and self.pending_acks[0][0] <= current:
            _, received_at = self.pending_acks.popleft()
            delay = max(0.0, current - received_at)
            if delay > 0:
                self.evidence.data["ping"]["ack_delayed"] += 1
            ping = self.evidence.data["ping"]
            ping["ack_delay_seconds"] += delay
            ping["ack_delay_max_seconds"] = max(
                ping["ack_delay_max_seconds"], delay
            )
            ping["ack_delays_seconds"].append(delay)
            self._send("ACK PING")
        if self.ready and self.ping_interval and self.next_ping_at is not None:
            while current >= self.next_ping_at:
                if self._send("PING"):
                    self.pending_pings.append(current)
                self.next_ping_at += self.ping_interval

    def handshake_timed_out(self, now: Optional[float] = None) -> bool:
        if self.ready or self._closed:
            return False
        current = _monotonic(self.clock) if now is None else float(now)
        if current - self._started < self.handshake_timeout:
            return False
        self._fail(ConformanceError("DIRECT handshake timed out", "handshake_timeout"))
        return True

    def close(
        self,
        reason: str = "local_close",
        clean: bool = False,
        *,
        _record: bool = True,
    ) -> None:
        if self._closed:
            return
        self._closed = True
        if self.state != "failed":
            self.state = "closed"
        if _record:
            self.evidence.close(peer=False, reason=reason, clean=clean)
        try:
            if self.sock is not None:
                try:
                    self.sock.shutdown(socket.SHUT_RDWR)
                except (AttributeError, OSError):
                    pass
                self.sock.close()
        except OSError:
            pass
        self.evidence.data["cleanup"]["sockets_closed"] = True
        self.evidence.finish_session(
            self.session_index,
            _monotonic(self.clock),
            "failed" if self.failed else self.state,
        )

    def json(self) -> dict[str, Any]:
        return self.evidence.to_dict()


class _RealClock:
    monotonic = staticmethod(time.monotonic)
    sleep = staticmethod(time.sleep)


def send_all(
    sock: Any,
    data: bytes,
    *,
    timeout: float = 5.0,
    clock: Any = None,
    wait_writable: Optional[Callable[[Any, float], Any]] = None,
) -> None:
    """Send all bytes, tolerating partial writes and would-block sockets."""

    clock = clock or _RealClock()
    view = memoryview(data)
    deadline = _monotonic(clock) + max(0.0, timeout)
    while view:
        try:
            sent = sock.send(view)
        except BlockingIOError:
            sent = 0
        except (InterruptedError,):
            continue
        except OSError as exc:
            raise SendError(str(exc), "send_error") from exc
        if sent is None:
            raise SendError("socket send returned None", "send_error")
        if sent < 0 or sent > len(view):
            raise SendError("socket returned an invalid send count", "send_error")
        if sent:
            view = view[sent:]
            continue
        remaining = deadline - _monotonic(clock)
        if remaining <= 0:
            raise SendError("socket remained blocked", "send_timeout")
        if wait_writable is not None:
            wait_writable(sock, remaining)
            continue
        try:
            ready, _, _ = select.select([], [sock], [], min(remaining, 0.25))
        except (OSError, ValueError, TypeError) as exc:
            raise SendError(f"socket is not writable: {exc}", "send_error") from exc
        if not ready and _monotonic(clock) >= deadline:
            raise SendError("socket remained blocked", "send_timeout")


@dataclass
class ProbeConfig:
    mode: str = "guest"
    host: str = "127.0.0.1"
    bind: str = "0.0.0.0"
    port: int = DEFAULT_PORT
    machine: Optional[str] = "PC"
    white_owner: str = "HOST"
    duration: float = 900.0
    handshake_timeout: float = 10.0
    ping_interval: Optional[float] = None
    ack_delay: float = 0.0
    max_ack_delay: float = MAX_ACK_DELAY
    max_frame_size: int = DEFAULT_MAX_FRAME_SIZE
    reconnect_attempts: int = 0
    reconnect_backoff: float = 1.0
    reconnect_after: float = 0.0
    require_reconnects: int = 0
    require_mach: bool = False
    artifact_paths: tuple[str, ...] = field(default_factory=tuple)

    def __post_init__(self) -> None:
        self.mode = self.mode.lower()
        if self.mode not in ("host", "guest"):
            raise ValueError("mode must be host or guest")
        if self.ping_interval is None:
            self.ping_interval = 3.0 if self.mode == "guest" else 0.0
        self.port = int(self.port)
        if self.port < 0 or self.port > 65535:
            raise ValueError("port must be in range 0..65535")
        for name in (
            "duration",
            "handshake_timeout",
            "ping_interval",
            "ack_delay",
            "max_ack_delay",
            "reconnect_backoff",
            "reconnect_after",
        ):
            value = float(getattr(self, name))
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"{name} must be non-negative")
        if self.handshake_timeout <= 0:
            raise ValueError("handshake_timeout must be positive")
        if self.duration <= 0:
            raise ValueError("duration must be positive")
        if self.max_frame_size <= 0:
            raise ValueError("max_frame_size must be positive")
        if self.reconnect_attempts < 0 or self.require_reconnects < 0:
            raise ValueError("reconnect counts must be non-negative")
        if self.require_reconnects > self.reconnect_attempts:
            # Keep the run useful and let evidence explain the unmet contract;
            # this is a runtime requirement failure, not a parser crash.
            pass


class DirectConformanceRunner:
    """Socket runner with bounded reconnects and no process-management hooks."""

    def __init__(
        self,
        config: Optional[ProbeConfig] = None,
        *,
        clock: Any = None,
        connector: Optional[Callable[[str, int], Any]] = None,
        listener: Any = None,
        socket_factory: Callable[..., Any] = socket.socket,
        poller: Optional[Callable[[Any, float], Any]] = None,
        peer_factory: Optional[Callable[..., DirectPeer]] = None,
        **overrides: Any,
    ) -> None:
        if config is None:
            config = ProbeConfig(**overrides)
        elif overrides:
            config = replace(config, **overrides)
        self.config = config
        self.clock = clock or _RealClock()
        self.connector = connector
        self.listener = listener
        self.socket_factory = socket_factory
        self.poller = poller
        self.peer_factory = peer_factory
        self.evidence = Evidence(self.config.mode, self._config_dict())
        self.start_time = _monotonic(self.clock)
        self.evidence.set_started(self.start_time)
        self._current_peer: Optional[DirectPeer] = None
        self._listener_owned = False

    def _config_dict(self) -> dict[str, Any]:
        config = dict(vars(self.config))
        config["artifact_paths"] = list(config["artifact_paths"])
        config["duration"] = float(config["duration"])
        config["handshake_timeout"] = float(config["handshake_timeout"])
        config["ack_delay"] = float(config["ack_delay"])
        return config

    def _new_peer(self, sock: Any) -> DirectPeer:
        if self.peer_factory is not None:
            peer = self.peer_factory(sock, self.config, self.evidence, self.clock)
            if peer.sock is None:
                peer.attach(sock)
            return peer
        return DirectPeer(
            self.config.mode,
            machine=self.config.machine,
            white_owner=self.config.white_owner,
            require_mach=self.config.require_mach,
            max_frame_size=self.config.max_frame_size,
            ack_delay=self.config.ack_delay,
            max_ack_delay=self.config.max_ack_delay,
            ping_interval=self.config.ping_interval,
            handshake_timeout=self.config.handshake_timeout,
            sock=sock,
            clock=self.clock,
            evidence=self.evidence,
        )

    def _connect(self) -> Any:
        if self.connector is not None:
            sock = self.connector(self.config.host, self.config.port)
        else:
            sock = self.socket_factory(socket.AF_INET, socket.SOCK_STREAM)
            try:
                sock.settimeout(self.config.handshake_timeout)
                sock.connect((self.config.host, self.config.port))
            except Exception:
                try:
                    sock.close()
                except OSError:
                    pass
                self.evidence.data["cleanup"]["sockets_closed"] = True
                raise
        self._set_nonblocking(sock)
        return sock

    @staticmethod
    def _set_nonblocking(sock: Any) -> None:
        setter = getattr(sock, "setblocking", None)
        if setter is not None:
            try:
                setter(False)
            except OSError:
                pass

    def _open_listener(self) -> Any:
        if self.listener is not None:
            return self.listener
        sock = self.socket_factory(socket.AF_INET, socket.SOCK_STREAM)
        self._listener_owned = True
        try:
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            except (AttributeError, OSError):
                pass
            sock.bind((self.config.bind, self.config.port))
            sock.listen(4)
            self._set_nonblocking(sock)
            self.listener = sock
        except Exception:
            try:
                sock.close()
            except OSError:
                pass
            self.evidence.data["cleanup"]["sockets_closed"] = True
            raise
        try:
            self.evidence.data["listen"] = {"address": sock.getsockname()[0], "port": sock.getsockname()[1]}
        except (AttributeError, OSError):
            pass
        return sock

    def _readable(self, sock: Any, timeout: float) -> bool:
        if self.poller is not None:
            result = self.poller(sock, max(0.0, timeout))
            if isinstance(result, (list, tuple, set)):
                return bool(result)
            return bool(result)
        try:
            ready, _, _ = select.select([sock], [], [], max(0.0, timeout))
            return bool(ready)
        except (OSError, ValueError, TypeError):
            # Fake sockets can provide a poll/readable method without a real
            # fileno.  No unbounded busy loop is allowed when neither exists.
            method = getattr(sock, "readable", None) or getattr(sock, "poll", None)
            if method is not None:
                return bool(method(max(0.0, timeout)))
            if timeout:
                _sleep(self.clock, min(timeout, 0.01))
            return False

    def _accept(self, listener: Any, timeout: float) -> Optional[Any]:
        if not self._readable(listener, timeout):
            return None
        try:
            accepted = listener.accept()
        except BlockingIOError:
            return None
        if isinstance(accepted, tuple):
            sock = accepted[0]
        else:
            sock = accepted
        self._set_nonblocking(sock)
        return sock

    def _next_timeout(
        self, peer: DirectPeer, now: float, end: float,
        reconnect_at: Optional[float] = None,
    ) -> float:
        values = [max(0.0, end - now)]
        if not peer.ready:
            values.append(max(0.0, peer._started + peer.handshake_timeout - now))
        if peer.pending_acks:
            values.append(max(0.0, peer.pending_acks[0][0] - now))
        if peer.ready and peer.next_ping_at is not None:
            values.append(max(0.0, peer.next_ping_at - now))
        if peer.ready and reconnect_at is not None:
            values.append(max(0.0, reconnect_at - now))
        return min(values)

    def _run_connection(
        self, peer: DirectPeer, end: float, *, force_reconnect: bool = False
    ) -> str:
        reconnect_at = (
            _monotonic(self.clock) + self.config.reconnect_after
            if force_reconnect and self.config.reconnect_after > 0
            else None
        )
        peer.start()
        while not peer.closed:
            now = _monotonic(self.clock)
            if now >= end:
                if not peer.ready:
                    peer.handshake_timed_out(now)
                    return "failure"
                peer.close(reason="duration_reached", clean=True)
                return "duration"
            if peer.ready and reconnect_at is not None and now >= reconnect_at:
                peer.close(reason="reconnect_cycle", clean=True)
                return "reconnect"
            if peer.handshake_timed_out(now):
                return "failure"
            peer.tick(now)
            timeout = self._next_timeout(peer, now, end, reconnect_at)
            if not self._readable(peer.sock, timeout):
                peer.tick(_monotonic(self.clock))
                continue
            try:
                data = peer.sock.recv(4096)
            except BlockingIOError:
                continue
            except OSError as exc:
                peer._fail(ConformanceError(str(exc), "recv_error"))
                return "failure"
            if not data:
                peer.receive_eof()
                return "peer_close"
            try:
                peer.feed(data, _monotonic(self.clock))
            except ConformanceError:
                return "failure"
        return "failure" if peer.failed else "peer_close"

    def _reconnect_wait(self) -> None:
        _sleep(self.clock, self.config.reconnect_backoff)

    def _record_reconnect_failure(self, message: str) -> None:
        self.evidence.data["reconnect"]["failures"] += 1
        self.evidence.failure("reconnect", message)

    def _run_guest(self, end: float) -> None:
        reconnect_used = 0
        while _monotonic(self.clock) < end or self.config.duration == 0:
            sock: Any = None
            try:
                sock = self._connect()
                peer = self._new_peer(sock)
                self._current_peer = peer
                outcome = self._run_connection(
                    peer,
                    end,
                    force_reconnect=(
                        reconnect_used < self.config.reconnect_attempts
                        and self.config.reconnect_after > 0
                    ),
                )
                if reconnect_used and peer.had_ready:
                    self.evidence.data["reconnect"]["succeeded"] += 1
                if outcome == "duration":
                    return
                if _monotonic(self.clock) >= end and self.config.duration > 0:
                    return
                if reconnect_used >= self.config.reconnect_attempts:
                    self._record_reconnect_failure("reconnect attempts exhausted")
                    return
                reconnect_used += 1
                self.evidence.data["reconnect"]["attempts"] = reconnect_used
                self._reconnect_wait()
            except (OSError, ConformanceError) as exc:
                if sock is not None:
                    try:
                        sock.close()
                    except OSError:
                        pass
                    self.evidence.data["cleanup"]["sockets_closed"] = True
                if _monotonic(self.clock) >= end and self.config.duration > 0:
                    self._record_reconnect_failure(str(exc))
                    return
                if reconnect_used >= self.config.reconnect_attempts:
                    self._record_reconnect_failure(str(exc))
                    return
                reconnect_used += 1
                self.evidence.data["reconnect"]["attempts"] = reconnect_used
                self._reconnect_wait()

    def _run_host(self, end: float) -> None:
        listener = self._open_listener()
        reconnect_used = 0
        while _monotonic(self.clock) < end or self.config.duration == 0:
            now = _monotonic(self.clock)
            remaining = max(0.0, end - now)
            sock = self._accept(listener, remaining)
            if sock is None:
                if self.config.duration == 0:
                    self.evidence.failure("no_connection", "host received no guest")
                return
            try:
                peer = self._new_peer(sock)
            except Exception:
                try:
                    sock.close()
                except OSError:
                    pass
                raise
            self._current_peer = peer
            outcome = self._run_connection(peer, end)
            if reconnect_used and peer.had_ready:
                self.evidence.data["reconnect"]["succeeded"] += 1
            if outcome == "duration":
                return
            if _monotonic(self.clock) >= end and self.config.duration > 0:
                return
            if reconnect_used >= self.config.reconnect_attempts:
                self._record_reconnect_failure("reconnect attempts exhausted")
                return
            reconnect_used += 1
            self.evidence.data["reconnect"]["attempts"] = reconnect_used
            self._reconnect_wait()

    def _finish_requirements(self, now: Optional[float] = None) -> Optional[str]:
        data = self.evidence.data
        current = _monotonic(self.clock) if now is None else float(now)
        data["duration_seconds"] = max(0.0, current - self.evidence._started_monotonic)
        data["handshake"]["ok"] = data["handshake"]["completed"] > 0
        data["requirements"]["duration_seconds"] = float(self.config.duration)
        data["requirements"]["reconnects"] = int(self.config.require_reconnects)
        reconnect = data["reconnect"]
        reconnect["configured"] = int(self.config.reconnect_attempts)
        reconnect["required"] = int(self.config.require_reconnects)
        reconnect["required_met"] = reconnect["succeeded"] >= reconnect["required"]
        duration_met = self.config.duration == 0 or (
            data["duration_seconds"] + 1e-6 >= self.config.duration
        )
        data["requirements"]["duration_met"] = duration_met
        data["requirements"]["handshake_met"] = data["handshake"]["ok"]
        if not data["handshake"]["ok"]:
            return "handshake requirement was not met"
        if not duration_met:
            return "duration requirement was not met"
        if not reconnect["required_met"]:
            return "reconnect requirement was not met"
        if data["failures"]:
            return data["failures"][0]["message"]
        return None

    def run(self) -> dict[str, Any]:
        end = self.start_time + self.config.duration
        try:
            artifact_records = hash_artifacts(self.config.artifact_paths)
            self.evidence.data["artifacts"] = artifact_records
            for record in artifact_records:
                if not record.get("ok"):
                    self.evidence.failure("artifact", record.get("error", "artifact unavailable"))
            if self.config.mode == "host":
                self._run_host(end)
            else:
                self._run_guest(end)
        except (OSError, ConformanceError, ValueError) as exc:
            self.evidence.failure(getattr(exc, "code", "runner_error"), str(exc))
        finally:
            if self._current_peer is not None and not self._current_peer.closed:
                self._current_peer.close(reason="runner_cleanup", clean=False)
            if self.listener is not None:
                try:
                    self.listener.close()
                except OSError:
                    pass
                self.evidence.data["cleanup"]["listener_closed"] = True
        finished_at = _monotonic(self.clock)
        reason = self._finish_requirements(finished_at)
        return self.evidence.finalize(finished_at, ok=reason is None, reason=reason)


def run_conformance(config: Optional[ProbeConfig] = None, **kwargs: Any) -> dict[str, Any]:
    return DirectConformanceRunner(config, **kwargs).run()


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("guest", "host"), default="guest")
    parser.add_argument("--host", default="127.0.0.1", help="CREATE host address in guest mode")
    parser.add_argument("--bind", default="0.0.0.0", help="listen address in host mode")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--machine", choices=sorted(MACH_CODES), default="PC")
    parser.add_argument("--white-owner", choices=("HOST", "GUEST"), default="HOST")
    parser.add_argument("--duration", type=float, default=900.0)
    parser.add_argument("--handshake-timeout", type=float, default=10.0)
    parser.add_argument(
        "--ping-interval",
        type=float,
        default=None,
        help="seconds between local PINGs (default: guest 3, host disabled)",
    )
    parser.add_argument("--ack-delay", type=float, default=0.0)
    parser.add_argument("--max-ack-delay", type=float, default=MAX_ACK_DELAY)
    parser.add_argument("--max-frame-size", "--max-frame", dest="max_frame_size", type=int, default=DEFAULT_MAX_FRAME_SIZE)
    parser.add_argument("--reconnect-attempts", type=int, default=0)
    parser.add_argument("--reconnect-backoff", type=float, default=1.0)
    parser.add_argument(
        "--reconnect-after",
        type=float,
        default=0.0,
        help="force a guest reconnect after this many seconds (0 disables)",
    )
    parser.add_argument("--require-reconnects", type=int, default=0)
    parser.add_argument("--require-mach", action="store_true")
    parser.add_argument("--artifact", "--artifact-path", action="append", dest="artifact_paths", default=[])
    parser.add_argument("--output", "--json-output", "--evidence", type=Path)
    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_argument_parser()
    args = parser.parse_args(argv)
    try:
        config = ProbeConfig(
            mode=args.mode,
            host=args.host,
            bind=args.bind,
            port=args.port,
            machine=args.machine,
            white_owner=args.white_owner,
            duration=args.duration,
            handshake_timeout=args.handshake_timeout,
            ping_interval=args.ping_interval,
            ack_delay=args.ack_delay,
            max_ack_delay=args.max_ack_delay,
            max_frame_size=args.max_frame_size,
            reconnect_attempts=args.reconnect_attempts,
            reconnect_backoff=args.reconnect_backoff,
            reconnect_after=args.reconnect_after,
            require_reconnects=args.require_reconnects,
            require_mach=args.require_mach,
            artifact_paths=tuple(args.artifact_paths),
        )
        evidence = run_conformance(config)
        if args.output is not None:
            atomic_write_json(args.output, evidence)
    except (OSError, ValueError, ConformanceError) as exc:
        print(f"[ERR] DIRECT conformance: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(evidence, ensure_ascii=False, sort_keys=True))
    return 0 if evidence.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
