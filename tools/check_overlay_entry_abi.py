#!/usr/bin/env python3
"""Check overlay ASM wrappers that pass spectrum_overlay_context to C fastcall."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


LABEL_RE = re.compile(r"^([A-Za-z0-9_.$]+):")
JUMP_C_RE = re.compile(r"^\s+jp\s+(_[A-Za-z0-9_]+_ovl)\s*$")
DW_RE = re.compile(r"^\s+DW\s+([A-Za-z0-9_.$]+)\s*$")
DEFINE_RE = re.compile(r"^#define\s+(SPECTRUM_OVL_[A-Za-z0-9_]+)\s+(.+)$")
EQU_RE = re.compile(r"^(SPECTRUM_OVL_[A-Za-z0-9_]+)\s+EQU\s+(\d+)\s*$")
ASM_EQU_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s+EQU\s+(0x[0-9A-Fa-f]+|\d+)\s*$")
LOWRAM_DEFINE_RE = re.compile(
    r"^#define\s+(NETCHESSZX_LOWRAM_[A-Za-z0-9_]+_ADDR)\s+"
    r"(0x[0-9A-Fa-f]+|\d+)\s*$"
)


ENTRY_TABLES = {
    "asm/overlay/board/entry_board.asm": [
        ("SPECTRUM_OVL_BOARD_APPLY", "_board_apply_ovl_entry"),
    ],
    "asm/overlay/direct/entry_direct.asm": [
        ("SPECTRUM_OVL_DIRECT_LISTEN", "_direct_listen_ovl"),
        ("SPECTRUM_OVL_DIRECT_CONNECT", "_direct_connect_ovl"),
        ("SPECTRUM_OVL_DIRECT_WAIT_CONNECT", "_direct_wait_pc_connect_ovl"),
        ("SPECTRUM_OVL_DIRECT_READ", "_direct_read_payload_ovl_entry"),
        ("SPECTRUM_OVL_DIRECT_SEND", "_direct_send_text_ovl_entry"),
    ],
    "asm/overlay/gui_log/entry_gui_log.asm": [
        ("SPECTRUM_OVL_GUI_LOG_ADD_MOVE", "_gui_log_add_move_ovl_entry"),
        ("SPECTRUM_OVL_GUI_LOG_ADD_CHAT", "_gui_log_add_chat_ovl_entry"),
        ("SPECTRUM_OVL_APP_INPUT_PARSE_MOVE", "_app_input_parse_move_ovl_entry"),
        ("SPECTRUM_OVL_GUI_LOG_CONNECTION_PANEL", "_connection_panel_ovl_entry"),
    ],
    "asm/overlay/hints/entry_hints.asm": [
        ("SPECTRUM_OVL_HINTS_SHOW", "_rules_hints_ovl"),
        ("SPECTRUM_OVL_HINTS_CLEAR", "_rules_hints_clear_ovl"),
    ],
    "asm/overlay/menu_config/entry_menu_config.asm": [
        ("SPECTRUM_OVL_MENU_CONFIG_RUN", "_menu_config_run_ovl_entry"),
        ("SPECTRUM_OVL_MENU_CONFIG_PAINT_ATTRS", "_menu_config_paint_attrs_ovl_entry"),
        ("SPECTRUM_OVL_MENU_CONFIG_VALIDATE_IP", "_menu_config_validate_ip_ovl_entry"),
        ("SPECTRUM_OVL_MENU_CONFIG_EDIT_LINE", "_menu_config_edit_line_ovl_entry"),
    ],
    "asm/overlay/menu_logic/entry_menu_logic.asm": [
        ("SPECTRUM_OVL_MENU_LOGIC_UPDATE_ROOM", "_menu_logic_update_room_ovl_entry"),
        ("SPECTRUM_OVL_MENU_LOGIC_MOVE_FOCUS", "_menu_logic_move_focus_ovl_entry"),
        ("SPECTRUM_OVL_MENU_LOGIC_ROOM_APPEND", "_menu_logic_room_append_ovl_entry"),
        (
            "SPECTRUM_OVL_MENU_LOGIC_ROOM_EDITABLE",
            "_menu_logic_room_editable_ovl_entry",
        ),
        (
            "SPECTRUM_OVL_MENU_LOGIC_ROOM_BACKSPACE",
            "_menu_logic_room_backspace_ovl_entry",
        ),
        (
            "SPECTRUM_OVL_MENU_LOGIC_COMPUTE_VISIBLE",
            "_menu_logic_compute_visible_ovl_entry",
        ),
        ("SPECTRUM_OVL_MENU_LOGIC_STEP_ROW", "_menu_logic_step_row_ovl_entry"),
        ("SPECTRUM_OVL_STATUS_PHASE", "_status_phase_ovl_entry"),
    ],
    "asm/overlay/mqtt_connect/entry_mqtt_connect.asm": [
        ("SPECTRUM_OVL_MQTT_CONNECT_START", "_mqtt_connect_start_ovl"),
        ("SPECTRUM_OVL_MQTT_CONNECT_ACTIVATE", "_mqtt_activate_side_ovl"),
    ],
    "asm/overlay/mqtt_tx/entry_mqtt_tx.asm": [
        ("SPECTRUM_OVL_MQTT_TX_SEND_TEXT", "_mqtt_tx_send_text_ovl_entry"),
        ("SPECTRUM_OVL_MQTT_TX_PUBLISH_SETUP", "_mqtt_tx_publish_setup_ovl_entry"),
        ("SPECTRUM_OVL_MQTT_TX_PUBLISH_SESSION", "_mqtt_tx_publish_session_ovl"),
        ("SPECTRUM_OVL_MQTT_TX_SYNC_TIME", "_mqtt_tx_sync_time_ovl"),
    ],
    "asm/overlay/rules/entry_rules.asm": [
        ("SPECTRUM_OVL_RULES_PLAY", "_rules_play_with_context"),
        ("SPECTRUM_OVL_RULES_CHECK", "_rules_check_with_context"),
    ],
}


def check_file(path: Path) -> list[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    errors: list[str] = []
    for i, line in enumerate(lines):
        m = LABEL_RE.match(line)
        if not m or not m.group(1).endswith("_ovl_entry"):
            continue
        saw_ex = False
        j = i + 1
        while j < len(lines):
            next_line = lines[j]
            if LABEL_RE.match(next_line):
                break
            stripped = next_line.strip().lower()
            if stripped == "ex de, hl":
                saw_ex = True
            jm = JUMP_C_RE.match(next_line)
            if jm:
                if not saw_ex:
                    errors.append(
                        f"{path}:{i + 1}: {m.group(1)} jumps to {jm.group(1)} "
                        "without 'ex de, hl'"
                    )
                break
            j += 1
    return errors


def parse_overlay_defines(path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    aliases: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = DEFINE_RE.match(raw_line.strip())
        if not match:
            continue
        name, raw_value = match.groups()
        value_text = raw_value.split("/*", 1)[0].strip().rstrip("uUlL")
        if value_text.isdigit():
            values[name] = int(value_text)
        else:
            aliases[name] = value_text
    changed = True
    while changed:
        changed = False
        for name, alias in list(aliases.items()):
            if alias in values:
                values[name] = values[alias]
                del aliases[name]
                changed = True
    return values


def parse_screen_equ(path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = EQU_RE.match(raw_line.strip())
        if match:
            values[match.group(1)] = int(match.group(2))
    return values


def parse_int(value: str) -> int:
    return int(value, 16 if value.lower().startswith("0x") else 10)


def parse_lowram_defines(path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = LOWRAM_DEFINE_RE.match(raw_line.strip())
        if match:
            values[match.group(1)] = parse_int(match.group(2))
    return values


def parse_asm_equ(path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = ASM_EQU_RE.match(raw_line.strip())
        if match:
            values[match.group(1)] = parse_int(match.group(2))
    return values


def check_expected_addr(
    errors: list[str],
    rel: str,
    expected: int | None,
    actual_values: dict[str, int],
    actual_name: str,
) -> None:
    actual = actual_values.get(actual_name)
    if expected is None:
        errors.append(f"{rel}: missing low-RAM expected address for {actual_name}")
    elif actual is None:
        errors.append(f"{rel}: missing EQU {actual_name}")
    elif actual != expected:
        errors.append(
            f"{rel}: {actual_name} is 0x{actual:04X}, expected 0x{expected:04X}"
        )


def check_lowram_addresses(root: Path) -> list[str]:
    errors: list[str] = []
    lowram = parse_lowram_defines(root / "src" / "spectrum" / "lowram_map.h")
    screen = parse_asm_equ(root / "asm" / "spectrum" / "screen.asm")
    loader = parse_asm_equ(root / "asm" / "esxdos" / "overlay_loader.asm")
    rules_entry = parse_asm_equ(root / "asm" / "overlay" / "rules" / "entry_rules.asm")
    context = lowram.get("NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR")
    rules_board = lowram.get("NETCHESSZX_LOWRAM_RULES_BOARD_ADDR")

    check_expected_addr(
        errors,
        "asm/spectrum/screen.asm",
        rules_board,
        screen,
        "NETCHESSZX_RULES_BOARD_BASE",
    )
    check_expected_addr(
        errors,
        "asm/spectrum/screen.asm",
        context,
        screen,
        "NETCHESSZX_OVERLAY_CONTEXT",
    )
    check_expected_addr(
        errors,
        "asm/esxdos/overlay_loader.asm",
        context,
        loader,
        "_spectrum_overlay_context",
    )
    check_expected_addr(
        errors,
        "asm/overlay/rules/entry_rules.asm",
        context,
        rules_entry,
        "_spectrum_overlay_context",
    )
    return errors


def parse_entry_table(path: Path) -> tuple[int, list[str]] | None:
    entries: list[str] = []
    count: int | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split(";", 1)[0]
        match = DW_RE.match(line)
        if not match:
            if count is not None and entries:
                break
            continue
        value = match.group(1)
        if count is None:
            if not value.isdigit():
                continue
            count = int(value)
            continue
        entries.append(value)
        if len(entries) == count:
            break
    if count is None:
        return None
    return count, entries


def check_entry_tables(root: Path) -> list[str]:
    errors: list[str] = []
    overlay_values = parse_overlay_defines(
        root / "src" / "spectrum" / "overlay" / "overlay.h"
    )
    screen_values = parse_screen_equ(root / "asm" / "spectrum" / "screen.asm")

    for rel, expected in ENTRY_TABLES.items():
        path = root / rel
        parsed = parse_entry_table(path)
        if parsed is None:
            errors.append(f"{path}: missing DW entry table")
            continue
        count, entries = parsed
        if count != len(expected):
            errors.append(f"{path}: DW count {count} != expected {len(expected)}")
        if entries != [symbol for _, symbol in expected]:
            errors.append(f"{path}: entry table order mismatch: {entries}")
        for index, (macro, _) in enumerate(expected):
            if overlay_values.get(macro) != index:
                errors.append(
                    f"{path}: {macro} in overlay.h is {overlay_values.get(macro)!r}, expected {index}"
                )
            if macro in screen_values and screen_values[macro] != index:
                errors.append(
                    f"{path}: {macro} in screen.asm is {screen_values[macro]!r}, expected {index}"
                )
    return errors


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = Path(args.root)
    errors: list[str] = []
    for path in sorted((root / "asm" / "overlay").glob("*/entry_*.asm")):
        errors.extend(check_file(path))
    errors.extend(check_entry_tables(root))
    errors.extend(check_lowram_addresses(root))

    if errors:
        for error in errors:
            print(f"[ERR] {error}", file=sys.stderr)
        return 1
    print("[OK] overlay entry ABI wrappers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
