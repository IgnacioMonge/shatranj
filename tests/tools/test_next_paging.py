#!/usr/bin/env python3
"""Execute the production Next dispatcher/setters and reject unsafe NEX maps."""

import contextlib
import io
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_next_nex as nex


def block(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


def check_dispatch(work):
    source = (ROOT / "asm/next/overlay_loader_next.asm").read_text()
    production = block(source, "SECTION bss_user", "_spectrum_assets_load:")
    production += block(source, "ovl_ensure_loaded:", "next_graphics_bank_call:")
    for name in ("nextreg_write", "next_map_slot0", "next_map_slot1", "next_map_slot2", "next_map_slot3"):
        production += re.search(rf"(?ms)^{name}:.*?(?=^\w+:)", source)[0]
    prefix = source[:source.index("SECTION bss_user")]
    prefix = re.sub(r"(?m)^(?:SECTION|PUBLIC|EXTERN)\b[^\n]*", "", prefix)
    (work / "overlay_atlas_table.asm").write_text("ovl_atlas_count EQU 2\novl_atlas_table: DW 11,11\n")
    harness = r"""
SECTION code_user
start:
    ld sp, 0xFF00
    ld a, 254
    ld (0xF800), a
    ld ix, 0x1234
    ld iy, 0x5C3A
    ld a, next_overlay_page_base
    DEFB 0xed, 0x92, next_mmu_slot3
    ld hl, entry_table
    ld de, 0x6000
    ld bc, 11
    ldir
    ld a, next_overlay_page_base + 1
    DEFB 0xed, 0x92, next_mmu_slot3
    ld hl, entry_table
    ld de, 0x6000
    ld bc, 11
    ldir
    ld hl, 0
    push hl
    call _spectrum_overlay_exec
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, fail
    ld hl, 0x0200
    push hl
    call _spectrum_overlay_exec_cached
    ld a, h
    or l
    jp nz, fail
    ld hl, 2
    push hl
    call _spectrum_overlay_exec
    ld a, h
    or l
    jp nz, fail
    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, fail
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, fail
    ld hl, 0
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, fail
    jp setters
entry_table:
    defb 2
    defw 0x6005, 0x6008
    jp outer_entry
    jp fail
outer_entry:
    ld bc, _spectrum_overlay_context
    or a
    sbc hl, bc
    jp nz, fail
    ex de, hl
    or a
    sbc hl, bc
    jp nz, fail
    ld hl, 0x0101
    push hl
    call _spectrum_overlay_exec_cached
    ld a, h
    or l
    jp nz, fail
    ld hl, 0x0101
    push hl
    call _spectrum_overlay_exec
    ld a, h
    or l
    jp nz, fail
    ld a, (ovl_id)
    or a
    jp nz, fail
    ld a, (ovl_entry_id)
    or a
    jp nz, fail
    ld a, i
    jp pe, fail
    ld hl, 0x1234
    ret
fail:
    ld a, 255
    ld (0xF800), a
    jp 0
setters:
"""
    for enabled in (False, True):
        for slot in range(4):
            harness += f"""
    {'ei' if enabled else 'di'}
    ld bc, 0x2345
    ld de, 0x6789
    ld hl, 0xABCD
    xor a
    scf
    ld a, 0x35
    call next_map_slot{slot}
    jp nz, fail
    jp nc, fail
    cp 0x35
    jp nz, fail
    ld a, i
    jp {'po' if enabled else 'pe'}, fail
    ld a, b
    cp 0x23
    jp nz, fail
    ld a, c
    cp 0x45
    jp nz, fail
    ld a, d
    cp 0x67
    jp nz, fail
    ld a, e
    cp 0x89
    jp nz, fail
    ld de, 0xABCD
    or a
    sbc hl, de
    jp nz, fail
"""
    harness += "xor a\nld (0xF800),a\njp 0\n"
    (work / "vector.asm").write_text(harness + prefix + production)
    for cmd in (
        [shutil.which("z88dk-z80asm"), "-b", "-m", "-r0x8000", "-I.", f"-I{ROOT}",
         "-O=.", "-o=vector.bin", "vector.asm"],
        [shutil.which("z88dk-ticks"), "-mz80n", "-l", "0x8000", "-pc", "8000", "-end", "0",
         "-counter", "1000000", "-output", "vector.ram", "vector.bin"],
    ):
        assert cmd[0], "z88dk tools required"
        run = subprocess.run(cmd, cwd=work, capture_output=True, text=True)
        assert run.returncode == 0, run.stdout + run.stderr
    ram = (work / "vector.ram").read_bytes()
    assert ram[0xF800] == 0 and int.from_bytes(ram[0x10006:0x10008], "little") == 0, (
        f"Next dispatcher/register/IFF vector failed: {ram[0xF800]}"
    )
    symbols = nex.parse_map(work / "vector.map")
    binary = (work / "vector.bin").read_bytes()
    for slot in range(4):
        at = symbols[f"next_map_slot{slot}"] - 0x8000
        assert binary[at:at + 4] == bytes((0xED, 0x92, 0x50 + slot, 0xC9))


def check_stack_floor(work):
    symbols = {"__crt_org_code": 0x7000, "__register_sp": 0xFF58, "__DATA_END_tail": 0x9000}
    for section in ("data_compiler", "data_user", "bss_compiler", "bss_user"):
        symbols[f"__{section}_head"] = symbols[f"__{section}_tail"] = 0x9000
    symbols.update({name: 0x8000 for name in nex.MIRROR_INSTALL_SYMBOLS})
    (work / "code.bin").write_bytes(bytes(8192))
    (work / "atlas.ovl").write_bytes(bytes(nex.OVERLAY_PAGE_COUNT * nex.PAGE_SIZE))
    (work / "assets.dat").write_bytes(b"")
    args = ["--map", str(work / "input.map"), "--code-bin", str(work / "code.bin"),
            "--ovl", str(work / "atlas.ovl"), "--dat", str(work / "assets.dat"),
            "--out", str(work / "output.nex"), "--org", "0x7000"]
    for gap in (3071, 3072):
        symbols["__BSS_END_tail"] = symbols["__register_sp"] - gap
        (work / "input.map").write_text("\n".join(f"{name} = ${value:04X} ; const" for name, value in symbols.items()))
        with contextlib.redirect_stdout(io.StringIO()):
            try:
                nex.main(args)
            except SystemExit as exc:
                assert gap == 3071 and "SP_GAP" in str(exc), str(exc)
            else:
                assert gap == 3072, "NEX generator accepted an unsafe stack gap"


def main():
    with tempfile.TemporaryDirectory(prefix="next-paging-") as temp:
        work = Path(temp)
        check_dispatch(work)
        check_stack_floor(work)
    print("[OK] Next dispatch, nested rejection, MMU opcodes/registers/IFF, NEX stack floor")


if __name__ == "__main__":
    main()
