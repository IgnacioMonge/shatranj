#!/usr/bin/env python3
"""Production cartridge loader against ROM models; adapted from MirrorShift."""

import re
import subprocess
from pathlib import Path

from test_spectrum_asm_vectors import required_tool, z88dk_sources

ROOT = Path(__file__).resolve().parents[2]


def build_and_run(real_seam):
    """Assemble and execute the vector; REAL_SEAM links the production XFS seam."""
    name = "spectranext-dat-retain-vector" if real_seam else "spectranext-paging-vector"
    work = ROOT / "build" / name
    work.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "asm/esxdos/overlay_loader.asm").read_text()
    start = source.index("SECTION spxn_overlay_backend")
    end = source.index("SECTION code_user", start)
    backend = source[start:end]
    source = source[:start] + source[end:] + "\nIFDEF NETCHESSZX_SPECTRANEXT\n" + backend + "\nENDIF\n"
    # Keep production instructions and physical BSS addresses. Only link
    # sections change for ticks' flat image; ROM vectors use real addresses.
    source = source.replace("SECTION bss_user", "PHASE 0x5B00", 1)
    at = source.index("SECTION code_user", source.index("PHASE 0x5B00"))
    source = source[:at] + "DEPHASE\n" + source[at:]
    source = re.sub(r"(?m)^(?:SECTION|ORG|EXTERN|PUBLIC)\b[^\n]*", "", source)
    (work / "loader.asm").write_text(source)
    xfs = (ROOT / "asm/esxdos/xfs_loader_spectranext.asm").read_text()
    (work / "xfs.asm").write_text(re.sub(r"(?m)^(?:SECTION|EXTERN|PUBLIC)\b[^\n]*", "", xfs))
    offsets = [100, 4196] + [4196 + 8 * i for i in range(1, 18)]
    (work / "overlay_atlas_table.asm").write_text(
        "ovl_atlas_count EQU 18\n"
        + "\n".join(f"ovl_atlas_fingerprint_{i} EQU {v}" for i, v in enumerate((11, 22, 33, 44)))
        + "\novl_atlas_table:\n" + "\n".join(f"DW {v}" for v in offsets) + "\n"
    )
    (work / "config_private.inc").write_text(
        "defc __CPU_RABBIT__=0\ndefc __CPU_8085__=0\ndefc __Z80=1\ndefc __Z80_NMOS=1\n"
    )
    assembler = required_tool("z88dk-z80asm")
    helpers = z88dk_sources(
        assembler, "arch/z80/z80", ("asm_z80_push_di.asm", "asm_z80_pop_ei.asm")
    )
    (work / "vector.asm").write_text(
        (ROOT / "tests/spectrum/spectranext_paging_vector.asm").read_text()
        + '\nEXTERN asm_z80_push_di, asm_z80_pop_ei\nINCLUDE "loader.asm"\n'
        + ('INCLUDE "xfs.asm"\n' if real_seam else "")
    )
    defines = ["-DNETCHESSZX_SPECTRANEXT"]
    if real_seam:
        defines += ["-DREAL_SEAM", "-DSPXN_XFS_STATE_BASE=0x5B50", "-DSPXN_XFS_DIR_SCRATCH=0x662B"]
    for cmd in (
        [assembler, "-b", "-m", "-r0x8000", *defines, "-I.", "-O=.",
         "-o=vector.bin", "vector.asm", *(str(path) for path in helpers)],
        [required_tool("z88dk-ticks"), "-mz80", "-l", "0x8000", "-pc", "8000",
         "-end", "0", "-int", "10000", "-counter", "6000000",
         "-output", "vector.ram", "vector.bin"],
    ):
        run = subprocess.run(cmd, cwd=work, text=True, capture_output=True)
        assert run.returncode == 0, run.stdout + run.stderr
    ram = (work / "vector.ram").read_bytes()
    assert len(ram) >= 65536, "SpectraNext paging vector did not finish"
    if real_seam:
        failed = [i for i, v in enumerate(ram[0x7300:0x7300 + 11]) if v]
        assert not failed and ram[0x7000] == 0, (
            "SpectraNext DAT retention scenarios failed: "
            + ", ".join(f"{i}: log {bytes(ram[0x7400 + 16 * i:0x7410 + 16 * i])!r}" for i in failed))
    else:
        assert ram[0x7000] == 0, "SpectraNext paging vector failed"


def main():
    build_and_run(False)
    print("[OK] SpectraNext preload/rollback, Page B, entry bounds, nested/frame dispatch, IY/IFF, close retry")
    build_and_run(True)
    print("[OK] SpectraNext retained DAT: firmware 1.0 probe, launch mount, slot-0 fallback, borrow/detach, release, fd $FF")


if __name__ == "__main__":
    main()
