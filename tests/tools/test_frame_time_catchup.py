#!/usr/bin/env python3
"""Execute the production ROM-frame catch-up wrapper on Z80."""

import re
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


def main():
    source = (ROOT / "src/spectrum/app/net_runtime.c").read_text()
    match = re.search(
        r"void spectrum_app_tick\(void\) __naked\s*"
        r"\{\s*#asm\n(.*?)#endasm",
        source,
        re.S,
    )
    assert match, "missing production spectrum_app_tick ASM"

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "frame_time.asm"
        harness.write_text('''SECTION code_user
test_start:
    ld sp,0xff00
    ld ix,0x1234
    ld iy,0x5c3a
    ld hl,(frame_seed)
    ld (0x5c78),hl
    ld hl,(last_seed)
    ld (_last_frame_count),hl
    call _spectrum_app_tick
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
frame_seed: defw 0
last_seed: defw 0
tick_calls: defw 0
present_calls: defb 0
present_dirty: defb 0
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
_last_frame_count: defw 0
_spectrum_app_tick_frame:
    ld hl,(tick_calls)
    inc hl
    ld (tick_calls),hl
    ld l,2
    ret
_spectrum_gui_present_tick:
    ld a,l
    ld (present_dirty),a
    ld hl,present_calls
    inc (hl)
    ret
_spectrum_app_tick:
''' + match.group(1), encoding="ascii")
        binary = temp / "frame_time.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        addr = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()

        cases = (
            (42, 42, 0),
            (42, 47, 5),
            (65534, 2, 4),
            (42, 342, 300),
        )
        for last, now, elapsed in cases:
            image = bytearray(original)
            for name, value in (("last_seed", last), ("frame_seed", now)):
                offset = addr(name) - 0x8000
                image[offset:offset + 2] = value.to_bytes(2, "little")
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=200000
            )
            ram = binary.with_suffix(".ram").read_bytes()
            case = (last, now, elapsed)
            assert result == 0 and pc == addr("test_done"), case
            assert int.from_bytes(
                ram[addr("tick_calls"):addr("tick_calls") + 2], "little"
            ) == elapsed, case
            assert ram[addr("present_calls")] == bool(elapsed), case
            assert ram[addr("present_dirty")] == (2 if elapsed else 0), case
            assert int.from_bytes(
                ram[addr("_last_frame_count"):addr("_last_frame_count") + 2],
                "little",
            ) == now, case
            for name, value in (
                ("saved_ix", 0x1234),
                ("saved_iy", 0x5c3a),
                ("saved_sp", 0xff00),
            ):
                got = int.from_bytes(ram[addr(name):addr(name) + 2], "little")
                assert got == value, (case, name, got)
    print("ROM-frame catch-up: 16-bit init, backlog, wrap and ABI passed")


if __name__ == "__main__":
    main()
