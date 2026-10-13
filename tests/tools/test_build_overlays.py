#!/usr/bin/env python3
"""Focused tests for incremental overlay assembly and overlay-size checks."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from build_overlays import (  # noqa: E402
    BLOCK_SIZE,
    BUILDER_PATH,
    OverlaySpec,
    deps_changed,
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
    write_deps,
)
from gen_overlay_atlas import ORDER  # noqa: E402
from gen_overlay_defs import EDITOR_PRIVATE_SYMBOLS, SETUP_PRIVATE_SYMBOLS  # noqa: E402


CLASSIC_ENV = {
    "EDIT_BUF_OVL": "asm/overlay/edit/edit_buf.asm",
    "ESX_COMMON_ASM": "asm/esxdos/esx_fileio_spectalk.asm",
    "ESX_FILEUI_ASM": "asm/esxdos/esx_fileui.asm",
    "ESX_SAVELOAD_ASM": "asm/esxdos/esx_saveload.asm",
    "GAME_PROTOCOL_MACH_SRC": "src/common/protocol/game_protocol_mach.c",
}


def test_classic_specs_match_atlas_order() -> None:
    names = [spec.name for spec in overlay_specs({**CLASSIC_ENV, "ATLAS_EXTRA": "JOURNAL RECOVERY"})]
    assert names == list(ORDER) + ["JOURNAL", "RECOVERY"]
    assert "TIME" not in names
    assert [spec.name for spec in overlay_specs(CLASSIC_ENV)] == list(ORDER)
    time_config = next(spec for spec in overlay_specs(CLASSIC_ENV)
                       if spec.name == "TIME_CONFIG")
    assert "asm/session/identity_setup.inc" in time_config.extra_inputs


def test_next_menu_tracks_its_visual_data_only_on_next() -> None:
    for flags, expected in (("", ()), ("-DNETCHESSZX_NEXT", (
        "assets/next/theme_constants.asm", "assets/next/piece_set_menu.asm"
    ))):
        specs = overlay_specs({**CLASSIC_ENV, "ZX_OVL_CFLAGS": flags})
        menu = next(spec for spec in specs if spec.name == "MENU_CONFIG")
        assert tuple(p for p in menu.extra_inputs if p.startswith("assets/next/")) == expected
        assert "build/ui_layout.inc" in menu.extra_inputs
        family = "next" if flags else "zx"
        assert f"src/spectrum/ui/layout_{family}.h" in menu.extra_inputs


def test_fileui_runtime_dependency_is_explicit() -> None:
    from gen_overlay_defs import AUTO_RUNTIME_PREFIXES, REQUIRED_SYMBOLS

    helper = "____sdcc_2_copy_src_mhl_dst_deix"
    assert helper.startswith(AUTO_RUNTIME_PREFIXES)
    assert helper not in REQUIRED_SYMBOLS  # Compiler runtime, not product ABI.
    platform = (ROOT / "src/spectrum/platform/platform.c").read_text(encoding="utf-8")
    assert f"EXTERN {helper}" in platform
    assert f"defc netchesszx_overlay_copy2_runtime = {helper}" in platform


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
    assert "asm/esxdos/esx_fopen_update.asm" not in spec.asm
    assert "asm/esxdos/esx_fsync.asm" not in spec.asm


def test_classic_saveload_links_fsync() -> None:
    spec = next(item for item in overlay_specs(CLASSIC_ENV) if item.name == "SAVELOAD")
    assert "asm/esxdos/esx_fsync.asm" in spec.asm
    assert "asm/esxdos/esx_fopen_update.asm" not in spec.asm


def test_journal_capabilities_are_independent() -> None:
    def spec_for(env: dict[str, str], name: str) -> OverlaySpec:
        return next(item for item in overlay_specs(env) if item.name == name)

    def c_flags(spec: OverlaySpec, source: str) -> tuple[str, ...]:
        return dict(spec.c_sources)[source]

    append = "asm/overlay/saveload/journal_append.asm"
    codec = "asm/overlay/journal/stj_codec.asm"
    glue = "asm/overlay/journal/stj_saveload.asm"
    saveload_c = "src/spectrum/overlay/saveload_ovl.c"

    # Classic: JOURNAL overlay packaged and hooked; codec stays out of SAVELOAD.
    classic = {**CLASSIC_ENV, "ATLAS_EXTRA": "JOURNAL RECOVERY", "JOURNAL_HOOK": "1"}
    saveload = spec_for(classic, "SAVELOAD")
    assert append in saveload.asm
    assert "asm/esxdos/esx_journal.asm" in saveload.asm
    assert codec not in saveload.asm and glue not in saveload.asm
    assert "asm/esxdos/esx_fseek_set.asm" not in saveload.asm
    journal = spec_for(classic, "JOURNAL")
    assert "asm/esxdos/esx_journal.asm" in journal.asm
    assert codec in journal.asm
    assert "asm/esxdos/esx_fseek_set.asm" in journal.asm
    assert "asm/esxdos/esx_saveload.asm" in spec_for(classic, "CONFIG").asm
    assert "asm/esxdos/esx_journal.asm" not in spec_for(classic, "CONFIG").asm
    recovery = spec_for(classic, "RECOVERY")
    assert "asm/esxdos/esx_saveload.asm" in recovery.asm
    assert "asm/esxdos/esx_fsync.asm" in recovery.asm
    assert "src/spectrum/overlay/recovery_ovl.c" in {
        source for source, _ in recovery.c_sources
    }
    assert c_flags(recovery, "src/spectrum/overlay/restore_ovl.c") == (
        "-DNETCHESSZX_RESTORE_BUILD_ONLY",
    )
    flags = c_flags(saveload, saveload_c)
    assert "-DSAVELOAD_APPEND_ASM" in flags
    assert "-DSTJ_SAVELOAD_ASM" not in flags
    replay = "asm/overlay/journal/journal_replay.inc"
    rcv_replay = "asm/overlay/recovery/rcv_replay.asm"
    assert replay in saveload.extra_inputs
    assert rcv_replay not in recovery.asm and replay not in recovery.extra_inputs

    # Next final: no JOURNAL overlay; append and STJ1 codec live in SAVELOAD.
    next_env = {
        **CLASSIC_ENV,
        "ATLAS_EXTRA": "RECOVERY",
        "JOURNAL_HOOK": "1",
        "STJ_SAVELOAD": "1",
    }
    names = [spec.name for spec in overlay_specs(next_env)]
    assert "JOURNAL" not in names
    assert names == ORDER + ["RECOVERY"]
    saveload = spec_for(next_env, "SAVELOAD")
    assert append in saveload.asm
    assert codec in saveload.asm and glue in saveload.asm
    assert "asm/esxdos/esx_fseek_set.asm" in saveload.asm
    assert "asm/esxdos/esx_journal.asm" in saveload.asm
    assert "asm/esxdos/esx_saveload.asm" in spec_for(next_env, "CONFIG").asm
    assert "asm/esxdos/esx_journal.asm" not in spec_for(next_env, "CONFIG").asm
    flags = c_flags(saveload, saveload_c)
    assert "-DSAVELOAD_APPEND_ASM" in flags and "-DSTJ_SAVELOAD_ASM" in flags
    assert replay in saveload.extra_inputs
    assert rcv_replay not in spec_for(next_env, "RECOVERY").asm

    # SpectraNext: TIME id 17 and no JOURNAL overlay in any configuration.
    spxn = {
        **CLASSIC_ENV,
        "ATLAS_EXTRA": "TIME",
        "ESX_SAVELOAD_ASM": "",
        "SPXN_CLOCK_C": "/driver/spxudp.c /driver/spxtime.c",
        "SPXN_CLOCK_HEADERS": "/driver/spxudp.h /driver/spxtime.h",
        "SPXN_ATOMIC_SRC": "/driver/spxf_replace.asm",
        "SPXN_RESOLVE_C": "/driver/spxresolve.c",
        "SPXN_XFS_OVERLAY_ASM": "/build/xfs_compat.asm",
    }
    names = [spec.name for spec in overlay_specs(spxn)]
    assert names == ORDER + ["TIME"]
    saveload = spec_for(spxn, "SAVELOAD")
    assert append not in saveload.asm
    assert codec not in saveload.asm and glue not in saveload.asm
    assert c_flags(saveload, saveload_c) == ()

    # SpectraNext S1: hook and append in SAVELOAD, no codec yet.
    spxn_s1 = {**spxn, "JOURNAL_HOOK": "1"}
    names = [spec.name for spec in overlay_specs(spxn_s1)]
    assert names == ORDER + ["TIME"]
    saveload = spec_for(spxn_s1, "SAVELOAD")
    assert append in saveload.asm
    assert "asm/esxdos/esx_journal.asm" not in saveload.asm
    assert "asm/esxdos/esx_fsync.asm" not in saveload.asm
    assert "asm/esxdos/esx_fseek_set.asm" not in saveload.asm
    assert codec not in saveload.asm and glue not in saveload.asm
    flags = c_flags(saveload, saveload_c)
    assert "-DSAVELOAD_APPEND_ASM" in flags
    assert "-DSTJ_SAVELOAD_ASM" not in flags

    # SpectraNext S2 (final): the shared codec replaces LOAD/SAVE, still no
    # esxDOS wrappers and no JOURNAL overlay.
    spxn_s2 = {**spxn_s1, "STJ_SAVELOAD": "1"}
    saveload = spec_for(spxn_s2, "SAVELOAD")
    assert codec in saveload.asm and glue in saveload.asm
    assert "asm/esxdos/esx_fseek_set.asm" not in saveload.asm
    assert "asm/esxdos/esx_fsync.asm" not in saveload.asm
    flags = c_flags(saveload, saveload_c)
    assert "-DSTJ_SAVELOAD_ASM" in flags
    assert "/driver/spxf_replace.asm" not in [path for path, _ in saveload.c_sources]
    # SpectraNext SAVELOAD is full: RECOVERY hosts the move-list replay.
    assert replay not in saveload.extra_inputs
    spxn_final = {**spxn_s2, "ATLAS_EXTRA": "TIME RECOVERY"}
    recovery = spec_for(spxn_final, "RECOVERY")
    assert recovery.asm[:3] == (
        "asm/overlay/recovery/entry_recovery.asm",
        "asm/overlay/recovery/rcv_store.asm",
        rcv_replay,
    )
    assert "/build/xfs_compat.asm" in recovery.asm
    assert replay in recovery.extra_inputs
    assert "_spxn_xfs_fseek" in recovery.local_symbols
    no_hook = spec_for({**spxn, "ATLAS_EXTRA": "TIME RECOVERY"}, "RECOVERY")
    assert rcv_replay not in no_hook.asm


def test_journal_code_is_guarded_by_capabilities() -> None:
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    entry = (ROOT / "asm/overlay/journal/entry_journal.asm").read_text(
        encoding="utf-8"
    )
    codec = (ROOT / "asm/overlay/journal/stj_codec.asm").read_text(
        encoding="utf-8"
    )
    assert (
        "JOURNAL_FLAG := $(if $(filter JOURNAL,$(ATLAS_EXTRA)),-DATLAS_JOURNAL)"
        in makefile
    )
    assert "JOURNAL_HOOK ?= $(if $(filter JOURNAL,$(ATLAS_EXTRA)),1,0)" in makefile
    assert "STJ_SAVELOAD ?= 0" in makefile
    assert makefile.count("-DNETCHESSZX_SDCC_IY $(JOURNAL_FLAG) $(RECOVERY_FLAG)") == 2
    assert "defined(NETCHESSZX_JOURNAL_HOOK)" in app
    assert "IFNDEF ATLAS_JOURNAL" in entry
    assert "DEFC _journal_load_ovl_entry = _stj_load_ovl" in entry
    assert "IFDEF NETCHESSZX_NEXT_BANKING" in codec
    assert "stj_scratch EQU 0x672B" in codec


def test_classic_boot_resident_extra_is_idempotent() -> None:
    boot = "asm/spectrum/classic_boot_refs.asm"
    journal = "asm/spectrum/journal.asm"
    with tempfile.TemporaryDirectory() as tmp:
        makefile = Path(tmp) / "Makefile"
        makefile.write_text(
            f"include {ROOT / 'Makefile'}\n"
            ".PHONY: show-resident-extra\n"
            "show-resident-extra:\n\t@echo $(RESIDENT_EXTRA_ASM)\n",
            encoding="utf-8",
        )
        result = subprocess.run(
            [
                "make", "-s", "-f", str(makefile),
                f"RESIDENT_EXTRA_ASM={boot} {journal}",
                "show-resident-extra",
            ],
            cwd=ROOT, capture_output=True, text=True, check=True,
        )
    extras = result.stdout.split()
    assert extras.count(boot) == 1
    assert extras.count(journal) == 1


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
    assert (ROOT / "tools/netchesszx_overlay_copt").resolve() in inputs
    # CONFIG passes a live DE after pushing its frame argument; the resident
    # push-frame helper clobbers DE and must not be applied to overlays.
    assert "_netchesszx_asm_push_frame_arg" not in (
        ROOT / "tools/netchesszx_overlay_copt"
    ).read_text(encoding="utf-8")
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
        timestamp = int((ROOT / "tools/netchesszx_overlay_copt").stat().st_mtime) + 1
        old = (timestamp, timestamp)
        for path in (output, header, source, defs, builder):
            os.utime(path, old)
        os.utime(header, (timestamp + 1, timestamp + 1))
        spec = OverlaySpec("MENU_LOGIC", (), ((str(source), ()),))
        inputs = spec_inputs(spec, defs, None, builder=builder, headers=[header])
        assert is_stale(output, inputs) is True
        os.utime(header, old)
        assert is_stale(output, inputs) is False


def test_asm_overlay_inputs_follow_includes() -> None:
    spec = next(item for item in overlay_specs(CLASSIC_ENV) if item.name == "SETUP")
    inputs = _resolved_inputs(spec)
    assert (ROOT / "asm/app/player_name_layout.inc").resolve() in inputs


def test_include_change_marks_asm_overlay_stale() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output = root / "SETUP.OVL"
        builder = root / "build_overlays.py"
        defs = root / "overlay_defs.asm"
        asm = root / "entry_setup.asm"
        inner = root / "layout.inc"
        deeper = root / "gen" / "deep.inc"
        deeper.parent.mkdir()
        for path, text in (
            (builder, "# builder\n"),
            (defs, "; defs\n"),
            (asm, 'INCLUDE "layout.inc"\nnop\n'),
            (inner, 'X EQU 1\n    include "deep.inc"\n'),
            (deeper, "Y EQU 2\n"),
        ):
            path.write_text(text, encoding="utf-8")
        output.write_bytes(b"x")
        old = (1_700_000_000, 1_700_000_000)
        for path in (output, builder, defs, asm, inner, deeper):
            os.utime(path, old)
        spec = OverlaySpec("SETUP", (str(asm),), ())
        inputs = spec_inputs(spec, defs, None, builder=builder, headers=[],
                             build_dir=root / "gen")
        assert is_stale(output, inputs) is False
        os.utime(deeper, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs) is True


def _include_fixture(root: Path) -> tuple[Path, Path, Path, OverlaySpec, Path]:
    """SETUP.OVL built from entry_setup.asm -> layout.inc -> deep.inc (gen/)."""
    output = root / "SETUP.OVL"
    builder = root / "build_overlays.py"
    defs = root / "overlay_defs.asm"
    asm = root / "asm" / "entry_setup.asm"
    inner = root / "asm" / "layout.inc"
    deeper = root / "gen" / "deep.inc"
    asm.parent.mkdir()
    deeper.parent.mkdir()
    for path, text in (
        (builder, "# builder\n"),
        (defs, "; defs\n"),
        (asm, 'INCLUDE "layout.inc"\nnop\n'),
        (inner, 'X EQU 1\nINCLUDE "deep.inc"\n'),
        (deeper, "Y EQU 2\n"),
    ):
        path.write_text(text, encoding="utf-8")
    output.write_bytes(b"x")
    old = (1_700_000_000, 1_700_000_000)
    for path in (output, builder, defs, asm, inner, deeper):
        os.utime(path, old)
    return output, builder, defs, OverlaySpec("SETUP", (str(asm),), ()), deeper


def test_missing_include_marks_asm_overlay_stale() -> None:
    for gone in ("asm/layout.inc", "gen/deep.inc"):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output, builder, defs, spec, _deeper = _include_fixture(root)

            def inputs() -> list[Path]:
                return spec_inputs(spec, defs, None, builder=builder,
                                   headers=[], build_dir=root / "gen")

            assert is_stale(output, inputs()) is False, gone
            (root / gone).unlink()
            # Nothing that remains is newer than the overlay; only the
            # unresolvable include can force the assembler to run and fail.
            assert is_stale(output, inputs()) is True, gone


def test_include_resolution_change_marks_overlay_stale() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output, builder, defs, spec, deeper = _include_fixture(root)
        manifest = root / "gen" / "ovl" / "SETUP" / "inputs.txt"

        def inputs() -> list[Path]:
            return spec_inputs(spec, defs, None, builder=builder, headers=[],
                               build_dir=root / "gen")

        assert deps_changed(manifest, inputs()) is True  # never built
        write_deps(manifest, inputs())
        assert deps_changed(manifest, inputs()) is False
        # An older deep.inc next to its includer now shadows gen/deep.inc:
        # every mtime is still older than the overlay, but the assembled
        # source set changed.
        shadow = root / "asm" / "deep.inc"
        shadow.write_text("Y EQU 3\n", encoding="utf-8")
        os.utime(shadow, (1_600_000_000, 1_600_000_000))
        assert is_stale(output, inputs()) is False
        assert deps_changed(manifest, inputs()) is True
        write_deps(manifest, inputs())
        assert deps_changed(manifest, inputs()) is False
        assert deeper.exists()


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
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    start = makefile.index("ifeq ($(ATLAS_FINAL),1)")
    final_rule = makefile[start:makefile.index("endif", start) + len("endif")]
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "atlas").write_text("final offsets")
        (root / "resident").write_text("old offsets")
        (root / "overlay").write_text("final overlays")
        for name in ("atlas", "resident", "overlay"):
            os.utime(root / name, (1, 1))
        (root / "Makefile").write_text(
            "ZX_TAP := resident\n" + final_rule + "\n"
            ".PHONY: FORCE\nFORCE:\n"
            "atlas:\n\t@echo bootstrap > atlas\n"
            "resident: atlas\n\t@echo rebuilt > resident\n"
            "overlay: resident\n\t@echo repacked > overlay\n"
        )
        subprocess.run(["make", "-s", "-o", "atlas", "ATLAS_FINAL=0", "overlay"],
                       cwd=root, check=True)
        assert (root / "resident").read_text() == "old offsets"
        subprocess.run(["make", "-s", "-o", "atlas", "ATLAS_FINAL=1", "overlay"],
                       cwd=root, check=True)
        assert (root / "atlas").read_text() == "final offsets"
        assert (root / "resident").read_text().strip() == "rebuilt"
        assert (root / "overlay").read_text().strip() == "repacked"


def test_editor_imports_are_private_to_input_edit() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "defs.asm"
        output = Path(tmp) / "filtered.asm"
        source.write_text("".join(
            f"PUBLIC {name}\nDEFC {name} = $8000\n"
            for name in EDITOR_PRIVATE_SYMBOLS
        ))
        for spec in overlay_specs(CLASSIC_ENV):
            result = filter_local_symbols(source, output, spec.excluded_resident_symbols())
            assert bool(result.read_text()) == (spec.name == "INPUT_EDIT"), spec.name


def test_setup_imports_are_private_to_setup_overlays() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "defs.asm"
        output = Path(tmp) / "filtered.asm"
        source.write_text("".join(
            f"PUBLIC {name}\nDEFC {name} = $8000\n"
            for name in SETUP_PRIVATE_SYMBOLS
        ))
        for spec in overlay_specs(CLASSIC_ENV):
            result = filter_local_symbols(source, output, spec.excluded_resident_symbols())
            assert bool(result.read_text()) == (spec.name in (
                "MENU_CONFIG", "SETUP", "INPUT_EDIT", "TIME_CONFIG")), spec.name


def main() -> int:
    tests = [
        test_classic_specs_match_atlas_order,
        test_next_menu_tracks_its_visual_data_only_on_next,
        test_editor_imports_are_private_to_input_edit,
        test_setup_imports_are_private_to_setup_overlays,
        test_local_overlay_symbols_are_removed_from_resident_defs,
        test_spectranext_adds_time_overlay,
        test_normalize_overlay_name,
        test_is_stale_compares_mtimes,
        test_size_check_same_and_growth,
        test_classic_config_links_esx_saveload,
        test_classic_saveload_links_fsync,
        test_journal_capabilities_are_independent,
        test_journal_code_is_guarded_by_capabilities,
        test_classic_boot_resident_extra_is_idempotent,
        test_setup_uses_edit_buf,
        test_c_overlay_inputs_include_spectrum_headers_and_builder,
        test_asm_overlay_inputs_include_builder_not_c_headers,
        test_asm_overlay_inputs_follow_includes,
        test_include_change_marks_asm_overlay_stale,
        test_missing_include_marks_asm_overlay_stale,
        test_include_resolution_change_marks_overlay_stale,
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
