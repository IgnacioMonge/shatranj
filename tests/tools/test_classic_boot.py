#!/usr/bin/env python3
"""Execute the Classic boot capsule's asset success and read-failure paths."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import ROOT, required_tool, symbol_address


DEFS = """\
DEFC asset_load_addr = $6001
DEFC asset_load_size = 1273
DEFC frame_clock_vector_page = $fc
DEFC frame_clock_vector_pointer = $fcff
DEFC frame_clock_frames = $5c78
DEFC ovl_close_overlay_file = $7000
DEFC ovl_esx_open_readonly = $7001
DEFC ovl_read_chunked = $7005
DEFC ovl_close = $700c
DEFC assets_success = $7011
DEFC assets_fail_close = $7019
DEFC assets_fail = $701f
DEFC classic_boot_frame_clock_isr = $7027
DEFC ovl_handle = $7037
DEFC asset_filename = $7038
DEFC _last_frame_count = $5fea
"""

HARNESS = r'''ORG $7000
ovl_close_overlay_file: ret
ovl_esx_open_readonly:
    ld a, 7
    or a
    ret
ovl_read_chunked:
    ld a, (test_mode)
    or a
    ret z
    scf
    ret
ovl_close:
    ld hl, close_count
    inc (hl)
    ret
assets_success:
    pop iy
    pop ix
    ld hl, 1
    ret
assets_fail_close:
    call ovl_close
    jp assets_fail
assets_fail:
    pop iy
    pop ix
    ld hl, 0
    ret
frame_clock_isr: reti
test_mode: defb 0
close_count: defb 0
success_result: defw 0
success_vector: defw 0
success_last: defw 0
failure_result: defw 0
failure_vector: defw 0
failure_last: defw 0
ovl_handle: defb 0
asset_filename: defm "SHATRANJ.DAT", 0
test_start:
    ld sp, $ff00
    xor a
    ld (test_mode), a
    ld (close_count), a
    ld hl, $1234
    ld ($5c78), hl
    call $6800
    di
    ld (success_result), hl
    ld hl, ($fcff)
    ld (success_vector), hl
    ld hl, ($5fea)
    ld (success_last), hl

    ld a, 1
    ld (test_mode), a
    xor a
    ld (close_count), a
    ld hl, 0
    ld ($fcff), hl
    ld hl, $abcd
    ld ($5fea), hl
    call $6800
    di
    ld (failure_result), hl
    ld hl, ($fcff)
    ld (failure_vector), hl
    ld hl, ($5fea)
    ld (failure_last), hl
    xor a
    ld (test_result), a
test_done:
    jp test_done
test_result: defb 255
'''


def assemble(assembler: str, source: Path, output: Path, include: Path | None = None) -> str:
    command = [assembler, "-b", "-m", f"-O={output.parent}", f"-o={output.name}"]
    if include is not None:
        command.append(f"-I={include}")
    command.append(str(source))
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return output.with_suffix(".map").read_text(encoding="utf-8")


def main() -> None:
    assembler = required_tool("z80asm")
    ticks = required_tool("z88dk-ticks")
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        (temp / "classic_boot_defs.asm").write_text(DEFS, encoding="ascii")
        boot = temp / "boot.bin"
        boot_map = assemble(
            assembler, ROOT / "asm/spectrum/classic_boot.asm", boot, temp,
        )
        section = temp / "boot_code_boot.bin"
        boot_bytes = section.read_bytes() if section.exists() else boot.read_bytes()
        assert len(boot_bytes) == 148
        assert symbol_address(boot_map, "_spectrum_assets_fatal") == 0x6849

        harness_source = temp / "harness.asm"
        harness_source.write_text(HARNESS, encoding="ascii")
        harness = temp / "harness.bin"
        harness_map = assemble(assembler, harness_source, harness)
        image = temp / "image.bin"
        image_bytes = (
            boot_bytes + bytes(0x7000 - 0x6800 - len(boot_bytes))
            + harness.read_bytes()
        )
        image.write_bytes(image_bytes)
        ram = temp / "ram.bin"
        start = symbol_address(harness_map, "test_start")
        done = symbol_address(harness_map, "test_done")
        assert image_bytes[symbol_address(harness_map, "test_result") - 0x6800] == 0xFF
        result = subprocess.run(
            [
                ticks, "-mz80", "-l", "0x6800", "-pc", f"{start:04x}",
                "-end", f"{done:04x}", "-counter", "200000", "-output",
                str(ram), str(image),
            ],
            cwd=ROOT, capture_output=True, text=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        memory = ram.read_bytes()
        word = lambda name: int.from_bytes(
            memory[symbol_address(harness_map, name):][:2], "little"
        )
        observed = {
            name: word(name) for name in (
                "success_result", "success_vector", "success_last",
                "failure_result", "failure_vector", "failure_last",
            )
        }
        assert observed == {
            "success_result": 1,
            "success_vector": symbol_address(harness_map, "frame_clock_isr"),
            "success_last": 0x1234,
            "failure_result": 0,
            "failure_vector": 0,
            "failure_last": 0xABCD,
        }, (
            observed, len(memory),
            int.from_bytes(memory[0x10006:0x10008], "little"),
            result.stdout, result.stderr,
        )
        assert memory[symbol_address(harness_map, "close_count")] == 1
        assert memory[symbol_address(harness_map, "test_result")] == 0

    print("Classic boot capsule: asset success/read-failure paths passed")


if __name__ == "__main__":
    main()
