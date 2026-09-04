#!/usr/bin/env python3
"""Focused tests for incremental overlay assembly and overlay-size checks."""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from build_overlays import (  # noqa: E402
    BLOCK_SIZE,
    BUILDER_PATH,
    OverlaySpec,
    filter_local_symbols,
    is_stale,
    normalize_overlay_name,
    overlay_block_size,
    overlay_size_limit,
    overlay_specs,
    require_map_current_for_size_check,
    require_slot_matches_block_size,
    size_check_lines,
    spec_inputs,
    spectrum_headers,
)
from gen_overlay_atlas import ORDER  # noqa: E402


CLASSIC_ENV = {
    "EDIT_BUF_OVL": "asm/overlay/edit/edit_buf.asm",
    "ESX_COMMON_ASM": "asm/esxdos/esx_fileio_spectalk.asm",
    "ESX_FILEUI_ASM": "asm/esxdos/esx_fileui.asm",
    "ESX_SAVELOAD_ASM": "asm/esxdos/esx_saveload.asm",
    "GAME_PROTOCOL_MACH_SRC": "src/common/protocol/game_protocol_mach.c",
}


def test_classic_specs_match_atlas_order() -> None:
    names = [spec.name for spec in overlay_specs(CLASSIC_ENV)]
    assert names == list(ORDER)
    assert "TIME" not in names


def test_spectranext_adds_time_overlay() -> None:
    env = dict(CLASSIC_ENV)
    env["SPXN_CLOCK_C"] = "/driver/spxudp.c /driver/spxtime.c"
    env["SPXN_CLOCK_HEADERS"] = "/driver/spxudp.h /driver/spxtime.h"
    env["SPXN_ATOMIC_SRC"] = "/driver/spxf_replace.asm"
    env["SPXN_RESOLVE_C"] = "/driver/spxresolve.c"
    env["SPXN_XFS_OVERLAY_ASM"] = "/build/xfs_compat.asm"
    names = [spec.name for spec in overlay_specs(env)]
    assert names == ORDER + ["TIME"]
    saveload = next(spec for spec in overlay_specs(env) if spec.name == "SAVELOAD")
    assert "/driver/spxf_replace.asm" in [path for path, _flags in saveload.c_sources]
    time = next(spec for spec in overlay_specs(env) if spec.name == "TIME")
    assert time.extra_inputs == ("/driver/spxudp.h", "/driver/spxtime.h")
    assert "/driver/spxresolve.c" in [path for path, _flags in time.c_sources]
    for name in ("SAVELOAD", "ABOUT", "FILEUI", "CONFIG"):
        spec = next(item for item in overlay_specs(env) if item.name == name)
        assert "/build/xfs_compat.asm" in spec.asm
        assert "_esx_fopen" in spec.local_symbols


def test_local_overlay_symbols_are_removed_from_resident_defs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "overlay_defs.asm"
        output = root / "overlay_defs_local.asm"
        source.write_text(
            "PUBLIC _local\nDEFC _local = $1234\n"
            "PUBLIC _resident\nDEFC _resident = $5678\n",
            encoding="utf-8",
        )
        assert filter_local_symbols(source, output, ("_local",)) == output
        assert output.read_text(encoding="utf-8") == (
            "PUBLIC _resident\nDEFC _resident = $5678\n"
        )


def test_normalize_overlay_name() -> None:
    specs = overlay_specs(CLASSIC_ENV)
    assert normalize_overlay_name("setup", specs) == "SETUP"
    assert normalize_overlay_name("time-config", specs) == "TIME_CONFIG"
    try:
        normalize_overlay_name("hints", specs)
    except SystemExit as exc:
        assert "unknown overlay" in str(exc)
    else:
        raise AssertionError("retired HINTS overlay must be rejected")


def test_is_stale_compares_mtimes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output = root / "SETUP.OVL"
        source = root / "entry_setup.asm"
        source.write_text("nop\n", encoding="utf-8")
        output.write_bytes(b"x")
        os.utime(output, (1_700_000_000, 1_700_000_000))
        os.utime(source, (1_700_000_000, 1_700_000_000))
        assert is_stale(output, [source]) is False
        os.utime(source, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, [source]) is True
        assert is_stale(root / "missing.OVL", [source]) is True


