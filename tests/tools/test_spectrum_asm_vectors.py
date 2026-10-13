#!/usr/bin/env python3
"""Execute focused target-ASM regressions with z88dk's Z80 emulator."""

from __future__ import annotations

import re
from itertools import combinations
import shutil
import subprocess
import tempfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from asm_source import read_asm

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
    from gen_ui_layout import layout_asm
    from roller_z80asm import adapt

    (binary.parent / "roller_z80.inc").write_text(
        adapt((ROOT / "asm/vendor/roller/roller.inc").read_text()), encoding="ascii"
    )
    family = "next" if "NETCHESSZX_NEXT" in (defines or []) else "zx"
    (binary.parent / "ui_layout.inc").write_text(
        layout_asm(ROOT / f"src/spectrum/ui/layout_{family}.h"), encoding="ascii"
    )
    result = subprocess.run(
        [
            assembler,
            *(f"-D{name}" for name in (defines or [])),
            "-b",
            "-m",
            "-r=32768",
            f"-I={ROOT}",
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


def sdcc_ix_helpers(assembler: str) -> list[Path]:
    """z88dk's IX frame helpers: the installed sources, else the copies kept in
    tests/spectrum for installs without libsrc (such as the CI image)."""
    installed = Path(assembler).resolve().parents[1] / "libsrc/l/sdcc"
    names = ("___sdcc_enter_ix.asm", "____sdcc_4_push_hlix.asm")
    if all((installed / name).is_file() for name in names):
        return [installed / name for name in names]
    return [ROOT / "tests/spectrum/z88dk_sdcc_ix_helpers.asm"]


def execute_vector(
    emulator: str,
    binary: Path,
    map_text: str,
    counter: int = 2000,
    cpu: str = "z80",
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
            f"-m{cpu}",
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


def run_shrink_helpers_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    kernels = ROOT / "asm/spectrum/shrink_kernels.asm"
    dispatch = ROOT / "asm/app/screen_dispatch.inc"
    panel_draw = ROOT / "asm/ula/panel_draw.inc"
    panel_entries = ROOT / "asm/ula/panel_entries.inc"
    kernel_text = kernels.read_text(encoding="utf-8")
    dispatch_text = dispatch.read_text(encoding="utf-8")
    panel_draw_text = panel_draw.read_text(encoding="utf-8")
    panel_entries_text = panel_entries.read_text(encoding="utf-8")

    assert kernel_text.count("call netchesszx_load_stack_args2") == 8
    assert dispatch_text.count("call netchesszx_load_stack_args2") == 2
    old_prologue = (
        "    ld hl, 2\n"
        "    add hl, sp\n"
        "    ld e, (hl)\n"
        "    inc hl\n"
        "    ld d, (hl)\n"
        "    inc hl\n"
        "    ld c, (hl)\n"
        "    inc hl\n"
        "    ld b, (hl)\n"
    )
    assert old_prologue not in kernel_text
    assert old_prologue not in dispatch_text

    descriptor_re = re.compile(
        r"call draw_ikkle_inline\s*\n"
        r"\s*DEFB\s+([^,\n]+),\s*([^,\n]+),\s*([^\n]+)\n"
        r"\s*DEFW\s+([^\s;]+)"
    )
    descriptors = [
        tuple(field.strip() for field in match)
        for match in descriptor_re.findall(panel_draw_text + "\n" + panel_entries_text)
    ]
    assert descriptors == [
        (
            "NETCHESSZX_INFO_CHAT_TITLE_ROW",
            "NETCHESSZX_INFO_TEXT_COL",
            "ATTR_CHAT_TITLE",
            "chat_msg",
        ),
        (
            "NETCHESSZX_INFO_SETUP_GAME_HEADER_ROW",
            "NETCHESSZX_INFO_TEXT_COL",
            "ATTR_MOVES_TITLE",
            "game_setup_msg",
        ),
        (
            "NETCHESSZX_INFO_SETUP_HEADER_ROW",
            "NETCHESSZX_INFO_TEXT_COL",
            "ATTR_MOVES_TITLE",
            "session_setup_msg",
        ),
        (
            "NETCHESSZX_INFO_SETUP_HEADER_ROW",
            "NETCHESSZX_INFO_TEXT_COL",
            "ATTR_MOVES_TITLE",
            "preflight_setup_msg",
        ),
    ]
    # The MOVES header labels and discs belong to the STATUS overlay.
    assert "moves_white_msg" not in panel_draw_text

    helper_start = panel_entries_text.index("draw_ikkle_inline:")
    helper_end = panel_entries_text.index("_spectrum_render_ikkle_abs_at:")
    helper_source = panel_entries_text[helper_start:helper_end]

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        panel_source = tmp / "panel_inline_helper.asm"
        panel_source.write_text(
            "SECTION code_user\n"
            "PUBLIC draw_ikkle_inline\n"
            "EXTERN tmp_row, tmp_col, tmp_scan, current_attr\n"
            "EXTERN draw_ikkle_text_at\n"
            + helper_source
            + "_spectrum_render_ikkle_abs_at:\n    ret\n",
            encoding="utf-8",
        )
        binary = tmp / "shrink_helpers.bin"
        harness = ROOT / "tests/spectrum/shrink_helpers_vector.asm"
        map_text = assemble_vector(
            assembler, harness, [kernels, panel_source], binary
        )
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=100000
        )
        assert result == 0, (
            "shrink helper vector failed: "
            f"result={result}, ticks={ticks}, pc={pc:04x}, "
            f"a={reg_a:02x}, near={near.hex()}"
        )
        assert (
            symbol_address(map_text, "_netchesszx_asm_direct_parse_role_owner")
            - symbol_address(map_text, "netchesszx_load_stack_args2")
            == 12
        )
        assert symbol_address(
            map_text, "_netchesszx_asm_net_copy"
        ) == symbol_address(map_text, "_netchesszx_asm_net_move")
        assert (
            symbol_address(map_text, "_spectrum_render_ikkle_at")
            - symbol_address(map_text, "draw_ikkle_inline")
            == 25
        )


def run_timer_tick_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")

    source = ROOT / "asm/spectrum/shrink_kernels.asm"
    harness = ROOT / "tests/spectrum/timer_tick_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        binary = tmp / "timer_tick.bin"
        map_text = assemble_vector(
            assembler, harness, [source], binary, defines=["NETCHESSZX_NEXT_BANKING"]
        )
        result, _, _, _, _ = execute_vector(emulator, binary, map_text, counter=100000)
        assert result == 0, f"timer tick target vector failed: result={result}"


def run_timer_roll_vector() -> None:
    """Pixel-only public Roller API: arbitrary ULA positions, no UI state."""
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    screen = bytes((i * 73 + (i >> 5) + 91) & 255 for i in range(6912))
    strip = bytes([0x13, 0x57, 0x9B, 0xDF, 0x24, 0x68, 0xAC, 0xE0])
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        harness = tmp / "roll.asm"
        harness.write_text(
            """SECTION code_user
test_start:
    ld sp,0xff00
    ld iy,0x5c3a
    ld hl,screen_seed
    ld de,0x4000
    ld bc,6912
    ldir
    ld ix,context
    ld a,(phase_seed)
    call roller_paint
    ld (saved_ix),ix
    ld (saved_iy),iy
    ld (saved_sp),sp
    xor a
    ld (test_result),a
test_done:
    jp test_done
test_result: defb 255
saved_ix: defw 0
saved_iy: defw 0
saved_sp: defw 0
phase_seed: defb 0
context:
    defb 4,1,0
    defw 0,strip_seed,0
strip_seed: defs 8
screen_seed: defs 6912
INCLUDE "asm/spectrum/timer_roll_asset.asm"
""",
            encoding="ascii",
        )
        binary = tmp / "roll.bin"
        symbols = assemble_vector(assembler, harness, [], binary)
        addr = lambda name: symbol_address(symbols, name)
        original = bytearray(binary.read_bytes())
        assert addr("roller_end") - addr("roller_start") == 130

        def vram(y, x):
            return 0x4000 | ((y & 192) << 5) | ((y & 7) << 8) | ((y & 56) << 2) | x

        total = 0
        for y, x in (
            (0, 0),
            (6, 31),
            (7, 17),
            (15, 3),
            (63, 20),
            (64, 9),
            (127, 30),
            (128, 1),
            (188, 31),
        ):
            for mask in (15, 240, 255):
                for phase in range(5):
                    image = bytearray(original)

                    def seed(label, data):
                        pos = addr(label) - 0x8000
                        image[pos : pos + len(data)] = data

                    seed("screen_seed", screen)
                    seed("strip_seed", strip)
                    seed("phase_seed", bytes([phase]))
                    seed(
                        "context",
                        bytes([4, 1, mask])
                        + vram(y, x).to_bytes(2, "little")
                        + addr("strip_seed").to_bytes(2, "little")
                        + bytes(2),
                    )
                    binary.write_bytes(image)
                    result, _, pc, _, _ = execute_vector(
                        emulator, binary, symbols, counter=1000000
                    )
                    case = (y, x, mask, phase)
                    assert result == 0 and pc == addr("test_done"), case
                    ram = binary.with_suffix(".ram").read_bytes()
                    expected = bytearray(screen)
                    for row in range(4):
                        pos = vram(y + row, x) - 0x4000
                        expected[pos] = (expected[pos] & ~mask) | (
                            strip[phase + row] & mask
                        )
                    assert ram[0x4000:0x5B00] == expected, case
                    assert ram[addr("strip_seed") : addr("strip_seed") + 8] == strip, (
                        case
                    )
                    for label, value in (
                        ("saved_ix", addr("context")),
                        ("saved_iy", 0x5C3A),
                        ("saved_sp", 0xFF00),
                    ):
                        assert (
                            int.from_bytes(ram[addr(label) : addr(label) + 2], "little")
                            == value
                        ), case
                    total += 1
    print(
        f"Original Roller: {total} arbitrary-coordinate, mask, phase and ABI cases passed"
    )


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
                "PUBLIC _setup_step_ovl_entry\n"
                "PUBLIC _setup_compute_visible_ovl_entry\n"
                "PUBLIC su_validate_ip\n"
                "PUBLIC su_step_row",
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
    setup = (ROOT / "src/spectrum/ui/setup.c").read_text(encoding="utf-8")
    screen = read_asm(ROOT / "asm/spectrum/screen.asm")
    edit = (ROOT / "asm/spectrum/edit_field.asm").read_text(encoding="utf-8")
    for alias in ("'o'", "'a'", "'q'", "'p'"):
        assert alias in setup, f"host setup alias {alias} missing"
    for row in (
        "DEFB '5', 0x83, 'o', 0x83",
        "DEFB '6', 0x82, 'a', 0x82",
        "DEFB '7', 0x81, 'q', 0x81",
        "DEFB '8', 0x84, 'p', 0x84",
        "DEFB 13, 'y', 32, 'y', 0x8a, 'n'",
    ):
        assert row in screen, f"target setup alias row missing: {row}"
    assert "#define nav_key_alias netchesszx_setup_nav_key_alias" in app
    assert "_netchesszx_confirm_key_alias:" in screen
    assert "_spectrum_gui_edit_show:" in edit
    assert "_spectrum_gui_edit_hide:" in edit
    assert "_spectrum_gui_edit_tick:" in edit
    assert "jp draw_char64_pixels_at_tmp" in edit
    assert "call _spectrum_gui_edit_tick" in screen
    assert edit.count("ret") > 3, "editor cursor routines were reduced to no-ops"


def run_key_alias_vector() -> None:
    """Execute the production setup/game and confirmation alias routines."""
    screen = read_asm(ROOT / "asm/spectrum/screen.asm")
    start = screen.index("_netchesszx_setup_nav_key_alias:")
    end = screen.index("_netchesszx_setup_render_edit_line:", start)
    aliases = screen[start:end]
    checks = (
        ("_netchesszx_setup_nav_key_alias", ord("5"), 0x83),
        ("_netchesszx_setup_nav_key_alias", ord("o"), 0x83),
        ("_netchesszx_setup_nav_key_alias", ord("6"), 0x82),
        ("_netchesszx_setup_nav_key_alias", ord("a"), 0x82),
        ("_netchesszx_setup_nav_key_alias", ord("7"), 0x81),
        ("_netchesszx_setup_nav_key_alias", ord("q"), 0x81),
        ("_netchesszx_setup_nav_key_alias", ord("8"), 0x84),
        ("_netchesszx_setup_nav_key_alias", ord("p"), 0x84),
        ("_netchesszx_setup_nav_key_alias", 0x81, 0x81),
        ("_netchesszx_setup_nav_key_alias", ord("x"), ord("x")),
        ("_netchesszx_confirm_key_alias", 13, ord("y")),
        ("_netchesszx_confirm_key_alias", 32, ord("y")),
        ("_netchesszx_confirm_key_alias", 0x8A, ord("n")),
        ("_netchesszx_confirm_key_alias", ord("x"), ord("x")),
    )
    body = ["SECTION code_user", "test_start: ld sp,0xff00"]
    for routine, key, expected in checks:
        body.extend(
            (
                f" ld l,{key}",
                f" call {routine}",
                " ld a,l",
                f" cp {expected}",
                " jp nz,test_fail",
            )
        )
    body.extend(
        (
            " xor a",
            " ld (test_result),a",
            "test_done: jp test_done",
            "test_fail: ld a,1",
            " ld (test_result),a",
            " jp test_done",
            "test_result: defb 255",
            aliases,
        )
    )
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "key_alias.asm"
        harness.write_text("\n".join(body), encoding="ascii")
        binary = temp / "key_alias.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        result, output, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, symbols, counter=200000
        )
        assert result == 0 and pc == symbol_address(symbols, "test_done"), output
    print(f"Target key aliases: {len(checks)} mapped/passthrough cases passed")


def run_name_tail_vector() -> None:
    """Execute CONTROL's production MNAME validator against the C mirror's cases."""
    control = read_asm(ROOT / "asm/overlay/control/entry_control.asm")
    start = control.index("_netchess_proto_name_tail:")
    end = control.index("_control_format_busy_ovl_entry:", start)
    routine = control[start:end]
    cases = (
        ("MNAME IGNACIO", 6),
        ("MNAME CONSTANTIN", 6),
        ("MNAME A-9_Z", 6),
        ("MNAME ", 0),
        ("MNAME CONSTANTINO", 0),
        ("MNAME PEPE ", 0),
        ("MNAME  PEPE", 0),
        ("MNAME pepe", 0),
        ("MNAME PE:PE", 0),
        ("MNAMEPEPE", 0),
        ("MACH ZX", 0),
        ("CHAT MNAME X", 0),
    )
    body = ["SECTION code_user", "test_start: ld sp,0xff00"]
    for index, (text, offset) in enumerate(cases):
        body.extend(
            (
                f" ld hl,case_{index}",
                " call _netchess_proto_name_tail",
                f" ld de,{f'case_{index}+{offset}' if offset else '0'}",
                " or a",
                " sbc hl,de",
                " jp nz,test_fail",
            )
        )
    body.extend(
        (
            " xor a",
            " ld (test_result),a",
            "test_done: jp test_done",
            "test_fail: ld a,1",
            " ld (test_result),a",
            " jp test_done",
            "test_result: defb 255",
        )
    )
    for index, (text, _offset) in enumerate(cases):
        body.append(f'case_{index}: defm "{text}"')
        body.append(" defb 0")
    body.append(routine)
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "name_tail.asm"
        harness.write_text("\n".join(body), encoding="ascii")
        binary = temp / "name_tail.bin"
        symbols = assemble_vector(required_tool("z80asm"), harness, [], binary)
        result, output, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, symbols, counter=200000
        )
        assert result == 0 and pc == symbol_address(symbols, "test_done"), output
    print(f"CONTROL MNAME validator: {len(cases)} accept/reject cases passed")


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
    journal = (ROOT / "asm/overlay/journal/stj_codec.asm").read_text(encoding="utf-8")
    journal_append = (ROOT / "asm/overlay/saveload/journal_append.asm").read_text(
        encoding="utf-8"
    )
    direct = (ROOT / "src/spectrum/overlay/direct_ovl.c").read_text(encoding="utf-8")

    assert "IFDEF NETCHESSZX_SPECTRANEXT" in loader
    assert "call _spxn_detect" in loader
    detect_sequence = "\n".join(
        (
            "    push hl",
            "    call _spxn_detect",
            "    xor a",
            "    dec l",
            "    jr nz, ovl_xfs_checked",
        )
    )
    assert detect_sequence in loader, "loader discarded the detection result"
    assert "call _spxn_xfs_firmware_probe" in loader, "firmware 1.0 probe was removed"
    assert "jr ovl_exec_after_xfs_helper" in loader
    assert "call _spxn_xfs_open_here" in loader, "startup must open on the launch mount"
    assert "call _spxn_xfs_select_slot0" in loader, "slot-0 fallback was removed"
    assert "call _spxn_dat_bind" in loader, "piece sets must borrow the retained DAT"
    assert "call _spxn_dat_release" in loader
    assert "call _esx_fopen" not in loader and "call _spxn_xfs_fseek" not in loader
    assert "call _esx_fread" in loader
    assert "call _esx_fclose" in loader
    assert "ovl_verify_atlas:" in loader
    assert "call ovl_verify_atlas" in loader
    assert "ovl_atlas_fingerprint_0" in loader
    assert "jr c, ovl_load_fail_close" in loader
    assert "rst 8" in loader, "Classic esxDOS path was removed"
    assert "IFDEF NETCHESSZX_SPECTRANEXT" in about
    assert "call _spxn_dat_bind" in about, "About must borrow the retained DAT"
    assert "call _esx_fopen" not in about and "call _spxn_xfs_fseek" not in about
    assert "call _esx_fclose" in about
    assert about.count("call about_dzx0_check_room") == 2
    assert "ld (about_output_left), de" in about
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
    assert "esx_frename(SAVELOAD_PATH_ARG);" in saveload
    assert "end[-3] = 'D';" in saveload
    assert "end[-2] = 'E';" in saveload
    assert "end[-1] = 'L';" in saveload
    assert "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR + 108u" in saveload
    assert "SAVELOAD_SPECTRANEXT_PATH_SIZE 18u" in saveload
    # The ROM pages its XFS module over Page B. STJ code executes there, so
    # generated and constant path arguments must be copied below the overlay
    # before every file call; payload/header buffers retain the same scratch.
    assert "stj_path EQU STJ_CHUNK + STJ_CHUNK_SIZE" in journal
    assert "stj_xfs_path_arg EQU stj_path + STJ_PATH_SIZE" in journal
    assert "stj_xfs_stage_path:" in journal
    for operation in (
        "_esx_fcreate",
        "_esx_fopen_update",
        "_esx_commit",
        "_esx_frename",
    ):
        assert f"call stj_xfs_stage_path\n    call {operation}" in journal
    save_start = journal.index("stj_xfs_save:")
    save_end = journal.index("stj_xfs_save_fail:", save_start)
    save_path = journal[save_start:save_end]
    validate = (
        "    ld hl, stj_save_tmp\n    call stj_xfs_stage_path\n    call _esx_fopen\n"
    )
    assert validate in save_path
    assert save_path.index(validate) < save_path.index("    call _esx_commit")
    assert "call stj_xfs_file_check" in save_path
    assert "JA_PATH EQU journal_append_buf + JA_CHUNK" in journal_append
    assert "ja_spxn_stage_path:" in journal_append
    seek_ok = "call _spxn_xfs_fseek\n    ld a, l\n    dec a\n    or h"
    assert journal_append.count(seek_ok) == 2
    read_any_start = journal.index("stj_io_read_any:")
    read_any = journal[read_any_start : journal.index("; CRC-16/", read_any_start)]
    assert "call _spxn_rom_error" in read_any
    xfs_read = journal[
        journal.index("stj_xfs_read_chunk:") : journal.index("stj_xfs_write_chunk:")
    ]
    assert "call stj_io_read_any\n    jr c, stj_xfs_io_bad_close" in xfs_read
    for operation in ("_esx_fopen", "_esx_fopen_update"):
        assert f"call ja_spxn_stage_path\n    call {operation}" in journal_append
    adopt = journal_append[
        journal_append.index("_stj_spxn_adopt_new:") : journal_append.index(
            "ja_spxn_adopt_done:"
        )
    ]
    assert "call ja_create_tmp" in adopt
    assert "call _spxn_rom_error" in adopt
    assert "call _esx_fcreate" not in adopt
    append_io = journal_append[
        journal_append.index("ja_spxn_io:") : journal_append.index(
            "ja_spxn_write_failed:"
        )
    ]
    assert "ld hl, (ja_rec)\n    ld (journal_append_buf), hl" in append_io
    assert "ld hl, journal_append_buf\n    ld bc, 2\n    call ja_io_write" in append_io
    assert "ld hl, ja_rec" not in append_io
    assert saveload.count("if (!saveload_ensure_dir())") == 1
    assert "esx_opendir(fileui_dir)" not in fileui
    assert fileui.count("esx_opendir(FILEUI_DIR_ARG)") == 2
    assert "while (count < FILEUI_SLOTS)" not in fileui
    assert "FILEUI_CANDIDATE_BYTES" in fileui
    assert "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE" in fileui
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


