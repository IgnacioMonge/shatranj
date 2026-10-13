#!/usr/bin/env python3
"""Compile and execute the production recovery CAPTURE path on a Z80."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT,
    assemble_vector,
    execute_vector,
    required_tool,
    sdcc_ix_helpers,
    symbol_address,
)

sys.path.insert(0, str(ROOT))
from tools.stj_read import b64_decode, unpack_wire


START = (
    b"rnbqkbnr"
    b"pppppppp"
    b"........"
    b"........"
    b"........"
    b"........"
    b"PPPPPPPP"
    b"RNBQKBNR"
)
SNAPSHOT = START + bytes((1, 15, 0xFF))
# Payloads of SNAPSHOT at ply 17 when RECOVERY still encoded the BOARD
# overlay's copy (03ea62c): ACTIVE|CHECK, and RETIRE's ACTIVE|GAME_OVER.
GOLDEN_ACTIVE_CHECK = b"uazam4iIiIgAAAAAAAAAAAAAAAAAAAAAEREREUI1YyR__xEABQECAwQFBgGL"
GOLDEN_RETIRE = b"uazam4iIiIgAAAAAAAAAAAAAAAAAAAAAEREREUI1YyR__xEAAwECAwQFBgHh"


def compile_unit(compiler: str, source: Path, output: Path, *extra: str) -> None:
    command = [
        compiler,
        "+z80",
        "-vn",
        "-clib=sdcc_iy",
        "-SO3",
        "-compiler=sdcc",
        "-Cs--no-reg-params",
        "--opt-code-size",
        "--fomit-frame-pointer",
        "-DNETCHESSZX_SDCC_IY",
        "-DNETCHESSZX_RECOVERY",
        "-DNETCHESSZX_FIXED_LOW_RAM",
        f"-I{ROOT / 'src'}",
        f"-custom-copt-rules={ROOT / 'tools/netchesszx_overlay_copt'}",
        *extra,
        "-S",
        str(source),
        "-o",
        str(output),
    ]
    run = subprocess.run(
        command,
        cwd=output.parent,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert run.returncode == 0, run.stdout + run.stderr


def add_size_markers(path: Path, prefix: str) -> None:
    path.write_text(
        path.read_text(encoding="utf-8")
        + f"""
SECTION code_compiler
PUBLIC {prefix}_code_end
{prefix}_code_end:
SECTION bss_compiler
PUBLIC {prefix}_bss_end
{prefix}_bss_end:
SECTION rodata_compiler
PUBLIC {prefix}_rodata_end
{prefix}_rodata_end:
""",
        encoding="utf-8",
    )


HARNESS = r"""
SECTION code_user
EXTERN _recovery_capture_ovl, _recovery_retire_ovl
PUBLIC _recovery_write_ovl_entry, _spectrum_time_snapshot
PUBLIC _side_to_move, _castle_rights, _ep_square
PUBLIC _netchesszx_session_state, _netchesszx_host_color
PUBLIC _spectrum_board_view_flipped

