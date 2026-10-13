#!/usr/bin/env python3
"""DIRECT burst stress probe for a real Spectrum host.

Connects as the DIRECT guest, plays a scripted game against the human at the
Spectrum and floods it with chat while it is busiest: right after it sends a
move and right after it receives ours. Every burst carries PINGs, so a lost
byte shows up as a missing ``ACK PING`` (or a dropped session) instead of
relying on someone reading the chat panel.

The wire peer is tools/spcx_direct_conformance.DirectPeer; this file only adds
the game script and the bursts.
"""

from __future__ import annotations

import argparse
import json
import select
import socket
import sys
import time
from pathlib import Path
from typing import Optional

if __package__:
    from .spcx_direct_conformance import (
        ConformanceError,
        DirectPeer,
        atomic_write_json,
        encode_frame,
        hash_artifacts,
        send_all,
    )
else:
    from spcx_direct_conformance import (
        ConformanceError,
        DirectPeer,
        atomic_write_json,
        encode_frame,
        hash_artifacts,
        send_all,
    )

CHAT_TEXT_MAX = 42
# Closed Ruy Lopez, Chigorin: 42 legal plies in coordinate form, castling as
# the king's two-square move.
SCRIPT = (
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 "
    "c2c3 e8g8 h2h3 c6a5 b3c2 c7c5 d2d4 d8c7 b1d2 c5d4 c3d4 a5c6 d2b3 a6a5 "
    "c1e3 a5a4 b3d2 c8d7 a1c1 c7b7 d1e2 f8e8 c2d3 a8b8 d4e5 c6e5 f3e5 d6e5"
).split()


def burst_lines(serial: int, count: int, ping_every: int, hint: str) -> list[str]:
    """``count`` numbered full-width chat lines with a PING every few lines.

    The last chat line carries the hint, so it is the one left on screen.
    """

    lines: list[str] = []
    for index in range(count):
        tail = hint if index == count - 1 else ""
        text = f"T{serial + index:04d} {tail}".rstrip()
        lines.append("CHAT " + text.ljust(CHAT_TEXT_MAX, ".")[:CHAT_TEXT_MAX])
        if ping_every and (index + 1) % ping_every == 0:
            lines.append("PING")
    return lines


def hint_for(ply_index: int) -> str:
    """Tell the human which move the script expects next, if any."""

    if ply_index >= len(SCRIPT):
        return "SCRIPT DONE"
    return "PLAY " + SCRIPT[ply_index].upper()