def run_edit_buf_vector() -> None:
    """Pin the overlay editor to its frozen reference over every valid state."""
    edit = (ROOT / "asm/spectrum/edit_field.asm").read_text(encoding="utf-8")
    # edit_buf.asm steps HL from _edit_len to _edit_pos.
    assert re.search(r"^_edit_len:.*\n_edit_pos:", edit, re.MULTILINE), (
        "_edit_pos must directly follow _edit_len in edit_field.asm"
    )
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    harness = ROOT / "tests/spectrum/edit_buf_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "edit_buf.bin"
        map_text = assemble_vector(
            assembler, harness, [ROOT / "asm/overlay/edit/edit_buf.asm"], binary
        )
        result, ticks, pc, reg_a, near = execute_vector(
            emulator, binary, map_text, counter=200_000_000
        )
        assert result == 0, (
            "edit_buf old-vs-new vector failed: "
            f"result={result}, max/len/pos/key={near[:4].hex()}, "
            f"ticks={ticks}, pc={pc:04x}"
        )


def run_input_queue_vector() -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    source = ROOT / "asm/platform/input_queue.inc"
    harness = ROOT / "tests/spectrum/input_queue_vector.asm"
    with tempfile.TemporaryDirectory() as tmp_name:
        binary = Path(tmp_name) / "input_queue.bin"
        map_text = assemble_vector(
            assembler,
            harness,
            [source],
            binary,
        )
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
            emulator, binary, map_text, counter=800000
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
            emulator, next_binary, next_map, counter=800000
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
    fixture = (ROOT / "tests/spectrum/gui_log_notify_vector.asm").read_text()
    header = (ROOT / "src/common/ui_messages.h").read_text()
    restart = re.search(
        r'^#define NETCHESSZX_UI_SPECTRUM_CONFIRM_RESTART_GAME "([^"\n]*)"$',
        header, re.MULTILINE,
    )
    assert restart is not None, "missing Spectrum restart prompt macro"
    old_expected = '    DEFB "Restart game? Y/N", 0'
    assert fixture.count(old_expected) == 1
    fixture = fixture.replace(old_expected, f'    DEFB "{restart[1]}", 0')
    # Message 11 is the OVER-only opponent restart prompt that also offers S.
    request = re.search(
        r'^#define NETCHESSZX_UI_SPECTRUM_CONFIRM_RESTART_REQUEST "([^"\n]*)"$',
        header, re.MULTILINE,
    )
    assert request is not None and request[1].startswith("S:STATS ")
    assert fixture.count(f'    DEFB "{request[1]}", 0') == 1, request[1]
    # Menu HINTS toggle: ids 48/49 must decode to the public notice macros.
    for state in ("OFF", "ON"):
        macro = re.search(
            rf'^#define NETCHESSZX_UI_SPECTRUM_NOTICE_HINTS_{state} "([^"\n]*)"$',
            header, re.MULTILINE,
        )
        assert macro is not None, f"missing HINTS {state} notice macro"
        assert fixture.count(f'    DEFB "{macro[1]}", 0') == 1, macro[1]
    assert re.search(r"^#define SPECTRUM_GUI_MSG_HINTS_ON 49u$", header, re.MULTILINE)
    assert re.search(r"^#define SPECTRUM_GUI_MSG_RESTART_WAIT 50u$", header, re.MULTILINE)
    assert re.search(r"^#define SPECTRUM_GUI_MSG_COUNT 51u$", header, re.MULTILINE)
    with tempfile.TemporaryDirectory() as tmp_name:
        harness = Path(tmp_name) / "gui_log_notify_vector.asm"
        harness.write_text(fixture, encoding="ascii")
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
    print(f"GUI_LOG message 19 matches macro: {restart[1]}")


GUI_CHAT_LINES = 0x5D96
GUI_CHAT_ROWS = 9
GUI_CHAT_SLOT = 28


def gui_log_chat_model(
    buf: bytearray, count: int, who: int, render: int, clock: bytes, text: bytes
) -> tuple[int, bytes]:
    """The C gui_log_add_chat word wrap that the ASM chat path replaced."""
    log = bytearray(b"C")
    pos = 0
    first = True
    while True:
        index = count
        if index >= GUI_CHAT_ROWS:
            buf[: (GUI_CHAT_ROWS - 1) * GUI_CHAT_SLOT] = buf[GUI_CHAT_SLOT:]
            if render:
                log += b"S"
            index = GUI_CHAT_ROWS - 1
        else:
            count += 1
        line = bytearray(GUI_CHAT_SLOT)
        if first:
            line[0] = who
            line[1:7] = clock
            col = 7
            first = False
        else:
            col = 1
        while text[pos : pos + 1] == b" ":
            pos += 1
        while pos < len(text) and col < 25:
            word = 0
            while pos + word < len(text) and text[pos + word] != 32 and word < 24:
                word += 1
            space = int(col > (7 if line[0] else 1))
            if word + space > 25 - col:
                if not space:
                    while pos < len(text) and text[pos] != 32 and col < 25:
                        line[col] = text[pos] if 32 <= text[pos] <= 126 else 32
                        col += 1
                        pos += 1
                break
            if space:
                line[col] = 32
                col += 1
            for _ in range(word):
                line[col] = text[pos] if 32 <= text[pos] <= 126 else 32
                col += 1
                pos += 1
            while text[pos : pos + 1] == b" ":
                pos += 1
        base = index * GUI_CHAT_SLOT
        buf[base : base + GUI_CHAT_SLOT] = line
        if render:
            log += b"L" + (GUI_CHAT_LINES + base).to_bytes(2, "little") + line
        if pos >= len(text):
            break
    return count, bytes(log + b"\x01" + buf + bytes([count]))


def gui_log_chat_batches() -> list[list[tuple[int, int, int, bytes, bytes]]]:
    import random

    long_word = bytes(range(ord("a"), ord("z") + 1)) * 3
    hand = [
        (0, ord("W"), 1, b"12:34 ", b"hello world"),
        (0xFE, ord("B"), 1, b"12:35 ", b"the quick brown fox jumps over the lazy dog"),
        (0xFE, ord("W"), 1, b"--:-- ", b"abcdefghijklmnopqr"),
        (0xFE, ord("W"), 1, b"--:-- ", b"abcdefghijklmnopqr x"),
        (0xFE, ord("W"), 1, b"--:-- ", b"abcdefghijklmnopqr   "),
        (0xFE, ord("B"), 1, b"00:01 ", b"abcdefghijklmnopqrs"),
        (0, ord("B"), 1, b"00:01 ", long_word[:30]),
        (0, ord("W"), 1, b"00:02 ", long_word[:60]),
        (0, ord("W"), 1, b"00:03 ", b"x " + long_word[:24]),
        (0, ord("W"), 1, b"00:03 ", b"x " + long_word[:25] + b" y"),
        (0, ord("W"), 1, b"00:04 ", b"a " + long_word[:16]),
        (0, ord("W"), 1, b"00:04 ", b"a " + long_word[:17]),
        (0, ord("W"), 1, b"00:04 ", b"a " + long_word[:16] + b" " + long_word[:23]),
        (0, ord("W"), 1, b"00:04 ", b"a " + long_word[:16] + b" " + long_word[:22] + b" z"),
        (0, ord("B"), 1, b"00:05 ", b"   leading"),
        (0, ord("B"), 1, b"00:05 ", b"trailing   "),
        (0, ord("B"), 1, b"00:05 ", b"     "),
        (0, ord("B"), 1, b"00:05 ", b""),
        (0, ord("B"), 1, b"00:05 ", b"a    b     c"),
        (0, ord("W"), 1, b"00:06 ", b"a\x01b \x7fc~ \x1f \x80\xff! {|}"),
        (0, ord("W") | 0x80, 1, b"00:07 ", b"TAKEBACK"),
        (0, ord("B") | 0x80, 0, b"00:07 ", b"no render here at all, long enough to wrap"),
        (0, ord("W"), 2, b"00:08 ", b"render flag two"),
        (8, ord("W"), 1, b"00:09 ", b"one two three four five six seven eight nine"),
        (9, ord("B"), 1, b"00:10 ", b"full log scrolls every new row of this text"),
        (9, ord("B"), 0, b"00:11 ", b"quiet scroll of a full log over two rows"),
        (9, ord("W"), 1, b"00:12 ", long_word * 3),
        (0xFE, ord("W"), 1, b"00:13 ", b"x" * 40 + b" " + b"y" * 3),
    ]
    rng = random.Random(0xC4A7)
    alphabet = b"abcdefghij" * 4 + b"     " * 3 + b"\x01\x1f\x7f\x80\xff~!"
    batches = [hand]
    for _ in range(4):
        batch = []
        for _ in range(36):
            size = rng.choice((0, 1, 5, 17, 18, 19, 24, 25, 30, 42, 42, 60, 90))
            text = bytes(rng.choice(alphabet) for _ in range(size))
            if rng.random() < 0.3:
                text = text.replace(b" ", b"")
            batch.append((
                rng.choice((0xFE, 0xFE, 0xFE, 0, 7, 8, 9)),
                rng.choice((ord("W"), ord("B"), ord("W") | 0x80, ord("B") | 0x80)),
                rng.choice((0, 1, 1, 2)),
                bytes(rng.choice(b"0123456789:- ") for _ in range(6)),
                text,
            ))
        batches.append(batch)
    return batches


def gui_log_move_cells(restored: bytes) -> tuple[list[str], bytes]:
    """Cases and expected 16-byte cells of the replaced C gui_log_copy_move."""
    table, expected = [], bytearray()
    for clock in (b"12:34 ", b"\0" * 6, b"\0bcdef", b"9:59  "):
        for move in (None, b"", b"e2e4", b"e7e8q", b"abcdefg", b"abcdefgh",
                     b"abcdefghijkl", b"O-O-O"):
            if move is None:
                cell = restored + b"\0"
                table.append("    DEFB 0, " + ", ".join(str(v) for v in clock))
            else:
                shown = clock if clock[0] else b"--:-- "
                cell = shown + move[:7].ljust(7, b" ") + b"\0"
                table.append("    DEFB 1, " + ", ".join(
                    str(v) for v in (*clock, *move, 0)))
            expected += cell + b"\xaa" * (16 - len(cell))
    table.append("    DEFB 0xff")
    return table, bytes(expected)


def run_gui_log_chat_batches(sources: list[Path], fixture: str) -> None:
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    header = (ROOT / "src/common/ui_messages.h").read_text()
    restored = re.search(
        r'^#define NETCHESSZX_UI_EVENT_RESTORED "([^"\n]*)"$', header, re.MULTILINE
    )
    assert restored is not None, "missing RESTORED marker macro"
    move_table, move_cells = gui_log_move_cells(restored[1].encode("ascii"))
    for number, cases in enumerate(gui_log_chat_batches()):
        table = []
        buf = bytearray(GUI_CHAT_ROWS * GUI_CHAT_SLOT)
        count = 0
        expected = bytearray()
        starts = []
        for preset, who, render, clock, text in cases:
            starts.append(len(expected))
            if preset != 0xFE:
                count = preset
            count, log = gui_log_chat_model(buf, count, who, render, clock, text)
            expected += log
            table.append(
                "    DEFB " + ", ".join(
                    str(value) for value in (preset, who, render, *clock, *text, 0)
                )
            )
        table.append("    DEFB 0xff")
        table.append("test_move_cases:")
        table += move_table
        expected += move_cells + b"E"
        assert 0xA000 + len(expected) < 0xF000, "chat event log reaches test_pixels"
        with tempfile.TemporaryDirectory() as tmp_name:
            harness = Path(tmp_name) / "gui_log_chat_vector.asm"
            harness.write_text(fixture + "\n".join(table) + "\n", encoding="ascii")
            binary = Path(tmp_name) / "gui_log_chat.bin"
            map_text = assemble_vector(assembler, harness, sources, binary)
            result, ticks, pc, reg_a, near = execute_vector(
                emulator, binary, map_text, counter=20000000
            )
            ram = binary.with_suffix(".ram").read_bytes()
        assert result == 0, (
            f"GUI log chat vector batch {number} did not finish: result={result}, "
            f"ticks={ticks}, pc={pc:04x}, a={reg_a:02x}, near={near.hex()}"
        )
        actual = ram[0xA000 : 0xA000 + len(expected)]
        if actual != expected:
            at = next(i for i in range(len(expected)) if actual[i] != expected[i])
            case = max(i for i, start in enumerate(starts) if start <= at)
            assert at < len(expected) - len(move_cells) - 1, (
                f"GUI log move cell {(at - len(expected) + len(move_cells) + 1) // 16} "
                f"differs: expected {expected[at:at + 16].hex()} "
                f"got {actual[at:at + 16].hex()}"
            )
            raise AssertionError(
                f"GUI log chat batch {number} case {case} {cases[case]!r} differs "
                f"at log byte {at}: expected {expected[at:at + 32].hex()} "
                f"got {actual[at:at + 32].hex()}"
            )


def run_gui_log_chat_vector() -> None:
    """The ASM chat word wrap and move cell match the replaced C byte for byte."""
    source = ROOT / "asm/overlay/gui_log/entry_gui_log.asm"
    fixture = (ROOT / "tests/spectrum/gui_log_chat_vector.asm").read_text()
    run_gui_log_chat_batches([source], fixture)


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


