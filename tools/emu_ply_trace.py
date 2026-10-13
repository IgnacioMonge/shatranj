#!/usr/bin/env python3
"""Count Classic overlay loads and SD I/O per ply in ZEsarUX/ZXESPEmu.

``run`` boots this checkout's release/SHATRANJ.tap in the ZXESPEmu harness
with a writable SD and the ZRCP port open, sets the Spectrum up as the DIRECT
host (white, SAN), plays a scripted game against a DIRECT guest peer and
prints, for every wire event, the overlays loaded and the SD operations made
until the next one, then the two recovery checkpoint slots it left (killing
the emulator is a power cut). ``analyze`` reprints that from a saved run.

ZEsarUX serves esxDOS reads instantly, so this measures load and I/O counts,
not SD latency. Its esxDOS handler rejects open mode 0x0F, so the journal
(SHATJRNL.TMP) is never created here; on hardware it is written inside the
SAVELOAD visit already counted. macOS AirPlay also listens on *:5000: the
peer connects only after the Spectrum's AT+CIPSERVER, when the modem's
127.0.0.1:5000 socket takes precedence.

usage:
  emu_ply_trace.py run [--zxespemu DIR] [--out DIR] [--plies N] [--zrcp PORT]
  emu_ply_trace.py analyze --out DIR
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from gen_overlay_atlas import ORDER  # noqa: E402
from stj_read import b64_decode, crc16_ccitt, unpack_wire  # noqa: E402

EXTRAS = ["JOURNAL", "RECOVERY"]  # Classic ATLAS_EXTRA
WHITE = ["e2e4", "g1f3", "f1b5", "b5a4", "e1g1", "f1e1", "a4b3", "c2c3"]
BLACK = ["e7e5", "b8c6", "a7a6", "g8f6", "f8e7", "b7b5", "d7d6", "e8g8"]


def atlas_names(build: Path) -> dict[int, str]:
    text = (build / "overlay_atlas_table.asm").read_text()
    offsets = [int(v) for v in re.findall(r"^\s*DW (\d+)", text, re.M)]
    return dict(zip(offsets, ORDER + EXTRAS))


def map_symbol(build: Path, name: str) -> int:
    match = re.search(rf"^{re.escape(name)}\s+= \$([0-9A-F]+)",
                      (build / "SHATRANJ.map").read_text(), re.M)
    if not match:
        raise SystemExit(f"{name} not in build/SHATRANJ.map")
    return int(match.group(1), 16)


class Zrcp:
    def __init__(self, port: int) -> None:
        deadline = time.monotonic() + 30
        while True:
            try:
                self.conn = socket.create_connection(("127.0.0.1", port), 5)
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.5)
        self.conn.settimeout(30)
        self._prompt()

    def _prompt(self) -> str:
        data = bytearray()
        while not data.endswith(b"> "):
            data.extend(self.conn.recv(65536))
        return data.decode(errors="replace")

    def cmd(self, text: str) -> str:
        self.conn.sendall(text.encode() + b"\n")
        return self._prompt()

    def key(self, code: int, gap: float = 0.8, hold: int = 150) -> None:
        self.cmd(f"send-keys-ascii {hold} {code}")
        time.sleep(gap)

    def peek(self, address: int) -> int:
        reply = self.cmd(f"read-memory {address} 1")
        return int(reply.split()[0][:2], 16)

    def until(self, address: int, value: int, keys: list[int], tries: int = 6,
              wait: float = 8.0) -> None:
        """Press ``keys`` until the byte at ``address`` reads ``value``."""
        for _ in range(tries):
            for code in keys:
                self.key(code)
            deadline = time.monotonic() + wait
            while time.monotonic() < deadline:
                if self.peek(address) == value:
                    return
                time.sleep(0.5)
        raise SystemExit(f"setup stuck: byte {address:#06x} never read {value}")


def launch(zxespemu: Path, sd: Path, run_dir: Path, port: int):
    sys.path.insert(0, str(zxespemu))
    import run as harness  # ZXESPEmu/run.py: same modem/emulator pairing

    uart = run_dir / "uart"
    modem = subprocess.Popen(
        [sys.executable, str(harness.ROOT / "zxesp.py"), "--link", str(uart),
         "--bind", "127.0.0.1", "--reported-ip", "127.0.0.1",
         "--wifi-mode", "simulated", "--verbose"],
        stdout=(run_dir / "zxesp.log").open("wb"), stderr=subprocess.STDOUT)
    harness.wait_for_uart(modem, uart)
    command = harness.base_emulator_command("ZXUNO", uart, True) + [
        "--enable-divmmc", "--enable-esxdos-handler", "--esxdos-root-dir",
        str(sd), "--enable-remoteprotocol", "--remoteprotocol-port", str(port),
        "--remoteprotocol-prompt", "ZXESP", "--verbose", "3",
        "--verbose-always-console", str(sd / "SHATRANJ.tap")]
    harness.apply_uart_endpoint(command, uart)
    emulator = subprocess.Popen(
        command, cwd=harness.RESOURCES,
        stdout=(run_dir / "zesarux.log").open("wb"), stderr=subprocess.STDOUT)
    return modem, emulator


def spectrum_sent(modem_log: Path) -> bytes:
    """What the Spectrum wrote to the modem; --verbose logs one byte a line."""
    stream = bytearray()
    for line in modem_log.read_text(errors="replace").splitlines():
        if m := re.match(r"\S+ ZX→ESP (b'.*'|b\".*\")$", line):
            stream += ast.literal_eval(m.group(1))
    return bytes(stream)


def setup_direct_host(zrcp: Zrcp, tz_text: int, cursor: int) -> None:
    zrcp.until(cursor, 1, [13])                 # GAME: CREATE -> LINK row
    zrcp.until(cursor, 4, [ord("p"), 13])       # LINK: DIRECT -> TIME row
    zrcp.key(13, 1.0)                           # TIME: edit, set "+2"
    zrcp.cmd(f"write-memory {tz_text + 1} {ord('2')}")
    zrcp.cmd(f"write-memory {tz_text + 2} 0")
    zrcp.key(13, 2.5)
    zrcp.key(13, 1.0)                           # NAME: "zx"
    zrcp.key(ord("z"), 0.6); zrcp.key(ord("x"), 0.6); zrcp.key(13, 2.5)
    zrcp.key(13, 2.0)                           # COLOR: WHITE
    zrcp.key(ord("p")); zrcp.key(13, 2.0)       # NOTAT: SAN
    for _ in range(3):                          # BOARD, SET, HINTS OFF
        zrcp.key(13, 2.5)
    time.sleep(3.0)
    zrcp.key(ord("p")); zrcp.key(13, 1.0)       # START


def peer(emu_log: Path, events: list, plies: int, stop: threading.Event) -> None:
    from spcx_direct_conformance import DirectPeer, encode_frame, send_all

    def mark(kind: str, text: str) -> None:
        events.append([round(time.monotonic(), 3), kind, text,
                       emu_log.stat().st_size])

    sock = socket.create_connection(("127.0.0.1", 5000), timeout=5)
    sock.setblocking(False)
    link = DirectPeer("guest", machine="PC", sock=sock, ping_interval=6.0)
    link.start()
    reply_at, reply_ply, black = None, 0, 0
    while not stop.is_set() and not link.closed:
        link.tick(time.monotonic())
        if reply_at is not None and time.monotonic() >= reply_at:
            reply_at = None
            line = f"MOVE {reply_ply} {BLACK[black]}"
            black += 1
            send_all(sock, encode_frame(line))
            mark("OUT", line)
        if not select.select([sock], [], [], 0.05)[0]:
            continue
        data = sock.recv(4096)
        if not data:
            break
        for frame in link.feed(data, time.monotonic()):
            text = frame.decode("ascii")
            if text in ("PING", "ACK PING"):
                mark("KEEP", text)
                continue
            mark("IN", text)
            words = text.split()
            if text.startswith("GAME START"):
                send_all(sock, encode_frame("ACK GAME START"))
                mark("OUT", "ACK GAME START")
            elif words[0] == "MOVE":
                send_all(sock, encode_frame(f"ACK {words[1]}"))
                mark("OUT", f"ACK {words[1]}")
                if int(words[1]) < plies and black < len(BLACK):
                    reply_ply, reply_at = int(words[1]) + 1, time.monotonic() + 4
    sock.close()


def run(args) -> None:
    release, build = ROOT / "release", ROOT / "build"
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    sd = Path(tempfile.mkdtemp(prefix="emu-ply-sd-"))
    for name in ("SHATRANJ.tap", "SHATRANJ.OVL", "SHATRANJ.DAT"):
        shutil.copy(release / name, sd / name)
    (sd / "SYS" / "CONFIG").mkdir(parents=True)
    modem, emulator = launch(args.zxespemu, sd, out, args.zrcp)
    events: list = []
    stop = threading.Event()
    try:
        time.sleep(15)                          # boot to CONNECTION SETUP
        zrcp = Zrcp(args.zrcp)
        setup_direct_host(zrcp, map_symbol(build, "_setup_timezone_text"),
                          map_symbol(build, "_setup_cursor"))
        deadline = time.monotonic() + 30
        while b"AT+CIPSERVER=1" not in spectrum_sent(out / "zxesp.log"):
            if time.monotonic() > deadline:
                zrcp.cmd(f"save-screen {out / 'setup-failed.bmp'}")
                raise SystemExit("the Spectrum never opened its DIRECT server;"
                                 f" see {out / 'setup-failed.bmp'}")
            time.sleep(0.5)
        worker = threading.Thread(
            target=peer, args=(out / "zesarux.log", events, args.plies, stop))
        worker.start()
        time.sleep(6)                           # HELLO / MACH / MNAME
        zrcp.key(32, 6.0, hold=200)             # SPACE: start the game
        for move in WHITE[: (args.plies + 1) // 2]:
            zrcp.key(13, 1.5)                   # open the input editor
            for ch in move:
                zrcp.key(ord(ch), 0.5)
            zrcp.key(13, 16.0)                  # peer ACKs and replies in ~4 s
        stop.set()
        worker.join(10)
    finally:
        stop.set()
        emulator.terminate()
        modem.terminate()
        (out / "events.json").write_text(json.dumps(events, indent=1))
        for slot in (sd / "SYS" / "CONFIG").glob("SHATRCV*.RCV"):
            shutil.copy(slot, out / slot.name)
        shutil.rmtree(sd, ignore_errors=True)
    analyze(out, build)


def parse(raw: bytes, names: dict[int, str]) -> list[tuple]:
    out, prev, seek, path = [], None, None, "?"
    for line in raw.decode(errors="replace").splitlines():
        if line == prev:                        # ZEsarUX logs each line twice
            continue
        prev = line
        if m := re.search(r"offset is now at (\d+)", line):
            seek = int(m.group(1))
        elif m := re.search(r"F_READ\. Read (\d+) bytes at ([0-9A-F]+)H", line):
            if m.group(2) == "6800" and seek in names:
                out.append(("LOAD", names[seek]))
            else:
                out.append(("READ", path))
        elif m := re.search(r"fullpath file: (\S+)", line):
            path = m.group(1).rsplit("/", 1)[-1]
        elif "Opening file in system mode" in line:
            out.append(("OPEN", path))
        elif "F_Write" in line or "F_WRITE" in line:
            out.append(("WRITE", path))
        elif "F_SYNC" in line:
            out.append(("SYNC", path))
    return out


def analyze(out: Path, build: Path) -> None:
    names = atlas_names(build)
    raw = (out / "zesarux.log").read_bytes()
    events = json.loads((out / "events.json").read_text())
    for here, there in zip(events, events[1:]):
        seq = parse(raw[here[3]:there[3]], names)
        loads = [name for kind, name in seq if kind == "LOAD"]
        if not loads and not seq:
            continue
        io = {k: sum(1 for kind, _ in seq if kind == k)
              for k in ("OPEN", "READ", "WRITE", "SYNC")}
        print(f"{here[1]:>4} {here[2]:<24} loads={len(loads):2d} "
              + " ".join(f"{k.lower()}={v}" for k, v in io.items())
              + " :: " + " ".join(loads))
    for slot in sorted(out.glob("SHATRCV*.RCV")):
        data = slot.read_bytes()
        if (len(data) != 73 or data[:4] != b"SRC2" or
                crc16_ccitt(data[:71]) != int.from_bytes(data[71:], "little")):
            print(f"{slot.name}: invalid")
            continue
        wire = b64_decode(data[6:66])
        state = unpack_wire(wire) if wire else "bad Base64URL"
        ply = state["ply"] if isinstance(state, dict) else state
        print(f"{slot.name}: generation {int.from_bytes(data[4:6], 'little')}"
              f" ply {ply} flags 0x{data[70]:02x}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="mode", required=True)
    r = sub.add_parser("run")
    r.add_argument("--zxespemu", type=Path,
                   default=ROOT.parent / "ZXESPEmu")
    r.add_argument("--out", type=Path, default=ROOT / "build" / "emu-ply-trace")
    r.add_argument("--plies", type=int, default=16)
    r.add_argument("--zrcp", type=int, default=10099)
    a = sub.add_parser("analyze")
    a.add_argument("--out", type=Path, default=ROOT / "build" / "emu-ply-trace")
    args = parser.parse_args()
    if args.mode == "run":
        run(args)
    else:
        analyze(args.out, ROOT / "build")


if __name__ == "__main__":
    main()
