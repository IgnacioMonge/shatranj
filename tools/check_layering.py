#!/usr/bin/env python3
"""Check source include layering rules for the modular Spectrum refactor."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from check_lowmem_layout import parse_const


DEFAULT_ALLOWLIST = Path("docs/layering_allowlist.json")
DEFAULT_OVERLAY_STATE_ALLOWLIST = Path("docs/overlay_state_allowlist.json")

RULES = [
    ("transport-no-config", re.compile(r"^src/spectrum/transport/"),
     re.compile(r"^spectrum/config/")),
    ("transport-no-session", re.compile(r"^src/spectrum/transport/"),
     re.compile(r"^spectrum/session/")),
    ("transport-no-game-protocol", re.compile(r"^src/spectrum/transport/"),
     re.compile(r"^common/protocol/")),
    ("overlay-no-app", re.compile(r"^src/spectrum/overlay/"),
     re.compile(r"^spectrum/app/")),
    ("overlay-no-esp-at", re.compile(r"^src/spectrum/overlay/"),
     re.compile(r"^spectrum/transport/esp_at\.h$")),
    ("saveload-public-no-overlay", re.compile(r"^src/spectrum/saveload/.*\.h$"),
     re.compile(r"^spectrum/overlay/")),
    (
        "cell-cursor-ports-only",
        re.compile(r"^src/spectrum/ui/cell_cursor\.[ch]$"),
        re.compile(r"^(?!(?:common/space/|spectrum/ui/(?:cell_cursor|space_navigation)\.h$)).+"),
    ),
    (
        "space-self-contained",
        re.compile(r"^src/common/space/.*\.(?:[ch]|inc)$"),
        re.compile(r"^(?!(?:common/space/|stdint\.h$)).+"),
    ),
    (
        "transport-no-ui",
        re.compile(r"^src/spectrum/transport/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/ui/"),
    ),
    (
        "transport-no-app",
        re.compile(r"^src/spectrum/transport/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/app/"),
    ),
    (
        "ui-no-transport",
        re.compile(r"^src/spectrum/ui/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/transport/"),
    ),
    (
        "ui-no-board",
        re.compile(r"^src/spectrum/ui/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/board/"),
    ),
    (
        "ui-no-overlay",
        re.compile(r"^src/spectrum/ui/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/overlay/"),
    ),
    (
        "common-no-spectrum",
        re.compile(r"^src/common/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/"),
    ),
    (
        "common-session-no-pc-qt",
        re.compile(r"^src/common/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^(?:pc/|Qt|Q[A-Z])"),
    ),
    (
        "common-session-no-toolchain",
        re.compile(r"^src/common/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^(?:z88dk|sdcc|arch/)"),
    ),
    (
        "protocol-no-ui",
        re.compile(r"^src/(common/protocol|spectrum/session)/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/ui/"),
    ),
    (
        "session-no-app",
        re.compile(r"^src/spectrum/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/app/"),
    ),
    (
        "session-no-board",
        re.compile(r"^src/spectrum/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/board/"),
    ),
    (
        "session-no-overlay",
        re.compile(r"^src/spectrum/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/overlay/"),
    ),
    (
        "session-no-transport-internals",
        re.compile(r"^src/spectrum/session/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/transport/(net|mqtt_min|esp_at)\.h$"),
    ),
    (
        "app-no-overlay",
        re.compile(r"^src/spectrum/app/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/overlay/"),
    ),
    (
        "app-no-transport-internals",
        re.compile(r"^src/spectrum/app/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/transport/(net|mqtt_min|esp_at)\.h$"),
    ),
    (
        "time-owner-no-ui",
        re.compile(r"^src/spectrum/app/time\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/ui/"),
    ),
    (
        "ui-time-read-only",
        re.compile(r"^src/spectrum/ui/.*\.(?:[ch]|inc)$"),
        re.compile(r"^spectrum/app/time\.h$"),
    ),
]

# Reviewed direct dependencies: read-only mode/timing access, link keepalive
# grammar, and the resident input dispatcher. They are not known violations.
INCLUDE_PORTS = {
    ("transport-no-session", "src/spectrum/transport/net.c", "spectrum/session/identity.h"),
    ("transport-no-session", "src/spectrum/transport/net.c", "spectrum/session/timing.h"),
    ("transport-no-game-protocol", "src/spectrum/transport/keepalive_protocol.c", "common/protocol/game_protocol.h"),
    ("overlay-no-app", "src/spectrum/overlay/overlay.c", "spectrum/app/input_cmd.h"),
    ("overlay-no-app", "src/spectrum/overlay/recovery_ovl.c", "spectrum/app/time_view.h"),
}

CONFIG_OPTIONS = frozenset("""notation movement_hints board_theme_index
    piece_set_index mqtt_code direct_host direct_port timezone timezone_last
    player_name_line""".split())
# These share config storage but are runtime/render state, not saved options.
CONFIG_RUNTIME_STATE = frozenset(
    "rtc_available board_light_attr board_dark_attr hinted_rows peer_name".split())
CONFIG_SYMBOLS = CONFIG_OPTIONS | CONFIG_RUNTIME_STATE
CONFIG_ARRAYS = {"mqtt_code", "direct_host", "hinted_rows", "player_name_line",
                 "peer_name"}
CONFIG_WRITERS = {
    "src/spectrum/config/session.c": CONFIG_SYMBOLS,
    # CONFIG rebuilds the low-RAM identity block at every load.
    "src/spectrum/overlay/config_ovl.c": CONFIG_OPTIONS | {"peer_name"},
    # The peer name is owned by the peer lifecycle: accepted MNAME, cleared
    # with the peer.
    "src/spectrum/session/event.c": {"peer_name"},
    "src/spectrum/app/session_ports.h": {"peer_name"},
    "src/spectrum/app/app.c": {"piece_set_index", "timezone", "rtc_available",
                               "hinted_rows", "movement_hints", "peer_name"},
    "src/spectrum/overlay/time_ovl.c": {"timezone", "rtc_available"},
    "src/spectrum/overlay/mqtt_tx_ovl.c": {"timezone", "rtc_available"},
    "asm/overlay/time_config/entry_time_config.asm": {"notation", "movement_hints", "timezone", "timezone_last"},
    "asm/overlay/setup/entry_setup.asm": {"mqtt_code", "direct_host", "direct_port",
                                          "player_name_line"},
    "asm/spectrum/board_theme.inc": {"board_theme_index", "board_light_attr", "board_dark_attr"},
    "asm/next/board_theme.inc": {"board_theme_index", "board_light_attr", "board_dark_attr"},
    "asm/app/rules_hints_coordinator.inc": {"hinted_rows"},
}
# Address-taking also needs review: C array arguments and ASM register aliases
# can hide writes. These existing consumers borrow addresses for reading only.
CONFIG_READ_BORROWERS = {
    "src/spectrum/overlay/direct_ovl.c": {"direct_host"},
    # netchesszx_player_name is the name inside the advisory line.
    "src/spectrum/config/session.h": {"player_name_line", "peer_name"},
    "src/spectrum/session/transitions.inc": {"player_name_line"},
    "src/spectrum/overlay/mqtt_connect_ovl.c": {"mqtt_code"},
    "src/spectrum/overlay/mqtt_tx_ovl.c": {"mqtt_code"},
    "src/spectrum/overlay/status_ovl.c": {"mqtt_code", "direct_host",
                                          "player_name_line", "peer_name"},
    "asm/overlay/input_edit/setup_edit_line.asm": {"mqtt_code", "direct_host"},
    "asm/overlay/menu_config/entry_menu_config.asm": {"direct_host"},
    "asm/overlay/mqtt_connect/entry_mqtt_connect.asm": {"mqtt_code"},
    "asm/spectrum/board_hints_view.inc": {"hinted_rows"},
    "asm/next/board_hints_view.inc": {"hinted_rows"},
    "asm/next/board_hints_next_view.inc": {"hinted_rows"},
}
CONFIG_SYMBOL_RE = re.compile(r"\b_?netchesszx_(" + "|".join(sorted(CONFIG_SYMBOLS)) + r")\b")
CONFIG_ASSIGN_RE = re.compile(
    r"(?:\s*\[[^\]\n]*\])*\s*\)*\s*(?:[+*/%&|^\-]?=(?!=)|<<=|>>=|\+\+|--)"
)
CONFIG_WRITE_CALL_RE = re.compile(
    r"\b(?:memcpy|memmove|memset|strcpy|strncpy|strcat|strncat|sprintf|snprintf)"
    r"\s*\(\s*(?:\([^()]*\)\s*)?[&(]*\s*$"
)

INCLUDE_RE = re.compile(r"^\s*#\s*include\s+[<\"]([^>\"]+)[>\"]")
TRANSPORT_RE = re.compile(r"^src/spectrum/transport/.*\.(?:[ch]|inc)$")
OVERLAY_INCLUDE_RE = re.compile(r"^spectrum/overlay/")
OVERLAY_FACADE_INCLUDES = {
    "spectrum/overlay/overlay.h",
    "spectrum/overlay/overlay_api.h",
    "spectrum/overlay/overlay_context.h",
}
OVERLAY_DISPATCHER_RE = re.compile(
    r"^src/spectrum/(board|transport|fileui|saveload|restore)/.*\.(?:[ch]|inc)$"
)
UI_INCLUDE_RE = re.compile(r"^spectrum/ui/")
UI_SOURCE_RE = re.compile(r"^src/spectrum/ui/.*\.(?:[ch]|inc)$")
UI_BOARD_VIEW_OWNER = "src/spectrum/ui/gui.c"
UI_BOARD_VIEW_TOKEN = "NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR"
TIME_STATE_OWNERS = {"src/spectrum/app/time.c", "src/spectrum/app/time.h"}
TIME_STATE_DECL_RE = re.compile(
    r"^\s*static\s+.*\b(?:clock_frames|move_frame_phase|clock_day_delta|runtime_clock_ready|"
    r"runtime_fat_date|time_state|game_timers)\b"
)
TIME_STATE_ACCESS_RE = re.compile(
    r"\b_?spectrum_time_(?:state|frames|move_frame_phase|clock_frames|day_delta|"
    r"runtime_clock_ready|runtime_fat_date)\b"
)
TIME_MUTATION_RE = re.compile(
    r"\b(?:SPECTRUM_TIME_(?:TICK|SHIFT_CLOCK)\b|"
    r"spectrum_(?:time|app)_(?:set_clock|shift_clock|game_timer_(?:start|stop|restore)|"
    r"move_timer_reset|set_fat_date)\b|spectrum_net_runtime_set_(?:clock|fat_stamp)\b)"
)
OVERLAY_SOURCE_RE = re.compile(r"^src/spectrum/overlay/.*\.(?:[ch]|inc)$")
OVERLAY_UI_INCLUDE_ALLOWLIST = {
    ("src/spectrum/overlay/fileui_ovl.c", "spectrum/ui/layout.h"),
    ("src/spectrum/overlay/gui_log_ovl.c", "spectrum/ui/layout.h"),
    ("src/spectrum/overlay/status_ovl.c", "spectrum/ui/layout.h"),
    ("src/spectrum/overlay/overlay.c", "spectrum/ui/gui.h"),
    ("src/spectrum/overlay/overlay_api.h", "spectrum/ui/info_panel.h"),
}
NET_RUNTIME_BRIDGE_INCLUDE = "spectrum/platform/net_runtime.h"
NET_RUNTIME_BRIDGE_OWNERS = {
    "src/spectrum/transport/esp_at.c",
    "src/spectrum/transport/net.c",
}
LOWRAM_MAP_OWNER = "src/spectrum/lowram_map.h"
PLATFORM_PROTOCOL_OWNER = "src/common/protocol/platform_protocol.h"
OVERLAY_CONTEXT_OWNER = "src/spectrum/overlay/overlay_context.h"
LOWRAM_LITERAL_RE = re.compile(r"\b0x5[0-9A-Fa-f]{3}\b")
C_INT_RE = r"(?:0x[0-9A-Fa-f]+|[0-9]+)[uU]?"
LOWRAM_DEFINE_RE = re.compile(rf"^#define\s+([A-Z0-9_]+)\s+({C_INT_RE})\b")
ASM_EQU_RE = re.compile(rf"^\s*([A-Za-z0-9_]+)\s+EQU\s+({C_INT_RE})\b")
ASM_LOWRAM_EQUS = {
    (
        "asm/overlay/board/entry_board.asm",
        "NETCHESSZX_CHESS_BOARD",
    ): "NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR",
    (
        "asm/app/board_hints_contract.inc",
        "NETCHESSZX_RULES_BOARD_BASE",
    ): "NETCHESSZX_LOWRAM_RULES_BOARD_ADDR",
    (
        "asm/spectrum/screen.asm",
        "NETCHESSZX_OVERLAY_CONTEXT",
    ): "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_BOARD_LO",
    ): "SPECTRUM_OVL_CTX_HINTS_BOARD_LO",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_BOARD_HI",
    ): "SPECTRUM_OVL_CTX_HINTS_BOARD_HI",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_SIDE_TO_MOVE",
    ): "SPECTRUM_OVL_CTX_HINTS_SIDE_TO_MOVE",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_SQUARE",
    ): "SPECTRUM_OVL_CTX_HINTS_SQUARE",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_CASTLE",
    ): "SPECTRUM_OVL_CTX_HINTS_CASTLE",
    (
        "asm/app/board_hints_contract.inc",
        "SPECTRUM_OVL_CTX_HINTS_EP",
    ): "SPECTRUM_OVL_CTX_HINTS_EP",
    (
        "asm/esxdos/overlay_loader.asm",
        "_spectrum_overlay_context",
    ): "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    (
        "asm/esxdos/overlay_loader.asm",
        "_overlay_scratch_base",
    ): "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR",
    (
        "asm/next/overlay_loader_next.asm",
        "_spectrum_overlay_context",
    ): "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    (
        "asm/next/overlay_loader_next.asm",
        "_overlay_scratch_base",
    ): "NETCHESSZX_NEXT_OVERLAY_SCRATCH_ADDR",
    (
        "asm/overlay/setup/entry_setup.asm",
        "CTX",
    ): "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    (
        "asm/overlay/gui_log/entry_gui_log.asm",
        "chat_clock_line_src",
    ): "NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR",
    ("asm/overlay/time_config/entry_time_config.asm", "CTX"):
        "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    ("asm/overlay/menu_config/entry_menu_config.asm", "CTX_KEY"):
        "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    ("asm/next/screen.asm", "NETCHESSZX_OVERLAY_CONTEXT"):
        "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR",
    ("asm/spectrum/timer_roll_layout.inc", "timer_roll_state_addr"):
        "NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_ADDR",
}
BANNED_LINE_RULES = [
    (
        "app-no-overlay-dispatch",
        re.compile(r"^src/spectrum/app/"),
        re.compile(r"\b(?:spectrum_overlay_exec\w*|\w*overlay_context|"
                   r"NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR|SPECTRUM_OVL_\w+)\b"),
    ),
    (
        "transport-session-read-only",
        re.compile(r"^src/spectrum/transport/"),
        re.compile(r"\bnetchesszx_(?:session_\w+|local_(?:is_white|side_\w+)|remote_side_\w+)\b"),
    ),
    (
        "transport-mode-reader",
        re.compile(r"^src/spectrum/transport/(?!net\.c$)"),
        re.compile(r"\bnetchesszx_transport_is_mqtt\b"),
    ),
    (
        "esp-at-no-public-line-buffer-globals",
        re.compile(r"^src/spectrum/transport/esp_at\.h$"),
        re.compile(r"^\s*extern\s+.*\b(line_buf|last_ip|line_pos)\b"),
    ),
    (
        "ui-no-board-symbol",
        re.compile(r"^src/spectrum/ui/.*\.[ch]$"),
        re.compile(r"\bspectrum_board_cells\b"),
    ),
    (
        "app-no-extern-function-declarations",
        re.compile(r"^src/spectrum/app/.*\.[ch]$"),
        re.compile(r"^\s*extern\s+(?!__at\s*\().*\([^;]*\)"),
    ),
]
# Reviewed RTC result transfer, not permission to access the arena elsewhere.
# Match the complete read-only port, allowing whitespace changes without
# accepting writes, pointers, other offsets, or a different consumer.
APP_RTC_RESULT_PORT = "src/spectrum/app/app.c"
APP_RTC_RESULT_READ_RE = re.compile(
    r"\bspectrum_net_runtime_set_clock\s*\(\s*" +
    r"\s*,\s*".join(rf"netchesszx_setup_overlay_context\s*\[\s*{index}[uU]?\s*\]"
                   for index in (2, 1, 0)) + r"\s*\)\s*;"
)
ASM_SDCC_IY_TOKEN_RE = re.compile(r"\b[A-Z0-9_]*SDCC[A-Z0-9_]*IY[A-Z0-9_]*\b")
ASM_SDCC_IY_ALLOWED = {"NETCHESSZX_SDCC_IY"}
OVERLAY_C_RE = re.compile(r"^src/spectrum/overlay/.*\.c$")
EXTERN_VAR_RE = re.compile(
    r"^\s*extern\s+(?!.*\().*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;"
)


def is_fixed_lowram_literal(token: str) -> bool:
    value = int(token, 16)
    return 0x5CB6 <= value <= 0x5FFF


def parse_c_int(token: str) -> int:
    return int(token.rstrip("uU"), 0)


def load_lowram_map(root: Path) -> dict[str, int]:
    constants: dict[str, int] = {}
    for rel_path in (LOWRAM_MAP_OWNER, OVERLAY_CONTEXT_OWNER):
        path = root / rel_path
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = LOWRAM_DEFINE_RE.match(line)
            if match:
                constants[match.group(1)] = parse_c_int(match.group(2))
    return constants


def iter_sources(root: Path) -> list[Path]:
    roots = [root / "src" / "common", root / "src" / "spectrum"]
    out: list[Path] = []
    for base in roots:
        if not base.exists():
            continue
        out.extend(path for path in base.rglob("*") if path.suffix in {".c", ".h", ".inc"})
    return sorted(out)


def iter_asm_sources(root: Path) -> list[Path]:
    base = root / "asm"
    if not base.exists():
        return []
    return sorted(path for path in base.rglob("*") if path.suffix in {".asm", ".inc"})


def load_known(path: Path) -> set[tuple[str, str, str]]:
    if not path.exists():
        return set()
    data = json.loads(path.read_text(encoding="utf-8"))
    known = set()
    for item in data.get("known_violations", []):
        known.add((item["rule"], item["path"].replace("\\", "/"), item["include"]))
    return known


def load_overlay_state_allowlist(path: Path) -> set[tuple[str, str]]:
    if not path.exists():
        return set()
    data = json.loads(path.read_text(encoding="utf-8"))
    allowed = set()
    for item in data.get("allowed_extern_state", []):
        allowed.add((item["path"].replace("\\", "/"), item["symbol"]))
    return allowed


def check_config_state(rel: str, source: str) -> list[tuple[str, str, int, str]]:
    """Freeze writers and address borrowers without adding Spectrum setters.

    This is a lexical guard, like the session owner guard. Reviewed borrowers
    still require code review for indirect writes through their existing aliases.
    """
    violations = []
    writers = CONFIG_WRITERS.get(rel, set())
    borrowers = CONFIG_READ_BORROWERS.get(rel, set())
    is_asm = rel.startswith("asm/")
    if is_asm:
        code = "\n".join(line.split(";", 1)[0] for line in source.splitlines())
    else:
        # Keep positions/newlines stable for diagnostics, ignore comments/literals.
        code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                      lambda match: re.sub(r"[^\n]", " ", match[0]), source, flags=re.S)
    if rel == "src/spectrum/config/session.h":
        exported = set(re.findall(
            r"^\s*extern\s+(?!const\b)[^;\n]*?\bnetchesszx_(\w+)\s*(?:\[[^\]]*\])?\s*;",
            code, re.M))
        for symbol in sorted(exported - CONFIG_SYMBOLS):
            violations.append(("config-state-unregistered", rel, 0, symbol))
    for match in CONFIG_SYMBOL_RE.finditer(code):
        symbol = match[1]
        line_no = code.count("\n", 0, match.start()) + 1
        line = code[code.rfind("\n", 0, match.start()) + 1:].split("\n", 1)[0].strip()
        if re.match(r"extern\b", line, re.I):
            continue
        prefix, tail = code[:match.start()], code[match.end():]
        if is_asm:
            if re.fullmatch(r"ld\s+(?:a|bc|de|hl),\s*\(_netchesszx_\w+(?:\s*\+\s*\d+)?\)", line, re.I):
                continue
            borrow = bool(re.match(r"ld\s+(?:bc|de|hl|ix|iy),\s*_netchesszx_", line, re.I))
            write = not borrow
        else:
            write = bool(CONFIG_ASSIGN_RE.match(tail) or
                         re.search(r"(?:\+\+|--)\s*\(?\s*$", prefix) or
                         CONFIG_WRITE_CALL_RE.search(prefix))
            address = bool(re.search(r"&\s*$", prefix))
            array_decay = (symbol in CONFIG_ARRAYS and
                           not re.match(r"\s*\[", tail) and
                           not re.search(r"\bsizeof\s*\(?\s*$", prefix))
            macro_alias = (bool(re.search(r"#\s*define\s+\w+(?:\([^)]*\))?\s+\(*\s*$", prefix)) and
                           bool(re.match(r"\s*\)*\s*(?:\n|$)", tail)))
            borrow = address or array_decay or macro_alias
        if symbol not in writers and (write or (borrow and symbol not in borrowers)):
            rule = "config-state-writer" if write else "config-state-address"
            violations.append((rule, rel, line_no, "netchesszx_" + symbol))
    return violations


def find_violations(
    root: Path, overlay_state_allowlist: set[tuple[str, str]]
) -> list[tuple[str, str, int, str]]:
    violations: list[tuple[str, str, int, str]] = []
    lowram_constants = load_lowram_map(root)
    seen_asm_lowram: set[tuple[str, str]] = set()
    seen_overlay_state: set[tuple[str, str]] = set()
    for path in iter_sources(root):
        rel = path.relative_to(root).as_posix()
        source = path.read_text(encoding="utf-8", errors="replace")
        violations.extend(check_config_state(rel, source))
        text = source.splitlines()
        app_boundary_text = (APP_RTC_RESULT_READ_RE.sub(
            lambda match: re.sub(r"[^\n]", " ", match[0]), source
        ).splitlines() if rel == APP_RTC_RESULT_PORT else text)
        for line_no, line in enumerate(text, 1):
            if rel != LOWRAM_MAP_OWNER:
                for token in LOWRAM_LITERAL_RE.findall(line):
                    if is_fixed_lowram_literal(token):
                        violations.append(("lowram-map-owner", rel, line_no, token))
            if (
                UI_SOURCE_RE.match(rel)
                and UI_BOARD_VIEW_TOKEN in line
                and rel != UI_BOARD_VIEW_OWNER
            ):
                violations.append(
                    ("ui-board-live-view-owner", rel, line_no, UI_BOARD_VIEW_TOKEN)
                )
            if rel not in TIME_STATE_OWNERS and TIME_STATE_DECL_RE.search(line):
                violations.append(
                    ("time-state-owner", rel, line_no, "<declaration>")
                )
            if rel not in TIME_STATE_OWNERS and TIME_STATE_ACCESS_RE.search(line):
                violations.append(("time-state-owner", rel, line_no, "<private access>"))
            if UI_SOURCE_RE.match(rel) and TIME_MUTATION_RE.search(line):
                violations.append(("ui-time-read-only", rel, line_no, "<time mutation>"))
            for rule_id, banned_path_re, banned_re in BANNED_LINE_RULES:
                checked_line = (app_boundary_text[line_no - 1]
                                if rule_id == "app-no-overlay-dispatch" else line)
                if banned_path_re.match(rel) and banned_re.search(checked_line):
                    violations.append((rule_id, rel, line_no, "<declaration>"))
            extern_var = EXTERN_VAR_RE.match(line)
            if extern_var and OVERLAY_C_RE.match(rel):
                symbol = extern_var.group(1)
                key = (rel, symbol)
                seen_overlay_state.add(key)
                if key not in overlay_state_allowlist:
                    violations.append(
                        ("overlay-extern-state-allowlist", rel, line_no, symbol)
                    )
            match = INCLUDE_RE.match(line)
            if not match:
                continue
            include = match.group(1)
            include_parts = include.replace("\\", "/").split("/")
            if any(part in {".", ".."} for part in include_parts):
                violations.append(
                    ("noncanonical-relative-include", rel, line_no, include)
                )
            if (
                OVERLAY_DISPATCHER_RE.match(rel)
                and OVERLAY_INCLUDE_RE.match(include)
                and include not in OVERLAY_FACADE_INCLUDES
            ):
                violations.append(
                    ("overlay-dispatcher-facade-only", rel, line_no, include)
                )
            if (
                OVERLAY_SOURCE_RE.match(rel)
                and UI_INCLUDE_RE.match(include)
                and (rel, include) not in OVERLAY_UI_INCLUDE_ALLOWLIST
            ):
                violations.append(
                    ("overlay-ui-include-allowlist", rel, line_no, include)
                )
            if (
                TRANSPORT_RE.match(rel)
                and include == NET_RUNTIME_BRIDGE_INCLUDE
                and rel not in NET_RUNTIME_BRIDGE_OWNERS
            ):
                violations.append(
                    ("transport-net-runtime-bridge-only", rel, line_no, include)
                )
            for rule_id, path_re, include_re in RULES:
                if (path_re.match(rel) and include_re.match(include)
                        and (rule_id, rel, include) not in INCLUDE_PORTS):
                    violations.append((rule_id, rel, line_no, include))
    for path in iter_asm_sources(root):
        rel = path.relative_to(root).as_posix()
        source = path.read_text(encoding="utf-8", errors="replace")
        violations.extend(check_config_state(rel, source))
        text = source.splitlines()
        for line_no, line in enumerate(text, 1):
            if TIME_STATE_ACCESS_RE.search(line.split(";", 1)[0]):
                violations.append(("time-state-owner", rel, line_no, "<private access>"))
            equ_match = ASM_EQU_RE.match(line)
            if equ_match:
                equ_name = equ_match.group(1)
                key = (rel, equ_name)
                expected_name = ASM_LOWRAM_EQUS.get(key)
                if expected_name is not None:
                    seen_asm_lowram.add(key)
                    expected = lowram_constants.get(expected_name)
                    actual = parse_c_int(equ_match.group(2))
                    if expected is None or actual != expected:
                        violations.append(
                            (
                                "asm-lowram-map-match",
                                rel,
                                line_no,
                                f"{equ_name}={equ_match.group(2)}",
                            )
                        )
            for token in ASM_SDCC_IY_TOKEN_RE.findall(line):
                if token not in ASM_SDCC_IY_ALLOWED:
                    violations.append(("asm-sdcc-iy-macro-name", rel, line_no, token))
    for key, expected_name in ASM_LOWRAM_EQUS.items():
        if key not in seen_asm_lowram:
            violations.append(
                (
                    "asm-lowram-map-match",
                    key[0],
                    0,
                    f"{key[1]} missing for {expected_name}",
                )
            )
    # The roller buffer has distinct Classic/Next addresses. Reuse the linked
    # layout checker's conditional reader so both branches are checked here.
    timer_layout = "asm/spectrum/timer_roll_layout.inc"
    for target, defines in (("classic", frozenset()),
                            ("next", frozenset({"NETCHESSZX_NEXT_BANKING"}))):
        if not (root / timer_layout).exists():
            continue  # The required state EQU above reports a missing file.
        try:
            expected = parse_const(root / LOWRAM_MAP_OWNER,
                                   "NETCHESSZX_LOWRAM_TIMER_ROLL_ADDR", defines)
            actual = parse_const(root / timer_layout, "timer_roll_buffer", defines)
            if actual == expected:
                continue
        except (SystemExit, OSError):
            pass
        violations.append(("asm-lowram-map-match", timer_layout, 0,
                           f"timer_roll_buffer ({target})"))
    # Player identity also has distinct Classic/Next addresses; the peer name
    # follows the 17-byte advisory line in both maps.
    name_layout = "asm/app/player_name_layout.inc"
    for target, defines in (("classic", frozenset()),
                            ("next", frozenset({"NETCHESSZX_NEXT_BANKING"}))):
        try:
            expected = parse_const(root / LOWRAM_MAP_OWNER,
                                   "NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR",
                                   defines)
            actual = parse_const(root / name_layout, "PLAYER_NAME_LINE", defines)
            if actual == expected:
                continue
        except (SystemExit, OSError):
            pass
        violations.append(("asm-lowram-map-match", name_layout, 0,
                           f"PLAYER_NAME_LINE ({target})"))
    # The MNAME protocol fixes the rest of the identity shape: the local name
    # follows the "MNAME " prefix, holds the protocol maximum plus NUL, and the
    # peer name follows the whole advisory line. A capacity that drifts from
    # the protocol lets Setup or CONTROL write past the name it owns.
    try:
        prefix_len = parse_const(root / PLATFORM_PROTOCOL_OWNER,
                                 "NETCHESS_PROTO_NAME_PREFIX_LEN")
        name_max = parse_const(root / PLATFORM_PROTOCOL_OWNER,
                               "NETCHESS_PLAYER_NAME_MAX")
        line_size = parse_const(root / LOWRAM_MAP_OWNER,
                                "NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_SIZE")
        peer_size = parse_const(root / LOWRAM_MAP_OWNER,
                                "NETCHESSZX_LOWRAM_PEER_NAME_SIZE")
        name_text = (root / name_layout).read_text(encoding="utf-8")
    except (SystemExit, OSError):
        violations.append(("asm-lowram-map-match", name_layout, 0,
                           "player identity layout unreadable"))
    else:
        if (line_size != prefix_len + name_max + 1 or
                peer_size != name_max + 1):
            violations.append(("asm-lowram-map-match", LOWRAM_MAP_OWNER, 0,
                               "player identity sizes differ from MNAME"))
        for equ, value in (
                ("PEER_NAME", r"PLAYER_NAME_LINE\s*\+\s*%d" % line_size),
                ("PLAYER_NAME", r"PLAYER_NAME_LINE\s*\+\s*%d" % prefix_len),
                ("PLAYER_NAME_MAX", r"%d" % name_max)):
            if re.search(r"^%s\s+EQU\s+%s\s*$" % (equ, value),
                         name_text, re.M) is None:
                violations.append(("asm-lowram-map-match", name_layout, 0,
                                   f"{equ} differs from the MNAME layout"))
    for rel, symbol in sorted(overlay_state_allowlist - seen_overlay_state):
        violations.append(
            ("overlay-extern-state-stale", rel, 0, f"{symbol} not declared")
        )
    return violations


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOWLIST)
    parser.add_argument(
        "--overlay-state-allowlist",
        type=Path,
        default=DEFAULT_OVERLAY_STATE_ALLOWLIST,
    )
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--fail-known", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    known = load_known(args.allowlist)
    overlay_state_allowlist = load_overlay_state_allowlist(args.overlay_state_allowlist)
    unexpected = []
    known_hits = []

    for rule_id, rel, line_no, include in find_violations(
        root, overlay_state_allowlist
    ):
        key = (rule_id, rel, include)
        if key in known:
            known_hits.append((rule_id, rel, line_no, include))
        else:
            unexpected.append((rule_id, rel, line_no, include))

    if args.report:
        for rule_id, rel, line_no, include in known_hits:
            print(f"[DEBT] {rule_id}: {rel}:{line_no} includes {include}")
        for rule_id, rel, line_no, include in unexpected:
            print(f"[ERR] {rule_id}: {rel}:{line_no} includes {include}")

    if unexpected or (args.fail_known and known_hits):
        for rule_id, rel, line_no, include in unexpected:
            print(
                f"[ERR] {rule_id}: {rel}:{line_no} includes {include}", file=sys.stderr
            )
        if args.fail_known:
            for rule_id, rel, line_no, include in known_hits:
                print(
                    f"[ERR] known debt still present: {rule_id}: {rel}:{line_no} includes {include}",
                    file=sys.stderr,
                )
        return 1

    print(
        f"[OK] layering check: no unexpected violations ({len(known_hits)} known debt)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