test_start:
    ld sp, 0xff00
    ld iy, 0x5c3a
    ld hl, snapshot_initial                 ; live position in low RAM
    ld de, 0x5f60                           ; NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR
    ld bc, 64
    ldir
    ld a, (hl)
    ld (_side_to_move), a
    inc hl
    ld a, (hl)
    ld (_castle_rights), a
    inc hl
    ld a, (hl)
    ld (_ep_square), a
    ld hl, workspace_cache
    ld de, workspace_cache + 1
    ld bc, 59
    ld (hl), 0xa5
    ldir
    ld hl, 17
    ld (_netchesszx_session_state), hl
    ld a, 1
    ld (_netchesszx_host_color), a
    ld (_spectrum_board_view_flipped), a
    ; Checkpoint: WRITE sees the live position encoded in the record.
    xor a
    ld (write_fail), a
    call prepare_context
    call _recovery_capture_ovl
    ld (success_hl), hl
    ld a, (0x5fe4)
    ld (success_status), a
    ld hl, (0x5fe2)
    ld (success_payload_ptr), hl
    ld a, (0x5fe6)
    ld (success_link), a
    ld hl, saved_payload
    ld de, checkpoint_payload
    ld bc, 60
    ldir

    ; SAVE uses the same live encode but writes to the caller's buffer.
    call prepare_context
    ld hl, manual_payload
    ld (0x5fe0), hl
    ld hl, 0x5fe0
    call _recovery_capture_ovl
    ld (manual_hl), hl
    ld a, (0x5fe4)
    ld (manual_status), a
    ld a, (write_calls)
    ld (manual_write_calls), a
    ld hl, (0x5fe2)
    ld (manual_context_ptr), hl

    ; An invalid piece on the board makes the encoder fail before WRITE.
    ld a, '?'
    ld (0x5f60), a
    call prepare_context
    call _recovery_capture_ovl
    ld (encoder_hl), hl
    ld a, (0x5fe4)
    ld (encoder_status), a
    ld hl, (0x5fe2)
    ld (encoder_context_ptr), hl
    ld a, (write_calls)
    ld (encoder_write_calls), a
    ld a, (0x5f60)
    ld (encoder_piece_after), a

    ; A valid encode propagates WRITE's failure result unchanged.
    ld a, 'r'
    ld (0x5f60), a
    ld a, 1
    ld (write_fail), a
    call prepare_context
    call _recovery_capture_ovl
    ld (write_hl), hl
    ld a, (0x5fe4)
    ld (write_status), a
    ld a, (write_calls)
    ld (final_write_calls), a

    ; RETIRE after a chosen exit: nothing outside a live game; a live game
    ; becomes one WRITE of the exact position marked OVER, never offered,
    ; into the record even when the context still holds an output pointer.
    xor a
    ld (write_fail), a
    ld hl, 0x3412
    ld (0x5fe0), hl
    ld hl, 0x5fe0
    call _recovery_retire_ovl
    ld (retire_idle_hl), hl
    ld a, (write_calls)
    ld (retire_idle_calls), a
    ld a, 1
    ld (_netchesszx_session_state + 11), a  ; game_status_active
    ld a, 0xff
    ld (0x5fe6), a
    ld hl, 0x3412
    ld (0x5fe0), hl
    ld hl, 0x5fe0
    call _recovery_retire_ovl
    ld (retire_hl), hl
    ld a, (write_calls)
    ld (retire_live_calls), a
    ld a, (0x5fe6)
    ld (retire_link), a
    ld hl, (observed_payload_ptr)
    ld (retire_payload_ptr), hl
    ld hl, saved_payload
    ld de, retire_payload
    ld bc, 60
    ldir
    ld (final_sp), sp
    xor a
    ld (test_result), a
test_done:
    jp test_done

prepare_context:
    xor a
    ld (0x5fe0), a
    ld (0x5fe1), a
    ld hl, 0x1234                           ; BUF is WRITE's input, never CAPTURE's
    ld (0x5fe2), hl
    ld a, 5
    ld (0x5fe5), a
    ld a, 0xff
    ld (0x5fe4), a
    ld hl, 0x5fe0
    ret

_spectrum_time_snapshot:
    ld hl, test_time
    ret


_recovery_write_ovl_entry:
    ld hl, (0x5fe2)
    ld (observed_payload_ptr), hl
    ld de, saved_payload
    ld bc, 60
    ldir
    ld hl, write_calls
    inc (hl)
    ld a, (write_fail)
    or a
    jr nz, write_rejected
    xor a
    ld (0x5fe4), a
    ld hl, 1
    ret
write_rejected:
    ld a, 3
    ld (0x5fe4), a
    ld hl, 0
    ret

