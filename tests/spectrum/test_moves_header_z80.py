#!/usr/bin/env python3
"""Execute the production MOVES label cache and row-clear invalidation."""

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


HARNESS = """
SECTION code_user
INCLUDE "ui_layout.inc"
INCLUDE "asm/app/player_name_layout.inc"
EXTERN _moves_header_white_new
EXTERN _moves_header_black_new
PUBLIC _spectrum_moves_header_shown
PUBLIC _status_phase_ovl
PUBLIC _status_moves_header_ovl
test_start:
    ld sp, 0xff00
    ld ix, 0x1234
    ld iy, 0x5c3a
    ld hl, 0x4000
    ld de, 0x4001
    ld bc, 6911
    ld (hl), 0xa5
    ldir
    ld a, (clear_seed)
    or a
    call nz, draw_moves_header
    ld hl, text_seed
    ld a, (black_seed)
    or a
    jr nz, test_black
    call _moves_header_white_new
    jr test_return
test_black:
    call _moves_header_black_new
test_return:
    ld a, l
    ld (return_value), a
    ld (saved_sp), sp
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done
_status_phase_ovl:
_status_moves_header_ovl:
    ret
test_result: defb 255
return_value: defb 255
clear_seed: defb 0
black_seed: defb 0
text_seed: defs 11
saved_sp: defw 0
saved_ix: defw 0
saved_iy: defw 0
guard_before: defs 8, 0xa5
_spectrum_moves_header_shown: defs 22
guard_after: defs 8, 0xa5
INCLUDE "asm/ula/address.inc"
"""


def main():
    panel = (ROOT / "asm/ula/panel_draw.inc").read_text()
    row_clear = panel.split("draw_moves_header:\n", 1)[1].split("\n    ret", 1)[0]
    primitives = (ROOT / "asm/ula/panel_primitives.inc").read_text()
    pixel_clear = primitives.split("clear_pixels_8:\n", 1)[1].split("\nfill_attr_line:", 1)[0]
    cases = (
        (0, "WHITE", 1, 1), (1, "BLACK", 1, 0),
        (0, "AD", 1, 0), (1, "A_X", 1, 0),
        (0, "AD", 0, 0), (1, "A_X", 0, 0),
        (0, "WHITE", 1, 0), (0, "CONSTANTIN", 1, 0),
        (0, "A", 1, 0), (0, "AA", 1, 0), (0, "A", 1, 0),
        (0, "A_X", 1, 0), (1, "AD", 1, 0),
        (0, "WHITE", 1, 1), (1, "BLACK", 1, 0),
        (0, "WHITE", 0, 0), (1, "BLACK", 0, 0),
        (1, "BLACK", 1, 1), (0, "WHITE", 1, 0),
    )
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "moves_header.asm"
        harness.write_text(
            HARNESS + "\ndraw_moves_header:\n" + row_clear + "\n    ret\n"
            + "\nclear_pixels_8:\n" + pixel_clear
        )
        binary = temp / "moves_header.bin"
        symbols = assemble_vector(
            required_tool("z80asm"), harness,
            [ROOT / "asm/overlay/menu_logic/entry_menu_logic.asm"], binary,
            defines=["NETCHESSZX_NEXT_BANKING"],
        )
        address = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()
        cache = bytearray([0xa5] * 22)
        for black, text, changed, clear in cases:
            image = bytearray(original)

            def patch(name, value):
                offset = address(name) - 0x8000
                image[offset:offset + len(value)] = value

            encoded = text.encode("ascii") + b"\0"
            patch("text_seed", encoded.ljust(11, b"\0"))
            patch("black_seed", bytes([black]))
            patch("clear_seed", bytes([clear]))
            patch("_spectrum_moves_header_shown", cache)
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=200000,
            )
            ram = binary.with_suffix(".ram").read_bytes()
            case = (black, text, changed, clear)
            assert result == 0 and pc == address("test_done"), case
            assert ram[address("return_value")] == changed, case
            screen = bytearray([0xa5] * 6912)
            if clear:
                col = address("NETCHESSZX_INFO_PANEL_COL")
                width = address("NETCHESSZX_INFO_PANEL_WIDTH_BYTES")
                spans = [
                    (address("ula_screen_info_header") + col + scan * 256, 0)
                    for scan in range(8)
                ]
                spans.append((address("ula_attr_info_header") + col, address("ATTR_STATUS")))
                for start, value in spans:
                    expected = bytes([value]) * width
                    assert ram[start - 1:start + width + 1] == b"\xa5" + expected + b"\xa5", (case, hex(start))
                    screen[start - 0x4000:start - 0x4000 + width] = expected
            assert ram[0x4000:0x5b00] == screen, (case, "display bounds")
            if clear:
                cache[0] = cache[11] = 0
            offset = black * 11
            cache[offset:offset + len(encoded)] = encoded
            start = address("_spectrum_moves_header_shown")
            assert ram[start:start + 22] == cache, case
            for name in ("guard_before", "guard_after"):
                assert ram[address(name):address(name) + 8] == b"\xa5" * 8, case
            assert ram[address("text_seed"):address("text_seed") + 11] == encoded.ljust(11, b"\0"), case
            for name, expected in (("saved_sp", 0xff00), ("saved_ix", 0x1234),
                                   ("saved_iy", 0x5c3a)):
                got = int.from_bytes(ram[address(name):address(name) + 2], "little")
                assert got == expected, (case, name, got)
    print(f"MOVES header ASM cache passed ({len(cases)} sequential cases)")


if __name__ == "__main__":
    main()