def run_keyboard_vector(screen_text: str | None = None) -> None:
    """Preserve the pre-compaction physical-key priority, including zero keys."""
    ascending, descending = (0, 1, 2, 3, 4), (4, 3, 2, 1, 0)
    shift = [(4, ascending, (8, 0, 132, 129, 130)), (3, (0, 1, 4), (136, 137, 131))]
    symbol = [
        (0, (3, 1, 2, 4), "?:`/"),
        (1, ascending, "~|\\{}"),
        (2, ascending, (0, 0, 0, 60, 62)),
        (3, ascending, "!@#$%"),
        (4, descending, "&'()_"),
        (5, ascending, (34, 59, 0, 93, 91)),
        (6, (4, 3, 2, 1), "^-+="),
        (7, (2, 3, 4), ".,*"),
    ]
    letters = [
        (0, (3, 1, 2, 4), "czxv"),
        (2, ascending, "qwert"),
        (1, ascending, "asdfg"),
        (7, (4, 2, 3), "bmn"),
        (6, (4, 3, 2, 1), "hjkl"),
        (5, ascending, "poiuy"),
        (3, ascending, "12345"),
        (4, descending, "67890"),
    ]

    def scan(matrix, table):
        for row, bits, keys in table:
            for bit, key in zip(bits, keys):
                if matrix[row] & (1 << bit):
                    return ord(key) if isinstance(key, str) else key
        return None

    def expected(matrix):
        caps, sym = matrix[0] & 1, matrix[7] & 2
        if caps and matrix[3] & 1:
            return 0x90
        if caps and matrix[7] & 1:
            return 0x8A
        if matrix[6] & 1:
            return 13
        if matrix[7] & 1:
            return 32
        if caps:
            key = scan(matrix, shift)
            if key is not None:
                return key
        return scan(matrix, symbol if sym else letters) or 0

    matrices = set()
    for modifiers in range(4):
        base = [0] * 8
        base[0], base[7] = modifiers & 1, modifiers & 2
        matrices.add(tuple(base))
        # Every row combination, then every cross-row key pair, with modifiers.
        for row in range(8):
            for mask in range(32):
                matrix = base.copy()
                matrix[row] |= mask
                matrices.add(tuple(matrix))
        for left, right in combinations(range(40), 2):
            matrix = base.copy()
            for key in (left, right):
                matrix[key // 5] |= 1 << (key % 5)
            matrices.add(tuple(matrix))
    cases = sorted(matrices)
    screen = (
        screen_text
        if screen_text is not None
        else (ROOT / "asm/platform/input_scan.inc").read_text(encoding="utf-8")
    )
    scanner = screen[
        screen.index("_spectrum_key_edit_pressed:") : screen.index(
            "_spectrum_input_flush_until_release:"
        )
    ]
    # Replace only physical IN: all production selection and table code executes.
    scanner = scanner.replace("in a, (c)", "call read_ula")
    with tempfile.TemporaryDirectory() as tmp_name:
        for offset in range(-512, len(cases), 512):
            batch = [(0,) * 8] if offset < 0 else cases[offset : offset + 512]
            harness = Path(tmp_name) / f"keyboard_{offset}.asm"
            harness.write_text(
                "SECTION code_user\ntest_start:\n ld sp,0xff00\n ld ix,cases\n"
                "test_case:\n push ix\n pop hl\n ld de,matrix\n ld bc,8\n ldir\n"
                " call spectrum_input_scan_raw\n ld a,(ix+8)\n cp l\n jr nz,test_bad\n"
                " ld a,h\n or a\n jr nz,test_bad\n ld bc,9\n add ix,bc\n"
                " push ix\n pop hl\n ld de,cases_end\n or a\n sbc hl,de\n jr nz,test_case\n"
                " xor a\n jr test_store\ntest_bad:\n ld a,1\n"
                "test_store:\n ld (test_result),a\ntest_done:\n jp test_done\n"
                "read_ula:\n ld a,c\n cp 0xfe\n jr nz,test_bad\n push hl\n push de\n"
                " ld a,b\n cpl\n ld de,0\nread_row:\n rra\n jr c,read_found\n"
                " inc e\n jr read_row\nread_found:\n ld hl,matrix\n add hl,de\n"
                " ld a,(hl)\n cpl\n pop de\n pop hl\n ret\n"
                "test_result: DEFB 255\nmatrix: DEFS 8\n"
                + scanner
                + "\nscanner_end:\ncases:\n"
                + "".join(
                    " DEFB " + ",".join(map(str, (*matrix, expected(matrix)))) + "\n"
                    for matrix in batch
                )
                + "cases_end:\n",
                encoding="utf-8",
            )
            binary = harness.with_suffix(".bin")
            map_text = assemble_vector(required_tool("z80asm"), harness, [], binary)
            result, ticks, pc, _, _ = execute_vector(
                required_tool("z88dk-ticks"), binary, map_text, counter=100000000
            )
            assert result == 0 and pc == symbol_address(map_text, "test_done"), (
                f"keyboard mismatch in batch {offset}"
            )
            if offset < 0:
                print(f"Keyboard idle vector (includes port stub): {ticks}")
    size = symbol_address(map_text, "scanner_end") - symbol_address(
        map_text, "_spectrum_key_edit_pressed"
    )
    size -= scanner.count(
        "call read_ula"
    )  # Three-byte stub calls replace two-byte INs.
    print(f"Keyboard ASM: {len(cases)} matrices passed; scanner/tables {size} bytes")


def run_piece_pointer_vector() -> None:
    """Execute the production lookup for every byte and both square parities."""
    screen = read_asm(ROOT / "asm/spectrum/screen.asm")
    lookup = screen[
        screen.index("piece_sprite_for_char:") : screen.index(
            "draw_char64_at_tmp:", screen.index("piece_sprite_for_char:")
        )
    ]
    parity = screen[
        screen.index("square_parity:") : screen.index("_spectrum_render_moves:")
    ]
    cases = []
    for square in (0, 1):
        for char in range(256):
            normalized = char - 32 if char >= ord("a") else char
            piece = "KQRBNP".find(chr(normalized))
            expected = (
                0
                if piece < 0
                else 0x6373 + piece * 64 + ((char >= ord("a")) ^ square) * 32
            )
            cases.append(f" DEFB {char},{square}\n DEFW {expected}\n")
    with tempfile.TemporaryDirectory() as tmp_name:
        harness = Path(tmp_name) / "piece_pointer.asm"
        harness.write_text(
            "SECTION code_user\npiece_sprites_16x16 EQU 0x6373\n"
            "test_start:\n ld sp,0xff00\n ld ix,cases\n"
            "test_case:\n ld a,(ix+1)\n ld (board_col),a\n"
            " ld a,(ix+0)\n call piece_sprite_for_char\n"
            " ld e,(ix+2)\n ld d,(ix+3)\n or a\n sbc hl,de\n jr nz,test_bad\n"
            " inc ix\n inc ix\n inc ix\n inc ix\n push ix\n pop hl\n"
            " ld de,cases_end\n or a\n sbc hl,de\n jr nz,test_case\n"
            " xor a\n jr test_store\ntest_bad:\n ld a,1\n"
            "test_store:\n ld (test_result),a\ntest_done:\n jp test_done\n"
            "test_result: DEFB 255\nboard_row: DEFB 0\nboard_col: DEFB 0\n"
            'psfc_piece_chars: DEFM "KQRBNP"\n'
            + lookup
            + "\nlookup_end:\n"
            + parity
            + "\ncases:\n"
            + "".join(cases)
            + "cases_end:\n",
            encoding="utf-8",
        )
        binary = harness.with_suffix(".bin")
        map_text = assemble_vector(required_tool("z80asm"), harness, [], binary)
        result, _, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, map_text, counter=1000000
        )
        assert result == 0 and pc == symbol_address(map_text, "test_done"), (
            "piece pointer mismatch"
        )
        size = symbol_address(map_text, "lookup_end") - symbol_address(
            map_text, "piece_sprite_for_char"
        )
    print(f"Piece pointer ASM: 512 cases passed; lookup {size} bytes")


def run_ikkle_text_vector() -> None:
    """Compare every screen byte after ikkle, 64-column and cursor draws."""
    import random

    def ula(y: int) -> int:
        return ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)

    entries = (ROOT / "asm/ula/panel_entries.inc").read_text(encoding="utf-8")
    primitives = (ROOT / "asm/ula/panel_primitives.inc").read_text(encoding="utf-8")
    helpers = (
        entries[
            entries.index("_spectrum_render_game_timer_clear:") : entries.index(
                "_spectrum_render_turn_label:"
            )
        ]
        + entries[
            entries.index("attr_cell_for_tmpcol:") : entries.index(
                "_spectrum_info_show_preflight:"
            )
        ]
        + primitives[
            primitives.index("pixel_down_hl:") : primitives.index(
                "clear_right_text_row:"
            )
        ]
    )
    rng = random.Random(0x1CC1E)
    screen = bytes(rng.randrange(256) for _ in range(6912))
    glyphs = bytes(rng.randrange(256) for _ in range(128))
    font = bytes(rng.randrange(256) for _ in range(256))
    timer_text = b"GAME:12h34m TURN:05m59s"
    # Row, half-column, attr, text: both nibble phases, case folding, blanks
    # (controls, space, DEL+), and a stop at column 64 before the NUL.
    cases = (
        (22, 1, 0x38, b"Ab z{|}~\x7f\x80\xff 09:"),
        (5, 60, 0x3A, b"OVERFLOW"),
        (12, 33, 0x45, b"`a_@!\x1f"),
    )
    pixels = bytearray(screen[:6144])
    attrs = bytearray(screen[6144:])

    def draw(text: bytes, y: int, col: int, attr: int) -> tuple[int, int]:
        for index, char in enumerate(text):
            if col >= 64:
                return col, index
            byte = col >> 1
            for k in range(4):
                pixels[ula(y + k) + byte] &= 0xF0 if col & 1 else 0x0F
            attrs[(y >> 3) * 32 + byte] = attr
            if 33 <= char < 128:
                at = (char - 64 if char >= 96 else char - 32) * 2
                d, e = glyphs[at], glyphs[at + 1]
                for k, nibble in enumerate((d >> 4, d & 15, e >> 4, e & 15)):
                    pixels[ula(y + k) + byte] |= nibble if col & 1 else nibble << 4
            col += 1
        return col, len(text)

    # Timer: blank 12 bytes x 4 scanlines from half-column 40, then the text.
    for y in range(18, 22):
        pixels[ula(y) + 20 : ula(y) + 32] = bytes(12)
    attrs[2 * 32 + 20 : 2 * 32 + 32] = bytes([0x07] * 12)
    draw(timer_text, 18, 41, 0x07)
    exits = [draw(text, row * 8 + 2, col, attr) for row, col, attr, text in cases]

    # 64-column text: row, half-column, bytes, attr (None: pixels only), text.
    # Covers both start phases, NUL padding and the blank first/last scans.
    lines = (
        (20, 0, 1, 0x38, b"> "),
        (21, 2, 6, 0x47, b"Hi!\x7f"),
        (19, 5, 4, None, b"ODDCOLUMNS"),
    )
    for row, col, count, attr, text in lines:
        for scan in range(8):
            at = ula(row * 8 + scan) + (col >> 1)
            chars = iter(text)

            def glyph() -> int:
                char = next(chars, 0)
                return font[(char * 8 + scan - 1) & 0xFF] if char else 0

            left_pending = not col & 1
            for byte in range(count):
                if scan in (0, 7):
                    pixels[at + byte] = 0
                    continue
                left = glyph() & 0xF0 if left_pending else 0
                left_pending = True
                pixels[at + byte] = left | (glyph() & 0x0F)
        if attr is not None:
            attrs[row * 32 + (col >> 1) : row * 32 + (col >> 1) + count] = bytes(
                [attr] * count
            )
    # Half-width cells sharing one byte, then the input cursor under each.
    cells = ((17, 10, ord("Q"), 0x16), (17, 11, ord("z"), 0x29))
    for row, col, char, attr in cells:
        for scan in range(8):
            at = ula(row * 8 + scan) + (col >> 1)
            bits = font[(char * 8 + scan - 1) & 0xFF] if 0 < scan < 7 else 0
            mask = 0x0F if col & 1 else 0xF0
            pixels[at] = (pixels[at] & ~mask & 0xFF) | (bits & mask)
        attrs[row * 32 + (col >> 1)] = attr
    for row, col, _, _ in cells:
        pixels[ula(row * 8 + 7) + (col >> 1)] |= 0x0F if col & 1 else 0xF0

    def defb(data: bytes) -> str:
        return " DEFB " + ",".join(str(value) for value in data) + "\n"

    calls = "".join(
        f" ld a,{attr}\n ld (current_attr),a\n ld a,{row}\n ld (tmp_row),a\n"
        f" ld a,{col}\n ld (tmp_col),a\n ld a,2\n ld (tmp_scan),a\n"
        f" ld hl,case_{i}\n call draw_ikkle_text_at\n ld (exit_hl_{i}),hl\n"
        f" ld a,(tmp_col)\n ld (exit_col_{i}),a\n"
        for i, (row, col, attr, _) in enumerate(cases)
    )
    calls += "".join(
        (f" ld a,{attr}\n ld (current_attr),a\n" if attr is not None else "")
        + f" ld b,{row}\n ld c,{col}\n ld d,{count}\n ld hl,line_{i}\n"
        + " call draw_text64_line_"
        + ("pixels" if attr is None else "attr")
        + "_fast\n"
        for i, (row, col, count, attr, _) in enumerate(lines)
    )
    calls += "".join(
        f" ld a,{attr}\n ld (current_attr),a\n ld a,{row}\n ld (tmp_row),a\n"
        f" ld a,{col}\n ld (tmp_col),a\n ld a,{char}\n call draw_char64_at_tmp\n"
        for row, col, char, attr in cells
    )
    calls += "".join(
        f" ld a,{row}\n ld (tmp_row),a\n ld a,{col}\n ld (tmp_col),a\n"
        " call draw_input_cursor_at_tmp\n"
        for row, col, _, _ in cells
    )
    data = "".join(
        f"line_{i}:\n{defb(text + bytes(1))}" for i, (*_, text) in enumerate(lines)
    )
    data += "".join(
        f"case_{i}:\n{defb(text + bytes(1))}exit_hl_{i}: DEFW 0\nexit_col_{i}: DEFB 0\n"
        for i, (_, _, _, text) in enumerate(cases)
    )
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        (tmp / "screen.bin").write_bytes(screen)
        harness = tmp / "ikkle_text.asm"
        harness.write_text(
            'SECTION code_user\nINCLUDE "ui_layout.inc"\n'
            "test_start:\n ld sp,0xff00\n ld hl,screen_seed\n ld de,0x4000\n"
            " ld bc,6912\n ldir\n ld hl,timer_text\n"
            " call _spectrum_render_game_timer_clear\n"
            + calls
            + " xor a\n ld (test_result),a\ntest_done:\n jp test_done\n"
            + helpers
            # Stub keeps the production clobbers: A, DE, HL; preserves BC.
            + "font_scanline:\n add a,a\n add a,a\n add a,a\n ld e,a\n"
            " ld a,(tmp_scan)\n add a,e\n ld e,a\n ld d,0\n ld hl,font\n"
            " add hl,de\n ld a,(hl)\n ret\n"
            'INCLUDE "asm/ula/address.inc"\nINCLUDE "asm/ula/text.inc"\n'
            "tmp_row: DEFB 0\ntmp_col: DEFB 0\ntmp_char: DEFB 0\ntmp_attr: DEFB 0\n"
            "tmp_scan: DEFB 0\ncurrent_attr: DEFB 0\npiece_row: DEFB 0\n"
            "piece_col: DEFB 0\npiece_scan: DEFB 0\nrender_skip_clear: DEFB 0\n"
            "text_ptr: DEFW 0\nsprite_ptr: DEFW 0\n"
            "input_prompt_msg: DEFB 62,32,0\ntimer_ikkle_packed:\n"
            + defb(glyphs)
            + "font:\n"
            + defb(font)
            + "timer_text:\n"
            + defb(timer_text + bytes(1))
            + data
            + "test_result: DEFB 255\nscreen_seed: BINARY \"screen.bin\"\n",
            encoding="utf-8",
        )
        binary = harness.with_suffix(".bin")
        map_text = assemble_vector(required_tool("z80asm"), harness, [], binary)
        result, _, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, map_text, counter=4000000
        )
        assert result == 0 and pc == symbol_address(map_text, "test_done")
        ram = binary.with_suffix(".ram").read_bytes()
        assert ram[0x4000:0x5800] == pixels, "ikkle text pixels differ"
        assert ram[0x5800:0x5B00] == attrs, "ikkle text attrs differ"
        assert ram[symbol_address(map_text, "render_skip_clear")] == 0
        for i, (col, drawn) in enumerate(exits):
            hl_at = symbol_address(map_text, f"exit_hl_{i}")
            # dm_tab_loop resumes from HL and tmp_col after each label.
            assert int.from_bytes(ram[hl_at : hl_at + 2], "little") == (
                symbol_address(map_text, f"case_{i}") + drawn
            ), f"ikkle exit HL differs: case {i}"
            assert ram[symbol_address(map_text, f"exit_col_{i}")] == col, (
                f"ikkle exit tmp_col differs: case {i}"
            )
    print(
        f"Ikkle text ASM: timer + {len(cases)} cell-row, {len(lines)} 64-column, "
        f"{len(cells)} char/cursor cases match"
    )


def run_scaled_text_vector() -> None:
    """Compare every screen byte after the double-height banner title."""
    import random

    def ula(y: int) -> int:
        return ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)

    rng = random.Random(0x5CA1ED)
    screen = bytes(rng.randrange(256) for _ in range(6912))
    font = bytes(rng.randrange(256) for _ in range(256))
    expand = bytes(rng.randrange(256) for _ in range(16))
    text = b"SHATRANJ\x01\x7f\xff 0"
    pixels = bytearray(screen)
    # Six source rows per glyph, each doubled from pixel row 2 of cell row 0.
    for col, char in enumerate(text):
        for scan in range(6):
            value = expand[font[(char * 8 + scan) & 0xFF] >> 4]
            for y in (scan * 2 + 2, scan * 2 + 3):
                pixels[ula(y) + col] = value

    def defb(data: bytes) -> str:
        return " DEFB " + ",".join(str(value) for value in data) + "\n"

    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        (tmp / "screen.bin").write_bytes(screen)
        harness = tmp / "scaled_text.asm"
        harness.write_text(
            'SECTION code_user\nINCLUDE "ui_layout.inc"\n'
            "test_start:\n ld sp,0xff00\n ld hl,screen_seed\n ld de,0x4000\n"
            " ld bc,6912\n ldir\n ld hl,text\n ld bc,0\n call draw_scaled_text\n"
            " xor a\n ld (test_result),a\ntest_done:\n jp test_done\n"
            # Stub keeps the production clobbers: A, DE, HL; preserves BC.
            "font_scanline:\n add a,a\n add a,a\n add a,a\n ld e,a\n"
            " ld a,(tmp_scan)\n add a,e\n ld e,a\n ld d,0\n ld hl,font\n"
            " add hl,de\n ld a,(hl)\n ret\n"
            'INCLUDE "asm/ula/address.inc"\nINCLUDE "asm/ula/scaled_text.inc"\n'
            "tmp_scan: DEFB 0\npiece_row: DEFB 0\npiece_col: DEFB 0\n"
            "piece_char: DEFB 0\npiece_scan: DEFB 0\nscaled_row: DEFB 0\n"
            "scaled_col: DEFB 0\ntext_ptr: DEFW 0\nexpand_2x:\n"
            + defb(expand)
            + "font:\n"
            + defb(font)
            + "text:\n"
            + defb(text + bytes(1))
            + "test_result: DEFB 255\nscreen_seed: BINARY \"screen.bin\"\n",
            encoding="utf-8",
        )
        binary = harness.with_suffix(".bin")
        map_text = assemble_vector(required_tool("z80asm"), harness, [], binary)
        result, _, pc, _, _ = execute_vector(
            required_tool("z88dk-ticks"), binary, map_text, counter=4000000
        )
        assert result == 0 and pc == symbol_address(map_text, "test_done")
        ram = binary.with_suffix(".ram").read_bytes()
        assert ram[0x4000:0x5B00] == pixels, "scaled text pixels/attrs differ"
    print(f"Scaled text ASM: {len(text)} glyphs match")


def run_board_coords_vector() -> None:
    """Compare every screen byte after drawing both board orientations."""
    screen = read_asm(ROOT / "asm/spectrum/screen.asm")
    coords = screen[
        screen.index("draw_board_coords:") : screen.index("flip_board_row_col:")
    ]
    helpers = screen[
        screen.index("compute_screen_base:") : screen.index("compute_screen_base_de8:")
    ]
    helpers += screen[screen.index("compute_attr_base:") : screen.index("draw_badge:")]
    # Distinct scanlines expose missing/reversed rows and the file glyph spill bit.
    ranks = bytes(range(1, 41))
    files = bytes(range(41, 81))
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        for flipped in (0, 1):
            harness = tmp / f"coords_{flipped}.asm"
            harness.write_text(
                "SECTION code_user\n"
                "NETCHESSZX_GAME_BOARD_TOP_ROW EQU 5\n"
                "NETCHESSZX_GAME_BOARD_LEFT_COL EQU 1\n"
                "NETCHESSZX_MENU_ROW EQU 2\n"
                "NETCHESSZX_INFO_HEADER_ROW EQU 5\n"
                "NETCHESSZX_TOP_TURN_ROW EQU 3\n"
                "NETCHESSZX_FILEUI_SEPARATOR_ROW EQU 8\n"
                "NETCHESSZX_STATUS_ROW EQU 22\n"
                "test_start:\n ld sp,0xff00\n"
                " ld hl,0x4000\n ld de,0x4001\n ld bc,6911\n"
                " ld (hl),0x55\n ldir\n call draw_board_coords\n"
                " xor a\n ld (test_result),a\n test_done: jp test_done\n"
                "test_result: DEFB 255\n"
                f"_spectrum_gui_board_flipped: DEFB {flipped}\n"
                "board_light_line_attr: ld a,0x38\n ret\n"
                "board_col: DEFB 0\nboard_row: DEFB 0\ncurrent_attr: DEFB 0\n"
                "tmp_char: DEFB 0\ntmp_attr: DEFB 0\npiece_scan: DEFB 0\n"
                "sprite_ptr: DEFW 0\nrank_digit_patterns: DEFB "
                + ",".join(map(str, ranks))
                + "\nfile_letter_patterns: DEFB "
                + ",".join(map(str, files))
                + "\n"
                + coords
                + helpers,
                encoding="utf-8",
            )
            binary = harness.with_suffix(".bin")
            map_text = assemble_vector(assembler, harness, [], binary)
            result, _, pc, _, _ = execute_vector(
                emulator, binary, map_text, counter=1000000
            )
            assert result == 0 and pc == symbol_address(map_text, "test_done")
            expected = bytearray([0x55] * 6912)
            for cell in range(8):
                rank = cell if flipped else 7 - cell
                file = 7 - cell if flipped else cell
                expected[6144 + (5 + 2 * cell) * 32] = 0x38
                expected[6144 + (6 + 2 * cell) * 32] = 0x38
                expected[6144 + 4 * 32 + 1 + 2 * cell] = 0x38
                for row in range(5):
                    y = 45 + 16 * cell + row
                    offset = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
                    expected[offset] = ranks[rank * 5 + row]
                    bits = files[file * 5 + row]
                    rotated = ((bits >> 4) | (bits << 4)) & 255
                    offset = ((row + 1) << 8) + 128 + 1 + 2 * cell
                    expected[offset] = rotated >> 1
                    if rotated & 1:
                        expected[offset + 1] |= 0x80
            ram = binary.with_suffix(".ram").read_bytes()
            assert ram[0x4000:0x5B00] == expected, (
                f"coordinate pixels/attrs differ: flipped={flipped}"
            )