test_result: DEFB 255
success_hl: DEFW 0
encoder_hl: DEFW 0
write_hl: DEFW 0
manual_hl: DEFW 0
success_status: DEFB 0
encoder_status: DEFB 0
write_status: DEFB 0
manual_status: DEFB 0
success_payload_ptr: DEFW 0
success_link: DEFB 0
observed_payload_ptr: DEFW 0
encoder_context_ptr: DEFW 0
write_calls: DEFB 0
encoder_write_calls: DEFB 0
final_write_calls: DEFB 0
manual_write_calls: DEFB 0
manual_context_ptr: DEFW 0
encoder_piece_after: DEFB 0
write_fail: DEFB 0
final_sp: DEFW 0
_side_to_move: DEFB 0
_castle_rights: DEFB 0
_ep_square: DEFB 0
retire_idle_hl: DEFW 0
retire_idle_calls: DEFB 0
retire_hl: DEFW 0
retire_live_calls: DEFB 0
retire_link: DEFB 0
retire_payload_ptr: DEFW 0
retire_payload: DEFS 60
checkpoint_payload: DEFS 60
_netchesszx_session_state: DEFS 21
_netchesszx_host_color: DEFB 0
_spectrum_board_view_flipped: DEFB 0
test_time: DEFB 0,0,0,0,0,1,2,3,4,5,6
snapshot_initial:
    DEFM "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR"
    DEFB 1,15,255
