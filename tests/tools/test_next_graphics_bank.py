#!/usr/bin/env python3
"""Focused host checks for the Next executable graphics-bank contract."""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_next_nex import (  # noqa: E402
    BANK_SIZE,
    BundleRegion,
    EXTENSION_TABLE_SIZE,
    append_bundle_region,
    render_bundle,
    validate_graphics_bank,
)
from build_next_piece_sprites import source_digest  # noqa: E402


def numeric(text: str, name: str, operator: str) -> int:
    match = re.search(
        rf"(?m)^{re.escape(name)}\s+{re.escape(operator)}\s+(0x[0-9A-Fa-f]+|[0-9]+)\s*$",
        text,
    )
    if not match:
        raise AssertionError(f"{name} not found")
    return int(match.group(1), 0)


def expect_rejected(blob: bytes, offset: int, origin: int, used=()) -> None:
    try:
        validate_graphics_bank(blob, offset, origin, list(used))
    except SystemExit:
        return
    raise AssertionError("invalid graphics-bank layout accepted")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="next_sprite_digest_") as temp_name:
        first = Path(temp_name) / "first.svg"
        second = Path(temp_name) / "second.svg"
        first.write_bytes(b"first")
        second.write_bytes(b"second")
        before = source_digest([first, second])
        second.write_bytes(b"SECOND")
        assert source_digest([first, second]) != before

    regions = [BundleRegion("first", 0, b"AB")]
    append_bundle_region(
        regions, BundleRegion("second", 4, b"CD"), "unexpected overlap"
    )
    assert render_bundle(regions, 8) == b"AB\0\0CD\0\0"
    try:
        append_bundle_region(
            regions, BundleRegion("overlap", 1, b"XX"), "expected overlap"
        )
    except SystemExit as exc:
        assert str(exc) == "expected overlap"
    else:
        raise AssertionError("overlapping bundle region accepted")

    origin = 0x2000
    blob = bytearray(200)
    for index in range(EXTENSION_TABLE_SIZE // 3):
        base = index * 3
        blob[base] = 0xC3
        blob[base + 1 : base + 3] = (origin + EXTENSION_TABLE_SIZE).to_bytes(
            2, "little"
        )

    end = validate_graphics_bank(bytes(blob), 0, origin, [(8192, 9000)])
    assert end == 200
    expect_rejected(bytes(blob), 1, origin)
    expect_rejected(bytes(blob), 8186, 0x3FFA)
    expect_rejected(bytes(blob), 0, origin, [(100, 200)])
    bad_entry = bytearray(blob)
    bad_entry[3] = 0
    expect_rejected(bytes(bad_entry), 0, origin)

    source = (ROOT / "asm/next/graphics_bank_next.asm").read_text(
        encoding="utf-8"
    )
    loader = (ROOT / "asm/next/overlay_loader_next.asm").read_text(encoding="utf-8")
    stubs = "\n".join(
        (ROOT / path).read_text(encoding="utf-8")
        for path in (
            "asm/next/screen_extension_stubs_top.asm",
            "asm/next/screen_extension_stubs_tail.asm",
        )
    )
    uart = (ROOT / "asm/uart/next_uart.asm").read_text(encoding="utf-8")
    layout = (ROOT / "asm/next/extension_bank_layout.asm").read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    sprite_rule = makefile[
        makefile.index("$(NEXT_SPRITE_BIN) $(NEXT_SPRITE_PALETTE_ASM)") :
        makefile.index("$(NEXT_NEX_CONFIG_STAMP)")
    ]
    assert "$(NEXT_SPRITE_SOURCE_ASSETS)" in sprite_rule
    layout_origin = numeric(layout, "next_extension_org", "EQU")
    layout_page = numeric(layout, "next_extension_page", "EQU")
    layout_limit = numeric(layout, "next_extension_code_limit", "EQU")
    layout_table_size = numeric(layout, "next_extension_table_size", "EQU")
    sprite_base = numeric(layout, "next_sprite_offset", "EQU")
    sprite_set_size = numeric(layout, "next_sprite_set_size", "EQU")
    sprite_patterns = numeric(layout, "next_sprite_pattern_count", "EQU")
    sprite_palette = numeric(layout, "next_sprite_pal_offset", "EQU")
    dat_offset = numeric(layout, "next_bundle_dat_offset", "EQU")
    about_palette = numeric(layout, "next_about_pal_offset", "EQU")
    about_bank = numeric(layout, "next_about_bank", "EQU")
    overlay_scratch = numeric(layout, "next_overlay_scratch", "EQU")
    palette_stage = numeric(layout, "next_palette_stage", "EQU")
    make_offset = numeric(makefile, "NEXT_GRAPHICS_BANK_OFFSET", ":=")
    make_origin = numeric(makefile, "NEXT_GRAPHICS_BANK_ORG", ":=")
    make_limit = numeric(makefile, "NEXT_GRAPHICS_BANK_LIMIT", ":=")
    free_min = numeric(makefile, "NEXT_EXTENSION_FREE_MIN", ":=")
    raw_bank_base = numeric(makefile, "NEXT_RAW_BANK_BASE", ":=")
    assert make_offset == 0
    assert layout_origin == make_origin == 0x2000
    assert layout_page == 32 + make_offset // 8192
    assert make_limit == layout_limit - layout_origin
    assert free_min == 512
    assert "graphics_max=$$(( $(NEXT_GRAPHICS_BANK_LIMIT) - $(NEXT_EXTENSION_FREE_MIN) ))" in makefile
    assert layout_table_size == EXTENSION_TABLE_SIZE
    assert dat_offset == numeric(makefile, "NEXT_BUNDLE_DAT_OFFSET", ":=")
    assert sprite_base == numeric(makefile, "NEXT_SPRITE_OFFSET", ":=")
    assert sprite_palette == numeric(makefile, "NEXT_SPRITE_PAL_OFFSET", ":=")
    assert about_palette == numeric(makefile, "NEXT_ABOUT_PAL_OFFSET", ":=")
    assert about_bank == raw_bank_base + (
        numeric(makefile, "NEXT_ABOUT_PIXELS_OFFSET", ":=") // BANK_SIZE
    )
    assert overlay_scratch + 160 == palette_stage
    assert palette_stage + 512 <= 0x4000

    assert sprite_patterns * 256 == sprite_set_size
    stride = " ".join(
        source[
            source.index("ngb_sprite_upload_set_current:") : source.index(
                "ngb_sprite_upload_common:"
            )
        ].lower().split()
    )
    assert stride.count("add hl, hl") == 3
    assert (
        "ld h, a ld l, 0 add hl, hl add hl, hl ld d, h ld e, l "
        "add hl, hl add hl, de ld de, next_sprite_offset add hl, de"
    ) in stride
    offsets = [index * 256 * sprite_patterns for index in range(3)]
    assert offsets == [index * sprite_set_size for index in range(3)]
    common_offset = sprite_base + 3 * sprite_set_size
    assert all(
        sprite_base + offset + sprite_set_size <= common_offset for offset in offsets
    )
    assert common_offset <= sprite_palette

    assert not re.search(r"(?mi)^\s*ei(?:\s|$)", source), "cold bank enables interrupts"
    assert not re.search(r"(?mi)^\s*rst(?:\s|$)", source), "cold bank calls ROM"
    assert "next_map_slot0" not in source, "cold bank remaps its executing slot"
    assert "next_copy_bundle_ei" not in source, "cold bank uses the EI copy entry"
    assert stubs.count("call next_extension_restore") == 49
    assert not re.search(r"(?m)^\s*[^;\n]*:\s+jp\s+next_ext_", stubs)
    assert "DEFB 0xed, 0x91, next_mmu_slot1, next_extension_page" in loader
    assert "out (c), a\n    jp ngb_success" not in source
    for resident in (loader, uart):
        assert not re.search(r"(?mi)^\s*ld\s+a\s*,\s*i(?:\s|$)", resident), (
            "racy LD A,I interrupt-state probe"
        )
    labels = set(re.findall(r"(?m)^([A-Za-z_][A-Za-z0-9_]*):", source))
    targets = set(
        re.findall(r"(?mi)^\s*(?:call|jp)\s+([A-Za-z_][A-Za-z0-9_]*)", source)
    )
    allowed_resident = {
        "_spectrum_next_sprites_hide_all",
        "next_copy_bundle",
        "nextreg_read",
        "nextreg_write",
    }
    assert not targets - labels - allowed_resident, "cold bank calls an unguarded service"
    assert "EXTERN _spectrum_uart_background_pump" in loader
    assert "call z, next_copy_pump" in loader
    assert "call _spectrum_uart_background_pump" in loader
    copy = loader[loader.index("next_copy_begin:") : loader.index("next_copy_pump:")]
    assert "ld a, next_mmu_slot0" in copy
    assert "ld (next_saved_mmu0), a" in copy
    assert "ld a, (next_saved_mmu0)" in copy
    assert "call next_map_slot0" in copy
    assert "next_map_slot1" not in copy
    assert "NEXT_RESIDENT_ASM := asm/next/graphics_bank_next.asm" in makefile
    extension = (ROOT / "asm/next/extension_bank_next.asm").read_text(
        encoding="utf-8"
    )
    assert "graphics_bank_next.asm" not in extension
    print("Next graphics-bank contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
