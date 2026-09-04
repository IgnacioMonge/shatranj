#!/usr/bin/env python3
"""Focused contract for Next 4K overlays plus mirrored resident code."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_abi_manifest import (  # noqa: E402
    MANIFEST_VERSION,
    compare_manifests,
    parse_constants,
)
from gen_next_nex import (  # noqa: E402
    LOADER_RAW_BANK_COUNT,
    MAIN_BANKS,
    OVERLAY_EXEC_SIZE,
    OVERLAY_PAGE_COUNT,
    PAGE_SIZE,
    RESIDENT_MIRROR_BASE,
    RESIDENT_MIRROR_END,
    render_overlay_pages,
    resident_length,
    validate_mirror_installer,
    validate_resident_mirror,
)
from gen_overlay_atlas import MAGIC, ORDER  # noqa: E402
from gen_size_report import check_hard_limits  # noqa: E402


def source_int(text: str, name: str, operator: str) -> int:
    match = re.search(
        rf"(?m)^{re.escape(name)}\s*{operator}\s*(0x[0-9a-fA-F]+|[0-9]+)$",
        text,
    )
    assert match, name
    return int(match.group(1), 0)


def run_atlas(
    root: Path, block_size: int | None, size_limit: int | None = None
) -> subprocess.CompletedProcess[str]:
    cmd = [
        sys.executable,
        str(ROOT / "tools/gen_overlay_atlas.py"),
        "--build-dir",
        str(root),
        "--name",
        "SHATRANJ",
        "--out",
        str(root / "SHATRANJ.OVL"),
        "--asm-out",
        str(root / "overlay_atlas_table.asm"),
    ]
    if block_size is not None:
        cmd += ["--block-size", str(block_size)]
    if size_limit is not None:
        cmd += ["--size-limit", str(size_limit)]
    return subprocess.run(cmd, text=True, capture_output=True, check=False)


def write_overlays(root: Path, first_size: int) -> None:
    for index, name in enumerate(ORDER):
        size = first_size if index == 0 else index + 1
        (root / f"SHATRANJ_{name}.OVL").write_bytes(bytes([index]) * size)


def main() -> int:
    assert len(ORDER) == OVERLAY_PAGE_COUNT == 17
    assert resident_length(
        {"__data_compiler_tail": 0xF010, "__DATA_END_tail": 0xF011}, 0xF000
    ) == 0x11
    with tempfile.TemporaryDirectory(prefix="next_overlay_pages_") as temp_name:
        root = Path(temp_name)
        write_overlays(root, OVERLAY_EXEC_SIZE)
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode == 0, result.stderr
        paged = (root / "SHATRANJ.OVL").read_bytes()
        assert len(paged) == OVERLAY_PAGE_COUNT * PAGE_SIZE
        for index in range(OVERLAY_PAGE_COUNT):
            split = index * PAGE_SIZE + OVERLAY_EXEC_SIZE
            assert not any(paged[split : (index + 1) * PAGE_SIZE])
        mirror = bytes(range(256)) * (OVERLAY_EXEC_SIZE // 256)
        mirrored = render_overlay_pages(paged, mirror)
        for index in range(OVERLAY_PAGE_COUNT):
            start = index * PAGE_SIZE
            split = start + OVERLAY_EXEC_SIZE
            end = start + PAGE_SIZE
            assert mirrored[start:split] == paged[start:split]
            assert mirrored[split:end] == mirror
        table = (root / "overlay_atlas_table.asm").read_text(encoding="ascii")
        assert "ovl_atlas_count EQU 17" in table
        assert "DW 4096" in table

        contaminated = bytearray(paged)
        contaminated[OVERLAY_EXEC_SIZE] = 1
        try:
            render_overlay_pages(bytes(contaminated), mirror)
        except SystemExit as exc:
            assert "mirror half" in str(exc)
        else:
            raise AssertionError("non-empty resident mirror half accepted")

        (root / "SHATRANJ_RULES.OVL").write_bytes(bytes(OVERLAY_EXEC_SIZE + 1))
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode != 0 and "too large" in result.stderr

        (root / "SHATRANJ_RULES.OVL").unlink()
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode != 0

        result = run_atlas(root, OVERLAY_EXEC_SIZE, PAGE_SIZE)
        assert result.returncode != 0 and "size limit" in result.stderr
        result = run_atlas(root, PAGE_SIZE, 0)
        assert result.returncode != 0 and "size limit" in result.stderr

        write_overlays(root, 2049)
        result = run_atlas(root, None)
        assert result.returncode != 0 and "too large" in result.stderr
        write_overlays(root, 2048)
        result = run_atlas(root, None)
        assert result.returncode == 0, result.stderr
        classic = (root / "SHATRANJ.OVL").read_bytes()
        assert classic.startswith(MAGIC)
        assert len(classic) < OVERLAY_PAGE_COUNT * PAGE_SIZE

    writable = {
        "__data_compiler_head": 0xF000,
        "__data_compiler_tail": 0xF010,
        "__data_user_head": 0xF010,
        "__data_user_tail": 0xF011,
        "__bss_compiler_head": 0xF100,
        "__bss_compiler_tail": 0xF200,
        "__bss_user_head": 0xF200,
        "__bss_user_tail": 0xF210,
    }
    validate_resident_mirror(writable)
    writable["__data_compiler_head"] = RESIDENT_MIRROR_BASE
    writable["__data_compiler_tail"] = RESIDENT_MIRROR_END
    try:
        validate_resident_mirror(writable)
    except SystemExit as exc:
        assert "writable section" in str(exc)
    else:
        raise AssertionError("writable resident mirror accepted")

    installer = {name: 0xE000 for name in (
        "next_install_overlay_mirrors", "next_map_slot2",
        "next_map_slot3", "nextreg_write",
    )}
    validate_mirror_installer(installer)
    installer["next_map_slot3"] = RESIDENT_MIRROR_BASE
    try:
        validate_mirror_installer(installer)
    except SystemExit as exc:
        assert "remapped MMU window" in str(exc)
    else:
        raise AssertionError("self-overwriting mirror installer accepted")

    loader = (ROOT / "asm/next/overlay_loader_next.asm").read_text(encoding="utf-8")
    load = loader[loader.index("ovl_load:") : loader.index("ovl_load_fail:")]
    cached = loader[loader.index("ovl_ensure_loaded:") : loader.index("ovl_load:")]
    select = loader[
        loader.index("ovl_select_atlas_entry:") : loader.index("ovl_close_overlay_file:")
    ]
    assert "call next_map_slot3" in load
    assert "next_copy_bundle" not in load
    assert "next_map_slot3" not in cached
    assert "cp 16" in select
    assert "_overlay_code_slot EQU 0x6000" in loader
    install = loader[
        loader.index("next_install_overlay_mirrors:") :
        loader.index("; A = first MMU page of a compressed 16K bank")
    ]
    assert "ld a, next_compressed_page_base" in install
    assert "ld hl, 0x7000" in install
    assert "ld de, 0x4000" in install
    assert "ld a, next_overlay_page_base" in install
    assert "ld b, next_overlay_page_count" in install
    assert "ld hl, 0x4000" in install
    assert "ld de, 0x7000" in install
    assert source_int(loader, "next_overlay_page_count", r"EQU") == OVERLAY_PAGE_COUNT

    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    bundle_page = source_int(loader, "next_bundle_page_base", r"EQU")
    overlay_page = source_int(loader, "next_overlay_page_base", r"EQU")
    bundle_pages = source_int(loader, "next_bundle_page_count", r"EQU")
    overlay_offset = source_int(makefile, "NEXT_OVERLAY_OFFSET", r":=")
    assert overlay_offset % PAGE_SIZE == 0
    assert overlay_page == bundle_page + overlay_offset // PAGE_SIZE
    assert bundle_pages == LOADER_RAW_BANK_COUNT * 2
    assert overlay_page + OVERLAY_PAGE_COUNT <= bundle_page + bundle_pages

    header_path = ROOT / "src/spectrum/overlay/overlay.h"
    next_constants = {
        item["name"]: item["value"]
        for item in parse_constants(
            header_path,
            "SPECTRUM_OVL_",
            defines={"NETCHESSZX_NEXT", "NETCHESSZX_NEXT_BANKING"},
        )
    }
    classic_constants = {
        item["name"]: item["value"]
        for item in parse_constants(header_path, "SPECTRUM_OVL_")
    }
    assert next_constants["SPECTRUM_OVL_BLOCK_SIZE"] == 8192
    assert classic_constants["SPECTRUM_OVL_BLOCK_SIZE"] == 2048

    manifest = {
        "version": MANIFEST_VERSION,
        "api": [],
        "overlay_constants": [],
        "overlay_context_constants": [],
        "session_route_constants": [],
        "resident_symbols": [],
        "missing_required": [],
        "markers": {"_overlay_code_slot": "0x6000"},
    }
    baseline = dict(manifest)
    baseline["markers"] = {"_overlay_code_slot": "0x6800"}
    assert any(
        "_overlay_code_slot" in error
        for error in compare_manifests(baseline, manifest, False)
    )

    classic_loader = (ROOT / "asm/esxdos/overlay_loader.asm").read_text(
        encoding="utf-8"
    )
    assert "_overlay_code_slot EQU 0x6800" in classic_loader
    assert "cp 8" in classic_loader
    assert "0xed, 0x91" not in classic_loader.lower()
    assert "OVERLAY_BLOCK_SIZE=8192" in makefile
    assert "OVERLAY_SIZE_LIMIT=4096" in makefile
    assert source_int(makefile, "NEXT_ZX_ORG", r":=") == RESIDENT_MIRROR_BASE
    assert MAIN_BANKS[0] == (5, 0x4000)
    assert "ZX_ORG=$(NEXT_ZX_ORG)" in makefile
    flat_makefile = " ".join(makefile.split())
    assert "RESIDENT_INCLUDE_DEPS ?= asm/spectrum/input_queue.asm" in flat_makefile
    nex_inputs = makefile[
        makefile.index("NEXT_NEX_INPUTS :=") : makefile.index(".NOTPARALLEL:")
    ]
    assert "$(RESIDENT_INCLUDE_DEPS)" in nex_inputs
    assert "tools/build_overlays.py" in nex_inputs
    assert (
        "RESIDENT_INCLUDE_DEPS='$(RESIDENT_INCLUDE_DEPS) "
        "$(NEXT_GRAPHICS_BANK_LAYOUT) $(NEXT_EXTENSION_INCLUDES)'"
        in flat_makefile
    )
    assert source_int(makefile, "NEXT_MIN_SP_GAP", r":=") == 3072
    assert "--min-sp-gap $(NEXT_MIN_SP_GAP)" in makefile
    assert not check_hard_limits(
        {"memory": {"register_sp_gap_bytes": 3072}}, 3072
    )
    assert check_hard_limits(
        {"memory": {"register_sp_gap_bytes": 3071}}, 3072
    )
    print("Next mirrored direct-MMU overlay layout ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