def test_size_check_same_and_growth() -> None:
    baseline = {"overlays": {"SETUP": 1964}}
    info, errors = size_check_lines(
        "SETUP",
        1964,
        baseline,
        fail_on_growth=True,
        fail_on_missing_baseline=True,
    )
    assert errors == []
    assert any("same as baseline" in line for line in info)

    info, errors = size_check_lines(
        "SETUP",
        1970,
        baseline,
        fail_on_growth=True,
        fail_on_missing_baseline=True,
    )
    assert errors == ["OVL_SETUP grew by 6 bytes"]
    assert any("+6" in line for line in info)

    info, errors = size_check_lines(
        "SETUP",
        BLOCK_SIZE + 1,
        baseline,
        fail_on_growth=False,
        fail_on_missing_baseline=False,
    )
    assert errors == [f"SETUP {BLOCK_SIZE + 1} bytes exceeds {BLOCK_SIZE}"]


def test_classic_config_links_esx_saveload() -> None:
    spec = next(item for item in overlay_specs(CLASSIC_ENV) if item.name == "CONFIG")
    assert "asm/esxdos/esx_saveload.asm" in spec.asm
    assert "asm/esxdos/esx_fileio_spectalk.asm" in spec.extra_inputs


def test_setup_uses_edit_buf() -> None:
    spec = next(item for item in overlay_specs(CLASSIC_ENV) if item.name == "SETUP")
    assert spec == OverlaySpec(
        "SETUP",
        ("asm/overlay/setup/entry_setup.asm", "asm/overlay/edit/edit_buf.asm"),
        (),
    )


def _resolved_inputs(spec: OverlaySpec, stamp: Path | None = None) -> set[Path]:
    resolved = set()
    for path in spec_inputs(spec, Path("overlay_defs.asm"), stamp):
        resolved.add(path.resolve() if path.is_absolute() else (ROOT / path).resolve())
    return resolved


def test_c_overlay_inputs_include_spectrum_headers_and_builder() -> None:
    spec = next(
        item for item in overlay_specs(CLASSIC_ENV) if item.name == "MENU_LOGIC"
    )
    inputs = _resolved_inputs(spec)
    assert BUILDER_PATH.resolve() in inputs
    assert (ROOT / "src/spectrum/session/event.h").resolve() in inputs
    assert (ROOT / "src/spectrum/config/session.h").resolve() in inputs
    assert (ROOT / "src/spectrum/overlay/overlay_api.h").resolve() in inputs
    headers = {path.resolve() for path in spectrum_headers()}
    assert headers <= inputs
    assert (ROOT / "src/spectrum/session/event.h").resolve() in headers


def test_asm_overlay_inputs_include_builder_not_c_headers() -> None:
    spec = next(item for item in overlay_specs(CLASSIC_ENV) if item.name == "SETUP")
    inputs = _resolved_inputs(spec)
    assert BUILDER_PATH.resolve() in inputs
    assert (ROOT / "src/spectrum/session/event.h").resolve() not in inputs
    assert (ROOT / "asm/overlay/setup/entry_setup.asm").resolve() in inputs


def test_header_change_marks_c_overlay_stale() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output = root / "MENU_LOGIC.OVL"
        header = root / "event.h"
        source = root / "status_ovl.c"
        defs = root / "overlay_defs.asm"
        builder = root / "build_overlays.py"
        for path, text in (
            (header, "enum { X };\n"),
            (source, "int x;\n"),
            (defs, "; defs\n"),
            (builder, "# builder\n"),
        ):
            path.write_text(text, encoding="utf-8")
        output.write_bytes(b"x")
        old = (1_700_000_000, 1_700_000_000)
        for path in (output, header, source, defs, builder):
            os.utime(path, old)
        os.utime(header, (1_700_000_001, 1_700_000_001))
        spec = OverlaySpec("MENU_LOGIC", (), ((str(source), ()),))
        inputs = spec_inputs(spec, defs, None, builder=builder, headers=[header])
        assert is_stale(output, inputs) is True
        os.utime(header, old)
        assert is_stale(output, inputs) is False


