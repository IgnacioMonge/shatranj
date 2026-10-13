#!/usr/bin/env python3
"""Guard target-neutral MQTT expectations and session runtime ownership."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inc"}
SESSION_STEP = re.compile(r"\bsession_step\s*\(")
SESSION_STEP_ALLOWLIST = {
    "src/common/session/session.c": 1,
    "src/common/session/session.h": 1,
    "src/pc/client/desktop_session_adapter.cpp": 1,
}
PC_MACH_PARSE = re.compile(r"\bnetchess_proto_parse_mach\s*\(")
PC_CLIENT_ROOT = "src/pc/client/"
CORPUS_HEADER = "tests/session/mqtt_session_transcripts.h"
CORPUS_SOURCE = "tests/session/mqtt_session_transcripts.c"
JUDGE_SOURCE = "tests/session/test_mqtt_session_parity.c"
DIRECT_CORPUS_HEADER = "tests/session/direct_parity.h"
DIRECT_JUDGE_SOURCE = "tests/session/test_direct_session_parity.c"
BUILD_FILE = "Makefile"
MQTT_WIRE_SOURCE = "src/spectrum/session/mqtt_wire.c"
MQTT_WIRE_BUILDERS = (
    "spectrum_net_mqtt_out_suffix",
    "spectrum_net_mqtt_in_suffix",
    "spectrum_net_mqtt_out_ack_suffix",
    "spectrum_net_mqtt_in_ack_suffix",
    "spectrum_net_mqtt_presence_suffix",
    "spectrum_net_mqtt_peer_presence_suffix",
    "spectrum_net_mqtt_presence_payload",
    "spectrum_net_mqtt_setup_payload",
    "netchesszx_session_mqtt_publish_offline",
)
MQTT_ISOLATION_FILES = (
    "src/spectrum/overlay/overlay_context.h",
    "src/spectrum/overlay/mqtt_tx_ovl.c",
    "src/spectrum/overlay/mqtt_connect_ovl.c",
    "src/spectrum/transport/mqtt_publish.c",
    "src/spectrum/transport/net.c",
    "asm/overlay/mqtt_connect/entry_mqtt_connect.asm",
    "asm/spectrum/shrink_kernels.asm",
    "src/spectrum/session/mqtt_connect_actions_ovl.c",
    "src/spectrum/session/mqtt_tx_actions_ovl.c",
)
TARGET_POLICY_IDENTIFIER = re.compile(
    r"\b(?:[A-Za-z0-9_]*(?:spectrum|zx|next|pc|canonical|reference)"
    r"[A-Za-z0-9_]*(?:expected|expectation|flag|skip|only|deviation)"
    r"[A-Za-z0-9_]*|"
    r"[A-Za-z0-9_]*(?:expected|expectation|flag|skip|only|deviation)"
    r"[A-Za-z0-9_]*(?:spectrum|zx|next|pc|canonical|reference)"
    r"[A-Za-z0-9_]*)\b",
    re.IGNORECASE,
)
JUDGE_TARGET_EXPECTATION = re.compile(
    r"\b(?:[A-Za-z0-9_]*(?:spectrum|zx|next|pc|canonical|reference)"
    r"[A-Za-z0-9_]*(?:expected|expectation|skip|only|deviation)"
    r"[A-Za-z0-9_]*|"
    r"[A-Za-z0-9_]*(?:expected|expectation|skip|only|deviation)"
    r"[A-Za-z0-9_]*(?:spectrum|zx|next|pc|canonical|reference)"
    r"[A-Za-z0-9_]*)\b",
    re.IGNORECASE,
)
TARGET_CONDITIONAL = re.compile(
    r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b[^\n]*"
    r"(?:SPECTRUM|NETCHESSZX_(?:NEXT|PC)|__SDCC|__Z88DK|_WIN32)",
    re.IGNORECASE | re.MULTILINE,
)
SHARED_RUNNERS = re.compile(
    r"static\s+void\s+run_shared_transcript\s*\(\s*"
    r"const\s+MqttTranscript\s*\*\s*transcript\s*\)\s*\{\s*"
    r"mqtt_reference_run\s*\(\s*transcript\s*\)\s*;\s*"
    r"mqtt_spectrum_run\s*\(\s*transcript\s*\)\s*;\s*\}",
    re.DOTALL,
)
MQTT_PARITY_SOURCES = re.compile(
    r"SESSION_MQTT_PARITY_TEST_SRC\s*:=(.*?)"
    r"^SESSION_DIRECT_PARITY_TEST\s*:=",
    re.DOTALL | re.MULTILINE,
)


def check_session_step_owners(sources: dict[str, str]) -> list[str]:
    counts = {
        path: len(SESSION_STEP.findall(source))
        for path, source in sources.items()
        if SESSION_STEP.search(source)
    }
    errors: list[str] = []

    for path in sorted(set(counts) | set(SESSION_STEP_ALLOWLIST)):
        actual = counts.get(path, 0)
        expected = SESSION_STEP_ALLOWLIST.get(path, 0)
        if actual != expected:
            errors.append(
                f"{path}: session_step() count {actual}; expected {expected}"
            )
    return errors


def check_pc_mach_parse_boundaries(sources: dict[str, str]) -> list[str]:
    """Keep MACH wire parsing in the session reducer, not the PC UI seam."""
    return [
        f"{path}: UI/adapters must consume typed platform actions"
        for path, source in sorted(sources.items())
        if path.startswith(PC_CLIENT_ROOT) and PC_MACH_PARSE.search(source)
    ]


def check_mqtt_judge(header: str, corpus: str, judge: str) -> list[str]:
    errors: list[str] = []

    for path, source in ((CORPUS_HEADER, header), (CORPUS_SOURCE, corpus)):
        match = TARGET_POLICY_IDENTIFIER.search(source)
        if match:
            errors.append(
                f"{path}: target-specific shared-corpus policy "
                f"({match.group(0)!r})"
            )
        conditional = TARGET_CONDITIONAL.search(source)
        if conditional:
            errors.append(
                f"{path}: target-specific shared-expected conditional "
                f"({conditional.group(0)!r})"
            )
    judge_match = JUDGE_TARGET_EXPECTATION.search(judge)
    if judge_match:
        errors.append(
            f"{JUDGE_SOURCE}: target-specific expected path "
            f"({judge_match.group(0)!r})"
        )
    judge_conditional = TARGET_CONDITIONAL.search(judge)
    if judge_conditional:
        errors.append(
            f"{JUDGE_SOURCE}: target-specific shared-expected conditional "
            f"({judge_conditional.group(0)!r})"
        )
    if header.count(
        "MqttTranscriptObservation expected[MQTT_TRANSCRIPT_OBSERVATIONS_MAX];"
    ) != 1 or header.count("uint8_t expected_count;") != 1:
        errors.append(
            f"{CORPUS_HEADER}: expected must be one target-neutral field/count"
        )
    if not SHARED_RUNNERS.search(judge):
        errors.append(
            f"{JUDGE_SOURCE}: canonical and Spectrum runners must receive "
            "the same transcript"
        )
    if judge.count("observations_match(observations, action_count, step)") != 1:
        errors.append(f"{JUDGE_SOURCE}: canonical runner bypasses shared expected")
    if judge.count("observations_match(actual, actual_count, step)") != 1:
        errors.append(f"{JUDGE_SOURCE}: Spectrum runner bypasses shared expected")
    return errors


def check_direct_judge(header: str, judge: str) -> list[str]:
    errors: list[str] = []

    match = TARGET_POLICY_IDENTIFIER.search(header)
    if match:
        errors.append(
            f"{DIRECT_CORPUS_HEADER}: target-specific shared-corpus policy "
            f"({match.group(0)!r})"
        )
    conditional = TARGET_CONDITIONAL.search(header)
    if conditional:
        errors.append(
            f"{DIRECT_CORPUS_HEADER}: target-specific shared-expected conditional "
            f"({conditional.group(0)!r})"
        )
    judge_match = JUDGE_TARGET_EXPECTATION.search(judge)
    if judge_match:
        errors.append(
            f"{DIRECT_JUDGE_SOURCE}: target-specific expected path "
            f"({judge_match.group(0)!r})"
        )
    if header.count("const DirectParityObservation *expected;") != 1:
        errors.append(
            f"{DIRECT_CORPUS_HEADER}: expected must be one target-neutral field"
        )
    shared_calls = (
        "direct_reference_run(scenario, &reference)",
        "check_trace(scenario, &reference, \"reference\")",
        "direct_spectrum_run(scenario, &spectrum)",
        "check_trace(scenario, &spectrum, \"Spectrum\")",
    )
    for call in shared_calls:
        if judge.count(call) != 1:
            errors.append(
                f"{DIRECT_JUDGE_SOURCE}: shared scenario bypass at {call}"
            )
    return errors


def check_mqtt_wire_authority(build: str, judge: str) -> list[str]:
    errors: list[str] = []
    source_block = MQTT_PARITY_SOURCES.search(build)

    if source_block is None or MQTT_WIRE_SOURCE not in source_block.group(1):
        errors.append(
            f"{BUILD_FILE}: MQTT parity must compile {MQTT_WIRE_SOURCE}"
        )
    for name in MQTT_WIRE_BUILDERS:
        definition = re.compile(
            rf"^\s*(?:static\s+)?"
            rf"(?:const\s+char\s*\*|char\s+const\s*\*|void|uint8_t)"
            rf"\s*{name}\s*\(",
            re.MULTILINE,
        )
        if definition.search(judge):
            errors.append(
                f"{JUDGE_SOURCE}: substitutes production MQTT builder {name}"
            )
    return errors


def check_mqtt_transport_isolation(files: dict[str, str]) -> list[str]:
    errors: list[str] = []
    tx = files[MQTT_ISOLATION_FILES[1]]
    connect = files[MQTT_ISOLATION_FILES[2]]
    context = files[MQTT_ISOLATION_FILES[0]]
    producer = files[MQTT_ISOLATION_FILES[3]]
    net = files[MQTT_ISOLATION_FILES[4]]
    connect_asm = files[MQTT_ISOLATION_FILES[5]]
    shrink = files[MQTT_ISOLATION_FILES[6]]
    connect_actions = files[MQTT_ISOLATION_FILES[7]]
    tx_actions = files[MQTT_ISOLATION_FILES[8]]

    forbidden = (
        "NETCHESS_PROTO_",
        "netchesszx_session_is_host",
        "netchesszx_host_color_ready",
        '"ACK ',
        '"NACK ',
        '"GAME START',
        '"RESET"',
        '"DRAW"',
        '"RESIGN"',
        '"BYE"',
        '"MOVE ',
        '"TAKEBACK ',
    )
    for path, source in (
        (MQTT_ISOLATION_FILES[1], tx),
        (MQTT_ISOLATION_FILES[2], connect),
    ):
        for token in forbidden:
            if token in source:
                errors.append(f"{path}: MQTT transport decides session token {token}")

    for path, source in (
        (MQTT_ISOLATION_FILES[7], connect_actions),
        (MQTT_ISOLATION_FILES[8], tx_actions),
    ):
        if "spectrum_overlay_exec" in source:
            errors.append(f"{path}: session action nests an overlay call")

    required = (
        (context, "NETCHESSZX_MQTT_TX_CTX_FLAGS", MQTT_ISOLATION_FILES[0]),
        (producer, "NETCHESSZX_MQTT_TX_CTX_PAYLOAD_LO", MQTT_ISOLATION_FILES[3]),
        (tx, "NETCHESSZX_MQTT_TX_CTX_FLAGS", MQTT_ISOLATION_FILES[1]),
        (net, "NETCHESSZX_MQTT_CONNECT_CTX_FLAGS", MQTT_ISOLATION_FILES[4]),
        (connect_asm, "_spectrum_overlay_context + mqtt_connect_flags",
         MQTT_ISOLATION_FILES[5]),
        (shrink, "_netchesszx_session_send_ack_text", MQTT_ISOLATION_FILES[6]),
        (connect_actions, "mqtt_connect_transport_start_ovl",
         MQTT_ISOLATION_FILES[7]),
        (connect_actions, "mqtt_connect_transport_publish_ovl",
         MQTT_ISOLATION_FILES[7]),
        (tx_actions, "mqtt_tx_publish_flags_ovl", MQTT_ISOLATION_FILES[8]),
        (tx_actions, "NETCHESSZX_SESSION_MQTT_TX_CTX_FLAGS",
         MQTT_ISOLATION_FILES[8]),
    )
    for source, token, path in required:
        if token not in source:
            errors.append(f"{path}: missing MQTT boundary token {token}")
    if "_spectrum_net_send_text" in shrink:
        errors.append(
            f"{MQTT_ISOLATION_FILES[6]}: ACK/NACK bypasses the session sender"
        )
    return errors


SESSION_STORAGE = re.compile(
    r"\b_?(?:netchesszx_(?:session_role|transport|local_color|host_color|"
    r"host_color_ready|mqtt_session_id|session_peer_ready_state|session_state))\b"
)
SESSION_MACRO_START = re.compile(r"\s*#define\s+(SESSION_[A-Z0-9_]+)")
SESSION_PORT_SOURCE = "src/spectrum/app/session_ports.h"
SESSION_PORT_PRIVATE_REFS = {
    "SESSION_APP_CONFIRMATION": frozenset(("confirm_action",)),
    "SESSION_APP_FRAME_COUNT": frozenset(),
    "SESSION_APP_CONFIRM": frozenset(("confirm_action",)),
    "SESSION_APP_CLEAR_CONFIRMATION": frozenset(("confirm_action",)),
    "SESSION_APP_HAS_LOCAL_TURN": frozenset(("local_turn",)),
    "SESSION_APP_TAKEBACK_IS_LOCAL": frozenset(("takeback_snapshot_local",)),
    "SESSION_APP_TAKEBACK_PLY": frozenset(("takeback_snapshot_ply",)),
    "SESSION_APP_RESTORE_CHUNK": frozenset(("restore_b64_pending",)),
    "SESSION_APP_RESTORE_DECODE_VALID": frozenset((
        "restore_b64_pending", "restore_host_color_ok", "restore_snapshot",
        "spectrum_restore_decode",
    )),
    "SESSION_APP_RESTORE_APPLY": frozenset((
        "saveload_apply_snapshot", "spectrum_gui_is_board_flipped",
    )),
    "SESSION_APP_RESTORE_CACHE": frozenset((
        "restore_b64_pending", "restore_snapshot", "spectrum_restore_build_b64",
    )),
    "SESSION_APP_RESTORE_PREPARE_FRAME": frozenset((
        "local_input", "restore_b64_pending",
    )),
    "SESSION_APP_RESTORE_FRAME": frozenset(("local_input",)),
    "SESSION_APP_APPLY_LOADED": frozenset(("saveload_apply_snapshot",)),
    "SESSION_APP_BUILD_LOADED": frozenset((
        "restore_b64_pending", "restore_snapshot", "spectrum_restore_build_b64",
    )),
    "SESSION_APP_BEGIN_SETUP_EXIT": frozenset(("setup_restart_requested",)),
    "SESSION_APP_FINISH_SETUP_EXIT": frozenset((
        "clear_disconnected_session_state", "edit_stop_clear",
        "spectrum_gui_set_connected",
    )),
    "SESSION_APP_DISCONNECT": frozenset(("handle_opponent_disconnected",)),
    "SESSION_APP_DISCONNECT_WITH": frozenset((
        "handle_opponent_disconnected_with",
    )),
    "SESSION_APP_MQTT_PEER_LOST": frozenset(("mqtt_peer_disconnected_wait",)),
    "SESSION_APP_SHOW_READY": frozenset((
        "cursor_show", "ready_notice", "spectrum_gui_set_connected",
        "status_show_endpoint",
    )),
    "SESSION_APP_SHOW_PEER_MACHINE": frozenset((
        "status_phase_current", "status_show_phase",
    )),
    "SESSION_APP_SHOW_PEER_NAME": frozenset(("spectrum_gui_moves_header",)),
    "SESSION_APP_DIRECT_READY": frozenset((
        "ready_notice", "spectrum_gui_moves_header",
        "spectrum_gui_set_board_view", "status_show_endpoint",
    )),
    "SESSION_APP_PREPARE_ROOM_CONFLICT": frozenset(("cursor_hide", "local_turn")),
    "SESSION_APP_SHOW_ROOM_CONFLICT": frozenset((
        "spectrum_gui_set_connected", "status_show_endpoint",
    )),
    "SESSION_APP_REFRESH_SIDE": frozenset((
        "spectrum_gui_moves_header", "spectrum_gui_redraw_board_view",
        "spectrum_gui_set_board_view",
    )),
    "SESSION_APP_WAITING_OPPONENT": frozenset((
        "spectrum_gui_set_connected", "status_show_endpoint",
    )),
    "SESSION_APP_HOST_BUSY_EXIT": frozenset((
        "handle_opponent_disconnected_with", "setup_restart_requested",
        "wait_after_notice",
    )),
    "SESSION_APP_NOTICE_ERROR": frozenset(("notify_error",)),
    "SESSION_APP_NOTICE_INFO": frozenset(("notify_info",)),
    "SESSION_APP_NOTICE_WAIT": frozenset(("notify_wait_opponent",)),
    "SESSION_APP_NOTICE_CONTROL_TIMEOUT": frozenset(("notify_control_timeout",)),
    "SESSION_APP_REFRESH_STATUS": frozenset(("status_refresh_game",)),
    "SESSION_APP_SHOW_ENDPOINT": frozenset(("status_show_endpoint",)),
    "SESSION_APP_SHOW_PERSISTENT": frozenset(("spectrum_gui_notify_persistent",)),
    "SESSION_APP_HIDE_MENU": frozenset(("spectrum_gui_hide_menu",)),
    "SESSION_APP_SHOW_CURSOR": frozenset(("cursor_show",)),
    "SESSION_APP_HIDE_CURSOR": frozenset(("cursor_hide",)),
    "SESSION_APP_SUPPRESS_KEY": frozenset(("suppress_current_key",)),
    "SESSION_APP_SUPPRESS_KEY_UNTIL_RELEASE": frozenset((
        "suppress_key_until_release",
    )),
    "SESSION_APP_CLEAR_NOTICE": frozenset(("notify_info",)),
    "SESSION_APP_START_GAME": frozenset(("game_start_state",)),
    "SESSION_APP_RESET_START": frozenset(("reset_auto_start",)),
    "SESSION_APP_END_GAME": frozenset(("end_game_over",)),
    "SESSION_APP_END_DRAW": frozenset(("end_game_over",)),
    "SESSION_APP_END_OPPONENT_RESIGN": frozenset(("end_game_over",)),
    "SESSION_APP_END_LOCAL_RESIGN": frozenset(("end_game_over",)),
    "SESSION_APP_APPLY_TAKEBACK": frozenset(("apply_takeback_snapshot",)),
    "SESSION_APP_APPLY_PENDING_MOVE": frozenset(("apply_pending_local_move",)),
    "SESSION_APP_CLEAR_PENDING_MOVE": frozenset(("pending_local_clear",)),
    "SESSION_APP_REJECT_MOVE": frozenset(("notify_move_rejected",)),
    "SESSION_APP_WAIT_FOR_ACK": frozenset(("notify_wait_opponent_ack",)),
    "SESSION_APP_SEND_BUSY_MOVE_REPLY": frozenset(("send_busy_move_reply",)),
    "SESSION_APP_MOVE_IS_LEGAL": frozenset(("spectrum_board_is_legal_move",)),
    "SESSION_APP_SHOW_LOCAL_MOVE": frozenset((
        "move_san_or_fallback", "spectrum_gui_notify_persistent",
    )),
    "SESSION_APP_CLEAR_MOVE_HINTS": frozenset(("movement_hints_clear",)),
    "SESSION_APP_SHOW_LOCAL_RESIGN": frozenset(("spectrum_gui_notify_persistent",)),
    "SESSION_APP_SHOW_WAITING_RESIGN_ACK": frozenset((
        "spectrum_gui_notify_persistent",
    )),
    "SESSION_APP_NOTICE_LOAD_CANCELLED": frozenset(("notify_info",)),
    "SESSION_APP_STOP_INPUT_SHOW_CURSOR": frozenset((
        "input_stop_and_show_cursor",
    )),
    "SESSION_APP_STOP_EDIT_CLEAR": frozenset(("edit_stop_clear",)),
    "SESSION_APP_APPLY_REMOTE_MOVE": frozenset((
        "move_display_text", "move_san_prepare",
        "notify_move_rejected", "spectrum_board_apply_trusted_move_with_undo",
        "spectrum_board_cells", "spectrum_gui_prepare_move",
        "spectrum_gui_set_board_snapshot", "takeback_snapshot_save",
        "spectrum_stats_undo",
    )),
    "SESSION_APP_FINISH_REMOTE_MOVE": frozenset(("finish_applied_move",)),
}
LEGACY_PENDING_STORAGE = re.compile(
    r"\bstatic\s+(?:uint8_t|uint16_t|char)\s+(?:game_ply|pending_local_ply|"
    r"pending_local_move|takeback_pending_ply|last_accepted_takeback_ply|"
    r"last_control_accept|resign_pending|restore_rx_mask|game_status_active|game_over|start_pending|"
    r"control_pending|mqtt_seat_probed|reset_ack_replay_polls)\b"
)
SCHEDULE_FIELDS = (
    r"(?:mqtt_setup_reannounce_wait|direct_hello_reannounce_wait|"
    r"pending_retry_period|pending_retry_wait|control_retry_count|"
    r"control_cancel_period|control_cancel_wait|decision_kind)"
)


def session_macro_bodies(source: str) -> list[tuple[str, str]]:
    """Collect complete SESSION_* definitions, including continued lines."""
    macros: list[tuple[str, str]] = []
    lines = source.splitlines()
    index = 0
    while index < len(lines):
        match = SESSION_MACRO_START.match(lines[index])
        if match is None:
            index += 1
            continue
        body = [lines[index]]
        while body[-1].rstrip().endswith("\\") and index + 1 < len(lines):
            index += 1
            body.append(lines[index])
        macros.append((match.group(1), "\n".join(body)))
        index += 1
    return macros


def check_session_storage_owners(sources: dict[str, str]) -> list[str]:
    """C consumers use views/operations; ASM consumers may only load values."""
    errors: list[str] = []
    for path, source in sources.items():
        if (path.startswith("asm/") and
                path != "asm/overlay/time_config/entry_time_config.asm" and
                re.search(r'^\s*INCLUDE\s+[^\n]*identity_setup\.inc', source,
                          re.IGNORECASE | re.MULTILINE)):
            errors.append(f"{path}: identity bootstrap belongs only to TIME_CONFIG")
        if path.startswith("src/spectrum/session/") or path == "asm/session/identity_setup.inc":
            continue
        if path.startswith("asm/"):
            for line in source.splitlines():
                code = line.split(";", 1)[0].strip()
                if SESSION_STORAGE.search(code) and not re.fullmatch(
                    r"(?:EXTERN\s+\w+|ld\s+(?:a|bc|de|hl),\s*\(_\w+\))",
                    code, re.IGNORECASE,
                ):
                    errors.append(f"{path}: session storage is read-only in ASM")
        else:
            # A raw symbol is forbidden even through an alias, memset, or ++.
            code = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.DOTALL)
            if SESSION_STORAGE.search(code) or LEGACY_PENDING_STORAGE.search(code):
                errors.append(f"{path}: use session views/operations, not storage")
            without_views = re.sub(
                r"netchesszx_session_schedule_view\([^()\n]*\)->" + SCHEDULE_FIELDS,
                "", code,
            )
            if re.search(r"(?:->|\.)\s*" + SCHEDULE_FIELDS + r"\b", without_views):
                errors.append(f"{path}: use session schedule views/operations")
    return errors


def check_compact_transition_owners(sources: dict[str, str]) -> list[str]:
    """Same-TU compilation must not hide application dependencies in the FSM."""
    errors: list[str] = []
    clean_sources = {
        path: re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                     "", source, flags=re.DOTALL)
        for path, source in sources.items()
    }
    app_functions = set()
    for path, code in clean_sources.items():
        # Public UX operations remain application effects to session policy.
        if path.startswith(("src/spectrum/app/", "src/spectrum/ui/")):
            app_functions.update(re.findall(
                r"^\s*(?:static\s+)?(?:const\s+)?\w+\s+\*?\s*(\w+)\s*"
                r"\([^;{}]*\)[^;{}]*\{", code, flags=re.MULTILINE,
            ))
    private_app = re.compile(
        r"\b(?:confirm_action|local_turn|takeback_snapshot_\w+|spectrum_stats_undo|"
        r"restore_b64_pending|restore_snapshot|app_workspace|status_phase_current|"
        r"setup_restart_requested|local_input|cursor_row|cursor_col|"
        r"spectrum_(?:gui|board|restore|saveload|fileui|overlay)_\w+)\b"
    )
    wire_event = re.compile(
        r"\bNETCHESSZX_SESSION_EVENT_(?:ACK_\w+|NACK_\w+|RESTORE_\w+|"
        r"DIRECT_HELLO|MQTT_\w+|BYE|RESET|DRAW|RESIGN|GAME_START|"
        r"TAKEBACK|MOVE|CANCEL_\w+)\b"
    )
    pending_writer = re.compile(
        r"\bnetchesszx_session_set_(?:start_pending|control_pending|resign_pending|"
        r"restore_rx_mask|game_ply|pending_local_ply|takeback_pending_ply|"
        r"last_control_accept|last_accepted_takeback_ply|reset_ack_replay_polls)\b"
    )
    port_source = sources.get(SESSION_PORT_SOURCE, "")
    for name, body in session_macro_bodies(port_source):
        if not name.startswith("SESSION_APP_"):
            continue
        body = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                      "", body, flags=re.DOTALL)
        refs = set(private_app.findall(body))
        refs.update(
            function for function in app_functions
            if re.search(rf"\b{re.escape(function)}\s*\(", body)
        )
        expected = SESSION_PORT_PRIVATE_REFS.get(name, frozenset())
        if refs != expected:
            errors.append(
                f"{SESSION_PORT_SOURCE}: {name} private refs "
                f"{','.join(sorted(refs)) or '-'}; expected "
                f"{','.join(sorted(expected)) or '-'}"
            )
    for path, source in sources.items():
        for name, body in session_macro_bodies(source):
            body = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                          "", body, flags=re.DOTALL)
            if re.search(r"\breturn\b", body):
                errors.append(
                    f"{path}: {name} hides its caller return"
                )
    for path, code in clean_sources.items():
        if path.startswith("src/spectrum/session/"):
            match = private_app.search(code)
            if match:
                errors.append(f"{path}: FSM must use semantic facts/effects, not {match[0]}")
            for name in sorted(app_functions):
                if re.search(rf"\b{re.escape(name)}\s*\(", code):
                    errors.append(f"{path}: implicit application call {name}; use an explicit effect port")
        if path.startswith("src/spectrum/app/"):
            match = pending_writer.search(code)
            if match:
                errors.append(f"{path}: pending-operation policy belongs in session/: {match[0]}")
            match = wire_event.search(code)
            if match:
                errors.append(f"{path}: session event policy belongs in session/: {match[0]}")
    return errors


def check_tree(root: Path) -> list[str]:
    source_root = root / "src"
    if not source_root.is_dir():
        return ["src: missing production source directory"]

    sources = {
        path.relative_to(root).as_posix(): path.read_text(encoding="utf-8")
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
    }
    required_names = (
        CORPUS_HEADER,
        CORPUS_SOURCE,
        JUDGE_SOURCE,
        DIRECT_CORPUS_HEADER,
        DIRECT_JUDGE_SOURCE,
        BUILD_FILE,
        MQTT_WIRE_SOURCE,
        *MQTT_ISOLATION_FILES,
    )
    required = [root / path for path in required_names]
    missing = [path.relative_to(root).as_posix() for path in required if not path.is_file()]
    if missing:
        return [f"{path}: missing" for path in missing]
    return (
        check_session_step_owners(sources)
        + check_compact_transition_owners(sources)
        + check_session_storage_owners(sources | {
            path.relative_to(root).as_posix(): path.read_text(encoding="utf-8")
            for path in (root / "asm").rglob("*")
            if path.is_file() and path.suffix.lower() in {".asm", ".inc"}
        })
        + check_pc_mach_parse_boundaries(sources)
        + check_mqtt_judge(
            required[0].read_text(encoding="utf-8"),
            required[1].read_text(encoding="utf-8"),
            required[2].read_text(encoding="utf-8"),
        )
        + check_direct_judge(
            required[3].read_text(encoding="utf-8"),
            required[4].read_text(encoding="utf-8"),
        )
        + check_mqtt_wire_authority(
            required[5].read_text(encoding="utf-8"),
            required[2].read_text(encoding="utf-8"),
        )
        + check_mqtt_transport_isolation({
            path: (root / path).read_text(encoding="utf-8")
            for path in MQTT_ISOLATION_FILES
        })
    )


def self_test() -> None:
    if not check_compact_transition_owners({
        "src/spectrum/session/transitions.inc":
            "#define SESSION_ACCEPT_CONFIRM(done) do { return 1u; } while (0)"
    }):
        raise SystemExit("[ERR] compact FSM guard accepted a hidden macro return")
    if check_compact_transition_owners({
        "src/spectrum/session/transitions.inc":
            "#define SESSION_ACCEPT_CONFIRM(done) do { goto done; } while (0)"
    }):
        raise SystemExit("[ERR] compact FSM guard rejected an explicit macro exit")
    for code in (
        "if (confirm_action) {}", "snapshot = &app_workspace;",
        "spectrum_gui_hide_menu();", "spectrum_board_is_legal_move(move);",
        "takeback_snapshot_ply = 0;", "spectrum_restore_decode(a, b, c);",
    ):
        if not check_compact_transition_owners({"src/spectrum/session/transitions.inc": code}):
            raise SystemExit("[ERR] compact FSM guard accepted an application dependency")
    if not check_compact_transition_owners({"src/spectrum/app/app.c":
                                           "if (event == NETCHESSZX_SESSION_EVENT_ACK_MOVE) {}"}):
        raise SystemExit("[ERR] compact FSM guard accepted app ACK policy")
    if not check_compact_transition_owners({"src/spectrum/app/session_ports.h":
                                           "netchesszx_session_set_control_pending(1u);"}):
        raise SystemExit("[ERR] compact FSM guard accepted pending policy in an app port")
    if not check_compact_transition_owners({"src/spectrum/app/session_ports.h":
                                           "netchesszx_session_set_game_ply(1u);"}):
        raise SystemExit("[ERR] compact FSM guard accepted a primitive ply writer in an app port")
    if check_compact_transition_owners({"src/spectrum/app/app.c":
                                       "netchesszx_session_restore_committed(ply, active, over);"}):
        raise SystemExit("[ERR] compact FSM guard rejected a committed domain fact")
    if check_compact_transition_owners({"src/spectrum/session/transitions.inc":
                                       "if (session_fact_pending()) session_effect_apply();"}):
        raise SystemExit("[ERR] compact FSM guard rejected semantic ports")
    if not check_compact_transition_owners({
        "src/spectrum/app/app.c": "static void private_apply(void) { work(); }",
        "src/spectrum/session/transitions.inc": "private_apply();",
    }):
        raise SystemExit("[ERR] compact FSM guard accepted an implicit app call")
    if not check_compact_transition_owners({
        "src/spectrum/app/app.c": "static void private_apply(void) { work(); }",
        SESSION_PORT_SOURCE: "#define SESSION_APP_NEW() private_apply()",
    }):
        raise SystemExit("[ERR] compact FSM guard accepted a new private port call")
    for code in (
        "netchesszx_host_color = 1;",
        "++netchesszx_mqtt_session_id;",
        "memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);",
        "char *alias = netchesszx_session_state.pending_local_move;",
        "static uint8_t start_pending;",
        "schedule.pending_retry_wait = 0;",
        "alias->control_retry_count++;",
    ):
        if not check_session_storage_owners({"src/spectrum/app/app.c": code}):
            raise SystemExit("[ERR] session storage guard accepted a foreign writer")
    for code in (
        "ld (_netchesszx_host_color), a",
        "ld hl, _netchesszx_mqtt_session_id",
        "defc alias = _netchesszx_session_state",
    ):
        if not check_session_storage_owners({"asm/overlay/probe.asm": code}):
            raise SystemExit("[ERR] session storage guard accepted an ASM writer/alias")
        if not check_session_storage_owners({"asm/session/unregistered.inc": code}):
            raise SystemExit("[ERR] session storage guard accepted an unregistered ASM owner")
    if check_session_storage_owners({
        "src/spectrum/app/app.c": "if (netchesszx_session_view()->game_ply == 1) {}",
        "asm/overlay/probe.asm": "EXTERN _netchesszx_local_color\nld a, (_netchesszx_local_color)",
        "src/spectrum/session/state.h": "netchesszx_session_state.game_ply = value;",
    }):
        raise SystemExit("[ERR] session storage guard rejected owner/view/ASM read")
    if not check_session_storage_owners({
        "asm/overlay/probe.asm": 'INCLUDE "asm/session/identity_setup.inc"',
    }):
        raise SystemExit("[ERR] session storage guard accepted bootstrap in another overlay")
    sources = {
        path: "session_step(state, event);\n"
        for path in SESSION_STEP_ALLOWLIST
    }
    if check_session_step_owners(sources):
        raise SystemExit("[ERR] session boundaries self-test rejected allowlist")
    third = dict(sources)
    third["src/pc/client/third_runtime.cpp"] = "session_step(state, event);\n"
    if not check_session_step_owners(third):
        raise SystemExit("[ERR] session boundaries self-test accepted third runtime")
    if check_pc_mach_parse_boundaries({
        "src/common/session/direct_session.c":
        "netchess_proto_parse_mach(payload, platform);",
    }):
        raise SystemExit("[ERR] session boundaries self-test rejected common parser")
    if not check_pc_mach_parse_boundaries({
        "src/pc/client/main_window.cpp":
        "netchess_proto_parse_mach(payload, platform);",
    }):
        raise SystemExit("[ERR] session boundaries self-test accepted UI parser")

    header = (
        "MqttTranscriptObservation expected[MQTT_TRANSCRIPT_OBSERVATIONS_MAX];\n"
        "uint8_t expected_count;\n"
    )
    judge = (
        "static void run_shared_transcript(const MqttTranscript *transcript) {\n"
        "    mqtt_reference_run(transcript);\n"
        "    mqtt_spectrum_run(transcript);\n"
        "}\n"
        "observations_match(observations, action_count, step);\n"
        "observations_match(actual, actual_count, step);\n"
    )
    if check_mqtt_judge(header, "", judge):
        raise SystemExit("[ERR] session boundaries self-test rejected shared judge")
    if not check_mqtt_judge(header, "MQTT_SPECTRUM_EXPECTED", judge):
        raise SystemExit("[ERR] session boundaries self-test accepted target expected")
    if not check_mqtt_judge(header, "", judge.replace(
        "mqtt_spectrum_run(transcript)", "mqtt_spectrum_run(spectrum_expected)"
    )):
        raise SystemExit("[ERR] session boundaries self-test accepted split expected")

    direct_header = "const DirectParityObservation *expected;\n"
    direct_judge = (
        "direct_reference_run(scenario, &reference);\n"
        "check_trace(scenario, &reference, \"reference\");\n"
        "direct_spectrum_run(scenario, &spectrum);\n"
        "check_trace(scenario, &spectrum, \"Spectrum\");\n"
    )
    if check_direct_judge(direct_header, direct_judge):
        raise SystemExit("[ERR] session boundaries self-test rejected DIRECT judge")
    if not check_direct_judge(
        direct_header,
        direct_judge.replace(
            "check_trace(scenario, &spectrum, \"Spectrum\")",
            "check_trace(direct_spectrum_expected, &spectrum, \"Spectrum\")",
        ),
    ):
        raise SystemExit("[ERR] session boundaries self-test accepted DIRECT split expected")

    build = (
        "SESSION_MQTT_PARITY_TEST_SRC := "
        "src/spectrum/session/mqtt_wire.c\n"
        "SESSION_DIRECT_PARITY_TEST := direct\n"
    )
    if check_mqtt_wire_authority(build, judge):
        raise SystemExit("[ERR] session boundaries self-test rejected MQTT wire source")
    if not check_mqtt_wire_authority(
        build,
        judge + "const char *spectrum_net_mqtt_out_suffix(void) { return 0; }\n",
    ):
        raise SystemExit("[ERR] session boundaries self-test accepted MQTT wire stub")

    isolated = {
        path: token
        for path, token in zip(MQTT_ISOLATION_FILES, (
            "NETCHESSZX_MQTT_TX_CTX_FLAGS",
            "NETCHESSZX_MQTT_TX_CTX_FLAGS",
            "clean transport",
            "NETCHESSZX_MQTT_TX_CTX_PAYLOAD_LO",
            "NETCHESSZX_MQTT_CONNECT_CTX_FLAGS",
            "_spectrum_overlay_context + mqtt_connect_flags",
            "_netchesszx_session_send_ack_text",
            "mqtt_connect_transport_start_ovl mqtt_connect_transport_publish_ovl",
            "mqtt_tx_publish_flags_ovl NETCHESSZX_SESSION_MQTT_TX_CTX_FLAGS",
        ))
    }
    if check_mqtt_transport_isolation(isolated):
        raise SystemExit("[ERR] session boundaries self-test rejected MQTT isolation")
    isolated[MQTT_ISOLATION_FILES[1]] += " NETCHESS_PROTO_RESET"
    if not check_mqtt_transport_isolation(isolated):
        raise SystemExit("[ERR] session boundaries self-test accepted MQTT verb routing")
    isolated[MQTT_ISOLATION_FILES[7]] += " spectrum_overlay_exec"
    if not check_mqtt_transport_isolation(isolated):
        raise SystemExit("[ERR] session boundaries self-test accepted nested overlay")
    print("[OK] session boundaries self-test")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return
    errors = check_tree(Path(args.root))
    if errors:
        print("[ERR] session boundaries violated")
        for error in errors:
            print(f"  {error}")
        raise SystemExit(1)
    print(
        "[OK] session boundaries: target-neutral MQTT/DIRECT expected; "
        "production MQTT wire; one PC reducer call"
    )


if __name__ == "__main__":
    main()
