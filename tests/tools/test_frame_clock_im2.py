#!/usr/bin/env python3
"""Execute the Classic resident IM2 frame-clock initializer and ISR."""

import re
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT, assemble_vector, execute_vector, required_tool, symbol_address,
)
from check_lowmem_layout import classic_frame_clock_end


HARNESS = r'''SECTION code_user
test_start:
    ld sp, 0xff00
    call _spectrum_frame_clock_init
    di
    ld ix, 0x1234
    ld iy, 0x5c3a
    ld bc, 0x5678
    ld de, 0x9abc
    ld hl, 0xffff
    ld (0x5c78), hl
    xor a
    scf
    ld a, 0x5a
    call frame_clock_isr
    di
    push af
    pop hl
    ld (saved_af), hl
    ld (saved_ix), ix
    ld (saved_iy), iy
    ld (saved_bc), bc
    ld (saved_de), de
    ld (saved_sp), sp
    xor a
    ld (test_result), a
test_done:
    jp test_done
test_result: defb 255
saved_af: defw 0
saved_ix: defw 0
saved_iy: defw 0
saved_bc: defw 0
saved_de: defw 0
saved_sp: defw 0
'''


def check_fixed_reservation(temp: Path) -> None:
    loader = (ROOT / "asm/esxdos/overlay_loader.asm").read_text(encoding="ascii")
    start = loader.index("SECTION bss_user")
    state = loader[start:loader.index("\nSECTION code_user", start)]
    normal_ends = []
    for padding in (0, 97):
        harness = temp / f"layout_{padding}.asm"
        harness.write_text(
            f"SECTION code_user\ndefs {padding + 1}\n" + state,
            encoding="ascii",
        )
        binary = harness.with_suffix(".bin")
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        addr = lambda name: symbol_address(symbols, name)
        normal_end = addr("__bss_user_tail")
        normal_ends.append(normal_end)
        assert len(binary.read_bytes()) == normal_end - 0x8000
        fixed = temp / f"layout_{padding}_bss_frame_clock_vector.bin"
        assert fixed.read_bytes() == bytes(2)
        assert classic_frame_clock_end(symbols, normal_end) == 0xFD01
        for name, value, bss_end in (
            (None, 0, 0xFD00),
            ("_spectrum_frame_clock_vector_reservation", 0xFCFE, normal_end),
            ("__bss_frame_clock_vector_head", 0xFCFE, normal_end),
            ("__bss_frame_clock_vector_tail", 0xFD02, normal_end),
        ):
            bad_map = symbols if name is None else re.sub(
                rf"^({re.escape(name)}\s*=\s*)\$[0-9A-Fa-f]+",
                lambda match: f"{match[1]}${value:04X}", symbols, flags=re.M,
            )
            try:
                classic_frame_clock_end(bad_map, bss_end)
            except SystemExit:
                pass
            else:
                raise AssertionError(f"unsafe IM2 layout accepted: {name}")
    assert normal_ends[1] - normal_ends[0] == 97
    # Classic links the DIRECT receive slots above the vector; a slot whose
    # DEFS width drifts from SPECTRUM_LINK_PAYLOAD_MAX must be rejected.
    harness = temp / "layout_high.asm"
    # Match the CRT ordering: initialized DATA precedes ordinary/fixed BSS.
    harness.write_text("SECTION code_user\ndefs 1\nSECTION data_user\n" + state,
                       encoding="ascii")
    symbols = assemble_vector(required_tool("z80asm"), harness, [],
                              harness.with_suffix(".bin"),
                              ["NETCHESSZX_HIGH_SCRATCH"])
    normal_end = symbol_address(symbols, "__bss_user_tail")
    cache_size = 2 * (symbol_address(symbols, "PLAYER_NAME_MAX") + 1)
    assert classic_frame_clock_end(symbols, normal_end) == 0xFD61
    cache = symbol_address(symbols, "_spectrum_moves_header_shown")
    assert symbol_address(symbols, "spectrum_moves_header_shown_end") == cache + cache_size
    assert harness.with_suffix(".bin").read_bytes()[cache - 0x8000:cache - 0x8000 + cache_size] == bytes(cache_size)
    for name, value in (
        ("_direct_rx_payload2", 0xFD30),
        ("_direct_rx_payload_end", 0xFD60),
        ("__bss_high_scratch_tail", 0xFD77),
        ("_spectrum_moves_header_shown", 0xFD61),
        ("spectrum_moves_header_shown_end", cache + cache_size - 1),
    ):
        bad_map = re.sub(rf"^({re.escape(name)}\s*=\s*)\$[0-9A-Fa-f]+",
                         lambda match: f"{match[1]}${value:04X}", symbols, flags=re.M)
        try:
            classic_frame_clock_end(bad_map, normal_end)
        except SystemExit:
            pass
        else:
            raise AssertionError(f"unsafe DIRECT/cache layout accepted: {name}")


def main() -> None:
    source = (ROOT / "asm/spectrum/frame_clock.asm").read_text(encoding="ascii")
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    assets = app.index("if (!spectrum_assets_load())")
    install = app.index("spectrum_frame_clock_init();", assets)
    first_tick = app.index("spectrum_app_tick_init();", install)
    assert assets < install < first_tick
    assert (
        "#if !defined(NETCHESSZX_NEXT_BANKING) && "
        "!defined(NETCHESSZX_SPECTRANEXT)" in app[assets:install]
    )

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        check_fixed_reservation(temp)
        harness = temp / "frame_clock.asm"
        harness.write_text(HARNESS + source, encoding="ascii")
        binary = temp / "frame_clock.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        addr = lambda name: symbol_address(symbols, name)
        result, _, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, symbols, counter=200000
        )
        ram = binary.with_suffix(".ram").read_bytes()

        assert result == 0 and pc == addr("test_done")
        vector_page = addr("frame_clock_vector_page")
        vector_pointer = addr("frame_clock_vector_pointer")
        assert not 0x40 <= vector_page <= 0x7F
        assert vector_pointer == 0xFCFF
        assert int.from_bytes(ram[vector_pointer:vector_pointer + 2],
                              "little") == addr("frame_clock_isr")
        assert ram[0x5c78:0x5c7a] == b"\x00\x00"
        for name, value in (
            ("saved_af", 0x5a45),
            ("saved_ix", 0x1234),
            ("saved_iy", 0x5c3a),
            ("saved_bc", 0x5678),
            ("saved_de", 0x9abc),
            ("saved_sp", 0xff00),
        ):
            got = int.from_bytes(ram[addr(name):addr(name) + 2], "little")
            assert got == value, (name, got)

    print("Classic IM2 frame clock: vector, 16-bit tick and ABI passed")


if __name__ == "__main__":
    main()
