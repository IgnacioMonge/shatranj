#!/usr/bin/env python3
"""Execute the recovery checkpoint store and resume stage in the Z80 emulator.

The product ASM (RECOVERY write entry, shared slot store and the CONTROL resume
entry) runs against a small multi-handle esxDOS model with fault injection.
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT,
    assemble_vector,
    execute_vector,
    required_tool,
    symbol_address,
)

F_PARTIAL = 0x01
F_UNLINKED = 0x02
F_HOST = 0x04
F_BLACK = 0x08
F_PLAYED = 0x10
OFFER = F_HOST | F_PLAYED
SCRATCH = 0x672B
FILES = ("slot0", "slot1", "tmp", "new")
FILE_BASE = 0xC000
FILE_SPAN = 0x200


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else crc << 1
    return crc


def record(
    sequence: int,
    snapshot: bytes,
    count: int = 0,
    body_crc: int = 0xFFFF,
    flags: int = OFFER,
    magic: bytes = b"SRC2",
) -> bytes:
    data = (
        magic
        + sequence.to_bytes(2, "little")
        + snapshot
        + count.to_bytes(2, "little")
        + body_crc.to_bytes(2, "little")
        + bytes((flags,))
    )
    assert len(data) == 71
    return data + crc16(data).to_bytes(2, "little")


def body(records: int, seed: int = 0) -> bytes:
    return b"".join(
        (((i * 7 + seed) & 0x3F) | ((i * 5 + 11 + seed) & 0x3F) << 6).to_bytes(2, "little")
        for i in range(records)
    )


def asm_bytes(data: bytes) -> str:
    return "\n".join(
        "    DEFB " + ",".join(str(value) for value in data[i : i + 16])
        for i in range(0, len(data), 16)
    ) or "    DEFB 0"


FS_MODEL = r"""
FSB EQU 0xc000

; esxDOS pages DivMMC over 0x0000..0x3FFF during RST 8 (Next also maps ROM
; into MMU1): a path or buffer there is not the caller's memory.
fs_check_ix:
    push af
    push ix
    pop af
    cp 0x40
    jr nc, fs_check_ix_ok
    ld a, (fs_low_buffers)
    inc a
    ld (fs_low_buffers), a
fs_check_ix_ok:
    pop af
    ret

test_rom:
    ld (fs_arg_a), a
    ex (sp), hl
    ld a, (hl)
    inc hl
    ex (sp), hl
    cp 0x9a
    jp z, fs_open
    cp 0x9b
    jp z, fs_close
    cp 0x9c
    jp z, fs_sync
    cp 0x9d
    jp z, fs_read
    cp 0x9e
    jp z, fs_write
    cp 0xad
    jp z, fs_unlink
    cp 0xaa
    jp z, fs_ok
    ld a, 9
    scf
    ret

; IX = path -> A = file index, NC; C when unknown.
fs_path:
    ld b, 0
    ld de, fs_names
fs_path_next:
    push ix
    pop hl
fs_path_cmp:
    ld a, (de)
    cp (hl)
    jr nz, fs_path_skip
    inc de
    inc hl
    or a
    jr nz, fs_path_cmp
    ld a, b
    or a
    ret
fs_path_skip:
    ld a, (de)
    inc de
    or a
    jr nz, fs_path_skip
    inc b
    ld a, b
    cp 4
    jr c, fs_path_next
    scf
    ret

; A = index -> HL = file header (exists, length, data at +16).
fs_file_addr:
    add a, a
    add a, FSB / 256
    ld h, a
    ld l, 0
    ret

; A = index -> HL = position word.
fs_pos_addr:
    add a, a
    ld l, a
    ld h, 0
    ld de, fs_pos
    add hl, de
    ret

