#!/usr/bin/env python3
"""Run the actual SAVELOAD statistics reducer against the portable C oracle."""

import ctypes
import _ctypes
from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import tempfile

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


SIZE, FLAGS = 33, 18  # common/chess/game_stats.h


class Stats(ctypes.Structure):
    _fields_ = [("bytes", ctypes.c_uint8 * SIZE)]


def main():
    with tempfile.TemporaryDirectory(prefix="game-stats-") as directory, ExitStack() as cleanup:
        tmp = Path(directory)
        library = tmp / "oracle.dll"
        subprocess.run([
            required_tool("gcc"), "-shared", "-fPIC", "-Isrc",
            "src/common/chess/game_stats.c", "-o", str(library),
        ], cwd=ROOT, check=True)
        oracle = ctypes.CDLL(str(library))
        cleanup.callback(_ctypes.FreeLibrary if os.name == "nt" else _ctypes.dlclose,
                         oracle._handle)
        oracle.game_stats_reset.argtypes = [ctypes.POINTER(Stats), ctypes.c_uint16, ctypes.c_uint8]
        oracle.game_stats_record.argtypes = [ctypes.POINTER(Stats), ctypes.c_uint16,
                                           ctypes.c_uint32, ctypes.c_uint8]
        oracle.game_stats_takeback.argtypes = [ctypes.POINTER(Stats), ctypes.c_uint16, ctypes.c_uint8]
        stats = Stats()
        # kind, committed ply, seconds, captured type, en-passant
        cases = [("start", 0, 0, 0, False),
                 ("move", 1, 0, 0, False),
                 ("move", 2, 18, 1, False),
                 ("move", 3, 190, 5, False),
                 ("move", 4, 18, 1, True),
                 ("undo", 3, 0, 1, True),
                 ("move", 4, 108, 4, False),
                 ("restore", 43, 0, 0, False),
                 ("move", 44, 108, 1, True),
                 ("undo", 43, 0, 1, True),
                 ("move", 44, 65535, 0, False),
                 ("move", 45, 1, 0, False),
                 ("move", 46, 1, 0, False),
                 ("undo", 45, 0, 0, False),
                 ("restore", 65533, 0, 0, False),
                 ("move", 65534, 65536, 5, False),
                 ("move", 65535, 359999, 2, False),
                 ("undo", 65534, 0, 2, False),
                 ("undo", 65533, 0, 0, False),
                 ("start", 0, 0, 0, False)]
        captures = [5, 4, 4, 3, 3, 2, 2] + [1] * 8
        cases += [("move", ply, 1, captures[(ply - 1) // 2], False)
                  for ply in range(1, 31)]
        cases += [("undo", 29, 0, 1, False)]
        # FASTEST: first move, strict minimum, ties, undo of a new minimum.
        cases += [("start", 0, 0, 0, False),
                  ("move", 1, 30, 0, False),
                  ("move", 2, 9, 0, False),
                  ("move", 3, 12, 0, False),
                  ("move", 4, 40, 0, False),
                  ("move", 5, 12, 0, False),
                  ("move", 6, 5, 0, False),
                  ("undo", 5, 0, 0, False),
                  ("move", 6, 9, 0, False),
                  ("move", 7, 3600, 0, False)]
        cases += [("start", 0, 0, 0, False),
                  ("move", 1, 11, 5, False),
                  ("move", 2, 22, 2, False),
                  ("missing", 3, 0, 0, False),
                  ("undo", 2, 0, 4, False),
                  ("move", 3, 33, 3, False),
                  ("missing", 4, 0, 0, False),
                  ("move", 5, 55, 5, False),
                  ("undo", 4, 0, 5, False),
                  ("missing", 5, 0, 0, False),
                  ("restore", 43, 0, 0, False),
                  ("move", 44, 18, 2, False)]
        code, expected = [], []
        pending = False
        for kind, ply, seconds, captured, ep in cases:
            if kind == "start":
                pending = True
                code.append(" ld a,0x80\n ld (_spectrum_journal_pend),a")
                continue
            if kind == "missing":
                # The resident hook sets this when page-in skipped capture;
                # the following product reducer must consume it safely.
                stats.bytes[FLAGS] = 0x41
                code.append(f"""
 ld a,0x41
 ld (_spectrum_game_stats+{FLAGS}),a
 ld hl,_spectrum_game_stats
 ld de,sample_{len(expected)}
 ld bc,{SIZE}
 ldir
""")
                expected.append(bytes(stats))
                continue
            if kind == "restore":
                oracle.game_stats_reset(ctypes.byref(stats), ply, 1)
                context = 1
            elif kind == "undo":
                oracle.game_stats_takeback(ctypes.byref(stats), ply, captured)
                context = 0
            else:
                if pending:
                    oracle.game_stats_reset(ctypes.byref(stats), ply - 1, 0)
                    pending = False
                elif stats.bytes[FLAGS] & 0x40:
                    oracle.game_stats_reset(ctypes.byref(stats), ply - 1, 1)
                oracle.game_stats_record(ctypes.byref(stats), ply, seconds, captured)
                context = 0x6000
            hour, remainder = divmod(seconds, 3600)
            minute, second = divmod(remainder, 60)
            piece = ord(".pnbrq"[captured]) if not ep else ord('.')
            ep_square = 20 if ep else 255
            code.append(f"""
 ld hl,{ply}
 ld (_netchesszx_session_state),hl
 ld hl,{context}
 ld (0x5fe0),hl
 ld a,{hour}
 ld (test_time+8),a
 ld a,{minute}
 ld (test_time+9),a
 ld a,{second}
 ld (test_time+10),a
 ld a,29
 ld (_spectrum_stats_undo),a
 ld a,20
 ld (_spectrum_stats_undo+1),a
 ld a,{piece}
 ld (_spectrum_stats_undo+2),a
 ld a,{ep_square}
 ld (_spectrum_stats_undo+4),a
 ld a,'P'
 ld (0x5f60+20),a
 ld (0x5f60+29),a
 ld a,1
 ld (0x5fe7),a
 call _game_stats_append
 ld a,(0x5fe7)
 or a
 jp nz,test_capture_missing
 xor a
 ld (_spectrum_journal_pend),a
 ld hl,_spectrum_game_stats
 ld de,sample_{len(expected)}
 ld bc,{SIZE}
 ldir
""")
            expected.append(bytes(stats))
        harness = tmp / "vector.asm"
        harness.write_text("""SECTION code_user
PUBLIC test_start,test_done,test_result
PUBLIC _spectrum_game_stats,_spectrum_stats_undo,_spectrum_journal_pend
PUBLIC _netchesszx_session_state,_spectrum_time_snapshot
EXTERN _game_stats_append
_spectrum_game_stats EQU 0x65f5
test_start:
 ld sp,0xff00
 ld ix,0x1234
 ld iy,0x5c3a
""" + "\n".join(code) + """
 ld (test_ix),ix
 ld (test_iy),iy
 xor a
 ld (test_result),a
 jp test_done
test_capture_missing:
 ld a,1
 ld (test_result),a
test_done: halt
_spectrum_time_snapshot: ld hl,test_time
 ret
test_result: defb 255
test_ix: defw 0
test_iy: defw 0
test_time: defs 11
_spectrum_stats_undo: defs 5
_netchesszx_session_state: defw 0
_spectrum_journal_pend: defb 0
""" + "\n".join(f"sample_{i}: defs {SIZE}" for i in range(len(expected))),
                           encoding="ascii")
        binary = tmp / "vector.bin"
        mapping = assemble_vector(required_tool("z88dk-z80asm"), harness,
                                  [ROOT / "asm/overlay/saveload/game_stats.asm"], binary)
        result, output, _, _, _ = execute_vector(required_tool("z88dk-ticks"),
                                                      binary, mapping, counter=3000000)
        assert result == 0, output
        memory = binary.with_suffix(".ram").read_bytes()
        for i, target in enumerate(expected):
            addr = symbol_address(mapping, f"sample_{i}")
            assert memory[addr:addr+SIZE] == target, (i, memory[addr:addr+SIZE].hex(), target.hex())
        for name, value in (("test_ix", 0x1234), ("test_iy", 0x5c3a)):
            addr = symbol_address(mapping, name)
            assert int.from_bytes(memory[addr:addr+2], "little") == value, name
        print(f"[OK] game stats: {len(expected)} Z80/C snapshots, capture/undo/restore/limits/fastest/30 captures/MISSING")
        print(f"[INFO] capture ASM: {symbol_address(mapping, 'gs_sample') + 2 - symbol_address(mapping, '_game_stats_append')} bytes")


if __name__ == "__main__":
    main()