def test_builder_change_marks_overlay_stale() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output = root / "SETUP.OVL"
        builder = root / "build_overlays.py"
        defs = root / "overlay_defs.asm"
        asm = root / "entry_setup.asm"
        for path, text in (
            (builder, "# builder\n"),
            (defs, "; defs\n"),
            (asm, "nop\n"),
        ):
            path.write_text(text, encoding="utf-8")
        output.write_bytes(b"x")
        old = (1_700_000_000, 1_700_000_000)
        for path in (output, builder, defs, asm):
            os.utime(path, old)
        spec = OverlaySpec("SETUP", (str(asm),), ())
        inputs = spec_inputs(spec, defs, None, builder=builder, headers=[])
        assert is_stale(output, inputs) is False
        os.utime(builder, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs) is True


def test_size_check_rejects_stamp_newer_than_map() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        map_path = root / "SHATRANJ.map"
        stamp = root / "spectrum_config.json"
        map_path.write_text("map\n", encoding="utf-8")
        stamp.write_text("{}\n", encoding="utf-8")
        os.utime(map_path, (1_700_000_000, 1_700_000_000))
        os.utime(stamp, (1_700_000_001, 1_700_000_001))
        try:
            require_map_current_for_size_check(map_path, stamp)
        except SystemExit as exc:
            assert "newer than" in str(exc)
        else:
            raise AssertionError("stamp newer than map must fail overlay-size")
        os.utime(stamp, (1_700_000_000, 1_700_000_000))
        os.utime(map_path, (1_700_000_000, 1_700_000_000))
        require_map_current_for_size_check(map_path, stamp)
        os.utime(map_path, (1_700_000_002, 1_700_000_002))
        require_map_current_for_size_check(map_path, stamp)
        try:
            require_map_current_for_size_check(map_path, None)
        except SystemExit as exc:
            assert "spectrum_config.json" in str(exc)
        else:
            raise AssertionError("missing stamp must fail overlay-size")


def test_slot_matches_block_size() -> None:
    require_slot_matches_block_size(0x6800, 2048)
    require_slot_matches_block_size(0x2000, 4096)
    require_slot_matches_block_size(0x6000, 8192)
    for slot, cap in ((0x2000, 2048), (0x6800, 4096), (0x6800, 8192)):
        try:
            require_slot_matches_block_size(slot, cap)
        except SystemExit:
            pass
        else:
            raise AssertionError("mismatched slot/cap must fail")


def test_overlay_size_limit_can_be_smaller_than_page() -> None:
    assert overlay_block_size({"OVERLAY_BLOCK_SIZE": "8192"}) == 8192
    assert overlay_size_limit({"OVERLAY_SIZE_LIMIT": "4096"}, 8192) == 4096
    assert overlay_size_limit({}, 2048) == 2048
    for value in ("0", "8193"):
        try:
            overlay_size_limit({"OVERLAY_SIZE_LIMIT": value}, 8192)
        except SystemExit:
            pass
        else:
            raise AssertionError("invalid executable overlay limit accepted")

    _info, errors = size_check_lines(
        "NEXT",
        4097,
        None,
        fail_on_growth=False,
        fail_on_missing_baseline=False,
        size_limit=4096,
    )
    assert errors == ["NEXT 4097 bytes exceeds 4096"]


def test_overlay_size_make_refreshes_config_stamp() -> None:
    text = (ROOT / "Makefile").read_text(encoding="utf-8")
    assert "overlay-size: $(BUILD_CONFIG_STAMP)" in text


def test_final_atlas_pass_preserves_converged_table() -> None:
    text = BUILDER_PATH.read_text(encoding="utf-8")
    assert '"-o", str(atlas_table), "ATLAS_FINAL=1"' in text


def main() -> int:
    tests = [
        test_classic_specs_match_atlas_order,
        test_spectranext_adds_time_overlay,
        test_normalize_overlay_name,
        test_is_stale_compares_mtimes,
        test_size_check_same_and_growth,
        test_classic_config_links_esx_saveload,
        test_setup_uses_edit_buf,
        test_c_overlay_inputs_include_spectrum_headers_and_builder,
        test_asm_overlay_inputs_include_builder_not_c_headers,
        test_header_change_marks_c_overlay_stale,
        test_builder_change_marks_overlay_stale,
        test_size_check_rejects_stamp_newer_than_map,
        test_slot_matches_block_size,
        test_overlay_size_limit_can_be_smaller_than_page,
        test_overlay_size_make_refreshes_config_stamp,
        test_final_atlas_pass_preserves_converged_table,
    ]
    for test in tests:
        test()
        print(f"[OK] {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
