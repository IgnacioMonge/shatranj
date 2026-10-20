#!/usr/bin/env python3
"""Execute constant-address panel writes against production ULA layout."""

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
test_start:
    ld sp, 0xff00
    ld ix, 0x1234
    ld iy, 0x5c3a
    ld hl, 0x4000
    ld de, 0x4001
    ld bc, 6911
    ld (hl), 0xa5
    ldir
    ld hl, (entry_seed)
    ld de, test_return
    push de
    jp (hl)
test_return:
    ld (saved_sp), sp
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done
draw_board_frame:
    ld a, 1
    ld (frame_called), a
    ret
test_result: defb 255
entry_seed: defw 0
conn_attr: defb 0
frame_called: defb 0
saved_sp: defw 0
saved_ix: defw 0
saved_iy: defw 0
INCLUDE "asm/ula/address.inc"
"""


def main():
    primitives = (ROOT / "asm/ula/panel_primitives.inc").read_text()
    indicator = primitives.split("\ndraw_right_hline:", 1)[0]
    entries = (ROOT / "asm/ula/panel_entries.inc").read_text()
    turn = "clear_turn_strip:\n" + entries.split("clear_turn_strip:\n", 1)[1].split(
        "\n_spectrum_render_notice:", 1,
    )[0]
    separator = "fileui_separator:\n    ld hl, ula_screen_fileui_separator" + entries.split(
        "    ld hl, ula_screen_fileui_separator", 1,
    )[1].split("\nattr_cell_for_tmpcol:", 1)[0]
    assets = (ROOT / "assets/spectrum/ui_runtime_assets.asm").read_text()
    pattern = "conn_pattern:\n" + assets.split("conn_pattern:\n", 1)[1].split(
        "\nrank_digit_patterns:", 1,
    )[0]
    cases = (("draw_connection_indicator", 0), ("draw_connection_indicator", 0x3c),
             ("clear_turn_strip", 0), ("fileui_separator", 0))
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "panel_addresses.asm"
        harness.write_text(HARNESS + indicator + turn + separator + pattern)
        binary = temp / "panel_addresses.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        address = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()
        for entry, attr in cases:
            image = bytearray(original)
            for name, value in (("entry_seed", address(entry).to_bytes(2, "little")),
                                ("conn_attr", bytes([attr]))):
                offset = address(name) - 0x8000
                image[offset:offset + len(value)] = value
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=200000,
            )
            ram = binary.with_suffix(".ram").read_bytes()
            case = (entry, attr)
            assert result == 0 and pc == address("test_done"), case
            if entry == "draw_connection_indicator":
                start = address("conn_pattern") - 0x8000
                spans = [
                    (address("ula_screen_status") + 31 + scan * 256, bytes([value]))
                    for scan, value in enumerate(original[start:start + 8])
                ]
                spans.append((address("ula_attr_status") + 31,
                              bytes([attr or address("ATTR_CONN_OFF")])))
            elif entry == "clear_turn_strip":
                start = address("ula_screen_top_turn") + address("NETCHESSZX_TOP_TURN_BYTE_COL")
                spans = [(start + scan * 256,
                          bytes(address("NETCHESSZX_TOP_TURN_WIDTH_BYTES")))
                         for scan in range(8)]
            else:
                start = address("ula_screen_fileui_separator") + address("NETCHESSZX_GAME_BOARD_LEFT_COL")
                spans = [(start, b"\xff" * 16)]
            screen = bytearray([0xa5] * 6912)
            for start, expected in spans:
                end = start + len(expected)
                assert ram[start - 1:end + 1] == b"\xa5" + expected + b"\xa5", (case, hex(start))
                screen[start - 0x4000:end - 0x4000] = expected
            assert ram[0x4000:0x5b00] == screen, (case, "display bounds")
            assert ram[address("frame_called")] == int(entry == "fileui_separator"), case
            for name, expected in (("saved_sp", 0xff00), ("saved_ix", 0x1234),
                                   ("saved_iy", 0x5c3a)):
                got = int.from_bytes(ram[address(name):address(name) + 2], "little")
                assert got == expected, (case, name, got)
    print(f"Panel constant-address ASM writes passed ({len(cases)} cases)")


if __name__ == "__main__":
    main()