fs_open:
    call fs_check_ix
    ld a, b
    ld (fs_mode), a
    ld hl, fs_open_calls
    inc (hl)
    ld a, (fs_fail_open_call)
    cp (hl)
    jr z, fs_open_injected
    call fs_path
    jr c, fs_open_bad_path
    ld (fs_idx), a
    call fs_file_addr
    ld a, (fs_mode)
    and 0x0c
    jr z, fs_open_existing
    ld (hl), 1
    inc hl
    xor a
    ld (hl), a
    inc hl
    ld (hl), a
    jr fs_open_ok
fs_open_existing:
    ld a, (hl)
    or a
    jr z, fs_open_missing
fs_open_ok:
    ld a, (fs_idx)
    call fs_pos_addr
    xor a
    ld (hl), a
    inc hl
    ld (hl), a
    ld a, (fs_idx)             ; raw descriptor = index; zero stays valid
    or a
    ret
fs_open_missing:
    ld a, 5
    scf
    ret
fs_open_injected:
    ld a, (fs_fail_open_code)
    scf
    ret
fs_open_bad_path:
    ld hl, fs_bad_paths
    inc (hl)
    ld a, 9
    scf
    ret

fs_close:
    ld hl, fs_close_calls
    inc (hl)
    ld a, (fs_fail_close_call)
    cp (hl)
    jr nz, fs_ok
    ld a, 9
    scf
    ret

fs_sync:
    ld a, (fs_fail_sync)
    or a
    jr z, fs_ok
    xor a
    ld (fs_fail_sync), a
    ld a, (fs_arg_a)
    call fs_file_addr
    inc hl
    ld (hl), 20                ; model a torn, non-durable target
    inc hl
    ld (hl), 0
    ld a, 9
    scf
    ret
fs_ok:
    xor a
    ret

fs_unlink:
    call fs_check_ix
    call fs_path
    jr c, fs_unlink_bad
    call fs_file_addr
    ld a, (hl)
    or a
    jr z, fs_unlink_missing
    ld (hl), 0
    ld hl, fs_unlinks
    inc (hl)
    xor a
    ret
fs_unlink_bad:
    ld hl, fs_bad_paths
    inc (hl)
fs_unlink_missing:
    ld a, 5
    scf
    ret

; DE = position -> HL = data address of the file in fs_idx.
fs_data_at:
    ld a, (fs_idx)
    call fs_file_addr
    push de
    ld de, 16
    add hl, de
    pop de
    add hl, de
    ret

; Advance the fs_idx position by fs_n; DE = new position.
fs_advance:
    ld a, (fs_idx)
    call fs_pos_addr
    ld e, (hl)
    inc hl
    ld d, (hl)
    push hl
    ex de, hl
    ld de, (fs_n)
    add hl, de
    ex de, hl
    pop hl
    ld (hl), d
    dec hl
    ld (hl), e
    ret

fs_read:
    call fs_check_ix
    ld hl, fs_read_calls
    inc (hl)
    ld a, (fs_fail_read_call)
    cp (hl)
    jp z, fs_io_injected
    ld a, (fs_arg_a)
    ld (fs_idx), a
    push bc
    call fs_file_addr
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)                 ; DE = length
    push de
    ld a, (fs_idx)
    call fs_pos_addr
    ld e, (hl)
    inc hl
    ld d, (hl)                 ; DE = position
    pop hl
    or a
    sbc hl, de                 ; HL = available
    jr nc, fs_read_available
    ld hl, 0
fs_read_available:
    pop bc
    push hl
    or a
    sbc hl, bc
    pop hl
    jr c, fs_read_count
    ld h, b
    ld l, c
fs_read_count:
    ld (fs_n), hl
    call fs_data_at            ; DE still = position
    push ix
    pop de
    ld bc, (fs_n)
    ld a, b
    or c
    jr z, fs_read_done
    ldir
fs_read_done:
    call fs_advance
    ld bc, (fs_n)
    xor a
    ret

fs_write:
    call fs_check_ix
    ld hl, fs_write_calls
    inc (hl)
    ld a, (fs_arg_a)
    ld (fs_idx), a
    ld a, (fs_fail_write_call)
    cp (hl)
    jr nz, fs_write_all
    ld a, (fs_fail_write_bytes)
    ld c, a
    ld b, 0
    call fs_write_copy