def run_esxdos_handle_vectors() -> None:
    """Exercise real wrappers with raw handle zero, another handle and errors."""
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    source = ROOT / "asm/esxdos/esx_fileio_spectalk.asm"
    file_calls = (
        ("_esx_fopen", 0x9A),
        ("_esx_fcreate", 0x9A),
        ("_esx_fcreate_new", 0x9A),
        ("_esx_fread", 0x9D),
        ("_esx_fwrite", 0x9E),
        ("_esx_fclose", 0x9B),
    )
    dir_calls = (("_esx_opendir", 0xA3), ("_esx_readdir", 0xA4), ("_esx_fclose", 0x9B))
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        for next_target in (False, True):
            for directory, calls in ((False, file_calls), (True, dir_calls)):
                cases = [
                    (fn, hook, handle, failed)
                    for fn, hook in calls
                    for handle, failed in ((0, False), (7, False), (0, True))
                ]
                lines = [
                    "SECTION code_user",
                    "EXTERN _esx_handle, _esx_buf, _esx_count, _esx_result",
                    "EXTERN " + ",".join(fn for fn, _ in calls),
                    "test_start: ld sp,0xff00",
        " ld a,0xa5\n ld (0x672a),a\n ld (0x67cb),a",
                    " ld a,0xc3\n ld (8),a\n ld hl,test_rom\n ld (9),hl",
                ]
                data = [
                    "test_result: DEFB 255",
                    "rom_a: DEFB 0",
                    "rom_hook: DEFB 0",
                    "rom_result: DEFB 0",
                    "rom_fail: DEFB 0",
                ]
                for i, (_, hook, handle, failed) in enumerate(cases):
                    result = 5 if failed else (handle if hook in (0x9A, 0xA3) else 1)
                    lines.extend(
                        (
                            f" ld a,{handle + 1}\n ld (_esx_handle),a",
                            f" ld a,{result}\n ld (rom_result),a",
                            f" ld a,{int(failed)}\n ld (rom_fail),a",
                            " ld hl,0x4000\n ld (_esx_buf),hl",
                            " ld hl,73\n ld (_esx_count),hl",
                            " ld ix,0x1234\n ld iy,0x5c3a\n ld hl,0x4567",
                            f" call {cases[i][0]}",
                            f" ld (case_{i}_hl),hl\n ld (case_{i}_sp),sp",
                            f" ld (case_{i}_ix),ix\n ld (case_{i}_iy),iy",
                            f" ld a,(_esx_handle)\n ld (case_{i}_handle),a",
                            f" ld hl,(_esx_result)\n ld (case_{i}_count),hl",
                            f" ld a,(rom_a)\n ld (case_{i}_a),a",
                            f" ld a,(rom_hook)\n ld (case_{i}_hook),a",
                        )
                    )
                    data.extend(
                        f"case_{i}_{field}: DEFW 0"
                        for field in (
                            "hl",
                            "sp",
                            "ix",
                            "iy",
                            "handle",
                            "count",
                            "a",
                            "hook",
                        )
                    )
                lines.extend(
                    (
                        " xor a\n ld (test_result),a\ntest_done: jp test_done",
                        # RST return points to the inline hook byte; skip that byte.
                        "test_rom: ld (rom_a),a\n ex (sp),hl\n ld a,(hl)",
                        " ld (rom_hook),a\n inc hl\n ex (sp),hl",
                        " ld ix,0xbeef\n ld iy,0xdead\n ld bc,73",
                        " ld a,(rom_fail)\n or a\n ld a,(rom_result)\n ret z\n scf\n ret",
                        *data,
                    )
                )
                name = f"esxdos_handle_{int(next_target)}_{int(directory)}"
                harness = tmp / f"{name}.asm"
                harness.write_text("\n".join(lines) + "\n", encoding="utf-8")
                binary = harness.with_suffix(".bin")
                defines = ["NETCHESSZX_NEXT_BANKING"] if next_target else []
                if directory:
                    defines.append("ESX_FILEUI")
                map_text = assemble_vector(
                    assembler, harness, [source], binary, defines
                )
                result, _, pc, _, _ = execute_vector(
                    emulator,
                    binary,
                    map_text,
                    counter=100000,
                    cpu="z80n" if next_target else "z80",
                )
                assert result == 0 and pc == symbol_address(map_text, "test_done")
                ram = binary.with_suffix(".ram").read_bytes()
                for i, (fn, hook, handle, failed) in enumerate(cases):

                    def value(field: str) -> int:
                        addr = symbol_address(map_text, f"case_{i}_{field}")
                        return int.from_bytes(ram[addr : addr + 2], "little")

                    label = f"{name} {fn} raw={handle} failed={failed}"
                    assert (value("sp"), value("ix"), value("iy")) == (
                        0xFF00,
                        0x1234,
                        0x5C3A,
                    ), label
                    assert value("hook") == hook, label
                    if hook in (0x9A, 0xA3):
                        assert value("handle") == (0 if failed else handle + 1), label
                        if hook == 0x9A:
                            assert value("hl") & 255 == (5 if failed else 0), label
                    else:
                        assert value("a") == handle, label
                        if hook == 0x9B:
                            assert value("hl") & 255 == (255 if failed else 0), label
                        else:
                            count = 1 if hook == 0xA4 else 73
                            assert value("count") == (0 if failed else count), label


def run_next_rtc_vector() -> None:
    """Compile the production RTC wrapper/validator and exercise both ROM APIs."""
    compiler = required_tool("zcc")
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    # timezone, firmware (DRVAPI / GETDATE / both fail), FAT date, FAT time.
    cases = [
        (zone, firmware, date, 0x6000)
        for zone in (2, 127)
        for firmware, date in ((0, 0x5D27), (1, 0x5D27), (2, 0x5D27), (0, 0))
    ]
    cases += [
        (2, 0, 0x5D27, 0),
        (127, 0, 0x5D27, 0),
        (127, 0, 0x5D27, 0xFD),
        (127, 0, 0x5D27, 0x100),
    ]
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        generated = tmp / "mqtt_tx.asm"
        run = subprocess.run(
            [
                compiler,
                "+z80",
                "-vn",
                "-clib=sdcc_iy",
                "-SO3",
                "-compiler=sdcc",
                "-Cs--no-reg-params",
                "--opt-code-size",
                "--fomit-frame-pointer",
                "-DNETCHESSZX_SDCC_IY",
                "-DNETCHESSZX_NEXT",
                "-DNETCHESSZX_NEXT_BANKING",
                "-DNETCHESSZX_FIXED_LOW_RAM",
                f"-I{ROOT / 'src'}",
                f"-custom-copt-rules={ROOT / 'tools/netchesszx_overlay_copt'}",
                "-S",
                str(ROOT / "src/spectrum/overlay/mqtt_tx_ovl.c"),
                "-o",
                str(generated),
            ],
            cwd=tmp,
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
        )
        assert run.returncode == 0, run.stdout + run.stderr
        asm = generated.read_text(encoding="utf-8")
        functions = []
        for name in ("next_rtc_sync_time", "msdos_time_valid"):
            label = f"_{name}:\n"
            assert label in asm, f"SDCC did not emit {name}"
            functions.append(
                label
                + asm.split(label, 1)[1].split(
                    "\n;\t---------------------------------", 1
                )[0]
            )
        lines = [
            "SECTION code_user",
            "EXTERN ___sdcc_enter_ix, ____sdcc_4_push_hlix",
            "test_start: ld sp,0xff00",
        " ld a,0xa5\n ld (0x672a),a\n ld (0x67cb),a",
            " ld a,0xc3\n ld (8),a\n ld hl,rtc_rom\n ld (9),hl",
        ]
        for i, (zone, firmware, date, time) in enumerate(cases):
            lines.extend(
                (
                    " xor a\n ld (observed),a\n ld hl,observed",
                    " ld de,observed+1\n ld bc,15\n ldir",
                    f" ld a,{zone}\n ld (_netchesszx_timezone),a",
                    f" ld a,{firmware}\n ld (rtc_mode),a",
                    f" ld hl,{date}\n ld (rtc_date),hl",
                    f" ld hl,{time}\n ld (rtc_time),hl",
                    " ld ix,0x1234\n ld iy,0x5c3a",
                    # L also supplies the old fastcall argument for the failing version.
                    f" ld l,{int(zone == 127)}\n call _next_rtc_sync_time",
                    " ld a,l\n ld (observed+2),a\n ld (observed+10),sp",
                    " ld (observed+12),ix\n ld (observed+14),iy",
                    f" ld hl,observed\n ld de,records+{16 * i}\n ld bc,16\n ldir",
                )
            )
        lines.extend(
            (
                " xor a\n ld (test_result),a\ntest_done: jp test_done",
                # Execute the real RST and NEXTREG instructions in flat Z80N RAM.
                # Only the firmware at RST 8 is replaced; MMU placement has static guards.
                "rtc_rom: ex (sp),hl\n ld a,(hl)\n inc hl\n ex (sp),hl",
                " ld ix,0xabcd\n ld iy,0xdcba\n cp 0x92\n jr nz,rtc_getdate",
                " ld a,(rtc_mode)\n or a\n jr z,rtc_success\n scf\n ret",
                "rtc_getdate: cp 0x8e\n jr nz,rtc_failed",
                " ld a,(rtc_mode)\n cp 1\n jr z,rtc_success",
                "rtc_failed: scf\n ret",
                "rtc_success: ld bc,(rtc_date)\n ld de,(rtc_time)\n or a\n ret",
                "_reset_line_buf: ld hl,_line_buf\n ld b,6\n xor a",
                "rtc_clear: ld (hl),a\n inc hl\n djnz rtc_clear\n ret",
                "_spectrum_net_runtime_set_clock: ld hl,observed\n inc (hl)",
                " ld hl,2\n add hl,sp\n ld de,observed+3\n ld bc,3\n ldir\n ret",
                "_spectrum_net_runtime_set_fat_stamp: ld hl,observed+1\n inc (hl)",
                " ld hl,2\n add hl,sp\n ld de,observed+6\n ld bc,4\n ldir\n ret",
                *functions,
                "test_result: DEFB 255\n_line_buf: DEFS 6\n_netchesszx_timezone: DEFB 0",
                "rtc_mode: DEFB 0\nrtc_date: DEFW 0\nrtc_time: DEFW 0",
                f"observed: DEFS 16\nrecords: DEFS {16 * len(cases)}",
            )
        )
        harness = tmp / "next_rtc_vector.asm"
        harness.write_text("\n".join(lines) + "\n", encoding="utf-8")
        # Use the compiler helpers, including their real IX frame ABI.
        binary = harness.with_suffix(".bin")
        map_text = assemble_vector(
            assembler,
            harness,
            sdcc_ix_helpers(assembler),
            binary,
            ["__SDCC_IX"],
        )
        result, _, pc, _, _ = execute_vector(
            emulator, binary, map_text, counter=1000000, cpu="z80n"
        )
        assert result == 0 and pc == symbol_address(map_text, "test_done")
        ram = binary.with_suffix(".ram").read_bytes()
        records = symbol_address(map_text, "records")
        failures = []
        for i, (zone, firmware, date, time) in enumerate(cases):
            valid = int(firmware != 2 and date != 0)
            apply = int(valid and zone == 127)
            clock = bytes((time >> 11, (time >> 5) & 63, (time & 31) * 2))
            stamp = date.to_bytes(2, "little") + time.to_bytes(2, "little")
            expected = (
                bytes((apply, apply, valid))
                + (clock + stamp if apply else bytes(7))
                + bytes.fromhex("00 ff 34 12 3a 5c")
            )
            observed = ram[records + 16 * i : records + 16 * (i + 1)]
            if observed != expected:
                failures.append(
                    f"case {i} {cases[i]}: {observed.hex()} != {expected.hex()}"
                )
        assert not failures, "\n".join(failures)
    print(f"Next RTC SDCC/ASM vectors passed ({len(cases)} cases)")


def run_setup_state_vector() -> None:
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / "setup_state.bin"
        symbols = assemble_vector(
            required_tool("z80asm"),
            ROOT / "tests/spectrum/setup_state_vector.asm",
            [ROOT / "asm/spectrum/setup_state.inc"],
            binary,
        )
        result, *_ = execute_vector(required_tool("z88dk-ticks"), binary, symbols)
        assert result == 0
        ram = binary.with_suffix(".ram").read_bytes()
        base = symbol_address(symbols, "_app_workspace")
        expected = bytearray([0x5A] * 55)
        for offset, value in (
            (5, 2),
            (12, 2),
            (16, 0x5B),
            (17, 0x34),
            (18, 0x12),
            (19, 9),
            (33, 1),  # a different SET leaves the config unsaved
            (36, 0),
        ):
            expected[offset] = value
        assert ram[base : base + 55] == expected
        for name, expected_bytes in (
            ("saved_sp", b"\x00\xff"),
            ("saved_iy", b"\x3a\x5c"),
        ):
            address = symbol_address(symbols, name)
            assert ram[address : address + 2] == expected_bytes
        assert symbol_address(symbols, "_spectrum_setup_view") == base
        for line in symbols.splitlines():
            if re.match(r"_setup_\w+\s*=", line):
                assert ", local," in line, line
    print("Setup shared-state/private-alias ASM vector passed")


JOURNAL_FS_ASM = r"""
; Fake esxDOS: files TMP, NEW, STJ (digit name = SAVE target) and LOAD (any
; other .STJ), four handles. Strict access bits, create-failure mask and a
; write byte budget.
FS_BASE EQU 0xC000
FS_FILE_SIZE EQU 0x220
FS_HANDLES EQU 0xC900
FS_NH EQU 4

test_rom:
    ld (rom_a), a
    ex (sp), hl
    ld a, (hl)
    inc hl
    ex (sp), hl
    cp 0x9A
    jp z, fs_open
    cp 0x9B
    jp z, fs_close
    cp 0x9C
    jp z, fs_sync
    cp 0x9D
    jp z, fs_read
    cp 0x9E
    jp z, fs_write
    cp 0x9F
    jp z, fs_seek
    cp 0xAD
    jp z, fs_unlink
fs_err:
    ld a, 5
    scf
    ret

fs_base:
    ld hl, FS_BASE
    ld de, FS_FILE_SIZE
    inc b
fs_base_lp:
    dec b
    ret z
    add hl, de
    jr fs_base_lp

fs_path_file:
    push ix
    pop hl
    ld c, 0
fs_pf_lp:
    ld a, (hl)
    inc hl
    or a
    jr z, fs_pf_bad
    cp '/'
    jr nz, fs_pf_dot
    ld c, (hl)
    jr fs_pf_lp
fs_pf_dot:
    cp '.'
    jr nz, fs_pf_lp
    ld a, (hl)
    ld b, 0
    cp 'T'
    jr z, fs_pf_ok
    inc b
    cp 'N'
    jr z, fs_pf_ok
    inc b
    cp 'S'
    jr nz, fs_pf_bad
    ld a, c
    cp '0'
    jr c, fs_pf_load
    cp '9' + 1
    jr c, fs_pf_ok
fs_pf_load:
    inc b
    jr fs_pf_ok
fs_pf_bad:
    scf
    ret
fs_pf_ok:
    push bc
    call fs_base
    pop bc
    or a
    ret

fs_open:
    ld a, b
    ld (fs_mode), a
    push ix
    pop hl
    ld de, fs_last_open
    ld bc, 32
    ldir
    call fs_path_file
    jp c, fs_err
    push hl
    ld a, 1
    ld c, b
    inc c
fs_mask_lp:
    dec c
    jr z, fs_mask_ok
    add a, a
    jr fs_mask_lp
fs_mask_ok:
    ld c, a
    ld a, (fs_create_fail)
    and c
    pop hl
    jr z, fs_open_mode
    ld a, (fs_mode)
    and 0x0c
    jp nz, fs_err
fs_open_mode:
    ld a, (fs_mode)
    and 0x0c
    jr z, fs_open_ex
    cp 0x04
    jr z, fs_create_new
    cp 0x0c
    jr z, fs_create_al
    ld a, (hl)
    or a
    jr nz, fs_open_ok
    jr fs_create_al
fs_open_ex:
    ld a, (hl)
    or a
    jp z, fs_err
    jr fs_open_ok
fs_create_new:
    ld a, (hl)
    or a
    jp nz, fs_err
fs_create_al:
    ld (hl), 1
    inc hl
    ld (hl), 0
    inc hl
    ld (hl), 0
fs_open_ok:
    ld hl, FS_HANDLES
    ld c, 0
fs_alloc:
    ld a, (hl)
    or a
    jr z, fs_alloc_ok
    inc hl
    inc hl
    inc hl
    inc hl
    inc c
    ld a, c
    cp FS_NH
    jr nz, fs_alloc
    jp fs_err
fs_alloc_ok:
    ld a, b
    inc a
    ld (hl), a
    inc hl
    ld a, (fs_mode)
    ld (hl), a
    inc hl
    ld (hl), 0
    inc hl
    ld (hl), 0
    ld a, c
    or a
    ret

fs_handle:
    cp FS_NH
    jr nc, fs_hbad
    add a, a
    add a, a
    ld e, a
    ld d, 0
    ld hl, FS_HANDLES
    add hl, de
    ld a, (hl)
    or a
    ret nz
fs_hbad:
    scf
    ret

fs_load:
    ld (fs_hent), hl
    ld a, (hl)
    dec a
    ld b, a
    call fs_base
    ld (fs_file), hl
    ld hl, (fs_hent)
    inc hl
    ld a, (hl)
    ld (fs_hmode), a
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld (fs_pos), de
    ret

fs_store_pos:
    ld hl, (fs_hent)
    inc hl
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    ret

fs_close:
    ld a, (rom_a)
    call fs_handle
    jp c, fs_err
    ld a, (hl)
    ld c, a
    inc hl
    bit 1, (hl)
    dec hl
    jr z, fs_close_clear
    ld a, (fs_corrupt_close)
    cp c
    jr nz, fs_close_clear
    xor a
    ld (fs_corrupt_close), a
    push hl
    ld a, c
    dec a
    ld b, a
    call fs_base
    ld de, 16
    add hl, de
    inc (hl)
    pop hl
fs_close_clear:
    ld (hl), 0
    ld a, (fs_close_fail)
    cp c
    jr nz, fs_close_ok
    xor a
    ld (fs_close_fail), a
    jp fs_err
fs_close_ok:
    xor a
    ret

fs_sync:
    ld a, (rom_a)
    call fs_handle
    jp c, fs_err
    ld a, (fs_sync_fail)
    or a
    jr z, fs_sync_ok
    xor a
    ld (fs_sync_fail), a
    jp fs_err
fs_sync_ok:
    xor a
    ret

fs_read:
    ld hl, (fs_read_calls)
    inc hl
    ld (fs_read_calls), hl
    ld de, (fs_read_fail_call)
    push hl
    or a
    sbc hl, de
    pop hl
    jr nz, fs_read_call_ok
    ld de, 0xffff
    ld (fs_read_fail_call), de
    jp fs_err
fs_read_call_ok:
    ld (fs_count), bc
    ld (fs_buf), ix
    ld a, (rom_a)
    call fs_handle
    jp c, fs_err
    call fs_load
    ld a, (fs_hmode)
    and 1
    jp z, fs_err
    ld hl, (fs_read_fail_pos)
    inc hl
    ld a, h
    or l
    jr z, fs_read_allowed
    dec hl
    ld de, (fs_pos)
    or a
    sbc hl, de
    jr nz, fs_read_allowed
    ld a, (fs_read_fail_repeat)
    or a
    jr nz, fs_read_fail
    ld hl, 0xffff
    ld (fs_read_fail_pos), hl
fs_read_fail:
    jp fs_err
fs_read_allowed:
    ld hl, (fs_file)
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld hl, (fs_pos)
    ex de, hl
    or a
    sbc hl, de
    jr nc, fs_r_avail
    ld hl, 0
fs_r_avail:
    ld bc, (fs_read_limit)
    push hl
    or a
    sbc hl, bc
    pop hl
    jr c, fs_r_limit_ok
    ld h, b
    ld l, c
fs_r_limit_ok:
    ld bc, (fs_count)
    push hl
    or a
    sbc hl, bc
    pop hl
    jr c, fs_r_n
    ld h, b
    ld l, c
fs_r_n:
    ld (fs_n), hl
    ld hl, (fs_file)
    ld de, 16
    add hl, de
    ld de, (fs_pos)
    add hl, de
    ld de, (fs_buf)
    ld bc, (fs_n)
    ld a, b
    or c
    jr z, fs_r_done
    ldir
fs_r_done:
    ld hl, (fs_pos)
    ld bc, (fs_n)
    add hl, bc
    ex de, hl
    call fs_store_pos
    ld bc, (fs_n)
    xor a
    ret

fs_write:
    ld hl, (fs_write_calls)
    inc hl
    ld (fs_write_calls), hl
    ld (fs_count), bc
    ld (fs_buf), ix
    ld a, (rom_a)
    call fs_handle
    jp c, fs_err
    call fs_load
    ld a, (fs_hmode)
    and 2
    jp z, fs_err
    ld hl, (fs_budget)
    ld bc, (fs_count)
    ld a, h
    and l
    inc a
    jr z, fs_w_n
    ld a, h
    or l
    jp z, fs_err
    push hl
    or a
    sbc hl, bc
    pop hl
    jr nc, fs_w_budget_ok
    ld b, h
    ld c, l
    ld hl, 0
    ld (fs_budget), hl
    jr fs_w_n
fs_w_budget_ok:
    or a
    sbc hl, bc
    ld (fs_budget), hl
fs_w_n:
    ld (fs_n), bc
    ld hl, (fs_file)
    ld de, 16
    add hl, de
    ld de, (fs_pos)
    add hl, de
    ex de, hl
    ld hl, (fs_buf)
    ld a, b
    or c
    jr z, fs_w_done
    ldir
fs_w_done:
    ld hl, (fs_pos)
    ld bc, (fs_n)
    add hl, bc
    ex de, hl
    call fs_store_pos
    ld hl, (fs_file)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    push hl
    ld h, d
    ld l, e
    or a
    sbc hl, bc
    pop hl
    jr c, fs_w_len_ok
    ld (hl), d
    dec hl
    ld (hl), e
fs_w_len_ok:
    ld bc, (fs_n)
    xor a
    ret

fs_seek:
    ld (fs_seek_pos), de
    ld a, l
    ld hl, fs_seek_mode
    or (hl)
    ld (hl), a
    ld a, ixl
    or (hl)
    ld (hl), a
    push de
    ld a, (rom_a)
    call fs_handle
    pop de
    jp c, fs_err
    ld (fs_hent), hl
    call fs_store_pos
    xor a
    ret

fs_unlink:
    call fs_path_file
    jp c, fs_err
    ld a, (hl)
    or a
    jp z, fs_err
    ld (hl), 0
    inc hl
    ld (hl), 0
    inc hl
    ld (hl), 0
    xor a
    ret

_spectrum_net_background_drain:
    ld hl, (fs_drain_calls)
    inc hl
    ld (fs_drain_calls), hl
    ld bc, 0xbcbc
    ld de, 0xdede
    ld hl, 0xabab
    ret
_spectrum_net_runtime_fat_date:
    ld hl, (t_fat_date)
    ret
_spectrum_net_runtime_fat_time:
    ld hl, (t_fat_time)
    ret
_spectrum_net_runtime_fat_day_delta:
    ld hl, (t_day_delta)
    ret
"""

