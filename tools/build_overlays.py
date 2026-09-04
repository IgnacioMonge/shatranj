#!/usr/bin/env python3
"""Assemble Shatranj overlays incrementally and optionally check one overlay's size.

The overlay recipe used to live in tools/build_overlays.sh because a Make
recipe is one Windows command line (8191 characters). This file keeps that
work off the Make command line and skips overlays whose outputs are newer
than their inputs. C overlays also watch src/common and src/spectrum headers
and this builder. `make overlay-size OVERLAY=SETUP` refreshes the config
stamp, rebuilds one overlay against an existing map, and refuses a stamp
newer than that map.
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

sys.dont_write_bytecode = True

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from gen_overlay_atlas import BLOCK_SIZE, replace_if_changed  # noqa: E402
from gen_overlay_defs import parse_map  # noqa: E402

ROOT = _TOOLS.parent
BUILDER_PATH = Path(__file__).resolve()


@dataclass(frozen=True)
class OverlaySpec:
    name: str
    asm: tuple[str, ...]
    c_sources: tuple[tuple[str, tuple[str, ...]], ...]
    extra_inputs: tuple[str, ...] = ()
    local_symbols: tuple[str, ...] = ()


SPXN_XFS_LOCAL_SYMBOLS = (
    "_esx_fopen",
    "_esx_fread",
    "_esx_fclose",
    "_spxn_xfs_fseek",
    "_spxn_xfs_dir_scratch",
    "_spxn_xfs_state_end",
    "_esx_handle",
    "_esx_buf",
    "_esx_count",
    "_esx_result",
)


def env_value(env: dict[str, str], name: str) -> str:
    return env.get(name, "").strip()


def env_paths(env: dict[str, str], name: str) -> tuple[str, ...]:
    return tuple(part for part in env_value(env, name).split() if part)


def overlay_block_size(env: dict[str, str]) -> int:
    raw = env_value(env, "OVERLAY_BLOCK_SIZE")
    size = int(raw, 0) if raw else BLOCK_SIZE
    if size not in (2048, 4096, 8192):
        raise SystemExit(f"invalid OVERLAY_BLOCK_SIZE: {size}")
    return size


def overlay_size_limit(env: dict[str, str], block_size: int) -> int:
    raw = env_value(env, "OVERLAY_SIZE_LIMIT")
    size = int(raw, 0) if raw else block_size
    if size <= 0 or size > block_size:
        raise SystemExit(f"invalid OVERLAY_SIZE_LIMIT: {size}")
    return size


def overlay_specs(env: dict[str, str]) -> list[OverlaySpec]:
    edit_buf = env_value(env, "EDIT_BUF_OVL") or "asm/overlay/edit/edit_buf.asm"
    esx_common = env_paths(env, "ESX_COMMON_ASM")
    esx_fileui = env_paths(env, "ESX_FILEUI_ASM")
    esx_saveload = env_paths(env, "ESX_SAVELOAD_ASM")
    mach = env_value(env, "GAME_PROTOCOL_MACH_SRC") or (
        "src/common/protocol/game_protocol_mach.c"
    )
    atomic = env_paths(env, "SPXN_ATOMIC_SRC")
    resolve_c = env_paths(env, "SPXN_RESOLVE_C")
    xfs_overlay = env_paths(env, "SPXN_XFS_OVERLAY_ASM")
    clock_c = env_paths(env, "SPXN_CLOCK_C")
    clock_headers = env_paths(env, "SPXN_CLOCK_HEADERS")
    mqtt_flags = tuple(shlex.split(env_value(env, "MQTT_CONNECT_OVL_CFLAGS")))
    config_flags = tuple(shlex.split(env_value(env, "CONFIG_OVL_CFLAGS")))
    clock_flags = tuple(shlex.split(env_value(env, "SPXN_CLOCK_OVL_CFLAGS")))

    specs = [
        OverlaySpec(
            "RULES",
            ("asm/overlay/rules/entry_rules.asm", "asm/overlay/rules/rules_stub.asm"),
            (),
        ),
        OverlaySpec(
            "BOARD",
            ("asm/overlay/board/entry_board.asm", "asm/overlay/board/helpers.asm"),
            (("src/spectrum/overlay/board_apply_ovl.c", ()),),
            ("src/spectrum/board/board_apply_impl.h",),
        ),
        OverlaySpec(
            "GUI_LOG",
            ("asm/overlay/gui_log/entry_gui_log.asm",),
            (("src/spectrum/overlay/gui_log_ovl.c", ()),),
        ),
        OverlaySpec(
            "MQTT_CONNECT",
            ("asm/overlay/mqtt_connect/entry_mqtt_connect.asm",),
            (
                ("src/spectrum/overlay/mqtt_connect_ovl.c", mqtt_flags),
                *((src, ()) for src in resolve_c),
            ),
        ),
        OverlaySpec(
            "MQTT_TX",
            ("asm/overlay/mqtt_tx/entry_mqtt_tx.asm",),
            (("src/spectrum/overlay/mqtt_tx_ovl.c", ()),),
        ),
        OverlaySpec(
            "DIRECT",
            ("asm/overlay/direct/entry_direct.asm",),
            (
                ("src/spectrum/overlay/direct_ovl.c", ()),
                *((src, ()) for src in resolve_c),
            ),
        ),
        OverlaySpec(
            "MENU_CONFIG",
            ("asm/overlay/menu_config/entry_menu_config.asm",),
            (),
        ),
        OverlaySpec(
            "MENU_LOGIC",
            ("asm/overlay/menu_logic/entry_menu_logic.asm",),
            (("src/spectrum/overlay/status_ovl.c", ()),),
        ),
        OverlaySpec(
            "SETUP",
            ("asm/overlay/setup/entry_setup.asm", edit_buf),
            (),
        ),
        OverlaySpec(
            "INPUT_EDIT",
            (
                "asm/overlay/input_edit/entry_input_edit.asm",
                "asm/overlay/input_edit/setup_edit_line.asm",
            ),
            (("src/spectrum/overlay/input_edit_ovl.c", ()),),
        ),
        OverlaySpec(
            "SAVELOAD",
            (
                "asm/overlay/saveload/entry_saveload.asm",
                *esx_saveload,
                *xfs_overlay,
            ),
            (
                ("src/spectrum/overlay/saveload_ovl.c", ()),
                *((src, ()) for src in atomic),
            ),
            esx_common,
            SPXN_XFS_LOCAL_SYMBOLS if xfs_overlay else (),
        ),
        OverlaySpec(
            "RESTORE",
            ("asm/overlay/restore/entry_restore.asm",),
            (("src/spectrum/overlay/restore_ovl.c", ()),),
        ),
        OverlaySpec(
            "ABOUT",
            ("asm/overlay/about/entry_about.asm", *xfs_overlay),
            (),
            (),
            SPXN_XFS_LOCAL_SYMBOLS if xfs_overlay else (),
        ),
        OverlaySpec(
            "FILEUI",
            (
                "asm/overlay/fileui/entry_fileui.asm",
                *esx_fileui,
                *xfs_overlay,
            ),
            (("src/spectrum/overlay/fileui_ovl.c", ()),),
            esx_common,
            SPXN_XFS_LOCAL_SYMBOLS if xfs_overlay else (),
        ),
        OverlaySpec(
            "CONTROL",
            ("asm/overlay/control/entry_control.asm",),
            (
                ("src/spectrum/overlay/control_ovl.c", ()),
                (mach, ()),
            ),
        ),
        OverlaySpec(
            "CONFIG",
            (
                "asm/overlay/config/entry_config.asm",
                *esx_saveload,
                *xfs_overlay,
            ),
            (
                ("src/spectrum/overlay/config_ovl.c", config_flags),
                *((src, ()) for src in atomic),
            ),
            esx_common,
            SPXN_XFS_LOCAL_SYMBOLS if xfs_overlay else (),
        ),
        OverlaySpec(
            "TIME_CONFIG",
            ("asm/overlay/time_config/entry_time_config.asm", edit_buf),
            (),
        ),
    ]
    if clock_c:
        specs.append(
            OverlaySpec(
                "TIME",
                ("asm/overlay/time/entry_time.asm",),
                (
                    ("src/spectrum/overlay/time_ovl.c", clock_flags),
                    *((src, clock_flags) for src in clock_c),
                    *((src, ()) for src in resolve_c),
                ),
                clock_headers,
            )
        )
    return specs


def normalize_overlay_name(name: str, specs: list[OverlaySpec]) -> str:
    raw = name.strip().replace("-", "_")
    if not raw:
        raise SystemExit("overlay-size requires OVERLAY=NAME (for example SETUP)")
    wanted = raw.upper()
    aliases = {spec.name.upper(): spec.name for spec in specs}
    if wanted in aliases:
        return aliases[wanted]
    available = ", ".join(spec.name for spec in specs)
    raise SystemExit(f"unknown overlay {name!r}; expected one of: {available}")


def overlay_output(build_dir: Path, zx_name: str, spec: OverlaySpec) -> Path:
    return build_dir / f"{zx_name}_{spec.name}.OVL"


def spectrum_headers(root: Path = ROOT) -> list[Path]:
    found: list[Path] = []
    for tree in (root / "src" / "common", root / "src" / "spectrum"):
        if tree.is_dir():
            found.extend(sorted(path for path in tree.rglob("*.h") if path.is_file()))
    return found


def spec_inputs(
    spec: OverlaySpec,
    overlay_defs: Path,
    config_stamp: Path | None,
    *,
    builder: Path | None = None,
    headers: list[Path] | None = None,
) -> list[Path]:
    inputs = [overlay_defs, builder if builder is not None else BUILDER_PATH]
    if config_stamp is not None:
        inputs.append(config_stamp)
    inputs.extend(Path(path) for path in spec.asm)
    inputs.extend(Path(path) for path, _flags in spec.c_sources)
    inputs.extend(Path(path) for path in spec.extra_inputs)
    if spec.c_sources:
        inputs.extend(spectrum_headers() if headers is None else headers)
    return inputs


def resolve_config_stamp(build_dir: Path) -> Path | None:
    spectrum = build_dir / "spectrum_config.json"
    if spectrum.exists():
        return spectrum
    nex = build_dir / "nex_config.json"
    if nex.exists():
        return nex
    return None


def require_map_current_for_size_check(map_path: Path, stamp: Path | None) -> None:
    if stamp is None:
        raise SystemExit(
            f"[ERR] overlay-size needs {map_path.parent / 'spectrum_config.json'}; "
            "run make tap so the config stamp exists"
        )
    try:
        map_mtime = map_path.stat().st_mtime
        stamp_mtime = stamp.stat().st_mtime
    except OSError as exc:
        raise SystemExit(f"[ERR] overlay-size cannot stat map/config: {exc}") from exc
    if stamp_mtime > map_mtime:
        raise SystemExit(
            f"[ERR] overlay-size config stamp {stamp} is newer than {map_path}; "
            "relink TAP/NEX so the map matches the current flags"
        )


def require_slot_matches_block_size(slot: int, block_size: int = BLOCK_SIZE) -> None:
    expected = {2048: 0x6800, 4096: 0x2000, 8192: 0x6000}[block_size]
    if slot != expected:
        raise SystemExit(
            f"[ERR] overlay slot {slot:#06x} does not match {block_size}-byte cap "
            f"(expected {expected:#06x})"
        )


def is_stale(output: Path, inputs: list[Path]) -> bool:
    if not output.exists():
        return True
    try:
        out_mtime = output.stat().st_mtime
    except OSError:
        return True
    for source in inputs:
        try:
            if source.stat().st_mtime > out_mtime:
                return True
        except OSError:
            return True
    return False


def split_cmd(value: str) -> list[str]:
    if not value.strip():
        raise SystemExit("missing assembler or compiler command")
    return shlex.split(value)


def run_cmd(cmd: list[str], cwd: Path | None = None) -> None:
    result = subprocess.run(cmd, cwd=cwd, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def z80asm_object(work_dir: Path, source: Path) -> Path:
    nested = work_dir / source.with_suffix(".o")
    if nested.exists():
        return nested
    flat = work_dir / f"{source.stem}.o"
    if flat.exists():
        return flat
    adjacent = source.with_suffix(".o")
    if adjacent.exists():
        return adjacent
    raise SystemExit(f"z80asm produced no object for {source}")


def assemble_asm(z80asm: list[str], work_dir: Path, source: Path) -> Path:
    work_dir.mkdir(parents=True, exist_ok=True)
    run_cmd([*z80asm, f"-O={work_dir.as_posix()}", source.as_posix()])
    return z80asm_object(work_dir, source)


def compile_c(
    zcc: list[str],
    cflags: list[str],
    extra: tuple[str, ...],
    build_dir: Path,
    build_dir_up: str,
    source: Path,
    output: Path,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    if source.is_absolute():
        src_arg = source.as_posix()
    else:
        src_arg = (Path(build_dir_up) / source).as_posix()
    try:
        out_arg = output.relative_to(build_dir).as_posix()
    except ValueError:
        out_arg = output.as_posix()
    run_cmd(
        [*zcc, "+z80", *cflags, *extra, "-c", src_arg, "-o", out_arg],
        cwd=build_dir,
    )


def write_overlay_defs(python: str, map_path: Path, overlay_defs: Path) -> None:
    result = subprocess.run(
        [python, str(_TOOLS / "gen_overlay_defs.py"), str(map_path)],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.stderr:
        sys.stderr.write(result.stderr)
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    replace_if_changed(overlay_defs, result.stdout.encode("utf-8"))


def filter_local_symbols(source: Path, output: Path, symbols: tuple[str, ...]) -> Path:
    if not symbols:
        return source
    excluded = set(symbols)
    lines = []
    for line in source.read_text(encoding="utf-8").splitlines(keepends=True):
        fields = line.split()
        if len(fields) >= 2 and fields[0] in ("PUBLIC", "DEFC"):
            if fields[1] in excluded:
                continue
        lines.append(line)
    output.write_text("".join(lines), encoding="utf-8")
    return output


def overlay_slot(map_path: Path) -> int:
    symbols = parse_map(map_path)
    slot = symbols.get("_overlay_code_slot")
    if slot is None:
        raise SystemExit(f"[ERR] _overlay_code_slot not found in {map_path}")
    return slot


def build_one_overlay(
    spec: OverlaySpec,
    *,
    env: dict[str, str],
    build_dir: Path,
    zx_name: str,
    overlay_defs: Path,
    slot: int,
    size_limit: int,
    force: bool,
    headers: list[Path] | None = None,
) -> Path:
    output = overlay_output(build_dir, zx_name, spec)
    stamp = resolve_config_stamp(build_dir)
    inputs = spec_inputs(spec, overlay_defs, stamp, headers=headers)
    if not force and not is_stale(output, inputs):
        print(f"  skip {spec.name} ({output.stat().st_size} bytes)")
        return output

    work_dir = build_dir / "ovl" / spec.name
    work_dir.mkdir(parents=True, exist_ok=True)
    z80asm = split_cmd(env_value(env, "ZX_Z80ASM") or "z80asm")
    zcc = split_cmd(env_value(env, "ZCC") or "zcc")
    cflags = shlex.split(env_value(env, "ZX_OVL_CFLAGS"))
    build_dir_up = env_value(env, "BUILD_DIR_UP") or "../"

    objects: list[Path] = []
    for asm in spec.asm:
        objects.append(assemble_asm(z80asm, work_dir, Path(asm)))
    for index, (source, extra) in enumerate(spec.c_sources):
        obj = work_dir / f"{Path(source).stem}_{index}.o"
        compile_c(zcc, cflags, extra, build_dir, build_dir_up, Path(source), obj)
        objects.append(obj)

    link_defs = filter_local_symbols(
        overlay_defs, work_dir / "overlay_defs_local.asm", spec.local_symbols
    )
    defs_o = work_dir / link_defs.with_suffix(".o").name
    defs_o.unlink(missing_ok=True)
    (work_dir / f"{link_defs.stem}.o~").unlink(missing_ok=True)
    binary_name = output.name
    run_cmd(
        [
            *z80asm,
            f"-O={work_dir.as_posix()}",
            "-b",
            f"-r0x{slot:04X}",
            f"-o={binary_name}",
            *(path.as_posix() for path in objects),
            link_defs.as_posix(),
        ]
    )
    produced = work_dir / binary_name
    if not produced.exists():
        nested = work_dir / "build" / binary_name
        if nested.exists():
            produced = nested
    if not produced.exists():
        raise SystemExit(f"[ERR] z80asm did not write {binary_name} in {work_dir}")
    if produced.resolve() != output.resolve():
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(produced.read_bytes())
    size = output.stat().st_size
    if size > size_limit:
        raise SystemExit(
            f"[ERR] {spec.name} overlay too large: {size} bytes (max {size_limit})"
        )
    print(f"  built {spec.name}: {size}/{size_limit} bytes")
    return output


def pack_atlas(env: dict[str, str], python: str) -> None:
    build_dir = Path(env_value(env, "BUILD_DIR") or "build")
    zx_name = env_value(env, "ZX_NAME") or "SHATRANJ"
    zx_ovl = Path(env_value(env, "ZX_OVL") or f"release/{zx_name}.OVL")
    atlas_table = Path(
        env_value(env, "OVL_ATLAS_TABLE") or str(build_dir / "overlay_atlas_table.asm")
    )
    extra_args = shlex.split(env_value(env, "ATLAS_EXTRA_ARGS"))
    binding_args = shlex.split(env_value(env, "ATLAS_BINDING_ARGS"))
    cmd = [
        python,
        str(_TOOLS / "gen_overlay_atlas.py"),
        "--build-dir",
        str(build_dir),
        "--name",
        zx_name,
        "--out",
        str(zx_ovl),
        "--asm-out",
        str(atlas_table),
        "--changed-stamp",
        str(build_dir / "overlay_atlas_table.changed"),
        "--sizes-out",
        str(build_dir / "overlay_sizes.json"),
        "--block-size",
        str(overlay_block_size(env)),
        "--size-limit",
        str(overlay_size_limit(env, overlay_block_size(env))),
        *extra_args,
        *binding_args,
    ]
    run_cmd(cmd)


def maybe_rebuild_resident(env: dict[str, str]) -> None:
    build_dir = Path(env_value(env, "BUILD_DIR") or "build")
    atlas_table = Path(
        env_value(env, "OVL_ATLAS_TABLE") or str(build_dir / "overlay_atlas_table.asm")
    )
    stamp = build_dir / "overlay_atlas_table.changed"
    changed = stamp.read_text(encoding="ascii").strip() if stamp.exists() else "0"
    atlas_final = env_value(env, "ATLAS_FINAL")
    zx_ovl = env_value(env, "ZX_OVL")
    make = env_value(env, "MAKE") or "make"
    if changed == "1" and atlas_final != "1":
        print(
            "[INFO] overlay atlas table changed; rebuilding resident with baked offsets"
        )
        run_cmd(
            [*split_cmd(make), "-o", str(atlas_table), "ATLAS_FINAL=1", zx_ovl]
        )
        raise SystemExit(0)
    if changed == "1" and atlas_final == "1":
        raise SystemExit("[ERR] overlay atlas table changed during final pass")


def size_check_lines(
    name: str,
    size: int,
    baseline: dict[str, object] | None,
    *,
    fail_on_growth: bool,
    fail_on_missing_baseline: bool,
    size_limit: int = BLOCK_SIZE,
) -> tuple[list[str], list[str]]:
    info: list[str] = []
    errors: list[str] = []
    free = size_limit - size
    if size > size_limit:
        errors.append(f"{name} {size} bytes exceeds {size_limit}")
        return info, errors

    overlays = baseline.get("overlays") if isinstance(baseline, dict) else None
    before = overlays.get(name) if isinstance(overlays, dict) else None
    if not isinstance(before, int):
        info.append(f"[OK] OVL_{name}: {size}/{size_limit} bytes ({free} free)")
        if fail_on_missing_baseline:
            errors.append(f"OVL_{name} missing in baseline")
        elif baseline is not None:
            info.append(f"[WARN] OVL_{name}: missing in baseline")
        return info, errors

    delta = size - before
    if delta == 0:
        info.append(
            f"[OK] OVL_{name}: {size}/{size_limit} bytes ({free} free, same as baseline)"
        )
        return info, errors
    sign = "+" if delta > 0 else ""
    info.append(
        f"[INFO] OVL_{name}: {before} -> {size} bytes ({sign}{delta}), "
        f"{free} free of {size_limit}"
    )
    if fail_on_growth and delta > 0:
        errors.append(f"OVL_{name} grew by {delta} bytes")
    return info, errors


def load_baseline(path: Path | None) -> dict[str, object] | None:
    if path is None:
        return None
    if not path.exists():
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise SystemExit(f"size baseline is not an object: {path}")
    return data


def print_pack_summary(
    build_dir: Path, zx_name: str, zx_ovl: Path, specs: list[OverlaySpec]
) -> None:
    sizes = []
    for spec in specs:
        path = overlay_output(build_dir, zx_name, spec)
        sizes.append(f"{spec.name} {path.stat().st_size} bytes")
    total = zx_ovl.stat().st_size
    print(f"[OK] {zx_name}.OVL atlas: {', '.join(sizes)}, total {total} bytes")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", help="build a single overlay (SETUP, GUI_LOG, ...)")
    parser.add_argument(
        "--size-check",
        action="store_true",
        help="compare --only overlay size with the configured cap and optional baseline",
    )
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--fail-on-growth", action="store_true")
    parser.add_argument("--fail-on-missing-baseline", action="store_true")
    parser.add_argument(
        "--force", action="store_true", help="rebuild even if outputs are fresh"
    )
    parser.add_argument(
        "--list", action="store_true", help="print overlay names and exit"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None, env: dict[str, str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    environ = env if env is not None else dict(os.environ)
    specs = overlay_specs(environ)
    block_size = overlay_block_size(environ)
    size_limit = overlay_size_limit(environ, block_size)
    if args.list:
        print("\n".join(spec.name for spec in specs))
        return 0

    build_dir = Path(env_value(environ, "BUILD_DIR") or "build")
    zx_name = env_value(environ, "ZX_NAME") or "SHATRANJ"
    map_path = build_dir / f"{zx_name}.map"
    overlay_defs = Path(
        env_value(environ, "OVL_DEFS") or str(build_dir / "overlay_defs.asm")
    )
    python = env_value(environ, "PYTHON") or sys.executable

    if not map_path.exists():
        raise SystemExit(
            f"[ERR] overlay build needs {map_path}; run make tap once to create it"
        )

    only = (args.only or "").strip()
    if args.size_check and not only:
        raise SystemExit("[ERR] overlay-size requires OVERLAY=NAME (for example SETUP)")
    selected = specs
    if only:
        name = normalize_overlay_name(only, specs)
        selected = [spec for spec in specs if spec.name == name]
        if not selected:
            raise SystemExit(f"unknown overlay {args.only!r}")

    stamp = resolve_config_stamp(build_dir)
    if args.size_check:
        require_map_current_for_size_check(map_path, stamp)

    write_overlay_defs(python, map_path, overlay_defs)
    slot = overlay_slot(map_path)
    require_slot_matches_block_size(slot, block_size)
    print(f"  overlay_code_slot = {slot:#06x}")
    headers = spectrum_headers()

    built: list[Path] = []
    for spec in selected:
        built.append(
            build_one_overlay(
                spec,
                env=environ,
                build_dir=build_dir,
                zx_name=zx_name,
                overlay_defs=overlay_defs,
                slot=slot,
                size_limit=size_limit,
                force=args.force,
                headers=headers,
            )
        )

    if args.size_check:
        if len(selected) != 1:
            raise SystemExit("--size-check requires --only NAME")
        spec = selected[0]
        size = built[0].stat().st_size
        baseline = load_baseline(args.baseline)
        info, errors = size_check_lines(
            spec.name,
            size,
            baseline,
            fail_on_growth=args.fail_on_growth,
            fail_on_missing_baseline=args.fail_on_missing_baseline,
            size_limit=size_limit,
        )
        for line in info:
            print(line)
        if errors:
            for line in errors:
                print(f"[ERR] {line}", file=sys.stderr)
            return 1
        return 0

    if only:
        return 0

    pack_atlas(environ, python)
    maybe_rebuild_resident(environ)
    zx_ovl = Path(env_value(environ, "ZX_OVL") or f"release/{zx_name}.OVL")
    print_pack_summary(build_dir, zx_name, zx_ovl, specs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