class BurstProbe:
    def __init__(self, sock, args, clock=time) -> None:
        self.sock = sock
        self.args = args
        self.clock = clock
        self.peer = DirectPeer("guest", machine="PC", sock=sock, ping_interval=3.0)
        self.guest_is_white: Optional[bool] = None
        self.ply = 0  # plies applied so far
        self.serial = 1
        self.pending_move: Optional[tuple[int, float]] = None
        self.resend_at = 0.0
        self.report = {
            "bursts": 0,
            "chat_lines": 0,
            "burst_bytes_max": 0,
            "moves_received": 0,
            "moves_sent": 0,
            "our_move_resends": 0,
            "move_ack_seconds": [],
            "events": [],
            "failures": [],
        }
        self.done = False

    def log(self, kind: str, text: str) -> None:
        stamp = round(self.clock.monotonic(), 3)
        self.report["events"].append([stamp, kind, text])
        print(f"{stamp:10.3f} {kind:4} {text}", flush=True)

    def fail(self, text: str) -> None:
        self.report["failures"].append(text)
        self.log("FAIL", text)
        self.done = True

    def send_block(self, lines: list[str]) -> None:
        """One TCP write for the whole block; bookkeeping through the peer."""

        now = self.clock.monotonic()
        wire = b"".join(encode_frame(line) for line in lines)
        send_all(self.sock, wire)
        evidence = self.peer.evidence
        for line in lines:
            evidence.frame_out(encode_frame(line), self.peer.session_index)
            if line == "PING":
                evidence.data["ping"]["outbound"] += 1
                self.peer.pending_pings.append(now)
            elif line.startswith("CHAT "):
                self.report["chat_lines"] += 1
        self.report["burst_bytes_max"] = max(self.report["burst_bytes_max"], len(wire))
        self.log(
            "OUT", f"{len(lines)} lines, {len(wire)} bytes: {lines[0]} .. {lines[-1]}"
        )

    def burst(self, hint: str) -> list[str]:
        lines = burst_lines(self.serial, self.args.lines, self.args.ping_every, hint)
        self.serial += self.args.lines
        self.report["bursts"] += 1
        return lines

    def my_turn(self) -> bool:
        return (self.ply % 2 == 0) == bool(self.guest_is_white)

    def send_my_move(self, prefix: list[str]) -> None:
        """Optional block (burst + ACK) then our move and a burst behind it."""

        if self.ply >= min(len(SCRIPT), self.args.plies):
            if prefix:
                self.send_block(prefix)
            self.log("INFO", "script finished")
            self.done = True
            return
        move = SCRIPT[self.ply]
        self.ply += 1
        block = prefix + [f"MOVE {self.ply} {move}"] + self.burst(hint_for(self.ply))
        self.send_block(block)
        self.pending_move = (self.ply, self.clock.monotonic())
        self.resend_at = self.clock.monotonic() + self.args.resend
        self.report["moves_sent"] += 1

    def handle(self, text: str) -> None:
        if text.startswith("CHAT ") or text in ("PING", "ACK PING"):
            return
        self.log("IN", text)
        if text.startswith("GAME START"):
            self.guest_is_white = text.endswith("WHITE=GUEST")
            self.ply = 0
            opening = ["ACK GAME START"] + self.burst(hint_for(0))
            if self.my_turn():
                self.send_my_move(opening)
            else:
                self.send_block(opening)
            return
        words = text.split()
        if words[0] == "MOVE" and len(words) >= 3 and words[1].isdigit():
            ply = int(words[1])
            if ply <= self.ply:
                # The Spectrum retransmits while our ACK waits in its queue.
                self.report["move_retransmissions"] = (
                    self.report.get("move_retransmissions", 0) + 1
                )
                self.send_block([f"ACK {ply}"])
                return
            if ply != self.ply + 1 or self.ply >= len(SCRIPT):
                self.fail(f"unexpected ply in {text!r}; script is at {self.ply}")
                return
            if words[2].lower() != SCRIPT[self.ply]:
                self.fail(f"off script: got {words[2]}, expected {SCRIPT[self.ply]}")
                return
            self.ply = ply
            self.report["moves_received"] += 1
            # ACK first: the Spectrum then applies its move, and the burst
            # lands while it is loading overlays.
            self.send_my_move([f"ACK {ply}"] + self.burst(""))
            return
        if words[0] in ("ACK", "NACK") and len(words) >= 2 and words[1].isdigit():
            if self.pending_move and int(words[1]) == self.pending_move[0]:
                elapsed = self.clock.monotonic() - self.pending_move[1]
                self.pending_move = None
                if words[0] == "NACK":
                    self.fail(f"our move was rejected: {text!r}")
                else:
                    self.report["move_ack_seconds"].append(round(elapsed, 3))
            return
        if text == "BYE":
            self.fail("peer sent BYE")

    def run(self) -> dict:
        self.peer.start()
        named = False
        deadline = self.clock.monotonic() + self.args.duration
        while not self.done and not self.peer.closed:
            now = self.clock.monotonic()
            if now >= deadline:
                self.log("INFO", "duration reached")
                break
            if self.pending_move and now - self.pending_move[1] > self.args.ack_timeout:
                self.fail(f"no ACK for our ply {self.pending_move[0]}")
                break
            if self.pending_move and now >= self.resend_at:
                # A real peer retransmits an unanswered MOVE; each resend here
                # means the Spectrum dropped or has not reached the last one.
                self.resend_at = now + self.args.resend
                self.report["our_move_resends"] += 1
                self.send_block([f"MOVE {self.ply} {SCRIPT[self.ply - 1]}"])
            if self.peer.ready and not named:
                named = True
                self.send_block(["MNAME BURST"])
                self.log("INFO", "ready; waiting for GAME START on the Spectrum")
            self.peer.tick(now)
            ready, _, _ = select.select([self.sock], [], [], 0.05)
            if not ready:
                continue
            try:
                data = self.sock.recv(4096)
            except BlockingIOError:
                continue
            except OSError as exc:
                self.fail(f"socket error: {exc}")
                break
            if not data:
                self.fail("connection closed by the Spectrum side")
                break
            try:
                frames = self.peer.feed(data, self.clock.monotonic())
            except ConformanceError as exc:
                self.fail(f"wire error: {exc}")
                break
            for frame in frames:
                self.handle(frame.decode("ascii"))
        # Let the last burst's PINGs come back before judging them.
        settle = self.clock.monotonic() + self.args.settle
        while not self.peer.closed and self.clock.monotonic() < settle:
            ready, _, _ = select.select([self.sock], [], [], 0.1)
            if ready:
                try:
                    data = self.sock.recv(4096)
                    if not data:
                        break
                    self.peer.feed(data, self.clock.monotonic())
                except (OSError, ConformanceError):
                    break
        ping = self.peer.evidence.data["ping"]
        self.report["ping_sent"] = ping["outbound"]
        self.report["ping_acked"] = ping["ack_received"]
        self.report["ping_lost"] = ping["outbound"] - ping["ack_received"]
        latencies = ping["ack_latencies_seconds"]
        self.report["ping_latency_max_seconds"] = (
            round(max(latencies), 3) if latencies else None
        )
        self.report["plies"] = self.ply
        if self.report["ping_lost"] and not self.report["failures"]:
            self.report["failures"].append(
                f"{self.report['ping_lost']} PING without ACK"
            )
        self.report["ok"] = not self.report["failures"]
        return self.report

    def pump(self, seconds: float, want_acks: int) -> None:
        """Read until ``want_acks`` PING ACKs have arrived or time runs out."""

        ping = self.peer.evidence.data["ping"]
        end = self.clock.monotonic() + seconds
        while not self.peer.closed and self.clock.monotonic() < end:
            if ping["ack_received"] >= want_acks:
                return
            ready, _, _ = select.select([self.sock], [], [], 0.05)
            if not ready:
                continue
            try:
                data = self.sock.recv(4096)
            except BlockingIOError:
                continue
            if not data:
                self.peer.receive_eof()
                return
            self.peer.feed(data, self.clock.monotonic())

    def sweep(self) -> dict:
        """Find the burst size the Spectrum stops absorbing, without a game.

        ``tail``: N chat lines then one PING; the Spectrum sends nothing until
        it reaches the PING, so a missing ACK means bytes were lost on a pure
        receive. ``mid``: a PING first, so the Spectrum is sending its ACK
        while the N chat lines and the final PING are still arriving.
        """

        ping = self.peer.evidence.data["ping"]
        self.peer.start()
        self.pump(10.0, 10**9)  # handshake and the host's MACH/MNAME
        rows = []
        if self.peer.closed or not self.peer.ready:
            self.report["failures"].append(
                "the Spectrum closed or never answered HELLO"
            )
            self.report["ok"] = False
            return self.report
        for shape in ("tail", "mid"):
            for count in self.args.sweep:
                lines = burst_lines(self.serial, count, 0, f"{shape} {count}")
                self.serial += count
                lines = (["PING"] if shape == "mid" else []) + lines + ["PING"]
                want = lines.count("PING")
                before = ping["ack_received"]
                started = self.clock.monotonic()
                self.send_block(lines)
                # Stay well inside the host's silent-guest timeout.
                self.pump(min(12.0, 2.5 + 1.3 * count), before + want)
                got = ping["ack_received"] - before
                elapsed = round(self.clock.monotonic() - started, 2)
                # A lone PING tells a lost burst from a dead link, catches a
                # late burst ACK and keeps the host from expelling us.
                self.send_block(["PING"])
                self.pump(8.0, before + want + 1)
                total = ping["ack_received"] - before
                row = {
                    "shape": shape,
                    "chat_lines": count,
                    "bytes": sum(len(line) + 1 for line in lines),
                    "pings": want,
                    "acked": got,
                    "seconds": elapsed,
                    "acked_with_check": total,
                    "verdict": "ok"
                    if got == want
                    else "late"
                    if total == want + 1
                    else "lost"
                    if total > 0
                    else "link dead",
                }
                rows.append(row)
                self.log("ROW", json.dumps(row, sort_keys=True))
                if self.peer.closed:
                    self.report["failures"].append("link closed during the sweep")
                    break
                # Unanswered PINGs must not be matched to later ACKs.
                self.peer.pending_pings.clear()
                self.pump(3.0, 10**9)
        self.report["sweep"] = rows
        self.report["ok"] = not self.report["failures"]
        return self.report


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Spectrum DIRECT host address")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--lines", type=int, default=12, help="chat lines per burst")
    parser.add_argument("--ping-every", type=int, default=4)
    parser.add_argument("--plies", type=int, default=len(SCRIPT))
    parser.add_argument("--duration", type=float, default=1800.0)
    parser.add_argument("--ack-timeout", type=float, default=30.0)
    parser.add_argument("--settle", type=float, default=8.0)
    parser.add_argument(
        "--resend", type=float, default=8.0, help="seconds between MOVE retransmissions"
    )
    parser.add_argument(
        "--sweep",
        type=lambda text: [int(item) for item in text.split(",")],
        help="burst sizes for the no-game threshold sweep, e.g. 1,2,3,4,6,8,12",
    )
    parser.add_argument("--label", default="")
    parser.add_argument("--artifact", action="append", default=[])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    sock = socket.create_connection((args.host, args.port), timeout=10.0)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.setblocking(False)
    probe = BurstProbe(sock, args)
    try:
        if args.sweep:
            # No keepalive of our own: a burst is the only thing in flight.
            probe.peer.ping_interval = 0.0
            report = probe.sweep()
        else:
            report = probe.run()
    finally:
        probe.peer.close(reason="probe_done", clean=True)
    report["label"] = args.label
    report["target"] = f"{args.host}:{args.port}"
    report["artifacts"] = hash_artifacts(args.artifact)
    report["wire"] = probe.peer.evidence.to_dict()
    if args.output is not None:
        atomic_write_json(args.output, report)
    summary = {
        k: v for k, v in report.items() if k not in ("events", "wire", "artifacts")
    }
    print(json.dumps(summary, sort_keys=True))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