JOURNAL_FS_DATA = """
rom_a: DEFB 0
fs_mode: DEFB 0
fs_hmode: DEFB 0
fs_seek_mode: DEFB 0
fs_seek_pos: DEFW 0
fs_create_fail: DEFB 0
fs_corrupt_close: DEFB 0
fs_close_fail: DEFB 0
fs_read_fail_pos: DEFW 0xFFFF
fs_read_fail_call: DEFW 0xFFFF
fs_read_fail_repeat: DEFB 0
fs_read_limit: DEFW 0xFFFF
fs_sync_fail: DEFB 0
fs_budget: DEFW 0xFFFF
fs_read_calls: DEFW 0
fs_write_calls: DEFW 0
fs_drain_calls: DEFW 0
fs_hent: DEFW 0
fs_file: DEFW 0
fs_pos: DEFW 0
fs_count: DEFW 0
fs_buf: DEFW 0
fs_n: DEFW 0
fs_last_open: DEFS 32
t_fat_date: DEFW 0
t_fat_time: DEFW 0
t_day_delta: DEFW 0
_spectrum_journal_fd: DEFB 0
_spectrum_journal_pend: DEFB 0
_spectrum_journal_warning: DEFB 0
_move_line_count: DEFB 0
_last_ply_seen: DEFW 0
snapshot: DEFS 60
load_buf: DEFS 60
"""

JOURNAL_FILES = ("TMP", "NEW", "STJ", "LOAD")

# The append entry captures statistics before I/O on every target. Supply
# resident data/time only; link the production capture routine below.
GAME_STATS_FIXTURE = """
PUBLIC _spectrum_game_stats, _spectrum_stats_undo
PUBLIC _netchesszx_session_state, _spectrum_time_snapshot
_spectrum_game_stats EQU 0x65f5
_spectrum_stats_undo: DEFS 5
_netchesszx_session_state: DEFW 0
_spectrum_time_snapshot:
    ld hl, test_stats_time
    ret
test_stats_time: DEFS 11
"""


def journal_crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def journal_square(text: str) -> int:
    return (ord("8") - ord(text[1])) * 8 + ord(text[0]) - ord("a")


def journal_record(move: str | None) -> int:
    if move is None:
        return 0x8000
    promo = {"q": 1, "r": 2, "b": 3, "n": 4}.get(move[4:5].lower(), 0)
    return journal_square(move[:2]) | journal_square(move[2:4]) << 6 | promo << 12


def journal_body(moves: list[str | None]) -> bytes:
    return b"".join(journal_record(m).to_bytes(2, "little") for m in moves)


JOURNAL_SNAPSHOT = bytes(range(0x41, 0x41 + 60))
JOURNAL_LOADED = bytes(range(0x61, 0x61 + 26)) + bytes(range(0x30, 0x30 + 34))


def journal_stj(
    snapshot: bytes,
    body: bytes,
    complete: bool = True,
    body_crc: int | None = None,
    record_count: int | None = None,
) -> bytes:
    header = bytearray(b"STJ1")
    header += bytes((1, 1 if complete else 0, 76, 0))
    count = len(body) // 2 if record_count is None else record_count
    header += count.to_bytes(2, "little")
    header += (journal_crc16(body) if body_crc is None else body_crc).to_bytes(
        2, "little"
    )
    header += bytes(4)
    header += snapshot
    header[14:16] = journal_crc16(bytes(header)).to_bytes(2, "little")
    return bytes(header) + body


def journal_asm_bytes(data: bytes) -> str:
    if not data:
        return ""
    return "\n".join(
        " DEFB " + ",".join(str(b) for b in data[i : i + 16])
        for i in range(0, len(data), 16)
    )


def run_journal_scenario(
    tmp: Path,
    name: str,
    steps: list[tuple],
    files: dict[str, bytes] | None = None,
    fd: int = 0,
    pend: int = 0,
    resident_hook: bool = False,
    recovery: bool = False,
    next_target: bool = False,
) -> dict:
    """Run product JOURNAL LOAD/SAVE and SAVELOAD append against fake esxDOS.

    next_target assembles the NEX build (esxDOS wrappers bracket RST 8 with
    NEXTREG MMU1 switches; the file buffer is SAVELOAD's ja_io_buf).
    """
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    files = files or {}
    externs = (
        "_journal_load_ovl_entry,_journal_save_ovl_entry,"
        "_saveload_journal_append_ovl,_esx_fseek_set,_esx_handle"
    )
    publics = (
        "_spectrum_net_background_drain,_spectrum_net_runtime_fat_date,"
        "_spectrum_net_runtime_fat_time,_spectrum_net_runtime_fat_day_delta,"
        "_spectrum_journal_fd,_spectrum_journal_pend,_spectrum_journal_warning,"
        "_move_line_count,_last_ply_seen,"
        "_spectrum_gui_side_panels_visible,_spectrum_render_moves"
    )
    if resident_hook:
        externs += (",_spectrum_journal_after_move,_spectrum_journal_notify,"
                    "_spectrum_journal_fd,_spectrum_journal_pend,"
                    "_spectrum_journal_warning")
        publics = publics.replace(
            "_spectrum_journal_fd,_spectrum_journal_pend,_spectrum_journal_warning,", ""
        )
        publics += ",_spectrum_overlay_exec,_notify_error"
    lines = [
        "SECTION code_user",
        f"EXTERN {externs}",
        f"PUBLIC {publics}",
        "test_start: ld sp,0xff00",
        " ld a,0xa5\n ld (0x672a),a\n ld (0x67cb),a",
        " ld iy,0x5c3a",
        " ld a,0xc3\n ld (8),a\n ld hl,test_rom\n ld (9),hl",
        " ld hl,0xc000\n ld de,0xc001\n ld bc,0x09ff\n ld (hl),0\n ldir",
        f" ld a,{fd}\n ld (_spectrum_journal_fd),a",
        f" ld a,{pend}\n ld (_spectrum_journal_pend),a",
    ]
    data = [
        "test_result: DEFB 255",
        "test_sp: DEFW 0",
        JOURNAL_FS_DATA,
        GAME_STATS_FIXTURE,
        "snapshot_init: " + journal_asm_bytes(JOURNAL_SNAPSHOT).lstrip(),
    ]
    lines.append(" ld hl,snapshot_init\n ld de,snapshot\n ld bc,60\n ldir")
    lines.append(" ld hl,load_buf\n ld de,load_buf+1\n ld bc,59\n ld (hl),0xEE\n ldir")
    for index, ext in enumerate(JOURNAL_FILES):
        content = files.get(ext)
        if content is None:
            continue
        base = 0xC000 + index * 0x220
        lines.append(f" ld a,1\n ld ({base}),a")
        lines.append(f" ld hl,{len(content)}\n ld ({base + 1}),hl")
        if content:
            data.append(f"file_{ext}: " + journal_asm_bytes(content).lstrip())
            lines.append(
                f" ld hl,file_{ext}\n ld de,{base + 16}\n ld bc,{len(content)}\n ldir"
            )
    for i, step in enumerate(steps):
        kind = step[0]
        if kind in ("move", "undo"):
            if kind == "move":
                data.append(f'text_{i}: DEFM "{step[1]}"\n DEFB 0')
                lines.append(f" ld hl,text_{i}")
            else:
                lines.append(" ld hl,0")
            lines.append(" ld (0x5fe0),hl\n ld hl,0x5fe0\n ld d,h\n ld e,l")
            lines.append(" ld hl,(0x5fe0)\n call _spectrum_journal_after_move"
                         if resident_hook else " call _saveload_journal_append_ovl")
        elif kind in ("save", "load"):
            data.append(f'text_{i}: DEFM "{step[1]}"\n DEFB 0')
            buf = "snapshot" if kind == "save" else "load_buf"
            entry = (
                "_journal_save_ovl_entry"
                if kind == "save"
                else "_journal_load_ovl_entry"
            )
            lines.append(
                f" ld hl,text_{i}\n ld (0x5fe0),hl\n ld hl,{buf}\n"
                " ld (0x5fe2),hl\n ld a,0xff\n ld (0x5fe4),a\n"
                f" ld hl,0x5fe0\n ld d,h\n ld e,l\n call {entry}"
            )
        elif kind == "poke":
            lines.append(f" ld a,{step[2]}\n ld ({step[1]}),a")
        elif kind == "poke16":
            lines.append(f" ld hl,{step[2]}\n ld ({step[1]}),hl")
        elif kind == "seek":
            lines.append(
                " ld a,(_spectrum_journal_fd)\n and 0x3f\n"
                f" ld (_esx_handle),a\n ld hl,{step[1]}\n call _esx_fseek_set"
            )
        elif kind == "arm":
            lines.append(" ld hl,_spectrum_journal_pend\n set 7,(hl)")
            if resident_hook:
                lines.append(" xor a\n ld (_spectrum_journal_warning),a")
        elif kind == "notify" and resident_hook:
            lines.append(" call _spectrum_journal_notify")
        elif kind == "seedlog":
            lines.append(
                " ld hl,0x5cb6\n ld de,0x5cb7\n ld bc,223\n ld (hl),0xEE\n ldir"
            )
        elif kind == "replay":
            lines.append(
                f" ld hl,{step[1] if len(step) > 1 else 1}\n"
                " ld (0x5fe0),hl\n call " +
                ("_spectrum_journal_after_move" if resident_hook else
                 "_saveload_journal_append_ovl")
            )
        else:
            raise AssertionError(kind)
        if resident_hook:
            data.append(f"warning_{i}: DEFB 0\nnotices_{i}: DEFB 0")
            lines.append(
                f" ld a,(_spectrum_journal_warning)\n ld (warning_{i}),a\n"
                f" ld a,(test_notices)\n ld (notices_{i}),a"
            )
        if kind in ("move", "undo", "save", "load", "replay"):
            data.append(
                f"res_{i}: DEFW 0\nctx_{i}: DEFB 0\nfd_{i}: DEFB 0\npend_{i}: DEFB 0\n"
                f"stats_{i}: DEFS 33\ncapture_{i}: DEFB 0"
            )
            lines.append(
                f" ld (res_{i}),hl\n ld a,(0x5fe4)\n ld (ctx_{i}),a\n"
                f" ld a,(_spectrum_journal_fd)\n ld (fd_{i}),a\n"
                f" ld a,(_spectrum_journal_pend)\n ld (pend_{i}),a\n"
                f" ld a,(0x5fe7)\n ld (capture_{i}),a\n"
                f" ld hl,_spectrum_game_stats\n ld de,stats_{i}\n ld bc,33\n ldir"
            )
    lines.append(
        " ld (test_sp),sp\n xor a\n ld (test_result),a\n jp test_done\n"
        "_spectrum_gui_side_panels_visible: ld l,0\n ret\n"
        "_spectrum_render_moves: ret\n"
        "test_done: jp test_done"
    )
    lines.append(JOURNAL_FS_ASM)
    harness = tmp / f"journal_{name}.asm"
    harness.write_text("\n".join(lines + data) + "\n", encoding="utf-8")
    binary = harness.with_suffix(".bin")
    sources = [
        ROOT / "asm/overlay/journal/entry_journal.asm",
        ROOT / "asm/overlay/journal/stj_codec.asm",
        ROOT / "asm/overlay/saveload/journal_append.asm",
        ROOT / "asm/overlay/saveload/game_stats.asm",
        ROOT / "asm/esxdos/esx_journal.asm",
        ROOT / "asm/esxdos/esx_fsync.asm",
        ROOT / "asm/esxdos/esx_fseek_set.asm",
    ]
    if resident_hook:
        sources.append(ROOT / "asm/spectrum/journal.asm")
        source = harness.read_text().replace(
            "_spectrum_journal_fd: DEFB 0\n_spectrum_journal_pend: DEFB 0\n"
            "_spectrum_journal_warning: DEFB 0\n", ""
        )
        harness.write_text(source + """
_spectrum_overlay_exec:
    pop bc
    pop hl
    push bc
    ld a,(test_overlay_fail)
    or a
    jp z,_saveload_journal_append_ovl
    ld hl,0
    ret
_notify_error:
    ld hl,test_notices
    inc (hl)
    ld hl,2
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    ld a,1
    ld (test_notice_error),a
    ex de,hl
    ld de,test_notice
test_notice_copy:
    ld a,(hl)
    ld (de),a
    inc hl
    inc de
    or a
    jr nz,test_notice_copy
    ret
test_overlay_fail: DEFB 0
test_notices: DEFB 0
test_notice_error: DEFB 0
test_notice: DEFS 64
""", encoding="utf-8")
    if recovery and not resident_hook:
        harness.write_text(
            harness.read_text()
            + "PUBLIC _spectrum_journal_count,_spectrum_journal_crc\n"
            "_spectrum_journal_count: DEFW 0\n_spectrum_journal_crc: DEFW 0xFFFF\n",
            encoding="utf-8",
        )
    map_text = assemble_vector(
        assembler, harness, sources, binary,
        defines=[
            "ATLAS_JOURNAL",
            *(("NETCHESSZX_RECOVERY",) if recovery else ()),
            *(("NETCHESSZX_NEXT", "NETCHESSZX_NEXT_BANKING") if next_target else ()),
        ],
    )
    result, out, pc, _, _ = execute_vector(
        emulator, binary, map_text, counter=40_000_000,
        cpu="z80n" if next_target else "z80",
    )
    assert result == 0 and pc == symbol_address(map_text, "test_done"), (name, out)
    ram = binary.with_suffix(".ram").read_bytes()

    def sym(label: str, size: int = 1) -> int:
        addr = symbol_address(map_text, label)
        return int.from_bytes(ram[addr : addr + size], "little")

    state = {
        "fd": sym("_spectrum_journal_fd"),
        "pend": sym("_spectrum_journal_pend"),
        "seek_mode": sym("fs_seek_mode"),
        "seek_pos": sym("fs_seek_pos", 2),
        "handles": sum(1 for h in range(4) if ram[0xC900 + h * 4]),
        "reads": sym("fs_read_calls", 2),
        "writes": sym("fs_write_calls", 2),
        "drains": sym("fs_drain_calls", 2),
        "scratch_guards": (ram[0x672A], ram[0x67CB]),
    }
    if resident_hook:
        state["warnings"] = [sym(f"warning_{i}") for i in range(len(steps))]
        state["notices"] = [sym(f"notices_{i}") for i in range(len(steps))]
        notice = symbol_address(map_text, "test_notice")
        state["notice"] = bytes(ram[notice:notice + 64]).split(b"\0")[0]
        state["notice_error"] = sym("test_notice_error")
    if recovery:
        state["count"] = sym("_spectrum_journal_count", 2)
        state["crc"] = sym("_spectrum_journal_crc", 2)
    state["warning"] = sym("_spectrum_journal_warning")
    for index, ext in enumerate(JOURNAL_FILES):
        base = 0xC000 + index * 0x220
        length = int.from_bytes(ram[base + 1 : base + 3], "little")
        state[ext] = bytes(ram[base + 16 : base + 16 + length]) if ram[base] else None
    last = symbol_address(map_text, "fs_last_open")
    state["last_open"] = bytes(ram[last : last + 32]).split(b"\0")[0].decode()
    load_buf = symbol_address(map_text, "load_buf")
    state["load_buf"] = bytes(ram[load_buf : load_buf + 60])
    state["move_log"] = bytes(ram[0x5CB6 : 0x5CB6 + 224])
    state["move_rows"] = sym("_move_line_count")
    state["last_ply"] = sym("_last_ply_seen", 2)
    assert sym("test_sp", 2) == 0xFF00, (name, sym("test_sp", 2))
    assert state["scratch_guards"] == (0xA5, 0xA5), (name, state["scratch_guards"])
    state["steps"] = []
    state["stats_steps"] = []
    state["capture_steps"] = []
    for i, step in enumerate(steps):
        if step[0] in ("move", "undo", "save", "load", "replay"):
            state["steps"].append(
                (sym(f"res_{i}", 2), sym(f"ctx_{i}"), sym(f"fd_{i}"), sym(f"pend_{i}"))
            )
            addr = symbol_address(map_text, f"stats_{i}")
            state["stats_steps"].append(bytes(ram[addr:addr + 33]))
            state["capture_steps"].append(sym(f"capture_{i}"))
        else:
            state["steps"].append(None)
            state["stats_steps"].append(None)
            state["capture_steps"].append(None)
    return state


