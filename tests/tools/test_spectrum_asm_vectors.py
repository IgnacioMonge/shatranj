#!/usr/bin/env python3
"""Execute focused target-ASM regressions with z88dk's Z80 emulator."""

from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def required_tool(name: str) -> str:
    path = shutil.which(name)
    assert path is not None, f"{name} is required"
    return path


def symbol_address(map_text: str, symbol: str) -> int:
    match = re.search(
        rf"^{re.escape(symbol)}\s*=\s*\$([0-9A-Fa-f]+)\b",
        map_text,
        re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"missing {symbol} in z80asm map")
    return int(match.group(1), 16)


def assemble_vector(
    assembler: str,
    harness: Path,
    sources: list[Path],
    binary: Path,
    defines: list[str] | None = None,
) -> str:
    result = subprocess.run(
        [
            assembler,
            *(f"-D{name}" for name in (defines or [])),
            "-b",
            "-m",
            "-r=32768",
            "-O=.",
            f"-o={binary.name}",
            str(harness),
            *(str(source) for source in sources),
        ],
        cwd=binary.parent,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    return binary.with_suffix(".map").read_text(encoding="utf-8")


def execute_vector(
    emulator: str,
    binary: Path,
    map_text: str,
    counter: int = 2000,
) -> tuple[int, str, int, int, bytes]:
    start = symbol_address(map_text, "test_start")
    done = symbol_address(map_text, "test_done")
    result_addr = symbol_address(map_text, "test_result")
    image = binary.read_bytes()
    assert image[result_addr - 0x8000] == 0xFF, (
        "test_result is not initialized at its mapped binary address"
    )
    memory = binary.with_suffix(".ram")
    run = subprocess.run(
        [
            emulator,
            "-mz80",
            "-l",
            "0x8000",
            "-pc",
            f"{start:04x}",
            "-end",
            f"{done:04x}",
            "-counter",
            str(counter),
            "-output",
            str(memory),
            str(binary),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    ram = memory.read_bytes()
    assert len(ram) >= 65536, (
        "z88dk-ticks did not emit a full RAM image: "
        f"{len(ram)} bytes\n{run.stdout}{run.stderr}"
    )
    final_pc = int.from_bytes(ram[0x10006:0x10008], "little")
    return (
        ram[result_addr],
        run.stdout.strip(),
        final_pc,
        ram[0x10001],
        ram[result_addr - 4 : result_addr + 5],
    )


RULES_ASM_CASES = (
    (
        "start",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        0,
    ),
    ("normal-early", "K7/8/8/8/8/8/8/7k w - - 0 1", 0),
    ("castle-allowed", "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1", 0),
    ("castle-blocked", "4k3/8/8/8/8/8/8/R3KB1R w KQ - 0 1", 0),
    ("castle-attacked", "4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1", 0),
    ("ep-discovered-check", "k7/8/8/4KPpr/8/8/8/8 w - g6 0 1", 0),
    ("promotion", "7k/P7/8/8/8/8/8/7K w - - 0 1", 0),
    ("pinned", "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1", 0),
    ("double-check", "k3r3/8/8/8/1b6/8/8/4K3 w - - 0 1", 1),
    (
        "mate",
        "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3",
        2,
    ),
    ("stalemate", "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", 3),
    ("right-pawn-check", "4k3/5P2/8/8/8/8/8/K7 b - - 0 1", 1),
    (
        "near-exhaustive",
        "k6r/8/8/8/8/8/PPPPPPP1/7K w - - 0 1",
        1,
    ),
)

RULES_CHECK_MEASURE_CASES = (
    "normal-early",
    "right-pawn-check",
    "mate",
    "stalemate",
    "near-exhaustive",
)


def rules_case_label(name: str) -> str:
    return name.replace("-", "_")


def rules_parse_fen(fen: str) -> tuple[list[int], int, int, int]:
    board_text, side_text, castle_text, ep_text, *_ = fen.split()
    pieces = {piece: index + 1 for index, piece in enumerate("PNBRQK")}
    pieces.update({piece: 255 - index for index, piece in enumerate("pnbrqk")})
    board: list[int] = []
    for rank in board_text.split("/"):
        for token in rank:
            if token.isdigit():
                board.extend([0] * int(token))
            else:
                board.append(pieces[token])
    assert len(board) == 64, f"bad rules vector FEN: {fen}"
    castle = sum(
        bit for token, bit in zip("KQkq", (1, 2, 4, 8)) if token in castle_text
    )
    ep = 255
    if ep_text != "-":
        ep = (ord("8") - ord(ep_text[1])) * 8 + ord(ep_text[0]) - ord("a")
    return board, int(side_text == "b"), castle, ep


def rules_expected_bitset(oracle: Path, fen: str) -> bytes:
    run = subprocess.run(
        [str(oracle), "--moves", fen],
        cwd=ROOT,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    expected = bytearray(512)
    for move in run.stdout.splitlines():
        assert re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", move), (
            f"unexpected compact-rules move output: {move!r}"
        )
        from_sq = (ord("8") - ord(move[1])) * 8 + ord(move[0]) - ord("a")
        to_sq = (ord("8") - ord(move[3])) * 8 + ord(move[2]) - ord("a")
        pair = from_sq * 64 + to_sq
        expected[pair >> 3] |= 1 << (pair & 7)
    return bytes(expected)


def rules_emit_bytes(values: list[int] | bytes) -> list[str]:
    return [
        "    DEFB "
        + ",".join(f"0x{value:02x}" for value in values[offset : offset + 16])
        for offset in range(0, len(values), 16)
    ]


def write_rules_fixture(
    path: Path,
    oracle: Path,
    cases: tuple[tuple[str, str, int], ...],
) -> None:
    lines = [
        "SECTION code_user",
        "PUBLIC rules_case_table",
        "PUBLIC rules_case_count",
        "EXTERN _rules_check_ovl",
    ]
    for name, _, _ in cases:
        if name in RULES_CHECK_MEASURE_CASES:
            label = rules_case_label(name)
            lines.extend(
                (
                    f"PUBLIC measure_{label}_entry",
                    f"PUBLIC measure_{label}_call",
                    f"PUBLIC measure_{label}_done",
                )
            )
    lines.append("rules_case_table:")
    for name, _, check in cases:
        label = rules_case_label(name)
        lines.append(f"    DEFW rules_{label}_ctx, rules_{label}_expected")
        lines.append(f"    DEFB {check}")
    lines.extend(("rules_case_count:", f"    DEFB {len(cases)}"))

    for name, fen, _ in cases:
        label = rules_case_label(name)
        board, side, castle, ep = rules_parse_fen(fen)
        lines.extend(
            (
                f"rules_{label}_ctx:",
                f"    DEFW rules_{label}_board",
                f"    DEFB {side},0,0,{castle},{ep}",
                f"rules_{label}_board:",
                *rules_emit_bytes(board),
                f"rules_{label}_expected:",
                *rules_emit_bytes(rules_expected_bitset(oracle, fen)),
            )
        )
        if name in RULES_CHECK_MEASURE_CASES:
            lines.extend(
                (
                    f"measure_{label}_entry:",
                    f"    ld de, rules_{label}_ctx",
                    f"measure_{label}_call:",
                    "    call _rules_check_ovl",
                    f"measure_{label}_done:",
                    f"    jp measure_{label}_done",
                )
            )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def measure_rules_check(
    emulator: str,
    binary: Path,
    map_text: str,
    name: str,
) -> int:
    label = rules_case_label(name)
    entry = symbol_address(map_text, f"measure_{label}_entry")
    start = symbol_address(map_text, f"measure_{label}_call")
    done = symbol_address(map_text, f"measure_{label}_done")
    run = subprocess.run(
        [
            emulator,
            "-mz80",
            "-l",
            "0x8000",
            "-pc",
            f"{entry:04x}",
            "-start",
            f"{start:04x}",
            "-end",
            f"{done:04x}",
            "-counter",
            "1000000000",
            str(binary),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    match = re.search(r"(?:Ticks:\s*)?(\d+)\s*$", run.stdout + run.stderr)
    assert match is not None, f"missing T-state count: {run.stdout}{run.stderr}"
    return int(match.group(1))


def rules_vector_failure(binary: Path, map_text: str, result: int) -> str:
    ram = binary.with_suffix(".ram").read_bytes()
    case = RULES_ASM_CASES[result - 1] if 0 < result <= len(RULES_ASM_CASES) else None
    phase = ram[symbol_address(map_text, "test_phase")]
    from_sq = ram[symbol_address(map_text, "test_from")]
    to_sq = ram[symbol_address(map_text, "test_to")]
    got = ram[symbol_address(map_text, "test_got")]
    want = ram[symbol_address(map_text, "test_want")]
    if case is None:
        return f"case={result}, phase={phase}, from={from_sq}, to={to_sq}, got={got}, want={want}"
    return (
        f"case={case[0]}, fen={case[1]}, phase={'CHECK' if phase else 'PLAY'}, "
        f"side={case[1].split()[1]}, castle={case[1].split()[2]}, "
        f"ep={case[1].split()[3]}, from={from_sq}, to={to_sq}, "
        f"got={got}, want={want}"
    )


def run_rules_asm_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    oracle = ROOT / "build/netchesszx_rules_compact_perft_test.exe"
    source = ROOT / "asm/overlay/rules/rules_stub.asm"
    harness = ROOT / "tests/spectrum/rules_asm_vector.asm"
    assert oracle.exists(), f"missing compact-rules oracle: {oracle}"

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        fixture = tmp / "rules_cases.asm"
        write_rules_fixture(fixture, oracle, RULES_ASM_CASES)
        binary = tmp / "rules_vector.bin"
        map_text = assemble_vector(assembler, harness, [fixture, source], binary)
        result, _, _, _, _ = execute_vector(
            emulator, binary, map_text, counter=1000000000
        )
        assert result == 0, "RULES ASM divergence: " + rules_vector_failure(
            binary, map_text, result
        )

        measured = []
        for name in RULES_CHECK_MEASURE_CASES:
            cycles = measure_rules_check(emulator, binary, map_text, name)
            measured.append((name, cycles))
            print(
                f"RULES_CHECK {name}: {cycles} T-states, "
                f"{cycles / 3.5:.1f} us at 3.5 MHz"
            )
        print(f"RULES_CHECK maximum: {max(cycles for _, cycles in measured)} T-states")

        mutant = tmp / "rules_stub_pawn_offset_mutant.asm"
        original = source.read_text(encoding="utf-8")
        broken = original.replace("    ld b, 9\n", "    ld b, 7\n", 1)
        assert broken != original, "RULES pawn-offset mutant pattern missing"
        mutant.write_text(broken, encoding="utf-8")
        mutant_fixture = tmp / "rules_mutant_case.asm"
        mutant_case = tuple(
            case for case in RULES_ASM_CASES if case[0] == "right-pawn-check"
        )
        write_rules_fixture(mutant_fixture, oracle, mutant_case)
        fixed_regression_binary = tmp / "rules_pawn_offset_fixed.bin"
        assemble_vector(
            assembler,
            harness,
            [mutant_fixture, source],
            fixed_regression_binary,
        )
        mutant_binary = tmp / "rules_pawn_offset_mutant.bin"
        mutant_map = assemble_vector(
            assembler, harness, [mutant_fixture, mutant], mutant_binary
        )
        mutant_result, _, _, _, _ = execute_vector(
            emulator, mutant_binary, mutant_map, counter=1000000000
        )
        assert mutant_binary.read_bytes() != fixed_regression_binary.read_bytes(), (
            "z80asm reused the production RULES image for the pawn mutant"
        )
        assert mutant_result != 0, "RULES ASM vector accepted pawn-offset mutant"
        print(f"RULES ASM vector ok: {len(RULES_ASM_CASES)} positions; mutant detected")


def run_proto_copy_token_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/shrink_kernels.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "proto_copy_token.bin"
        harness = ROOT / "tests/spectrum/proto_copy_token_vector.asm"
        fixed_map = assemble_vector(assembler, harness, [source], binary)
        result, _, _, _, _ = execute_vector(emulator, binary, fixed_map)
        assert result == 0, f"MOVE 1 e7e8q target token vector failed: result={result}"

        # Prove that the vector detects token/digit dispatch damage, not merely
        # that the harness can run the current source. The shared prologue
        # makes the former token-to-function fall-through structurally
        # impossible; forcing token mode into the digit loop is its equivalent
        # regression.
        mutant = tmp / "shrink_kernels_fallthrough.asm"
        original = source.read_text(encoding="utf-8")
        broken = original.replace(
            "    jr nc, cpb_digit_loop\n    ld a, b",
            "    jr cpb_digit_loop\n    ld a, b",
            1,
        )
        assert broken != original, "proto-copy dispatch pattern missing"
        mutant.write_text(broken, encoding="utf-8")
        mutant_binary = tmp / "proto_copy_token_fallthrough.bin"
        mutant_map = assemble_vector(assembler, harness, [mutant], mutant_binary)
        mutant_result, mutant_ticks, mutant_pc, mutant_a, mutant_near = execute_vector(
            emulator, mutant_binary, mutant_map
        )
        assert mutant_binary.read_bytes() != binary.read_bytes(), (
            "z80asm reused the fixed object for the fall-through mutant"
        )
        assert mutant_result != 0, (
            "promotion vector accepted the old fall-through: "
            f"ticks={mutant_ticks}, pc={mutant_pc:04x}, "
            f"token={symbol_address(mutant_map, '_netchesszx_asm_proto_copy_token'):04x}, "
            f"digits={symbol_address(mutant_map, '_netchess_proto_copy_digits'):04x}, "
            f"fixed_digits={symbol_address(fixed_map, '_netchess_proto_copy_digits'):04x}, "
            f"a={mutant_a:02x}, near={mutant_near.hex()}"
        )


def run_timer_tick_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/shrink_kernels.asm"
    harness = ROOT / "tests/spectrum/timer_tick_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "timer_tick.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, _, _, _, _ = execute_vector(emulator, binary, map_text)
        assert result == 0, f"timer tick target vector failed: result={result}"


def run_mqtt_connect_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    harness = ROOT / "tests/spectrum/mqtt_connect_vector.asm"
    sources = [
        ROOT / "asm/overlay/mqtt_connect/entry_mqtt_connect.asm",
        ROOT / "asm/spectrum/text.asm",
    ]
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "mqtt_connect.bin"
        map_text = assemble_vector(assembler, harness, sources, binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=100000
        )
        assert result == 0, (
            "MQTT CONNECT target vector failed: "
            f"case={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )


def run_setup_visible_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/overlay/setup/entry_setup.asm"
    harness = ROOT / "tests/spectrum/setup_visible_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        exported = tmp / "entry_setup_test.asm"
        original = source.read_text(encoding="utf-8")
        exported.write_text(
            original.replace(
                "PUBLIC _setup_step_ovl_entry",
                "PUBLIC _setup_step_ovl_entry\nPUBLIC _setup_compute_visible_ovl_entry",
                1,
            ),
            encoding="utf-8",
        )
        binary = tmp / "setup_visible.bin"
        map_text = assemble_vector(
            assembler,
            harness,
            [
                exported,
                ROOT / "asm/spectrum/text.asm",
                ROOT / "asm/spectrum/shrink_kernels.asm",
            ],
            binary,
        )
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=500000
        )
        assert result == 0, (
            "setup visible-mask target vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, a={reg_a:02x}, near={near.hex()}"
        )


def run_time_config_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/overlay/time_config/entry_time_config.asm"
    harness = ROOT / "tests/spectrum/time_config_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "time_config.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=200000
        )
        assert result == 0, (
            "time config target vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, a={reg_a:02x}, near={near.hex()}"
        )


def run_setup_navigation_cursor_source_guard() -> None:
    """Keep the setup key aliases and resident editor cursor wired in."""
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    screen = (ROOT / "asm/spectrum/screen.asm").read_text(encoding="utf-8")
    edit = (ROOT / "asm/spectrum/edit_field.asm").read_text(encoding="utf-8")
    for alias in ("'o'", "'a'", "'q'", "'p'"):
        assert alias in app, f"host setup alias {alias} missing"
    for row in (
        "DEFB '5', 0x83, 'o', 0x83",
        "DEFB '6', 0x82, 'a', 0x82",
        "DEFB '7', 0x81, 'q', 0x81",
        "DEFB '8', 0x84, 'p', 0x84",
    ):
        assert row in screen, f"target setup alias row missing: {row}"
    assert "#define nav_key_alias netchesszx_setup_nav_key_alias" in app
    assert "_spectrum_gui_edit_show:" in edit
    assert "_spectrum_gui_edit_hide:" in edit
    assert "_spectrum_gui_edit_tick:" in edit
    assert "jp draw_char64_pixels_at_tmp" in edit
    assert "call _spectrum_gui_edit_tick" in screen
    assert edit.count("ret") > 3, "editor cursor routines were reduced to no-ops"


def run_spectranext_storage_source_guard() -> None:
    """Keep the XFS path, adapter ABI and Classic branch visibly separate."""
    loader = (ROOT / "asm/esxdos/overlay_loader.asm").read_text(encoding="utf-8")
    about = (ROOT / "asm/overlay/about/entry_about.asm").read_text(encoding="utf-8")
    lowram = (ROOT / "src/spectrum/lowram_map.h").read_text(encoding="utf-8")
    config = (ROOT / "src/spectrum/overlay/config_ovl.c").read_text(encoding="utf-8")
    net = (ROOT / "src/spectrum/transport/net.c").read_text(encoding="utf-8")
    time_ovl = (ROOT / "src/spectrum/overlay/time_ovl.c").read_text(encoding="utf-8")
    fileui = (ROOT / "src/spectrum/overlay/fileui_ovl.c").read_text(encoding="utf-8")
    saveload = (ROOT / "src/spectrum/overlay/saveload_ovl.c").read_text(
        encoding="utf-8"
    )
    direct = (ROOT / "src/spectrum/overlay/direct_ovl.c").read_text(
        encoding="utf-8"
    )

    assert "IFDEF NETCHESSZX_SPECTRANEXT" in loader
    assert "call _spxn_detect" in loader
    detect_sequence = "\n".join(
        (
            "    push hl",
            "    call _spxn_detect",
            "    ld a, l",
            "    pop hl",
            "    cp 1",
        )
    )
    assert detect_sequence in loader, "loader discarded the detection result"
    assert "jr ovl_exec_after_xfs_helper" in loader
    assert "call _esx_fopen" in loader
    assert "call _esx_fread" in loader
    assert "call _spxn_xfs_fseek" in loader
    assert "call _esx_fclose" in loader
    assert "ovl_verify_atlas:" in loader
    assert "call ovl_verify_atlas" in loader
    assert "ovl_atlas_fingerprint_0" in loader
    assert "jr c, ovl_load_fail_close" in loader
    assert "rst 8" in loader, "Classic esxDOS path was removed"
    assert "IFDEF NETCHESSZX_SPECTRANEXT" in about
    assert "call _esx_fopen" in about
    assert "call _spxn_xfs_fseek" in about
    assert "call _esx_fclose" in about
    assert "ld de, about_input" in about
    assert "ld hl, about_input\n    call _esx_fopen" in about
    assert "rst 8" in about, "Classic About esxDOS path was removed"
    assert "NETCHESSZX_LOWRAM_XFS_STATE_ADDR 0x5b50u" in lowram
    assert "NETCHESSZX_LOWRAM_XFS_STATE_SIZE 20u" in lowram
    assert "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR 0x662bu" in lowram
    assert "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_SIZE 0x100u" in lowram
    assert 'CONFIG_PATH "/CFG/SHATRANJ.CFG"' in config
    assert 'fileui_dir[] = "/CFG"' in fileui
    assert 'SAVELOAD_CONFIG_DIR "/CFG"' in saveload
    assert 'SAVELOAD_DIR "/CFG/"' in saveload
    # Durability: both writers replace through a staged temp plus RENAME.
    # esx_freplace truncated the live file before writing a byte, so a power
    # cut mid-write destroyed the stored record. Do not reintroduce it.
    assert "esx_freplace" not in config and "esx_freplace" not in saveload
    assert "spxf_replace_atomic(CONFIG_PATH_ARG, CONFIG_TEMP_PATH_ARG" in config
    assert "spxf_replace_atomic(SAVELOAD_PATH_ARG, SAVELOAD_TEMP_PATH_ARG" in saveload
    assert 'CONFIG_TEMP_PATH "/CFG/SHATRANJ.TMP"' in config
    assert 'SAVELOAD_TEMP_PATH "/CFG/SHATSAVE.TMP"' in saveload
    assert "spxn_detect" not in config
    assert "spxn_detect" not in saveload
    # Only the directory is committed now; the records ride the transaction.
    assert "esx_commit(CONFIG_DIR_ARG)" in config
    assert "esx_commit(SAVELOAD_CONFIG_DIR_ARG)" in saveload
    assert "spectrum_net_background_drain();" in config
    assert "spectrum_net_background_drain();" in saveload
    assert "spectrum_net_background_drain();" in fileui
    raw_start = net.index("void spectrum_net_background_drain(void)")
    raw_end = net.index("void spectrum_net_background_drain_clock(void)", raw_start)
    assert "spectrum_overlay_exec" not in net[raw_start:raw_end]
    # ROM services may page their own module into Page B. Arguments must not
    # point back into the executing overlay at $2000..$2fff.
    assert "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR" in config
    assert "config_read_record(CONFIG_PATH_ARG" in config
    assert "request.host = ntp_host_arg;" in time_ovl
    assert "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR" in time_ovl
    assert "saveload_stage_paths();" in saveload
    assert "esx_fopen(saveload_path)" not in saveload
    assert "esx_fcreate_new(saveload_path);" in saveload
    assert "esx_funlink(saveload_path);" in saveload
    assert saveload.count("if (!saveload_ensure_dir())") == 1
    assert "esx_opendir(fileui_dir)" not in fileui
    assert fileui.count("esx_opendir(FILEUI_DIR_ARG)") == 2
    assert "spxn_send_all(&newline, 1u, 150u)" in direct


def run_edit_field_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    source = ROOT / "asm/spectrum/edit_field.asm"
    harness = ROOT / "tests/spectrum/edit_field_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "edit_field.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=100000
        )
        assert result == 0, (
            "edit-field target vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )


def run_input_queue_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    source = ROOT / "asm/spectrum/input_queue.asm"
    harness = ROOT / "tests/spectrum/input_queue_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "input_queue.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=100000
        )
        assert result == 0, (
            "input queue target vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )


EXPECTED_WORST_EDIT_LINE = b"IP    192.168.100.200:65535\x00"


def assert_edit_line_within_buffer(map_text: str, ram: bytes, label: str) -> None:
    """Prove the worst-case DIRECT edit line still fits its buffer.

    setup_edit_line_buf is sized for the largest line it can hold: one
    row byte, the six-character IP label, a fifteen-character host, a colon,
    a five-digit port and the terminator. That is an exact fit, so there is no
    margin. Both addresses come from the z80asm map.
    """
    buf = symbol_address(map_text, "setup_edit_line_buf")
    end = symbol_address(map_text, "setup_edit_line_buf_end")
    capacity = end - buf
    needed = 1 + len(EXPECTED_WORST_EDIT_LINE)
    assert capacity == needed, (
        f"{label}: setup_edit_line_buf holds {capacity} bytes; "
        f"the worst-case line needs {needed}"
    )
    rendered = ram[buf + 1 : end]
    assert rendered == EXPECTED_WORST_EDIT_LINE, (
        f"{label}: worst-case DIRECT edit line is {rendered!r}, expected "
        f"{EXPECTED_WORST_EDIT_LINE!r}"
    )


def run_setup_menu_render_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    sources = [
        ROOT / "asm/overlay/menu_config/entry_menu_config.asm",
        ROOT / "asm/overlay/input_edit/setup_edit_line.asm",
    ]
    harness = ROOT / "tests/spectrum/setup_menu_render_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "setup_menu_render.bin"
        map_text = assemble_vector(assembler, harness, sources, binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=500000
        )
        assert result == 0, (
            "setup menu render target vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )
        assert_edit_line_within_buffer(
            map_text, binary.with_suffix(".ram").read_bytes(), "classic"
        )
        next_binary = tmp / "setup_menu_render_next.bin"
        next_map = assemble_vector(
            assembler,
            harness,
            sources,
            next_binary,
            defines=["NETCHESSZX_NEXT"],
        )
        next_result, next_ticks, next_pc, next_a, next_near = execute_vector(
            emulator, next_binary, next_map, counter=500000
        )
        assert next_result == 0, (
            "Next setup menu render target vector failed: "
            f"result={next_result}, ticks={next_ticks}, pc={next_pc:04x}, "
            f"a={next_a:02x}, near={next_near.hex()}"
        )
        assert_edit_line_within_buffer(
            next_map, next_binary.with_suffix(".ram").read_bytes(), "Next"
        )


def run_frame_arg_push_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/shrink_kernels.asm"
    harness = ROOT / "tests/spectrum/frame_arg_push_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "frame_arg_push.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, _, _, _, _ = execute_vector(emulator, binary, map_text)
        assert result == 0, f"frame argument push vector failed: result={result}"


def run_gui_log_notify_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/overlay/gui_log/entry_gui_log.asm"
    harness = ROOT / "tests/spectrum/gui_log_notify_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "gui_log_notify.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=2000000
        )
        assert result == 0, (
            "GUI log notify dictionary vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )


SAN_LEGAL_CALL = (
    "    call _spectrum_board_is_legal_move_coords\n"
    "    ld a, l\n"
    "    pop hl\n"
    "    pop de\n"
    "    pop bc\n"
    "    or a\n"
)
SAN_LEGAL_CALL_BUG = (
    "    call _spectrum_board_is_legal_move_coords\n"
    "    pop hl\n"
    "    pop de\n"
    "    pop bc\n"
    "    ld a, l\n"
    "    or a\n"
)


def run_san_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/san.asm"
    kernels = ROOT / "asm/spectrum/shrink_kernels.asm"
    harness = ROOT / "tests/spectrum/san_vector.asm"
    original = source.read_text(encoding="utf-8")
    assert SAN_LEGAL_CALL in original, "SAN legal-return preserve pattern missing"

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "san_vector.bin"
        map_text = assemble_vector(assembler, harness, [source, kernels], binary)
        result, _, _, _, _ = execute_vector(emulator, binary, map_text, counter=100000)
        assert result == 0, f"SAN vector failed: result={result}"

        mutant = tmp / "san_pop_clobber.asm"
        broken = original.replace(SAN_LEGAL_CALL, SAN_LEGAL_CALL_BUG, 1)
        assert broken != original, "SAN legal-return mutant pattern missing"
        mutant.write_text(broken, encoding="utf-8")
        mutant_binary = tmp / "san_pop_clobber.bin"
        mutant_map = assemble_vector(
            assembler, harness, [mutant, kernels], mutant_binary
        )
        mutant_result, _, _, _, _ = execute_vector(
            emulator, mutant_binary, mutant_map, counter=100000
        )
        assert mutant_binary.read_bytes() != binary.read_bytes(), (
            "z80asm reused the fixed object for the SAN pop-clobber mutant"
        )
        assert mutant_result != 0, (
            f"SAN vector accepted the pop-hl return clobber: result={mutant_result}"
        )


def run_streq_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/text.asm"
    harness = ROOT / "tests/spectrum/streq_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "streq_vector.bin"
        map_text = assemble_vector(assembler, harness, [source], binary)
        result, _, _, _, _ = execute_vector(emulator, binary, map_text)
        assert result == 0, f"streq vector failed: result={result}"


def main() -> int:
    run_rules_asm_vector()
    run_setup_navigation_cursor_source_guard()
    run_spectranext_storage_source_guard()
    run_edit_field_vector()
    run_input_queue_vector()
    run_proto_copy_token_vector()
    run_frame_arg_push_vector()
    run_timer_tick_vector()
    run_mqtt_connect_vector()
    run_setup_visible_vector()
    run_time_config_vector()
    run_setup_menu_render_vector()
    run_gui_log_notify_vector()
    run_san_vector()
    run_streq_vector()
    print("Spectrum ASM regression vectors ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
