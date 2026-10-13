#!/usr/bin/env python3
"""Execute the resident editor append routine and verify its linked bytes."""

import argparse
import subprocess
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)


HARNESS = r"""
SECTION code_user

test_start:
    ld sp, 0xff00
    ld hl, input_seed
    ld de, editor_input
    ld bc, 43
    ldir
    ld a, (len_seed)
    ld (_local_input_len), a
    ld a, (cursor_seed)
    ld (_local_input_cursor), a
    ld a, 1
    ld (_local_input_mode), a
    ld a, 3
    ld (_input_history_count), a
    ld a, (history_seed)
    ld (_input_history_pos), a
    xor a
    ld (render_count), a
    ld hl, render_records
    ld (hl), a
    ld de, render_records + 1
    ld bc, 5
    ldir
    ld hl, _spectrum_overlay_context
    ld (hl), a
    ld de, _spectrum_overlay_context + 1
    ld bc, 7
    ldir
    ld ix, 0x1234
    ld iy, 0x5c3a
    ld a, (key_seed)
    ld l, a
measure_call:
    call _spectrum_editor_append_hot
measure_done:
    ld a, l
    ld (return_value), a
    ld (saved_sp), sp
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done

_spectrum_render_input_cell:
    ld a, (render_count)
    ld de, render_records
    or a
    jr z, render_record
    ld de, render_records + 3
render_record:
    ld bc, 3
    ldir
    ld hl, render_count
    inc (hl)
    ret

test_result: DEFB 255
key_seed: DEFB 0
len_seed: DEFB 0
cursor_seed: DEFB 0
history_seed: DEFB 0
input_seed: DEFS 43
return_value: DEFB 0
render_count: DEFB 0
render_records: DEFS 6
saved_sp: DEFW 0
saved_ix: DEFW 0
saved_iy: DEFW 0
_spectrum_overlay_context: DEFS 8
"""


def check_behavior() -> None:
    product = (ROOT / "asm/spectrum/editor_state.asm").read_text(encoding="ascii")
    for symbol in ("_spectrum_render_input_cell", "_spectrum_overlay_context"):
        declaration = f"EXTERN {symbol}\n"
        assert declaration in product
        product = product.replace(declaration, "", 1)
    cases = (
        ("ordinary", ord("c"), b"ab", 2, 2, "append"),
        ("space", 32, b"a", 1, 1, "append"),
        ("ascii-high", 126, b"", 0, 0, "append"),
        ("last-slot", ord("z"), b"x" * 41, 41, 41, "append"),
        ("control", 31, b"ab", 2, 2, "cold"),
        ("non-ascii", 127, b"ab", 2, 2, "cold"),
        ("full", ord("z"), b"x" * 42, 42, 42, "consume"),
        ("full-mid-line", ord("z"), b"x" * 42, 42, 10, "consume"),
        ("mid-line", ord("z"), b"abc", 3, 1, "cold"),
    )
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "editor_append.asm"
        harness.write_text(HARNESS + "\n" + product, encoding="ascii")
        binary = temp / "editor_append.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        address = lambda name: symbol_address(symbols, name)
        original = binary.read_bytes()

        for name, key, text, length, cursor, outcome in cases:
            image = bytearray(original)

            def patch(label: str, value: bytes) -> None:
                offset = address(label) - 0x8000
                image[offset:offset + len(value)] = value

            seed = bytearray(43)
            seed[:len(text)] = text
            patch("key_seed", bytes((key,)))
            patch("len_seed", bytes((length,)))
            patch("cursor_seed", bytes((cursor,)))
            patch("history_seed", b"\x04")
            patch("input_seed", seed)
            binary.write_bytes(image)
            result, _, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, symbols, counter=100000
            )
            ram = binary.with_suffix(".ram").read_bytes()
            label = f"editor append {name}"
            assert result == 0 and pc == address("test_done"), label
            handled = outcome != "cold"
            assert ram[address("return_value")] == handled, label
            expected = bytearray(seed)
            expected_len = length
            expected_cursor = cursor
            expected_history = 4
            expected_render = b""
            if outcome == "append":
                expected[length] = key
                expected[length + 1] = 0
                expected_len += 1
                expected_cursor += 1
                expected_history = 0xff
                expected_render = bytes((cursor, key, 0, cursor + 1, 32, 1))
            assert ram[0x5eb2:0x5edd] == expected, label
            assert ram[address("_local_input_len")] == expected_len, label
            assert ram[address("_local_input_cursor")] == expected_cursor, label
            assert ram[address("_local_input_mode")] == 1, label
            assert ram[address("_input_history_count")] == 3, label
            assert ram[address("_input_history_pos")] == expected_history, label
            assert ram[address("render_count")] == 2 * (outcome == "append"), label
            assert ram[address("render_records"):address("render_records") + 6] == (
                expected_render.ljust(6, b"\0")
            ), label
            if outcome == "append":
                assert ram[address("_spectrum_overlay_context"):
                           address("_spectrum_overlay_context") + 3] == expected_render[3:], label
            for saved, expected_value in (
                ("saved_sp", 0xff00),
                ("saved_ix", 0x1234),
                ("saved_iy", 0x5c3a),
            ):
                got = int.from_bytes(
                    ram[address(saved):address(saved) + 2], "little"
                )
                assert got == expected_value, (label, saved, got)
    print(f"Resident editor append ASM vector passed ({len(cases)} cases)")


def check_linked_bytes(build_dir: Path) -> None:
    product = (ROOT / "asm/spectrum/editor_state.asm").read_text(encoding="ascii")
    map_path = build_dir / "SHATRANJ.map"
    code_path = build_dir / "SHATRANJ_CODE.bin"
    symbols = map_path.read_text(encoding="utf-8")
    start = symbol_address(symbols, "_spectrum_editor_append_hot")
    end = symbol_address(symbols, "editor_append_hot_end")
    origin = symbol_address(symbols, "__CODE_head")
    routine = product[product.index("_spectrum_editor_append_hot:"):
                      product.index("editor_append_hot_end:")]
    definitions = (
        "SECTION code_user\n"
        "editor_input EQU 0x5eb2\n"
        "editor_input_max EQU 42\n"
    )
    for name in (
        "_local_input_len",
        "_local_input_cursor",
        "_input_history_pos",
        "_spectrum_render_input_cell",
        "_spectrum_overlay_context",
    ):
        definitions += f"DEFC {name} = 0x{symbol_address(symbols, name):04x}\n"

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        source = temp / "linked_editor_append.asm"
        binary = temp / "linked_editor_append.bin"
        source.write_text(definitions + routine, encoding="ascii")
        result = subprocess.run(
            [
                required_tool("z80asm"),
                "-b",
                f"-r=0x{start:04x}",
                "-O=.",
                f"-o={binary.name}",
                source.name,
            ],
            cwd=temp,
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        expected = binary.read_bytes()
    linked = code_path.read_bytes()[start - origin:end - origin]
    assert len(expected) == end - start, (build_dir, len(expected), end - start)
    assert linked == expected, f"{build_dir}: linked editor append differs from source"
    print(f"{build_dir}: linked editor append bytes match ({len(linked)} bytes)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--linked-build", action="append", type=Path, default=[])
    parser.add_argument("--linked-only", action="store_true")
    args = parser.parse_args()

    if not args.linked_only:
        check_behavior()
    for build_dir in args.linked_build:
        check_linked_bytes(build_dir)


if __name__ == "__main__":
    main()