def check_journal_stj(
    data: bytes, moves: list[str | None], complete: bool, label: str
) -> None:
    body = journal_body(moves)
    assert data == journal_stj(JOURNAL_SNAPSHOT, body, complete), (
        label,
        data[:16].hex(),
        len(data),
        body.hex(),
    )


def run_journal_vectors() -> None:
    """Classic journal state machine: append/adopt, LOAD staging, SAVE."""
    # LOAD leaves the move list untouched. The apply-time replay (append HL=1)
    # is what the list renders. 0xEE means nothing wrote it; zeros would mean
    # a clear without a refill.
    shown = ["e2e4", "e7e8q", "g8f6", None]
    shown_stj = journal_stj(JOURNAL_LOADED, journal_body(shown))
    seed = bytes([0xEE]) * 224
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)
        s = run_journal_scenario(
            tmp,
            "load_does_not_paint",
            [("seedlog",), ("load", "saved")],
            files={"LOAD": shown_stj},
        )
        assert s["load_buf"] == JOURNAL_LOADED, "load snapshot"
        assert s["steps"][1][3] & 0x01, s["steps"][1]
        assert s["move_log"] == seed, "LOAD must not paint or clear the list"
        assert s["NEW"] == journal_body(shown), s["NEW"]

        s = run_journal_scenario(
            tmp,
            "load_visible",
            [("seedlog",), ("load", "saved"), ("replay",)],
            files={"LOAD": shown_stj},
        )
        assert s["steps"][2][0] == 1, s["steps"][2]
        assert s["steps"][2][3] == 0x01, s["steps"][2]
        assert s["move_rows"] == 1 and s["last_ply"] == 2, (
            s["move_rows"],
            s["last_ply"],
        )
        assert s["move_log"][0:14] == journal_visible_cell("e2e4"), s["move_log"][:14]
        assert s["move_log"][18:32] == journal_visible_cell("e7e8q"), s["move_log"][
            18:32
        ]
        assert s["move_log"][32:] == bytes(192), "takeback left a later row"
        assert b"g8f6" not in s["move_log"]
        assert s["NEW"] == journal_body(shown) and s["handles"] == 0, s["handles"]

        s = run_journal_scenario(
            tmp,
            "load_then_move",
            [
                ("load", "saved"),
                ("replay",),
                ("arm",),
                ("move", "d2d4"),
                ("save", "01"),
            ],
            files={"LOAD": shown_stj},
        )
        assert s["TMP"] == journal_body(shown + ["d2d4"]), s["TMP"].hex()
        assert s["NEW"] is None and s["handles"] == 1, s
        check_journal_stj(s["STJ"], shown + ["d2d4"], True, "load_then_move")
        assert s["move_rows"] == 1 and s["last_ply"] == 2, (
            s["move_rows"],
            s["last_ply"],
        )
        assert b"d2d4" not in s["move_log"], "append must not invent a list row"
        assert s["move_log"][0:14] == journal_visible_cell("e2e4")

        empty = journal_stj(JOURNAL_LOADED, b"")
        s = run_journal_scenario(
            tmp,
            "load_empty_visible",
            [("seedlog",), ("load", "saved")],
            files={"LOAD": empty},
        )
        assert (s["steps"][1][3] & 0x01) == 0, s["steps"][1]
        assert s["move_log"] == seed, "empty history must not paint moves"
        assert s["move_rows"] == 0 and s["last_ply"] == 0

        long = [
            "a2a3",
            "a7a6",
            "b2b3",
            "b7b6",
            "c2c3",
            "c7c6",
            "d2d3",
            "d7d6",
            "e2e4",
            "e7e5",
            "f2f3",
            "f7f6",
            "g2g3",
            "g7g6",
            "h2h3",
            "h7h6",
            None,
        ]
        s = run_journal_scenario(
            tmp,
            "load_visible_scroll",
            [("load", "saved"), ("replay",)],
            files={"LOAD": journal_stj(JOURNAL_LOADED, journal_body(long))},
        )
        assert s["move_rows"] == 7 and s["last_ply"] == 15, (
            s["move_rows"],
            s["last_ply"],
        )
        assert s["move_log"][0:14] == journal_visible_cell("b2b3"), s["move_log"][:14]
        assert s["move_log"][18:32] == journal_visible_cell("b7b6")
        assert s["move_log"][192:206] == journal_visible_cell("h2h3"), s["move_log"][
            192:206
        ]
        assert s["move_log"][210:224] == bytes(14), s["move_log"][210:224]
        assert b"a2a3" not in s["move_log"] and b"h7h6" not in s["move_log"]

        # UNDO after an overflowing white move retains all final seven rows.
        undone_tail = long[:15] + [None]
        s = run_journal_scenario(
            tmp,
            "load_visible_scroll_undo_white",
            [("load", "saved"), ("replay",)],
            files={"LOAD": journal_stj(JOURNAL_LOADED, journal_body(undone_tail))},
        )
        assert s["move_rows"] == 7 and s["last_ply"] == 14, s
        assert s["move_log"] == journal_expected_rows(long[:14]), s["move_log"]
    date = (46 << 9) | (8 << 5) | 19
    clock = [
        ("poke16", "t_fat_date", date),
        ("poke16", "t_fat_time", (15 << 11) | (26 << 5)),
    ]
    old = ["a2a3", "h7h6"]
    loaded = ["e2e4", "c7c5", None]
    loaded_stj = journal_stj(JOURNAL_LOADED, journal_body(loaded))
    with tempfile.TemporaryDirectory() as tmp_name:
        tmp = Path(tmp_name)

        # New game: moves, TAKEBACK, promotions (lower/upper), two SAVEs with
        # the live handle parked back at EOF in between; C stamp vectors.
        moves = ["e2e4", "e7e5", None, "g8f6", "a7a8n", "h2h1Q"]
        steps = [*clock]
        steps += [("move", m) if m else ("undo",) for m in moves]
        steps += [("save", "01")]
        steps += [
            ("poke", 0xC000 + 2 * 0x220, 0),
            ("poke16", "t_day_delta", 13),
            ("move", "d2d4"),
            ("save", "02"),
        ]
        s = run_journal_scenario(tmp, "new_game", steps, pend=0x80)
        assert s["TMP"] == journal_body(moves + ["d2d4"]), s["TMP"].hex()
        assert s["last_open"] == "/SYS/CONFIG/02691F26.STJ", s["last_open"]
        check_journal_stj(s["STJ"], moves + ["d2d4"], True, "new_game")
        save01 = s["steps"][len(clock) + len(moves)]
        assert save01[:2] == (1, 0), save01
        assert s["fd"] & 0x3F not in (0, 0x3F) and not s["fd"] & 0x40, s["fd"]
        assert s["pend"] == 0 and s["seek_mode"] == 0 and s["handles"] == 1

        s = run_journal_scenario(tmp, "stamp", [*clock, ("save", "01")], pend=0x80)
        assert s["last_open"] == "/SYS/CONFIG/0168JF26.STJ", s["last_open"]
        check_journal_stj(s["STJ"], [], True, "empty")
        s = run_journal_scenario(tmp, "nostamp", [("save", "03")], pend=0x80)
        assert s["last_open"] == "/SYS/CONFIG/03000000.STJ", s["last_open"]

        # Local LOAD staged, applied (armed) then adopted on the next ply.
        live = [("move", m) for m in old]
        s = run_journal_scenario(
            tmp,
            "load_apply",
            [*live, ("load", "saved"), ("arm",), ("move", "b2b3"), ("save", "01")],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        load = s["steps"][len(live)]
        assert load[:2] == (1, 0) and load[3] == 0x01, load
        assert load[2] & 0x3F not in (0, 0x3F), "LOAD must keep the live fd"
        assert s["load_buf"] == JOURNAL_LOADED
        assert s["TMP"] == journal_body(loaded + ["b2b3"]), s["TMP"].hex()
        assert s["NEW"] is None and s["handles"] == 1, s
        check_journal_stj(s["STJ"], loaded + ["b2b3"], True, "load_apply")

        # LOAD never applied (colour/decode/peer refusal): stays inert.
        s = run_journal_scenario(
            tmp,
            "load_refused",
            [*live, ("load", "saved"), ("move", "c2c3"), ("save", "01")],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        assert s["TMP"] == journal_body(old + ["c2c3"]), s["TMP"].hex()
        check_journal_stj(s["STJ"], old + ["c2c3"], True, "load_refused")

        # LOAD right after GAME START of the next game: the load wins.
        s = run_journal_scenario(
            tmp,
            "load_after_start",
            [
                *live,
                ("poke", "_spectrum_journal_pend", 0x80),
                ("load", "saved"),
                ("arm",),
                ("move", "b2b3"),
            ],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        assert s["TMP"] == journal_body(loaded + ["b2b3"]), s["TMP"].hex()
        assert s["handles"] == 1 and not s["fd"] & 0x40, s

        # LOAD refused right after GAME START: the new game still truncates.
        s = run_journal_scenario(
            tmp,
            "refused_after_start",
            [
                *live,
                ("poke", "_spectrum_journal_pend", 0x80),
                ("load", "saved"),
                ("move", "d2d4"),
                ("save", "01"),
            ],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        assert s["TMP"] == journal_body(["d2d4"]), s["TMP"].hex()
        assert s["handles"] == 1, s
        check_journal_stj(s["STJ"], ["d2d4"], True, "refused_after_start")

        # A second, refused LOAD first adopts the applied body's history.
        s = run_journal_scenario(
            tmp,
            "refused_after_applied",
            [
                *live,
                ("load", "saved"),
                ("arm",),
                ("load", "saved"),
                ("move", "d2d4"),
                ("save", "01"),
            ],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        assert s["TMP"] == journal_body(loaded + ["d2d4"]), s["TMP"].hex()
        check_journal_stj(s["STJ"], loaded + ["d2d4"], True, "refused_after_applied")

        # F_SEEK keeps SEEK_SET in L even when the requested offset is nonzero.
        s = run_journal_scenario(
            tmp, "seek_nonzero", [("move", "e2e4"), ("seek", 0x1234)], pend=0x80
        )
        assert s["seek_mode"] == 0 and s["seek_pos"] == 0x1234, s

        # GAME START after an applied, not yet adopted, LOAD: new game wins.
        s = run_journal_scenario(
            tmp,
            "start_after_load",
            [
                *live,
                ("load", "saved"),
                ("arm",),
                ("poke", "_spectrum_journal_pend", 0x80),
                ("move", "d2d4"),
            ],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        assert s["TMP"] == journal_body(["d2d4"]), s["TMP"].hex()

        # SAVE between an applied LOAD and its first ply reads SHATJRNL.NEW
        # and leaves the switch armed for that ply.
        s = run_journal_scenario(
            tmp,
            "save_before_adopt",
            [*live, ("load", "saved"), ("arm",), ("save", "01"), ("move", "b2b3")],
            files={"LOAD": loaded_stj},
            pend=0x80,
        )
        save = s["steps"][len(live) + 2]
        assert save[:2] == (1, 0) and save[3] == 0x81, save
        assert s["TMP"] == journal_body(loaded + ["b2b3"]), s["TMP"].hex()
        check_journal_stj(s["STJ"], loaded, True, "save_before_adopt")

        # Valid STJ1 with an incomplete history: accepted, continuation
        # explicitly partial.
        partial = journal_stj(JOURNAL_LOADED, journal_body(loaded), False)
        s = run_journal_scenario(
            tmp,
            "partial_stj",
            [*live, ("load", "saved"), ("arm",), ("move", "b2b3"), ("save", "01")],
            files={"LOAD": partial},
            pend=0x80,
        )
        assert s["load_buf"] == JOURNAL_LOADED, "partial_stj"
        assert s["steps"][len(live)][:2] == (1, 0), "partial_stj"
        assert s["TMP"] == journal_body(loaded + ["b2b3"]), s["TMP"].hex()
        assert s["fd"] & 0x40, s["fd"]
        check_journal_stj(s["STJ"], loaded + ["b2b3"], False, "partial_stj")

        # STJ1-only LOAD: malformed sizes, records, CRCs and headers are all
        # rejected without touching snapshot, fd, pend, TMP or SHATJRNL.NEW.
        bad_crc = journal_stj(JOURNAL_LOADED, journal_body(loaded), body_crc=0x1234)
        bad_control = journal_stj(JOURNAL_LOADED, b"\x01\x80")
        bad_promotion = journal_stj(JOURNAL_LOADED, b"\x00\x50")
        wrapped_count = journal_stj(JOURNAL_LOADED, b"", record_count=0x8000)
        sentinel = bytes([0xEE]) * 60
        rejected = (
            ("legacy60", JOURNAL_LOADED),
            ("bad_crc", bad_crc),
            ("bad_control", bad_control),
            ("bad_promotion", bad_promotion),
            ("wrapped_count", wrapped_count),
            ("short_body", loaded_stj[:-2]),
            ("trailing", loaded_stj + b"\0"),
            ("bad_header", loaded_stj[:4] + b"\x02" + loaded_stj[5:]),
        )
        for label, image in rejected:
            s = run_journal_scenario(
                tmp,
                label,
                [*live, ("load", "saved"), ("move", "c2c3"), ("save", "01")],
                files={"LOAD": image},
                pend=0x80,
            )
            load = s["steps"][len(live)]
            fd_before, pend_before = s["steps"][len(live) - 1][2:4]
            assert load[:2] == (0, 4), (label, load)
            assert load[2:4] == (fd_before, pend_before), (label, load)
            assert s["load_buf"] == sentinel, label
            assert s["NEW"] is None, label
            assert s["handles"] == 1, (label, s["handles"])
            assert not s["fd"] & 0x40, (label, s["fd"])
            assert s["TMP"] == journal_body(old + ["c2c3"]), (label, s["TMP"].hex())
            check_journal_stj(s["STJ"], old + ["c2c3"], True, label)

        # Remote RESTORE: the peer's position arrives without history.
        s = run_journal_scenario(
            tmp,
            "remote_restore",
            [
                *live,
                ("poke", "_spectrum_journal_pend", 0xC0),
                ("move", "g1f3"),
                ("save", "01"),
            ],
            pend=0x80,
        )
        assert s["TMP"] == journal_body(["g1f3"]) and s["fd"] & 0x40
        check_journal_stj(s["STJ"], ["g1f3"], False, "remote_restore")

        # Torn append: prefix survives, append disabled, SAVE floors the tail.
        s = run_journal_scenario(
            tmp,
            "torn_append",
            [
                *live,
                ("poke16", "fs_budget", 1),
                ("move", "b2b3"),
                ("poke16", "fs_budget", 0xFFFF),
                ("move", "c2c3"),
                ("save", "01"),
            ],
            pend=0x80,
        )
        assert s["steps"][len(live) + 1][0] == 0 and s["fd"] == 0x7F, s
        assert len(s["TMP"]) == 5 and s["handles"] == 0, s
        check_journal_stj(s["STJ"], old, False, "torn_append")

        # Failed TMP create: history restarts partial on a later ply.
        s = run_journal_scenario(
            tmp,
            "create_fail",
            [
                ("poke", "fs_create_fail", 1),
                ("move", "e2e4"),
                ("poke", "fs_create_fail", 0),
                ("move", "e7e5"),
                ("save", "01"),
            ],
            pend=0x80,
        )
        assert s["steps"][1][0] == 0 and s["steps"][1][2] == 0x40, s["steps"]
        assert s["TMP"] == journal_body(["e7e5"]), s["TMP"].hex()
        check_journal_stj(s["STJ"], ["e7e5"], False, "create_fail")

        # SAVE failing mid-body must not leave the live handle mid-file.
        many = [
            ("move", m)
            for m in (
                "a2a3",
                "a7a6",
                "b2b3",
                "b7b6",
                "c2c3",
                "c7c6",
                "d2d3",
                "d7d6",
                "e2e3",
                "e7e6",
                "f2f3",
                "f7f6",
                "g2g3",
                "g7g6",
                "h2h3",
                "h7h6",
                "a3a4",
                "a6a5",
            )
        ] * 3
        s = run_journal_scenario(
            tmp,
            "save_fail",
            [
                *many,
                ("poke16", "fs_budget", 76 + 16),
                ("save", "01"),
                ("poke16", "fs_budget", 0xFFFF),
                ("move", "b3b4"),
            ],
            pend=0x80,
        )
        save = s["steps"][len(many) + 1]
        assert save[:2] == (0, 3) and s["STJ"] is None, save
        expected = [m[1] for m in many] + ["b3b4"]
        assert s["TMP"] == journal_body(expected), s["TMP"].hex()
        assert s["handles"] == 1 and not s["fd"] & 0x40, s

        # A source READ error is not EOF: SAVE fails, removes its destination,
        # and parks the live source back at EOF so the next append is safe.
        s = run_journal_scenario(
            tmp,
            "save_read_fail",
            [
                *many,
                ("poke16", "fs_read_fail_pos", 84),
                ("save", "01"),
                ("move", "b3b4"),
            ],
            pend=0x80,
        )
        save = s["steps"][len(many) + 1]
        assert save[:2] == (0, 3) and s["STJ"] is None, save
        expected = [m[1] for m in many] + ["b3b4"]
        assert s["TMP"] == journal_body(expected), s["TMP"].hex()
        assert s["handles"] == 1 and not s["fd"] & 0x40, s

        # A persistent source error cannot be parked at EOF. Close and disable
        # append instead of returning with a handle that can overwrite TMP.
        s = run_journal_scenario(
            tmp,
            "save_read_fail_persistent",
            [
                *many,
                ("poke", "fs_read_fail_repeat", 1),
                ("poke16", "fs_read_fail_pos", 84),
                ("save", "01"),
            ],
            pend=0x80,
        )
        save = s["steps"][len(many) + 2]
        assert save[:2] == (0, 3) and s["STJ"] is None, save
        assert s["TMP"] == journal_body([m[1] for m in many]), s["TMP"].hex()
        assert s["fd"] == 0x7F and s["handles"] == 0, s

        # A write that reports success but corrupts the closed destination is
        # detected by SAVE read-back and removed before success is reported.
        s = run_journal_scenario(
            tmp,
            "save_readback_corrupt",
            [
                ("move", "e2e4"),
                ("poke", "fs_corrupt_close", 3),
                ("save", "01"),
                ("move", "e7e5"),
            ],
            pend=0x80,
        )
        save = s["steps"][2]
        assert save[:2] == (0, 3) and s["STJ"] is None, save
        assert s["TMP"] == journal_body(["e2e4", "e7e5"]), s["TMP"].hex()
        assert s["handles"] == 1 and not s["fd"] & 0x40, s


def main() -> int:
    run_rules_asm_vector()
    run_setup_navigation_cursor_source_guard()
    run_key_alias_vector()
    run_name_tail_vector()
    run_spectranext_storage_source_guard()
    run_spectranext_journal_adopt_vector()
    run_about_zx0_bounds_vector()
    run_esxdos_handle_vectors()
    run_journal_vectors()
    run_journal_failure_vectors()
    run_journal_batch_vectors()
    run_next_journal_replay_vectors()
    run_next_rtc_vector()
    run_edit_field_vector()
    run_edit_buf_vector()
    run_input_queue_vector()
    run_shrink_helpers_vector()
    run_proto_copy_token_vector()
    run_frame_arg_push_vector()
    run_timer_tick_vector()
    run_timer_roll_vector()
    run_mqtt_connect_vector()
    run_setup_visible_vector()
    run_setup_state_vector()
    run_time_config_vector()
    run_setup_menu_render_vector()
    run_gui_log_notify_vector()
    run_gui_log_chat_vector()
    run_san_vector()
    run_streq_vector()
    run_piece_pointer_vector()
    run_keyboard_vector()
    run_board_coords_vector()
    run_ikkle_text_vector()
    run_scaled_text_vector()
    print("Spectrum ASM regression vectors ok")
    return 0


def run_spectranext_journal_adopt_vector() -> None:
    """SpectraNext adoption truncates once and treats I/O failures as failures."""
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    body = ",".join(str(value) for value in range(34))
    source = f"""
SECTION code_user
EXTERN _stj_spxn_adopt_new, _saveload_journal_append_ovl
PUBLIC _esx_fopen, _esx_fopen_update, _esx_fcreate, _esx_fread
PUBLIC _esx_fwrite, _esx_fclose, _esx_fsync, _esx_funlink, _esx_mkdir
PUBLIC _esx_fseek_end, _spxn_xfs_fseek, _spectrum_net_background_drain
PUBLIC _spxn_rom_error, _esx_commit, _esx_fopen_write_trunc
PUBLIC _esx_handle, _esx_buf, _esx_count, _esx_result
PUBLIC _spectrum_journal_fd, _spectrum_journal_pend, _spectrum_journal_warning

test_start:
    ld sp, 0xff00
    ld hl, tmp_body
    ld de, tmp_body + 1
    ld bc, 39
    ld (hl), 0xa5
    ldir
    xor a
    ld (_esx_handle), a
    ld (active_mode), a
    ld (unlinked), a
    ld (create_calls), a
    ld (create_fail), a
    ld (rom_error), a
    ld (close_fail), a
    ld (_spectrum_journal_fd), a
    ld hl, 34
    ld (source_len), hl
    ld hl, 40
    ld (tmp_len), hl
    ld hl, 0xffff
    ld (read_fail_pos), hl
    ld (update_fail_pos), hl
    call _stj_spxn_adopt_new
    jp nz, test_fail
    ld a, (_spectrum_journal_fd)
    cp 1
    jp nz, test_fail
    ld a, (_esx_handle)
    or a
    jp nz, test_fail
    ld a, (unlinked)
    cp 1
    jp nz, test_fail
    ld hl, (tmp_len)
    ld de, 34
    or a
    sbc hl, de
    jp nz, test_fail
    ld hl, new_body
    ld de, tmp_body
    ld b, 34
compare_body:
    ld a, (de)
    cp (hl)
    jp nz, test_fail
    inc hl
    inc de
    djnz compare_body
    ; The new TMP is committed once so it survives power-off in flash.
    ld a, (commits)
    cp 1
    jp nz, test_fail

    ; An empty body still truncates TMP once and removes NEW.
    ld hl, 0
    ld (source_len), hl
    call _stj_spxn_adopt_new
    jp nz, test_fail
    ld hl, (tmp_len)
    ld a, h
    or l
    jp nz, test_fail
    ld a, (create_calls)
    cp 2
    jp nz, test_fail
    ld a, (unlinked)
    cp 2
    jp nz, test_fail

    ; A failed second update-open keeps the copied prefix and never recreates.
    ld hl, 34
    ld (source_len), hl
    ld hl, 32
    ld (update_fail_pos), hl
    call _stj_spxn_adopt_new
    jp z, test_fail
    ld a, (create_calls)
    cp 3
    jp nz, test_fail
    ld hl, (tmp_len)
    ld de, 32
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (unlinked)
    cp 2
    jp nz, test_fail

    ; A zero-byte failed READ is not EOF; CLOSE must not erase its diagnosis.
    ld hl, 0xffff
    ld (update_fail_pos), hl
    ld hl, 0
    ld (read_fail_pos), hl
    call _stj_spxn_adopt_new
    jp z, test_fail
    ld a, (create_calls)
    cp 4
    jp nz, test_fail
    ld a, (unlinked)
    cp 2
    jp nz, test_fail

    ; Failure of the initial truncation close also rejects adoption.
    ld hl, 0xffff
    ld (read_fail_pos), hl
    ld a, 1
    ld (close_fail), a
    call _stj_spxn_adopt_new
    jp z, test_fail
    ld a, (create_calls)
    cp 5
    jp nz, test_fail
    ld a, (unlinked)
    cp 2
    jp nz, test_fail

    ; A failed initial truncate cannot make the stale TMP eligible for SAVE.
    ld hl, 34
    ld (tmp_len), hl
    ld a, 1
    ld (create_fail), a
    xor a
    ld (_spectrum_journal_fd), a
    ld a, 0x81
    ld (_spectrum_journal_pend), a
    ld hl, move_text
    ld (0x5fe0), hl
    ld de, 0x5fe0
    ld h, d
    ld l, e
    call _saveload_journal_append_ovl
    ld a, h
    or l
    jp nz, test_fail
    ld a, (_spectrum_journal_fd)
    cp 0x40
    jp nz, test_fail
    ld hl, (tmp_len)
    ld de, 34
    or a
    sbc hl, de
    jp nz, test_fail

    ; The next ply retries partial and replaces the stale TMP before writing.
    xor a
    ld (create_fail), a
    ld hl, move_text
    ld (0x5fe0), hl
    ld de, 0x5fe0
    ld h, d
    ld l, e
    call _saveload_journal_append_ovl
    ld a, h
    or l
    jp z, test_fail
    ld a, (_spectrum_journal_fd)
    cp 0x41
    jp nz, test_fail
    ld hl, (tmp_len)
    ld de, 2
    or a
    sbc hl, de
    jp nz, test_fail
    ld hl, (tmp_body)
    ld de, 0x0934
    or a
    sbc hl, de
    jp nz, test_fail

    ; The append entry exposes adoption failure and keeps NEW for recovery.
    ld hl, 0
    ld (read_fail_pos), hl
    xor a
    ld (_spectrum_journal_fd), a
    ld a, 0x81
    ld (_spectrum_journal_pend), a
    ld hl, move_text
    ld (0x5fe0), hl
    ld de, 0x5fe0
    ld h, d
    ld l, e
    call _saveload_journal_append_ovl
    ld a, h
    or l
    jp nz, test_fail
    ld a, (_spectrum_journal_fd)
    cp 0x7f
    jp nz, test_fail
    ld a, (unlinked)
    cp 2
    jp nz, test_fail

    ; Append reports failure and disables the journal when its CLOSE fails.
    ld hl, 0xffff
    ld (read_fail_pos), hl
    ld a, 1
    ld (_spectrum_journal_fd), a
    ld (close_fail), a
    ld hl, move_text
    ld (0x5fe0), hl
    ld de, 0x5fe0
    ld h, d
    ld l, e
    call _saveload_journal_append_ovl
    ld a, h
    or l
    jp nz, test_fail
    ld a, (_spectrum_journal_fd)
    cp 0x7f
    jp nz, test_fail
    xor a
    ld (test_result), a
test_done:
    jp test_done
test_fail:
    ld a, 1
    ld (test_result), a
    jp test_done

_esx_fopen:
    ld hl, 0
    ld (src_pos), hl
    ld a, 1
    ld (active_mode), a
    ld (_esx_handle), a
    ret
_esx_fopen_update:
    ld hl, (dst_pos)
    ld de, (update_fail_pos)
    or a
    sbc hl, de
    jr z, open_failed
open_destination:
    ld hl, 0
    ld (dst_pos), hl
    ld a, 2
    ld (active_mode), a
    ld (_esx_handle), a
    ret
open_failed:
    xor a
    ld (_esx_handle), a
    ret
_esx_fcreate:
    ld a, (create_calls)
    inc a
    ld (create_calls), a
    ld a, (create_fail)
    or a
    jr z, create_ok
    xor a
    ld (_esx_handle), a
    ld (active_mode), a
    ret
create_ok:
    ld hl, 0
    ld (tmp_len), hl
    ld (dst_pos), hl
    jr open_destination
_spxn_xfs_fseek:
    ld a, (active_mode)
    cp 1
    jr z, seek_source
    ld (dst_pos), hl
    jr seek_ok
seek_source:
    ld (src_pos), hl
seek_ok:
    ld hl, 1
    ret
_esx_fread:
    xor a
    ld (rom_error), a
    ld hl, (src_pos)
    ld de, (read_fail_pos)
    or a
    sbc hl, de
    jr z, read_failed
    ld hl, (source_len)
    ld de, (src_pos)
    or a
    sbc hl, de
    jr nc, read_available
    ld hl, 0
read_available:
    ld de, (_esx_count)
    push hl
    or a
    sbc hl, de
    pop hl
    jr c, read_count_ready
    ex de, hl
read_count_ready:
    ld a, h
    or l
    jr z, read_empty
    push hl
    ld hl, new_body
    ld de, (src_pos)
    add hl, de
    ld de, (_esx_buf)
    pop bc
    push bc
    ldir
    pop hl
    ld (_esx_result), hl
    ld de, (src_pos)
    add hl, de
    ld (src_pos), hl
    ret
read_empty:
    ld (_esx_result), hl
    ld a, 0x21                  ; physical XFS READ at EOF carries with EOF
    ld (rom_error), a
    ret
read_failed:
    ld a, 5
    ld (rom_error), a
    ld hl, 0
    ld (_esx_result), hl
    ret
_esx_fwrite:
    ld hl, (_esx_buf)
    push hl
    ld hl, tmp_body
    ld de, (dst_pos)
    add hl, de
    ex de, hl
    pop hl
    ld bc, (_esx_count)
    push bc
    ldir
    pop hl
    ld (_esx_result), hl
    ld de, (dst_pos)
    add hl, de
    ld (dst_pos), hl
    ld de, (tmp_len)
    or a
    sbc hl, de
    ret c
    ld hl, (dst_pos)
    ld (tmp_len), hl
    ret
_esx_fclose:
    xor a
    ld (rom_error), a
    ld a, (close_fail)
    or a
    jr z, close_ok
    xor a
    ld (close_fail), a
    ld hl, 0x00ff
    ret
close_ok:
    xor a
    ld (_esx_handle), a
    ld (active_mode), a
    ld hl, 0
    ret
_esx_funlink:
    ld a, (unlinked)
    inc a
    ld (unlinked), a
    ld hl, 1
    ld (_esx_result), hl
    ret
_esx_mkdir:
    ret
_esx_commit:
    ld a, (commits)
    inc a
    ld (commits), a
    ret
_esx_fopen_write_trunc:         ; TMP modelled as absent: create path
    xor a
    ld (_esx_handle), a
    ret
_esx_fsync:
_spectrum_net_background_drain:
    ret
_esx_fseek_end:
    ld hl, (tmp_len)
    ld (dst_pos), hl
    ld hl, 0
    ret
_spxn_rom_error:
    ld a, (rom_error)
    ld l, a
    ld h, 0
    ret

test_result: DEFB 255
_esx_handle: DEFB 0
_esx_buf: DEFW 0
_esx_count: DEFW 0
_esx_result: DEFW 0
_spectrum_journal_fd: DEFB 0
_spectrum_journal_pend: DEFB 0
_spectrum_journal_warning: DEFB 0
commits: DEFB 0
active_mode: DEFB 0
unlinked: DEFB 0
create_calls: DEFB 0
create_fail: DEFB 0
rom_error: DEFB 0
close_fail: DEFB 0
src_pos: DEFW 0
dst_pos: DEFW 0
tmp_len: DEFW 0
source_len: DEFW 0
read_fail_pos: DEFW 0
update_fail_pos: DEFW 0
move_text: DEFM "e2e4"
 DEFB 0
new_body: DEFB {body}
tmp_body: DEFS 40
"""
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "spectranext_journal_adopt.asm"
        harness.write_text(source + GAME_STATS_FIXTURE, encoding="ascii")
        binary = temp / "spectranext_journal_adopt.bin"
        symbols = assemble_vector(
            assembler,
            harness,
            [ROOT / "asm/overlay/saveload/journal_append.asm",
             ROOT / "asm/overlay/saveload/game_stats.asm"],
            binary,
            defines=["NETCHESSZX_SPECTRANEXT"],
        )
        result, output, pc, _, _ = execute_vector(
            emulator, binary, symbols, counter=500_000
        )
        assert result == 0 and pc == symbol_address(symbols, "test_done"), output
    print("Spectranext staged-journal adoption vector passed")


def run_about_zx0_bounds_vector() -> None:
    """The Classic ZX0 decoder must stop before its declared output limit."""
    assembler = required_tool("z80asm")
    emulator = required_tool("z88dk-ticks")
    harness_source = r"""
SECTION code_user
test_start:
    ld sp, 0xff00
    ld hl, zx0_stream
    ld de, about_input
    ld bc, 128
    ld (about_output_left), bc
    call about_dzx0_standard
    jp c, test_fail
    ld hl, (about_output_left)
    ld a, h
    or l
    jp nz, test_fail
    ld a, (about_input)
    inc a
    jp nz, test_fail
    ld a, (about_input + 127)
    inc a
    jp nz, test_fail

    ld hl, about_input
    ld de, about_input + 1
    ld bc, 128
    ld (hl), 0
    ldir
    ld a, 0xa5
    ld (about_input + 64), a
    ld hl, zx0_stream
    ld de, about_input
    ld bc, 64
    ld (about_output_left), bc
    call about_dzx0_standard
    jp nc, test_fail
    ld a, (about_input + 64)
    cp 0xa5
    jp nz, test_fail
    xor a
    ld (test_result), a
test_done:
    jp test_done
test_fail:
    ld a, 1
    ld (test_result), a
    jp test_done

test_result: DEFB 255
zx0_stream: DEFB 0x95, 0xff, 0x57, 0x00, 0x00, 0x80

INCLUDE "asm/overlay/about/entry_about.asm"
"""
    stubs_source = """
SECTION code_user
PUBLIC ovl_close_overlay_file, ovl_read_chunked
ovl_close_overlay_file:
    ret
ovl_read_chunked:
    scf
    ret
"""
    # This vector invokes only the legacy ZX0 decoder. Entry 1's renderer has
    # its own framebuffer vectors; its resident imports are unreachable here.
    for name in (
        "_spectrum_game_stats", "_netchesszx_session_state",
        "_spectrum_journal_pend", "_spectrum_journal_warning",
        "_netchesszx_local_color", "_netchesszx_board_light_attr",
        "_spectrum_render_fileui_frame", "_spectrum_render_ikkle_abs_at",
        "_spectrum_frame_wait", "_spectrum_append_u16",
        "compute_screen_base", "piece_sprites_16x16",
    ):
        stubs_source += f"PUBLIC {name}\n{name} EQU 0\n"
    render_stubs = stubs_source[stubs_source.index("PUBLIC _spectrum_game_stats") :]
    stubs_source += "PUBLIC pixel_down_hl\npixel_down_hl EQU 0\n"
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        harness = temp / "about_zx0_bounds.asm"
        stubs = temp / "about_zx0_stubs.asm"
        harness.write_text(harness_source, encoding="ascii")
        stubs.write_text(stubs_source, encoding="ascii")
        binary = temp / "about_zx0_bounds.bin"
        symbols = assemble_vector(assembler, harness, [stubs], binary)
        result, output, pc, _, _ = execute_vector(
            emulator, binary, symbols, counter=500_000
        )
        assert result == 0 and pc == symbol_address(symbols, "test_done"), output
    print("Classic About ZX0 output-bound vector passed")
    run_about_render_vector(assembler, emulator, render_stubs)


ABOUT_RENDER_HARNESS = r"""
SECTION code_user
test_start:
    ld sp, 0xff00
    ld a, 0xc3                  ; JP rst8_handler at the esxDOS RST 8 vector
    ld (0x0008), a
    ld hl, rst8_handler
    ld (0x0009), hl
    ld hl, screen_seed
    ld de, 0x4000
    ld bc, 6912
    ldir
    ld ix, 0x1234
    ld iy, 0x5c3a
    call _about_render_ovl_entry
    ld (saved_sp), sp
    ld (returned), hl
    ld (saved_ix), ix
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done

; Skip the hook byte. Open returns A unchanged as the handle; close counts.
rst8_handler:
    ex (sp), hl
    ld b, (hl)
    inc hl
    ex (sp), hl
    call check_handle
    ld c, a
    ld a, b
    cp 0x9b
    jr z, rst8_close
    call io_step
    ld a, c
    ret
rst8_close:
    ld hl, closes
    inc (hl)
    ret

; Open, seek and every read count down io_budget; reaching zero fails.
io_step:
    ld hl, io_budget
    dec (hl)
    scf
    ret z
    or a
    ret

; Open passes drive '*', which the stub returns as the handle; every later
; call must pass it back.
check_handle:
    cp '*'
    ret z
    ld hl, bad_handles
    inc (hl)
    ret

; Defined here, EXTERN in entry_about.asm: older z80asm needs them PUBLIC.
PUBLIC ovl_read_chunked, ovl_close_overlay_file

; IX=destination, BC=bytes from the packed About payload.
ovl_read_chunked:
    call check_handle
    push bc
    call io_step
    pop bc
    ret c
    push ix
    pop de
    ld hl, (file_cursor)
    ldir
    ld (file_cursor), hl
    or a
    ret

ovl_close_overlay_file:
    ret

PUBLIC pixel_down_hl
PIXEL_DOWN_HL

test_result: DEFB 255
returned: DEFW 0
saved_ix: DEFW 0
saved_iy: DEFW 0
saved_sp: DEFW 0
closes: DEFB 0
bad_handles: DEFB 0
io_budget: DEFB IO_BUDGET
file_cursor: DEFW payload
payload: BINARY "about_payload.bin"
screen_seed: BINARY "about_screen.bin"

INCLUDE "asm/overlay/about/entry_about.asm"
"""


def run_about_render_vector(assembler: str, emulator: str, stubs: str) -> None:
    """Classic About paints the packed band exactly and fails closed.

    `stubs` defines entry 1's resident imports, unreachable from this entry;
    the harness supplies the real pixel_down_hl and a scripted esxDOS file.
    """
    import gen_assets

    raw = (ROOT / "assets/spectrum/about_board.bin").read_bytes()
    pixels, attrs = raw[: 152 * 32], raw[152 * 32 :]
    seed = bytes((index * 37 + 11) & 0xFF for index in range(6912))
    primitives = (ROOT / "asm/ula/panel_primitives.inc").read_text(encoding="ascii")
    pixel_down = primitives[
        primitives.index("pixel_down_hl:") : primitives.index("clear_right_text_row:")
    ]
    ula = lambda y: ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
    payload = gen_assets.build_about_payload(raw)
    sizes = [int.from_bytes(payload[i : i + 2], "little") for i in range(0, 26, 2)]
    streams, offset = [], 26
    for size in sizes:
        streams.append(payload[offset : offset + size])
        offset += size

    def packed(streams, sizes=None):
        sizes = sizes or [len(stream) for stream in streams]
        return b"".join(size.to_bytes(2, "little") for size in sizes) + b"".join(streams)

    # (I/O budget, payload): 0 never fails; 1 open, 2 seek, 3 directory and
    # 4..16 the 13 chunk reads. Then a zero and an oversized packed size and
    # a 448-byte stream in the 384-byte tail slot, which must not overflow.
    cases = [(budget, payload) for budget in range(17)] + [
        (0, packed(streams, [0] + sizes[1:])),
        (0, packed(streams, [449] + sizes[1:])),
        (0, packed(streams[:10] + streams[:1] + streams[11:])),
    ]
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        (temp / "about_screen.bin").write_bytes(seed)
        (temp / "about_render_stubs.asm").write_text(
            "SECTION code_user\n" + stubs, encoding="ascii"
        )
        for case, (budget, image) in enumerate(cases):
            (temp / "about_payload.bin").write_bytes(image)
            harness = temp / "about_render.asm"
            harness.write_text(
                ABOUT_RENDER_HARNESS.replace("PIXEL_DOWN_HL", pixel_down)
                .replace("IO_BUDGET", str(budget)),
                encoding="ascii",
            )
            binary = temp / "about_render.bin"
            symbols = assemble_vector(
                assembler, harness, [temp / "about_render_stubs.asm"], binary
            )
            result, output, pc, _, _ = execute_vector(
                emulator, binary, symbols, counter=20_000_000
            )
            assert result == 0 and pc == symbol_address(symbols, "test_done"), output
            ram = binary.with_suffix(".ram").read_bytes()
            word = lambda name: int.from_bytes(
                ram[symbol_address(symbols, name) :][:2], "little"
            )
            closes = ram[symbol_address(symbols, "closes")]
            assert ram[symbol_address(symbols, "bad_handles")] == 0, case
            assert (word("saved_ix"), word("saved_iy"), word("saved_sp")) == (
                0x1234, 0x5C3A, 0xFF00
            ), case
            assert (word("returned"), closes) == (
                (1, 1) if case == 0 else (0, 0 if budget == 1 else 1)
            ), (case, word("returned"), closes)
            # No decode, even an overflowing one, reaches the ZX0 directory.
            if budget == 0 or budget > 3:
                assert ram[0x67D9:0x67F3] == image[:26], case
            if case:
                continue
            for y in range(192):
                row = ram[0x4000 + ula(y) :][:32]
                want = (
                    pixels[(y - 24) * 32 :][:32] if 24 <= y < 176
                    else seed[ula(y) :][:32]
                )
                assert row == want, f"About pixel row {y}"
            assert ram[0x5800:0x5860] == seed[6144:6240]
            assert ram[0x5860:0x5AC0] == attrs, "About attributes"
            assert ram[0x5AC0:0x5B00] == seed[6848:]
    print(f"Classic About band render and {len(cases) - 1} failure exits passed")


def journal_visible_cell(move: str) -> bytes:
    return b"--:-- " + move.ljust(7).encode("ascii") + b"\0"


def run_journal_failure_vectors() -> None:
    """Disk failures cannot masquerade as EOF, durable writes or full history."""
    moves = ["e2e4", "e7e5"] * 41
    body = journal_body(moves)
    loaded = journal_stj(JOURNAL_LOADED, journal_body(["d2d4"]))
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        old = ["a2a3", "a7a6"]
        s = run_journal_scenario(
            tmp, "adopt_missing_new_stale_tmp",
            [("move", "e2e4"), ("save", "01")],
            files={"TMP": journal_body(old)}, pend=0x81,
        )
        assert s["steps"][0][0] == 0 and s["steps"][0][2] == 0x40, s
        assert s["TMP"] == journal_body(old) and s["NEW"] is None, s
        check_journal_stj(s["STJ"], [], False, "missing_new_stale_tmp")

        for action in (("move", "g1f3"), ("load", "saved")):
            label = "adopt_read_error_" + action[0]
            chunk = 160 if action[0] == "move" else 84
            s = run_journal_scenario(
                tmp, label,
                [("poke16", "fs_read_fail_pos", chunk), action, ("save", "01")],
                files={"NEW": body, "LOAD": loaded}, pend=0x81,
            )
            assert s["steps"][1][2] == 0x7F, (label, s["steps"])
            assert s["TMP"] == body[:chunk] and s["handles"] == 0, (label, s)
            check_journal_stj(s["STJ"], moves[:chunk // 2], False, label)
            if action[0] == "move":
                assert s["steps"][1][0] == 0 and s["NEW"] == body, s

        # Failure of the final one-byte read does not prove the exact length.
        s = run_journal_scenario(
            tmp, "load_eof_error",
            [("poke16", "fs_read_fail_pos", len(loaded)), ("load", "saved")],
            files={"LOAD": loaded + b"\0", "TMP": body, "NEW": body}, pend=0x81,
        )
        assert s["steps"][1] == (0, 4, 0, 0x81), s["steps"]
        assert s["load_buf"] == bytes([0xEE]) * 60, s["load_buf"]
        assert s["TMP"] == body and s["NEW"] == body and s["handles"] == 0, s

        # A failed close of the staged copy cannot promise complete history.
        s = run_journal_scenario(
            tmp, "load_staging_close_error",
            [("poke", "fs_close_fail", 2), ("load", "saved")],
            files={"LOAD": loaded, "TMP": body},
        )
        assert s["steps"][1] == (1, 0, 0, 0x40), s["steps"]
        assert s["load_buf"] == JOURNAL_LOADED and s["NEW"] is None, s
        assert s["TMP"] == body and s["handles"] == 0, s

        s = run_journal_scenario(
            tmp, "save_eof_error",
            [("move", "e2e4"), ("poke16", "fs_read_fail_pos", 78),
             ("save", "01"), ("move", "e7e5")], pend=0x80,
        )
        assert s["steps"][2][:2] == (0, 3) and s["STJ"] is None, s
        assert s["TMP"] == journal_body(["e2e4", "e7e5"]), s
        assert s["handles"] == 1 and not s["fd"] & 0x40, s

        s = run_journal_scenario(
            tmp, "append_sync_error",
            [("poke", "fs_sync_fail", 1), ("move", "e2e4"),
             ("move", "e7e5"), ("save", "01")], pend=0x80,
        )
        assert s["steps"][1][0] == 0 and s["fd"] == 0x7F, s
        assert s["TMP"] == journal_body(["e2e4"]) and s["handles"] == 0, s
        check_journal_stj(s["STJ"], ["e2e4"], False, "append_sync_error")

        s = run_journal_scenario(
            tmp, "save_sync_error",
            [("move", "e2e4"), ("poke", "fs_sync_fail", 1),
             ("save", "01"), ("move", "e7e5")], pend=0x80,
        )
        assert s["steps"][2][:2] == (0, 3) and s["STJ"] is None, s
        assert s["TMP"] == journal_body(["e2e4", "e7e5"]), s
        assert s["handles"] == 1 and not s["fd"] & 0x40, s

        for marker, offset, ply in ((1, 18, 2), (3, 0, 1)):
            for label, staged, partial, injection in (
                ("partial", journal_body(["e7e5"]), 0x40, []),
                ("empty_partial", None, 0x40, []),
                ("missing", None, 0, []),
                ("read_error", body, 0, [("poke16", "fs_read_limit", 2),
                                          ("poke16", "fs_read_fail_pos", 2)]),
                ("torn_record", body[:3], 0, []),
                ("close_error", body, 0, [("poke", "fs_close_fail", 2)]),
            ):
                # Staged but unarmed: the apply-time consume is a no-op here
                # (tests/tools/test_journal_link.py covers the armed switch).
                pend = partial | (0 if label == "empty_partial" else 1)
                label = f"replay_{label}_{marker}"
                s = run_journal_scenario(
                    tmp, label, [("seedlog",), *injection, ("replay", marker)],
                    files={"NEW": staged} if staged is not None else {},
                    pend=pend,
                )
                expected = bytearray(224)
                expected[offset:offset + 9] = b"RESTORED\0"
                assert s["move_log"] == expected, (label, s["move_log"])
                assert s["move_rows"] == 1 and s["last_ply"] == ply, (label, s)
                assert s["NEW"] == staged and s["handles"] == 0, (label, s)
                assert s["fd"] == 0 and s["pend"] == pend, (label, s)
    print("Journal disk-failure and partial-replay vectors passed")


def journal_expected_rows(moves: list[str]) -> bytes:
    """Visible seven rows for a complete move sequence, including an odd tail."""
    visible = moves[max(0, (len(moves) + 1) // 2 - 7) * 2:]
    rows = bytearray(224)
    for i, move in enumerate(visible):
        offset = (i // 2) * 32 + (18 if i % 2 else 0)
        rows[offset:offset + 14] = journal_visible_cell(move)
    return bytes(rows)


def run_journal_batch_vectors() -> None:
    """Scratch bounds, disk-call budget, short reads and replay block boundaries."""
    cycle = ["a2a3", "a7a6", "b2b3", "b7b6", "e7e8q", "e2e1n", "h2h3", "h7h6"]
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        # Around both 84-byte codec chunks and 160-byte append/replay chunks.
        for count in (0, 1, 41, 42, 43, 79, 80, 81, 84, 200):
            moves = (cycle * 25)[:count]
            body = journal_body(moves)
            blocks = (len(body) + 83) // 84
            s = run_journal_scenario(
                tmp, f"batch_load_{count}", [("load", "saved")],
                files={"LOAD": journal_stj(JOURNAL_LOADED, body)},
            )
            assert s["steps"][0][:2] == (1, 0), (count, s)
            assert s["load_buf"] == JOURNAL_LOADED, count
            assert s["NEW"] == (body or None), count
            assert (s["reads"], s["writes"]) == (2 * blocks + 2, blocks), (count, s)
            # Unarmed staged body: replay only; the consume adds one drain.
            s = run_journal_scenario(
                tmp, f"batch_replay_{count}", [("replay",)],
                files={"NEW": body}, pend=0x01,
            )
            assert s["move_log"] == journal_expected_rows(moves), count
            assert (s["last_ply"], s["move_rows"]) == (count, min(7, (count + 1) // 2)), count
            assert s["reads"] == 2 * ((len(body) + 159) // 160 + 1), (count, s["reads"])
            assert s["drains"] == 2 * count + 4, (count, s["drains"])
            assert (s["fd"], s["pend"], s["handles"], s["NEW"]) == (0, 0x01, 0, body), count
            s = run_journal_scenario(
                tmp, f"batch_adopt_{count}", [("move", "g1f3")],
                files={"NEW": body}, pend=0x81,
            )
            blocks = (len(body) + 159) // 160
            assert (s["reads"], s["writes"]) == (blocks + 1, blocks + 1), (count, s)
            assert s["TMP"] == body + journal_body(["g1f3"]), count

        moves = cycle * 25
        body = journal_body(moves)
        s = run_journal_scenario(
            tmp, "batch_save_200",
            [("poke", 0xc900, 1), ("poke", 0xc901, 3),
             ("poke16", 0xc902, len(body)), ("save", "01")],
            files={"TMP": body}, fd=1,
        )
        check_journal_stj(s["STJ"], moves, True, "batch_save_200")
        assert (s["reads"], s["writes"]) == (13, 7), s

        # UNDO crosses the refill boundary and removes a row before promotion.
        records = moves[:79] + [None, "a7a8r", "b2b1b"]
        expected = moves[:78] + ["a7a8r", "b2b1b"]
        s = run_journal_scenario(
            tmp, "batch_replay_undo", [("replay",)],
            files={"NEW": journal_body(records)}, pend=0x01,
        )
        assert s["move_log"] == journal_expected_rows(expected), s["move_log"]
        assert s["last_ply"] == len(expected) and s["drains"] == 2 * len(records) + 4, s

        # Valid repeated UNDO records can cross several evicted rows.
        records = moves[:31] + [None] * 17
        s = run_journal_scenario(
            tmp, "batch_replay_many_undo", [("replay",)],
            files={"NEW": journal_body(records)}, pend=0x01,
        )
        assert s["move_log"] == journal_expected_rows(moves[:14]), s["move_log"]
        assert (s["last_ply"], s["move_rows"]) == (14, 7), s

        # Short even reads are not EOF; all records still reach the display.
        s = run_journal_scenario(
            tmp, "batch_replay_short_even",
            [("poke16", "fs_read_limit", 6), ("replay",)],
            files={"NEW": body}, pend=0x01,
        )
        assert s["move_log"] == journal_expected_rows(moves), s["move_log"]
        assert (s["reads"], s["drains"]) == (136, 404), s

        # Odd successful reads and failures in the first/refilled/final read
        # must discard every visible prefix, with either RESTORED colour marker.
        for marker, offset, ply in ((1, 18, 2), (3, 0, 1)):
            for label, injection in (
                ("first", [("poke16", "fs_read_fail_pos", 0)]),
                ("refill", [("poke16", "fs_read_fail_pos", 160)]),
                ("eof", [("poke16", "fs_read_fail_pos", 400)]),
                ("second_pass", [("poke16", "fs_read_fail_call", 5)]),
                ("interior", [("poke16", "fs_read_limit", 84),
                              ("poke16", "fs_read_fail_pos", 84)]),
                ("odd", [("poke16", "fs_read_limit", 3)]),
            ):
                s = run_journal_scenario(
                    tmp, f"batch_replay_fail_{label}_{marker}",
                    [*injection, ("replay", marker)], files={"NEW": body}, pend=0x01,
                )
                expected_log = bytearray(224)
                expected_log[offset:offset + 9] = b"RESTORED\0"
                assert s["move_log"] == expected_log, (label, marker, s["move_log"])
                assert (s["last_ply"], s["move_rows"]) == (ply, 1), (label, marker)
                assert (s["fd"], s["pend"], s["handles"], s["NEW"]) == (0, 0x01, 0, body), s
    print("Journal chunk/replay boundaries and I/O budgets passed")


def run_next_journal_replay_vectors() -> None:
    """NEX SAVELOAD paints SHATJRNL.NEW before the apply-time consume."""
    cycle = ["a2a3", "a7a6", "b2b3", "b7b6", "e7e8q", "e2e1n", "h2h3", "h7h6"]
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for count in (0, 1, 14, 15, 80, 81, 200):
            moves = (cycle * 25)[:count]
            body = journal_body(moves)
            s = run_journal_scenario(
                tmp, f"next_replay_{count}", [("seedlog",), ("replay",)],
                files={"NEW": body}, pend=0x01, recovery=True, next_target=True,
            )
            assert s["steps"][1][0] == 1, (count, s["steps"])
            assert s["move_log"] == journal_expected_rows(moves), count
            assert (s["last_ply"], s["move_rows"]) == (
                count, min(7, (count + 1) // 2)), count
            assert s["reads"] == 2 * ((len(body) + 159) // 160 + 1), (count, s["reads"])
            assert (s["fd"], s["pend"], s["handles"], s["NEW"]) == (
                0, 0x01, 0, body), count

        # Load/resume apply: armed switch plus complete body. The call paints
        # the list, adopts NEW as the live TMP and the next move appends.
        moves = (cycle * 25)[:41] + [None, "d2d4"]
        shown = (cycle * 25)[:40] + ["d2d4"]
        body = journal_body(moves)
        s = run_journal_scenario(
            tmp, "next_replay_adopt",
            [("seedlog",), ("replay", 3), ("move", "g8f6")],
            files={"NEW": body}, pend=0x81, recovery=True, next_target=True,
        )
        # The append never invents a row: the list is the replayed history.
        assert s["move_log"] == journal_expected_rows(shown), s["move_log"]
        assert (s["last_ply"], s["move_rows"]) == (len(shown), 7), s
        assert s["steps"][1][0] == 1 and s["steps"][1][3] == 0, s["steps"]
        assert s["steps"][2][0] == 1, s["steps"]
        assert s["NEW"] is None and s["TMP"] == body + journal_body(["g8f6"]), s["TMP"]
        assert s["handles"] == 1 and s["count"] == len(moves) + 1, s

        # Partial or unreadable bodies keep the RESTORED marker contract.
        for marker, offset, ply in ((1, 18, 2), (3, 0, 1)):
            for label, pend, steps in (
                ("partial", 0x41, [("replay", marker)]),
                ("read_fail", 0x01,
                 [("poke16", "fs_read_fail_pos", 160), ("replay", marker)]),
                ("torn", 0x01, [("poke16", "fs_read_limit", 3), ("replay", marker)]),
            ):
                body = journal_body((cycle * 25)[:120])
                s = run_journal_scenario(
                    tmp, f"next_replay_{label}_{marker}", steps,
                    files={"NEW": body}, pend=pend, recovery=True, next_target=True,
                )
                expected_log = bytearray(224)
                expected_log[offset:offset + 9] = b"RESTORED\0"
                assert s["move_log"] == expected_log, (label, marker, s["move_log"])
                assert (s["last_ply"], s["move_rows"]) == (ply, 1), (label, marker)
                assert (s["pend"], s["handles"], s["NEW"]) == (pend, 0, body), (label, s)
    print("Next SAVELOAD journal replay vectors passed")


if __name__ == "__main__":
    raise SystemExit(main())