workspace_cache: DEFS 60
saved_payload: DEFS 60
manual_payload: DEFS 60
"""


# Overlay scratch per target (src/spectrum/lowram_map.h): the record is
# encoded at +6 and the live position staged at +73. On Next the store keeps
# its own buffer in overlay BSS, so +73 of the 0x3C78 scratch is unused there.
TARGETS = (
    ("Classic", 0x672B, ()),
    ("Next", 0x3C78, ("-DNETCHESSZX_NEXT", "-DNETCHESSZX_NEXT_BANKING")),
)


def run_target(label: str, scratch: int, defines: tuple[str, ...]) -> None:
    compiler = required_tool("zcc")
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    with tempfile.TemporaryDirectory(prefix="recovery_capture_z80_") as name:
        tmp = Path(name)
        recovery_asm = tmp / "recovery_ovl.asm"
        restore_asm = tmp / "restore_build.asm"
        compile_unit(
            compiler,
            ROOT / "src/spectrum/overlay/recovery_ovl.c",
            recovery_asm,
            *defines,
        )
        compile_unit(
            compiler,
            ROOT / "src/spectrum/overlay/restore_ovl.c",
            restore_asm,
            "-DNETCHESSZX_RESTORE_BUILD_ONLY",
            *defines,
        )
        add_size_markers(recovery_asm, "capture")
        add_size_markers(restore_asm, "restore")
        harness = tmp / "recovery_capture_vector.asm"
        harness.write_text(HARNESS, encoding="ascii")
        binary = tmp / "recovery_capture_vector.bin"
        symbols = assemble_vector(
            assembler,
            harness,
            [recovery_asm, restore_asm, *sdcc_ix_helpers(assembler)],
            binary,
            ["__SDCC_IX"],
        )
        result, output, pc, _, _ = execute_vector(
            emulator, binary, symbols, counter=2_000_000
        )
        assert result == 0 and pc == symbol_address(symbols, "test_done"), output
        ram = binary.with_suffix(".ram").read_bytes()

        def address(label: str) -> int:
            return symbol_address(symbols, label)

        def value(label: str, size: int = 1) -> int:
            start = address(label)
            return int.from_bytes(ram[start : start + size], "little")

        def data(label: str, size: int) -> bytes:
            start = address(label)
            return bytes(ram[start : start + size])

        success = value("success_hl", 2) & 0xFF, value("success_status")
        encoder = value("encoder_hl", 2) & 0xFF, value("encoder_status")
        write = value("write_hl", 2) & 0xFF, value("write_status")
        manual = value("manual_hl", 2) & 0xFF, value("manual_status")
        assert success == (1, 0), success
        assert manual == (1, 0), manual
        assert value("manual_write_calls") == 1
        # SAVE returns before WRITE and never touches WRITE's BUF input.
        assert value("manual_context_ptr", 2) == 0x1234
        # SAVE and the checkpoint encode the same live position to the bytes
        # the BOARD-copied snapshot produced before (golden from 03ea62c).
        assert data("checkpoint_payload", 60) == GOLDEN_ACTIVE_CHECK
        assert data("manual_payload", 60) == GOLDEN_ACTIVE_CHECK
        assert encoder == (0, 1), encoder
        assert write == (0, 3), write
        assert (value("encoder_write_calls"), value("final_write_calls")) == (1, 2)
        assert value("encoder_piece_after") == ord("?")
        assert value("encoder_context_ptr", 2) == 0x1234
        assert value("final_sp", 2) == 0xFF00
        # Capturing reads the live position and leaves it untouched.
        assert bytes(ram[0x5F60 : 0x5F60 + 64]) + bytes(
            (value("_side_to_move"), value("_castle_rights"), value("_ep_square"))
        ) == SNAPSHOT
        # It is staged in RCV_TMP, clear of the record encoded at RCV_REC + 6.
        assert bytes(ram[scratch + 73 : scratch + 73 + 67]) == SNAPSHOT
        assert data("workspace_cache", 60) == bytes([0xA5]) * 60

        # The checkpoint snapshot is encoded in place at RCV_REC + 6 in the
        # overlay scratch (asm/overlay/recovery/rcv_layout.inc).
        payload_ptr = value("success_payload_ptr", 2)
        assert payload_ptr == scratch + 6, hex(payload_ptr)
        # C supplies only PLAYED (ply 17); WRITE adds the resident bits.
        assert value("success_link") == 0x10, value("success_link")
        wire = b64_decode(data("checkpoint_payload", 60))
        assert wire is not None
        decoded = unpack_wire(wire)
        assert isinstance(decoded, dict), decoded
        assert decoded == {
            "cells": START.decode("ascii"),
            "side": "negras",
            "host": "negras",
            "castle": "KQkq",
            "flipped": True,
            "ep": "-",
            "ply": 17,
            "flags": 5,
            "state": "activa, jaque",
            "game_clock": "01:02:03",
            "move_clock": "04:05:06",
        }

        # RETIRE: no WRITE outside a live game, then one unoffered OVER record.
        assert value("retire_idle_hl", 2) & 0xFF == 1
        assert value("retire_idle_calls") == 2, value("retire_idle_calls")
        assert value("retire_hl", 2) & 0xFF == 1
        assert value("retire_live_calls") == 3, value("retire_live_calls")
        assert value("retire_link") == 0, value("retire_link")
        assert value("retire_payload_ptr", 2) == scratch + 6
        assert data("retire_payload", 60) == GOLDEN_RETIRE
        retired = unpack_wire(b64_decode(data("retire_payload", 60)))
        assert isinstance(retired, dict), retired
        assert (retired["cells"], retired["ply"], retired["flags"]) == (
            START.decode("ascii"), 17, 3), retired

        capture_code = address("capture_code_end") - address(
            "_recovery_capture_live_ovl"
        )
        restore_code = address("restore_code_end") - address("_restore_find_piece")
        restore_bss = address("restore_bss_end") - address("_restore_wire")
        restore_rodata = address("restore_rodata_end") - address("_restore_piece_table")
        combined = capture_code + restore_code + restore_bss + restore_rodata
        print(
            f"Recovery CAPTURE production-C Z80 vector passed ({label}, "
            f"{combined} synthetic code/rodata/BSS bytes)"
        )


def main() -> int:
    for label, scratch, defines in TARGETS:
        run_target(label, scratch, defines)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