fs_io_injected:
    ld a, 9
    scf
    ret
fs_write_all:
    call fs_write_copy
    ld bc, (fs_n)
    xor a
    ret
fs_write_copy:
    ld (fs_n), bc
    ld a, (fs_idx)
    call fs_pos_addr
    ld e, (hl)
    inc hl
    ld d, (hl)
    call fs_data_at
    ex de, hl
    push ix
    pop hl
    ld bc, (fs_n)
    ld a, b
    or c
    jr z, fs_write_len
    ldir
fs_write_len:
    call fs_advance            ; DE = new position
    ld a, (fs_idx)
    call fs_file_addr
    inc hl
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a                    ; HL = length
    or a
    sbc hl, de
    ret nc
    ld a, (fs_idx)
    call fs_file_addr
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    ret

fs_arg_a: DEFB 0
fs_mode: DEFB 0
fs_idx: DEFB 0
fs_n: DEFW 0
fs_pos: DEFS 8
fs_open_calls: DEFB 0
fs_read_calls: DEFB 0
fs_write_calls: DEFB 0
fs_close_calls: DEFB 0
fs_unlinks: DEFB 0
fs_fail_open_call: DEFB FAIL_OPEN_CALL
fs_fail_open_code: DEFB FAIL_OPEN_CODE
fs_fail_read_call: DEFB FAIL_READ_CALL
fs_fail_write_call: DEFB FAIL_WRITE_CALL
fs_fail_write_bytes: DEFB FAIL_WRITE_BYTES
fs_fail_sync: DEFB FAIL_SYNC
fs_fail_close_call: DEFB FAIL_CLOSE_CALL
fs_bad_paths: DEFB 0
fs_low_buffers: DEFB 0
fs_names:
    DEFM "/SYS/CONFIG/SHATRCV0.RCV"
    DEFB 0
    DEFM "/SYS/CONFIG/SHATRCV1.RCV"
    DEFB 0
    DEFM "/SYS/CONFIG/SHATJRNL.TMP"
    DEFB 0
    DEFM "/SYS/CONFIG/SHATJRNL.NEW"
    DEFB 0
"""


def run_case(
    tmp: Path,
    name: str,
    *,
    files: dict[str, bytes] | None = None,
    operation: str = "resume",
    snapshot: bytes = bytes(range(60)),
    pointer: str = "buffer",
    pend: int = 0x23,
    journal_fd: int = 0x2A,
    count: int = 0,
    body_crc: int = 0xFFFF,
    link: int = F_PLAYED,
    local_black: int = 0,
    role: int = 0,
    fail_open_call: int = 0,
    fail_open_code: int = 9,
    fail_read_call: int = 0,
    fail_write_call: int = 0,
    fail_write_bytes: int = 0,
    fail_sync: int = 0,
    fail_close_call: int = 0,
    next_target: bool = False,
    cache: tuple[int, int] = (0xFF, 0),
) -> dict[str, object]:
    files = files or {}
    init = []
    data = []
    for index, label in enumerate(FILES):
        content = files.get(label)
        if content is None:
            continue
        assert len(content) <= FILE_SPAN - 16
        base = FILE_BASE + index * FILE_SPAN
        init += [
            f"    ld a, 1\n    ld ({base}), a",
            f"    ld hl, {len(content)}\n    ld ({base + 1}), hl",
        ]
        if content:
            init.append(
                f"    ld hl, initial_{label}\n    ld de, {base + 16}\n"
                f"    ld bc, {len(content)}\n    ldir"
            )
            data.append(f"initial_{label}:\n{asm_bytes(content)}")
    if operation == "write":
        entry = "_recovery_write_ovl_entry"
        pointer_value = {
            "buffer": "write_snapshot",
            "record": str(SCRATCH + 6),
            "null": "0",
        }[pointer]
    else:
        entry = "_recovery_resume_ovl"
        pointer_value = "load_buffer" if pointer == "buffer" else "0"
    source = f"""
