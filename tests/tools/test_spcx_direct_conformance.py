#!/usr/bin/env python3
"""Fast, socket-free checks for the SPCX DIRECT conformance peer."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import socket
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.spcx_direct_conformance import (  # noqa: E402
    DirectFramer,
    DirectPeer,
    DirectConformanceRunner,
    Evidence,
    NulFrameError,
    OversizeFrameError,
    ProbeConfig,
    ProtocolError,
    TruncatedFrameError,
    atomic_write_json,
    encode_frame,
    hash_artifacts,
)


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class ScriptSocket:
    """Tiny non-fd socket for runner reconnect accounting."""

    def __init__(self, clock: FakeClock, persistent: bool) -> None:
        self.clock = clock
        self.persistent = persistent
        self.incoming: list[bytes] = []
        self.closed = False
        self.eof = False

    def setblocking(self, _value: bool) -> None:
        return None

    def send(self, data: bytes) -> int:
        wire = bytes(data)
        if wire == b"HELLO DIRECT GUEST\n":
            self.incoming.append(b"HELLO DIRECT HOST WHITE=HOST\n")
        elif wire == b"MACH PC\n":
            self.incoming.append(b"MACH NXT\n")
        elif wire == b"PING\n":
            self.incoming.append(b"ACK PING\n")
        return len(wire)

    def recv(self, _size: int) -> bytes:
        if self.incoming:
            return self.incoming.pop(0)
        if self.eof:
            return b""
        raise BlockingIOError

    def close(self) -> None:
        self.closed = True


class RunnerPoller:
    def __init__(self, clock: FakeClock) -> None:
        self.clock = clock

    def __call__(self, sock: ScriptSocket, timeout: float) -> bool:
        if sock.incoming:
            return True
        if not sock.persistent:
            if self.clock.now >= 0.2:
                sock.eof = True
                return True
            self.clock.now += min(timeout, 0.2 - self.clock.now)
            return False
        self.clock.now += timeout
        return False


def exchange(host: DirectPeer, guest: DirectPeer) -> None:
    """Move queued bytes until both protocol peers reach ready."""

    host.start()
    guest.start()
    for _ in range(8):
        changed = False
        for source, target in ((host, guest), (guest, host)):
            for wire in source.drain_outbox():
                target.feed(wire)
                changed = True
        if host.ready and guest.ready and host.mach_received and guest.mach_received:
            return
        if not changed:
            break
    raise AssertionError("peers did not complete HELLO/MACH exchange")


class DirectConformanceTests(unittest.TestCase):
    def test_fragmented_and_coalesced_frames(self) -> None:
        framer = DirectFramer()
        self.assertEqual(framer.feed(b"HELLO DIRECT G"), [])
        self.assertEqual(
            framer.feed(b"UEST\nPING\nACK PING\n"),
            [b"HELLO DIRECT GUEST", b"PING", b"ACK PING"],
        )
        self.assertEqual(framer.buffered_bytes, 0)

    def test_nul_oversize_and_truncated_rejection(self) -> None:
        with self.assertRaises(NulFrameError):
            DirectFramer().feed(b"PING\x00\n")
        with self.assertRaises(OversizeFrameError):
            DirectFramer(max_frame_size=4).feed(b"12345\n")
        framer = DirectFramer()
        framer.feed(b"PING")
        with self.assertRaises(TruncatedFrameError):
            framer.finish()

    def test_guest_and_host_exact_handshake(self) -> None:
        clock = FakeClock()
        evidence = Evidence("host")
        host = DirectPeer("host", machine="NXT", evidence=evidence, clock=clock)
        guest = DirectPeer("guest", machine="PC", clock=clock)
        exchange(host, guest)
        self.assertTrue(host.ready)
        self.assertTrue(guest.ready)
        self.assertEqual(host.peer_machine, "PC")
        self.assertEqual(guest.peer_machine, "NXT")
        self.assertEqual(host.peer_hello, "HELLO DIRECT GUEST")
        self.assertEqual(guest.peer_hello, "HELLO DIRECT HOST WHITE=HOST")
        self.assertGreaterEqual(host.json()["frames"]["inbound"], 2)
        self.assertGreaterEqual(host.json()["sessions"][0]["frames"]["inbound"], 2)

    def test_duplicate_hello_is_idempotent_but_conflict_fails(self) -> None:
        peer = DirectPeer("guest")
        peer.start()
        peer.drain_outbox()
        peer.feed(b"HELLO DIRECT HOST WHITE=HOST\n")
        peer.drain_outbox()
        peer.feed(b"HELLO DIRECT HOST WHITE=HOST\n")
        self.assertTrue(peer.ready)
        with self.assertRaises(ProtocolError):
            peer.feed(b"HELLO DIRECT HOST WHITE=GUEST\n")

    def test_role_default_ping_cadence(self) -> None:
        self.assertEqual(ProbeConfig(mode="guest").ping_interval, 3.0)
        self.assertEqual(ProbeConfig(mode="host").ping_interval, 0.0)

    def test_ping_ack_delay_accounting(self) -> None:
        clock = FakeClock()
        peer = DirectPeer("guest", ack_delay=0.25, clock=clock, ping_interval=0)
        peer.start()
        peer.drain_outbox()
        peer.feed(b"PING\n", now=clock.now)
        self.assertEqual(peer.drain_outbox(), [])
        clock.now = 0.25
        peer.tick()
        self.assertEqual(peer.drain_outbox(), [b"ACK PING\n"])
        self.assertEqual(peer.json()["ping"]["inbound"], 1)
        self.assertEqual(peer.json()["ping"]["ack_sent"], 1)
        self.assertEqual(peer.json()["ping"]["ack_delayed"], 1)
        self.assertEqual(peer.json()["ping"]["ack_delays_seconds"], [0.25])

    def test_peer_close_and_failure_are_recorded(self) -> None:
        peer = DirectPeer("guest")
        peer.start()
        peer.receive_eof()
        close = peer.json()["close"]
        self.assertTrue(close["peer"])
        self.assertEqual(close["reason"], "peer_eof")

        failed = DirectPeer("guest")
        failed.start()
        with self.assertRaises(NulFrameError):
            failed.feed(b"PING\x00\n")
        self.assertFalse(failed.json()["ok"])
        self.assertEqual(failed.json()["frames"]["nul"], 1)
        self.assertEqual(failed.json()["failures"][0]["code"], "nul_frame")

    def test_reconnect_fields_and_cleanup(self) -> None:
        clock = FakeClock()
        left, right = socket.socketpair()
        try:
            peer = DirectPeer("guest", sock=left, clock=clock)
            peer.start()
            self.assertTrue(peer.drain_outbox() == [])
            # The socket path sends the HELLO directly; the peer can still be
            # closed deterministically without touching the other endpoint.
            peer.close("test_cleanup")
            with self.assertRaises(OSError):
                left.getsockname()
        finally:
            try:
                left.close()
            except OSError:
                pass
            right.close()
        evidence = Evidence("guest")
        evidence.data["reconnect"].update(
            {"configured": 2, "attempts": 1, "succeeded": 1}
        )
        self.assertEqual(evidence.data["reconnect"]["succeeded"], 1)

        all_sockets = [
            ScriptSocket(clock, persistent=False),
            ScriptSocket(clock, persistent=True),
        ]
        sockets = list(all_sockets)
        runner = DirectConformanceRunner(
            ProbeConfig(
                mode="guest",
                duration=1.0,
                handshake_timeout=0.5,
                ping_interval=0,
                reconnect_attempts=1,
                reconnect_backoff=0,
                require_reconnects=1,
            ),
            clock=clock,
            connector=lambda _host, _port: sockets.pop(0),
            poller=RunnerPoller(clock),
        )
        result = runner.run()
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["reconnect"]["attempts"], 1)
        self.assertEqual(result["reconnect"]["succeeded"], 1)
        self.assertTrue(all(sock.closed for sock in all_sockets))

        with self.assertRaises(ValueError):
            ProbeConfig(duration=0)

    def test_guest_can_force_one_reconnect_cycle(self) -> None:
        clock = FakeClock()
        all_sockets = [
            ScriptSocket(clock, persistent=True),
            ScriptSocket(clock, persistent=True),
        ]
        sockets = list(all_sockets)
        runner = DirectConformanceRunner(
            ProbeConfig(
                mode="guest",
                duration=1.0,
                handshake_timeout=0.5,
                ping_interval=0,
                reconnect_attempts=1,
                reconnect_backoff=0,
                reconnect_after=0.2,
                require_reconnects=1,
            ),
            clock=clock,
            connector=lambda _host, _port: sockets.pop(0),
            poller=RunnerPoller(clock),
        )
        result = runner.run()
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["reconnect"]["attempts"], 1)
        self.assertEqual(result["reconnect"]["succeeded"], 1)
        self.assertEqual(len(result["sessions"]), 2)
        self.assertTrue(all(sock.closed for sock in all_sockets))

    def test_artifact_hash_and_atomic_json(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / "artifact.bin"
            artifact.write_bytes(b"spcx")
            expected = hashlib.sha256(b"spcx").hexdigest()
            records = hash_artifacts([artifact])
            self.assertEqual(records[0]["sha256"], expected)
            output = root / "evidence.json"
            atomic_write_json(output, {"artifact": records, "ok": True})
            self.assertEqual(json.loads(output.read_text()), {"artifact": records, "ok": True})
            self.assertEqual(list(root.glob(".evidence.json.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