SECTION code_user
EXTERN _recovery_write_ovl_entry, _recovery_resume_ovl
PUBLIC _spectrum_journal_pend, _spectrum_journal_fd
PUBLIC _spectrum_journal_count, _spectrum_journal_crc
PUBLIC _netchesszx_local_color, _spectrum_net_background_drain
PUBLIC _netchesszx_session_role
PUBLIC _recovery_capture_ovl, _recovery_retire_ovl
PUBLIC _spectrum_rcv_cache

test_start:
    ld sp, 0xff00
    ld a, 0xc3
    ld (8), a
    ld hl, test_rom
    ld (9), hl
    ld hl, {FILE_BASE}
    ld de, {FILE_BASE + 1}
    ld bc, {FILE_SPAN * 4 - 1}
    ld (hl), 0
    ldir
    ld a, 0xa5
    ld ({SCRATCH - 1}), a
    ld ({SCRATCH + 160}), a
{chr(10).join(init)}
    ld hl, snapshot_initial
    ld de, write_snapshot
    ld bc, 60
    ldir
    ld hl, snapshot_initial
    ld de, {SCRATCH + 6}
    ld bc, 60
    ldir
    ld hl, load_buffer
    ld de, load_buffer + 1
    ld bc, 59
    ld (hl), 0xa5
    ldir
    ld a, {pend}
    ld (_spectrum_journal_pend), a
    ld a, {journal_fd}
    ld (_spectrum_journal_fd), a
    ld hl, {count}
    ld (_spectrum_journal_count), hl
    ld hl, {body_crc}
    ld (_spectrum_journal_crc), hl
    ld a, {local_black}
    ld (_netchesszx_local_color), a
    ld a, {role}
    ld (_netchesszx_session_role), a
    ld hl, {pointer_value}
    ld (0x5fe2), hl
    ld a, 0xff
    ld (0x5fe4), a
    ld a, {link}
    ld (0x5fe6), a
    ld a, {cache[0]}
    ld (_spectrum_rcv_cache), a
    ld hl, {cache[1]}
    ld (_spectrum_rcv_cache + 1), hl
    ld iy, 0x5c3a
    call {entry}
    ld (entry_result), hl
    ld (test_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done

_recovery_capture_ovl:
_recovery_retire_ovl:
    ld hl, 0
    ret
_spectrum_net_background_drain:
    ld hl, drains
    inc (hl)
    ret

test_result: DEFB 255
entry_result: DEFW 0
test_iy: DEFW 0
drains: DEFB 0
_spectrum_journal_pend: DEFB 0
_spectrum_journal_fd: DEFB 0
_spectrum_journal_count: DEFW 0
_spectrum_journal_crc: DEFW 0
_netchesszx_local_color: DEFB 0
_netchesszx_session_role: DEFB 0
_spectrum_rcv_cache: DEFS 3
snapshot_initial:
{asm_bytes(snapshot)}
write_snapshot: DEFS 60
load_buffer: DEFS 60
{chr(10).join(data)}
""" + FS_MODEL
    replacements = {
        "FAIL_OPEN_CALL": fail_open_call,
        "FAIL_OPEN_CODE": fail_open_code,
        "FAIL_READ_CALL": fail_read_call,
        "FAIL_WRITE_CALL": fail_write_call,
        "FAIL_WRITE_BYTES": fail_write_bytes,
        "FAIL_SYNC": fail_sync,
        "FAIL_CLOSE_CALL": fail_close_call,
    }
    for marker, value in replacements.items():
        source = source.replace(marker, str(value))
    harness = tmp / f"recovery_{name}.asm"
    harness.write_text(source, encoding="ascii")
    binary = harness.with_suffix(".bin")
    symbols = assemble_vector(
        required_tool("z80asm"),
        harness,
        [
            ROOT / "asm/overlay/recovery/entry_recovery.asm",
            ROOT / "asm/overlay/recovery/rcv_store.asm",
            ROOT / "asm/overlay/recovery/rcv_resume.asm",
            ROOT / "asm/esxdos/esx_saveload.asm",
            ROOT / "asm/esxdos/esx_fsync.asm",
        ],
        binary,
        defines=["NETCHESSZX_JOURNAL_HOOK", "NETCHESSZX_RECOVERY"]
        + (["NETCHESSZX_NEXT_BANKING"] if next_target else []),
    )
    result, output, pc, _, _ = execute_vector(
        required_tool("z88dk-ticks"), binary, symbols, counter=4_000_000,
        cpu="z80n" if next_target else "z80",
    )
    assert result == 0 and pc == symbol_address(symbols, "test_done"), (name, output)
    ram = binary.with_suffix(".ram").read_bytes()

    def sym(label: str, size: int = 1) -> int:
        address = symbol_address(symbols, label)
        return int.from_bytes(ram[address : address + size], "little")

    def file_at(index: int) -> bytes | None:
        base = FILE_BASE + index * FILE_SPAN
        if not ram[base]:
            return None
        length = int.from_bytes(ram[base + 1 : base + 3], "little")
        return bytes(ram[base + 16 : base + 16 + length])

    load = symbol_address(symbols, "load_buffer")
    cache_at = symbol_address(symbols, "_spectrum_rcv_cache")
    state: dict[str, object] = {
        "hl": sym("entry_result", 2),
        "status": ram[0x5FE4],
        "pend": sym("_spectrum_journal_pend"),
        "journal_fd": sym("_spectrum_journal_fd"),
        "load": bytes(ram[load : load + 60]),
        "open_calls": sym("fs_open_calls"),
        "read_calls": sym("fs_read_calls"),
        "write_calls": sym("fs_write_calls"),
        "close_calls": sym("fs_close_calls"),
        "unlinks": sym("fs_unlinks"),
        "iy": sym("test_iy", 2),
        "cache": (ram[cache_at],
                  int.from_bytes(ram[cache_at + 1 : cache_at + 3], "little")),
    }
    for index, label in enumerate(FILES):
        state[label] = file_at(index)
    assert sym("fs_bad_paths") == 0, (name, "unexpected path")
    assert sym("fs_low_buffers") == 0, (name, "esxDOS path/buffer below 0x4000")
    assert (ram[SCRATCH - 1], ram[SCRATCH + 160]) == (0xA5, 0xA5), (
        name, "overlay scratch overrun"
    )
    assert state["iy"] == 0x5C3A, (name, "IY clobbered")
    return state


def check_write(tmp: Path) -> None:
    old = bytes(range(0x20, 0x20 + 60))
    new = bytes(range(0x70, 0x70 + 60))
    third = bytes(range(0xB0, 0xB0 + 60))

    # First checkpoint: raw descriptor zero, journal link and flags recorded.
    s = run_case(tmp, "write_first", operation="write", snapshot=new,
                 count=3, body_crc=0x1234, pend=0x23, local_black=1)
    assert (s["hl"], s["status"], s["slot0"], s["slot1"]) == (
        1, 0, record(1, new, 3, 0x1234, F_HOST | F_BLACK | F_PLAYED), None
    ), s
    assert (s["pend"], s["journal_fd"]) == (0x23, 0x2A), s
    assert s["cache"] == (0, 1), s

    # Flags come from resident state: guest, partial history, armed switch
    # (GAME START before its first ply) and a ply-zero capture.
    for label, args, flags in (
        ("guest", {"role": 1}, F_PLAYED),
        ("partial", {"journal_fd": 0x6A}, OFFER | F_PARTIAL),
        ("armed", {"pend": 0x80, "link": 0}, F_HOST | F_UNLINKED),
    ):
        s = run_case(tmp, "write_flags_" + label, operation="write",
                     snapshot=new, count=2, body_crc=0x55AA, **args)
        assert s["slot0"] == record(1, new, 2, 0x55AA, flags), (label, s)

    # The capture path encodes straight into the record: no copy needed.
    s = run_case(tmp, "write_in_place", operation="write", snapshot=new,
                 pointer="record", count=5, body_crc=0x4321)
    assert s["slot0"] == record(1, new, 5, 0x4321, OFFER), s

    s = run_case(tmp, "write_null", operation="write", pointer="null")
    assert (s["hl"], s["status"], s["write_calls"]) == (0, 1, 0), s

    # Alternation and modular generations.
    s = run_case(tmp, "write_wrap", operation="write", snapshot=new,
                 files={"slot0": record(0xFFFF, old)})
    assert (s["slot0"], s["slot1"]) == (record(0xFFFF, old), record(0, new)), s
    r10, r11 = record(10, old), record(11, new)
    s = run_case(tmp, "write_both", operation="write", snapshot=third,
                 files={"slot0": r10, "slot1": r11})
    assert (s["slot0"], s["slot1"]) == (record(12, third), r11), s
    assert s["cache"] == (0, 12), s
    s = run_case(tmp, "write_only1", operation="write", snapshot=third,
                 files={"slot1": r11})
    assert (s["slot0"], s["slot1"]) == (record(12, third), r11), s

    # A prototype SRC1 file is not a valid checkpoint and is replaced first.
    src1 = record(40, old, magic=b"SRC1")
    s = run_case(tmp, "write_src1", operation="write", snapshot=new,
                 files={"slot0": src1, "slot1": r11})
    assert (s["slot0"], s["slot1"]) == (record(12, new), r11), s

    # Uncertain scans fail before any create/truncate.
    for label, failure in (
        ("scan_open_error", {"fail_open_call": 1, "fail_open_code": 9}),
        ("slot1_open_error", {"fail_open_call": 2, "fail_open_code": 9}),
        ("slot1_read_error", {"fail_read_call": 3}),
        ("slot1_close_error", {"fail_close_call": 2}),
    ):
        s = run_case(tmp, label, operation="write", snapshot=third,
                     files={"slot0": r10, "slot1": r11}, **failure)
        assert (s["hl"], s["status"], s["slot0"], s["slot1"]) == (
            0, 2, r10, r11
        ), (label, s)
        assert s["write_calls"] == 0, (label, s)

    # Every interrupted replacement keeps the previous valid slot selectable.
    for label, failure in (
        ("create_open", {"fail_open_call": 3}),
        ("partial_write", {"fail_write_call": 1, "fail_write_bytes": 20}),
        ("sync", {"fail_sync": 1}),
        ("close", {"fail_close_call": 2}),
        ("readback_open", {"fail_open_call": 4}),
        ("readback_read", {"fail_read_call": 3}),
        ("readback_close", {"fail_close_call": 3}),
    ):
        s = run_case(tmp, label, operation="write", snapshot=new,
                     files={"slot0": r10}, **failure)
        assert (s["hl"], s["status"], s["slot0"]) == (0, 3, r10), (label, s)
        assert s["cache"][0] == 0xFF, (label, s)
        torn = s["slot1"]
        if torn is not None and len(torn) != 73:
            reboot = run_case(tmp, label + "_reboot",
                              files={"slot0": r10, "slot1": torn})
            assert (reboot["status"], reboot["load"]) == (0, old), (label, reboot)


def check_write_cache(tmp: Path) -> None:
    """The slot this boot last wrote and read back is cached resident, so the
    next checkpoint skips the two-slot scan: create and readback only."""
    old = bytes(range(0x20, 0x20 + 60))
    new = bytes(range(0x70, 0x70 + 60))
    r10 = record(10, old)
    for label, next_target in (("cache_hit", False), ("next_cache_hit", True)):
        s = run_case(tmp, label, operation="write", snapshot=new,
                     files={"slot0": r10}, cache=(0, 10),
                     next_target=next_target)
        assert (s["hl"], s["status"], s["slot0"], s["slot1"]) == (
            1, 0, r10, record(11, new)
        ), (label, s)
        assert (s["open_calls"], s["read_calls"]) == (2, 2), (label, s)
        assert s["cache"] == (1, 11), (label, s)
    # The cache, not the media, picks the target: slot 1 cached as newest.
    s = run_case(tmp, "cache_hit_slot1", operation="write", snapshot=new,
                 files={"slot0": r10, "slot1": record(11, old)},
                 cache=(1, 11))
    assert (s["slot0"], s["slot1"], s["cache"]) == (
        record(12, new), record(11, old), (0, 12)
    ), s
    # A failed write forgets the cache: the next checkpoint scans again.
    for label, failure in (
        ("cache_sync", {"fail_sync": 1}),
        ("cache_readback_open", {"fail_open_call": 2}),
    ):
        s = run_case(tmp, label, operation="write", snapshot=new,
                     files={"slot0": r10}, cache=(0, 10), **failure)
        assert (s["hl"], s["status"], s["slot0"]) == (0, 3, r10), (label, s)
        assert s["cache"][0] == 0xFF, (label, s)


def check_resume(tmp: Path) -> None:
    old = bytes(range(0x20, 0x20 + 60))
    new = bytes(range(0x70, 0x70 + 60))
    sentinel = bytes([0xA5]) * 60
    b3 = body(3)
    extra = b3 + body(1, seed=9)       # a record appended after the checkpoint
    linked = record(11, new, 3, crc16(b3), OFFER)

    # Probe: newest offerable checkpoint, no file or state changes.
    s = run_case(tmp, "probe", pointer="null",
                 files={"slot0": record(10, old), "slot1": linked, "tmp": extra})
    assert (s["hl"], s["status"], s["pend"], s["journal_fd"]) == (
        1, 0, 0x23, 0x2A
    ), s
    assert (s["load"], s["write_calls"], s["new"], s["tmp"]) == (
        sentinel, 0, None, extra
    ), s
    s = run_case(tmp, "probe_wrap", pointer="null",
                 files={"slot0": record(0xFFFF, old, flags=0),
                        "slot1": record(0, new)})
    assert s["status"] == 0, s

    # Offer rules: host writer, a played ply, same local colour; newest only.
    for label, flags, black in (
        ("guest_writer", F_PLAYED, 0),
        ("ply_zero", F_HOST, 0),
        ("colour_changed", OFFER | F_BLACK, 0),
        ("colour_changed_black", OFFER, 1),
    ):
        s = run_case(tmp, "probe_" + label, pointer="null", local_black=black,
                     files={"slot0": record(10, old), "slot1": record(11, new, flags=flags)})
        assert (s["hl"], s["status"]) == (0, 1), (label, s)
    s = run_case(tmp, "probe_black", pointer="null", local_black=1,
                 files={"slot1": record(11, new, flags=OFFER | F_BLACK)})
    assert s["status"] == 0, s
    s = run_case(tmp, "probe_none", pointer="null")
    assert (s["hl"], s["status"]) == (0, 1), s
    s = run_case(tmp, "probe_io", pointer="null",
                 files={"slot0": linked}, fail_read_call=1)
    assert (s["hl"], s["status"], s["pend"]) == (0, 2, 0x23), s
    for label, bad in (
        ("torn", linked[:31]),
        ("overlong", linked + b"x"),
        ("bad_crc", linked[:-1] + b"\0"),
        ("src1", record(11, new, magic=b"SRC1")),
    ):
        s = run_case(tmp, "fallback_" + label,
                     files={"slot0": record(10, old), "slot1": bad})
        assert (s["status"], s["load"], s["slot1"]) == (0, old, bad), (label, s)

    # Stage: snapshot out, linked prefix copied to NEW, pend staged unarmed.
    s = run_case(tmp, "stage_complete",
                 files={"slot0": record(10, old), "slot1": linked, "tmp": extra})
    assert (s["hl"], s["status"], s["load"]) == (1, 0, new), s
    assert (s["new"], s["tmp"], s["pend"], s["journal_fd"]) == (
        b3, extra, 0x01, 0
    ), s
    assert s["close_calls"] >= 1 and s["write_calls"] == 1, s

    s = run_case(tmp, "stage_partial",
                 files={"slot1": record(11, new, 3, crc16(b3), OFFER | F_PARTIAL),
                        "tmp": b3})
    assert (s["new"], s["pend"]) == (b3, 0x41), s

    long = body(150)                    # two chunks: 160 + 140 bytes
    s = run_case(tmp, "stage_long",
                 files={"slot1": record(11, new, 150, crc16(long)), "tmp": long})
    assert (s["new"], s["pend"]) == (long, 0x01), s

    # A disabled or absent live handle is left alone.
    for fd in (0, 0x3F, 0x7F):
        s = run_case(tmp, f"stage_fd_{fd:02x}", journal_fd=fd,
                     files={"slot1": linked, "tmp": b3})
        assert (s["journal_fd"], s["pend"]) == (fd, 0x01), (fd, s)

    # Any doubt about the history keeps the snapshot and stages RESTORED.
    for label, files, extra_args in (
        ("crc_mismatch", {"slot1": record(11, new, 3, crc16(b3) ^ 1), "tmp": b3}, {}),
        ("short_tmp", {"slot1": linked, "tmp": b3[:4]}, {}),
        ("missing_tmp", {"slot1": linked}, {}),
        ("unlinked", {"slot1": record(11, new, 3, crc16(b3), OFFER | F_UNLINKED),
                      "tmp": b3}, {}),
        ("no_records", {"slot1": record(11, new, 0, 0xFFFF), "tmp": b3}, {}),
        ("tmp_read_error", {"slot1": linked, "tmp": b3}, {"fail_read_call": 3}),
        ("new_create_error", {"slot1": linked, "tmp": b3}, {"fail_open_call": 4}),
        ("new_write_error", {"slot1": linked, "tmp": b3},
         {"fail_write_call": 1, "fail_write_bytes": 2}),
        # Closes: slot1 scan, live TMP handle, TMP source, then NEW.
        ("new_close_error", {"slot1": linked, "tmp": b3}, {"fail_close_call": 4}),
    ):
        s = run_case(tmp, "stage_" + label, files=files, **extra_args)
        assert (s["hl"], s["status"], s["load"]) == (1, 0, new), (label, s)
        assert (s["pend"], s["new"]) == (0x40, None), (label, s)
        assert s["tmp"] == files.get("tmp"), (label, s)

    # A stale NEW from an earlier LOAD is replaced, never adopted as is.
    s = run_case(tmp, "stage_stale_new",
                 files={"slot1": linked, "tmp": b3, "new": body(7, seed=3)})
    assert (s["new"], s["pend"]) == (b3, 0x01), s


def check_next(tmp: Path) -> None:
    """Next build: esxDOS pages DivMMC over 0x0000..0x3FFF during RST 8, so
    the store must not use the 0x3C78 scratch as a file buffer."""
    old = bytes(range(0x20, 0x20 + 60))
    new = bytes(range(0x70, 0x70 + 60))
    r10 = record(10, old)
    s = run_case(tmp, "next_write", operation="write", snapshot=new,
                 files={"slot0": r10}, count=2, body_crc=0x55AA,
                 next_target=True)
    assert (s["hl"], s["status"], s["slot0"], s["slot1"]) == (
        1, 0, r10, record(11, new, 2, 0x55AA, OFFER)
    ), s
    long = body(150)
    s = run_case(tmp, "next_stage_long", next_target=True, files={
        "slot0": r10, "slot1": record(11, new, 150, crc16(long)), "tmp": long})
    assert (s["hl"], s["status"], s["load"]) == (1, 0, new), s
    assert (s["new"], s["pend"]) == (long, 0x01), s


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="recovery_z80_") as directory:
        tmp = Path(directory)
        check_write(tmp)
        check_write_cache(tmp)
        check_resume(tmp)
        check_next(tmp)
    print("Recovery checkpoint/resume ASM vectors passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
