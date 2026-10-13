#!/usr/bin/env python3
"""Verify the Classic or banking-Next low-memory layout against its linker map."""

import argparse
from pathlib import Path
import re
import sys

PIECE_SPRITES_SIZE = 12 * 32
MQTT_STREAM_MAX = 223
MQTT_PACKET_MAX = 160
NEXT_SPRITE_STAGE_BYTES = 256
NEXT_PALETTE_STAGE_BYTES = 512
RULES_TMP_SIZE = 64
MIN_SP_GAP = 512
MIN_SP_GAP_WARN = 768


def parse_symbol(text, name):
    m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name), text, re.M)
    if not m:
        raise SystemExit("[ERR] symbol %s not found in map" % name)
    return int(m.group(1), 16)


def parse_first_symbol(text, names):
    for name in names:
        m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name),
                      text, re.M)
        if m:
            return int(m.group(1), 16)
    raise SystemExit("[ERR] none of symbols %s found in map" % ", ".join(names))


def parse_optional_symbol(text, name):
    m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name),
                  text, re.M)
    return int(m.group(1), 16) if m else None


def active_text(path, defines):
    lines = []
    frames = []
    active = True
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        directive = raw_line.strip().lstrip("#").split()
        keyword = directive[0].upper() if directive else ""
        if keyword in ("IFDEF", "IFNDEF", "IF"):
            condition = True
            if keyword in ("IFDEF", "IFNDEF") and len(directive) > 1:
                condition = directive[1] in defines
                if keyword == "IFNDEF":
                    condition = not condition
            frames.append((active, condition))
            active = active and condition
        elif keyword == "ELSE" and frames:
            parent, condition = frames[-1]
            active = parent and not condition
        elif keyword == "ELIF" and frames:
            parent, _ = frames[-1]
            frames[-1] = (parent, True)
            active = False
        elif keyword == "ENDIF" and frames:
            active, _ = frames.pop()
        elif active:
            lines.append(raw_line)
    return "\n".join(lines)


def parse_const(path, name, defines=frozenset()):
    text = active_text(path, defines)
    matches = re.findall(
        r"^(?:#define\s+)?%s\s+(?:EQU\s+)?(?:0x([0-9A-Fa-f]+)|([0-9]+))u?"
        % re.escape(name),
        text,
        re.M,
    )
    if len(matches) != 1:
        raise SystemExit("[ERR] %s not found in %s" % (name, path))
    value = matches[0]
    return int(value[0], 16) if value[0] else int(value[1], 10)


def assert_no_overlap(label, start, size, other_label, other_start, other_size):
    end = start + size
    other_end = other_start + other_size
    if start < other_end and other_start < end:
        raise SystemExit(
            "[ERR] %s 0x%04x..0x%04x overlaps %s 0x%04x..0x%04x"
            % (label, start, end - 1, other_label, other_start, other_end - 1)
        )


def classic_frame_clock_end(text, bss_end, defines=frozenset()):
    frame_clock_path = Path("asm/spectrum/frame_clock.asm")
    vector_page = parse_const(frame_clock_path, "frame_clock_vector_page", defines)
    vector_offset = parse_const(frame_clock_path, "frame_clock_vector_offset", defines)
    vector_pointer = parse_const(frame_clock_path, "frame_clock_vector_pointer", defines)
    vector_reservation = parse_symbol(text, "_spectrum_frame_clock_vector_reservation")
    vector_head = parse_symbol(text, "__bss_frame_clock_vector_head")
    vector_end = parse_symbol(text, "__bss_frame_clock_vector_tail")
    if vector_pointer != (vector_page << 8) | vector_offset:
        raise SystemExit("[ERR] Classic IM2 vector encoding mismatch")
    if 0x40 <= vector_page <= 0x7f:
        raise SystemExit("[ERR] Classic IM2 vector uses the ULA snow range")
    if (vector_reservation != vector_pointer or vector_head != vector_pointer or
            vector_end != vector_pointer + 2):
        raise SystemExit("[ERR] Classic IM2 vector requires its fixed two-byte BSS section")
    if bss_end > vector_head:
        raise SystemExit("[ERR] Classic resident BSS overlaps the fixed IM2 vector")
    # The DIRECT receive slots sit right above the vector, below the stack.
    high_head = parse_optional_symbol(text, "__bss_high_scratch_head")
    if high_head is not None:
        if high_head != vector_end:
            raise SystemExit("[ERR] Classic high scratch must follow the IM2 vector")
        # Their DEFS width must match the C bound that fills each slot.
        payload_max = parse_const(Path("src/spectrum/transport/link.h"),
                                  "SPECTRUM_LINK_PAYLOAD_MAX", defines)
        slots = [parse_symbol(text, name) for name in (
            "_direct_rx_payload", "_direct_rx_payload2",
            "_spectrum_moves_header_shown")]
        if slots != [high_head + n * payload_max for n in range(3)]:
            raise SystemExit("[ERR] Classic DIRECT receive slots must hold "
                             "SPECTRUM_LINK_PAYLOAD_MAX (%d) bytes each"
                             % payload_max)
        return parse_symbol(text, "__bss_high_scratch_tail")
    return vector_end


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", required=True)
    ap.add_argument(
        "--target", choices=("classic", "next", "spectranext"), required=True
    )
    args = ap.parse_args()

    with open(args.map, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()

    overlay_slot = parse_symbol(text, "_overlay_code_slot")
    is_next = args.target == "next"
    is_spectranext = args.target == "spectranext"
    defines = frozenset(
        ("NETCHESSZX_NEXT_BANKING",)
        if is_next
        else (("NETCHESSZX_SPECTRANEXT",) if is_spectranext else ())
    )
    expected_slot = 0x6000 if is_next else (0x2000 if is_spectranext else 0x6800)
    expected_scratch = 0x3C78 if is_next else 0x672B
    sprites = parse_symbol(text, "piece_sprites_16x16")
    assets_end = sprites + PIECE_SPRITES_SIZE
    stream = parse_symbol(text, "_mqtt_stream")
    packet = parse_const(Path("src/spectrum/transport/mqtt_min.h"),
                         "SPECTRUM_MQTT_SCRATCH_BASE", defines)
    lowram_path = Path("src/spectrum/lowram_map.h")
    lowram_scratch = parse_const(
        lowram_path,
        "NETCHESSZX_NEXT_OVERLAY_SCRATCH_ADDR"
        if is_next
        else "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR",
        defines,
    )
    overlay_scratch = parse_symbol(text, "_overlay_scratch_base")
    spxn_page_table = parse_optional_symbol(text, "_spxn_overlay_page_table")
    if is_spectranext and spxn_page_table is None:
        raise SystemExit("[ERR] Spectranext page table missing")
    lowram_limit = spxn_page_table if is_spectranext else overlay_slot
    overlay_scratch_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE")
    timer_roll = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIMER_ROLL_ADDR", defines)
    timer_roll_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIMER_ROLL_SIZE", defines)
    menu_labels = parse_const(
        Path("asm/spectrum/timer_roll_layout.inc"),
        "taboption_labels", defines)
    timer_roll_state = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_ADDR", defines)
    timer_roll_state_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_SIZE", defines)
    time_last_frame = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIME_LAST_FRAME_ADDR", defines)
    time_move_phase = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_TIME_MOVE_PHASE_ADDR", defines)
    menu_visible = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_MENU_VISIBLE_ADDR", defines)
    menu_focus = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_MENU_FOCUS_ADDR", defines)
    menu_item = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_MENU_ITEM_ADDR", defines)
    lowram_editor_input = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR", defines)
    lowram_editor_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE", defines)
    chat_message_max = parse_const(
        Path("src/spectrum/ui/log_contract.h"),
        "NETCHESSZX_CHAT_MESSAGE_TEXT_MAX", defines)
    editor_input = parse_const(
        Path("asm/spectrum/editor_state.asm"), "editor_input", defines)
    editor_input_max = parse_const(
        Path("asm/spectrum/editor_state.asm"), "editor_input_max", defines)
    direct_rx_spill = parse_const(
        Path("asm/spectrum/shrink_kernels.asm"), "direct_rx_spill", defines)
    linked_last_frame = parse_symbol(text, "_last_frame_count")
    linked_move_phase = parse_symbol(text, "_spectrum_time_move_frame_phase")
    overlay_context = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR", defines)
    overlay_context_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE", defines)
    hinted_rows = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_HINTED_ROWS_ADDR", defines)
    hinted_rows_size = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_HINTED_ROWS_SIZE", defines)
    gui_state = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_GUI_STATE_ADDR", defines)
    mqtt_overlay_scratch = parse_const(
        Path("asm/overlay/mqtt_connect/entry_mqtt_connect.asm"),
        "mqtt_packet_ovl", defines)
    rules_path = Path("asm/overlay/rules/rules_stub.asm")
    rules_tmp = parse_const(rules_path, "r_tmp", defines)
    gui_log_scratch = parse_const(
        Path("asm/overlay/gui_log/entry_gui_log.asm"),
        "gui_log_msg_scratch", defines)
    rules_tmp_size = parse_const(
        Path("asm/overlay/rules/rules_stub.asm"), "rules_tmp_size")

    if stream < assets_end:
        raise SystemExit(
            "[ERR] mqtt_stream at 0x%04x overlaps runtime assets ending at "
            "0x%04x (piece sprites corrupt under MQTT). Raise "
            "SPECTRUM_MQTT_RUNTIME_ASSETS_END in mqtt_min.h." % (stream, assets_end)
        )
    if assets_end > lowram_limit:
        raise SystemExit(
            "[ERR] runtime assets end 0x%04x beyond overlay slot 0x%04x"
            % (assets_end, lowram_limit)
        )
    if not is_next:
        about_path = Path("asm/overlay/about/entry_about.asm")
        about = parse_const(about_path, "about_input", defines)
        about_size = parse_const(about_path, "about_input_size", defines)
        assert_no_overlap("about_work", about, about_size,
                          "mqtt_stream", stream, MQTT_STREAM_MAX)
        if about != packet:
            raise SystemExit(
                "[ERR] time-disjoint ABOUT work must start at MQTT packet scratch"
            )
        if about + about_size > lowram_limit:
            raise SystemExit(
                "[ERR] about_work 0x%04x..0x%04x reaches overlay slot"
                % (about, about + about_size - 1)
            )
    if overlay_slot != expected_slot:
        raise SystemExit(
            "[ERR] linked overlay slot 0x%04x != fixed 0x%04x"
            % (overlay_slot, expected_slot)
        )
    if is_spectranext and spxn_page_table != 0x6800:
        raise SystemExit("[ERR] Spectranext page table moved from 0x6800")
    if is_spectranext:
        page_count = parse_symbol(text, "ovl_atlas_count")
        backend_start = parse_symbol(text, "_spxn_overlay_loader_start")
        backend_end = parse_symbol(text, "_spxn_overlay_loader_end")
        if (spxn_page_table + page_count > backend_start or
                backend_start != 0x6E00 or backend_end > 0x7000):
            raise SystemExit(
                "[ERR] Spectranext overlay backend/page table exceeds 0x6800..0x6fff"
            )
    if (overlay_scratch != expected_scratch or
            lowram_scratch != overlay_scratch or
            mqtt_overlay_scratch != overlay_scratch or
            rules_tmp != overlay_scratch or
            gui_log_scratch != overlay_scratch):
        raise SystemExit(
            "[ERR] overlay scratch mismatch: map 0x%04x, header 0x%04x, "
            "MQTT 0x%04x, rules 0x%04x, GUI log 0x%04x, fixed 0x%04x"
            % (overlay_scratch, lowram_scratch, mqtt_overlay_scratch,
               rules_tmp, gui_log_scratch, expected_scratch)
        )
    codec_path = Path("asm/overlay/journal/stj_codec.asm")
    append_path = Path("asm/overlay/saveload/journal_append.asm")
    if is_next:
        # Next runs esxDOS with MMU1 = ROM, so 0x3C78 cannot be a file
        # buffer: both use SAVELOAD's 160-byte overlay-BSS ja_io_buf.
        codec_text = active_text(codec_path, defines)
        append_text = active_text(append_path, defines)
        if ("stj_scratch EQU ja_io_buf" not in codec_text or
                "journal_append_buf EQU ja_io_buf" not in append_text or
                not re.search(r"^ja_io_buf:\s*\n\s*defs 160", append_text,
                              re.M)):
            raise SystemExit(
                "[ERR] Next journal file buffer must be SAVELOAD's "
                "160-byte ja_io_buf"
            )
        journal_scratch = append_scratch = overlay_scratch
    else:
        journal_scratch = parse_const(codec_path, "stj_scratch", defines)
        append_scratch = parse_const(append_path, "journal_append_buf",
                                     defines)
    header_size = parse_const(codec_path, "STJ_HDR_SIZE", defines)
    codec_chunk = parse_const(codec_path, "STJ_CHUNK_SIZE", defines)
    append_chunk = parse_const(append_path, "JA_CHUNK", defines)
    codec_used = header_size + codec_chunk
    append_used = append_chunk
    if is_spectranext:
        codec_used += 2 * parse_const(codec_path, "STJ_PATH_SIZE", defines)
        append_used += parse_const(append_path, "JA_PATH_SIZE", defines)
    # Record pairs and all per-chunk counters must fit in one byte.
    if (journal_scratch != overlay_scratch or
            append_scratch != overlay_scratch or
            not 0 < codec_chunk < 256 or codec_chunk % 2 or
            not 0 < append_chunk < 256 or append_chunk % 2 or
            max(codec_used, append_used) > overlay_scratch_size):
        raise SystemExit(
            "[ERR] journal scratch mismatch: codec 0x%04x, append "
            "0x%04x, map 0x%04x" % (journal_scratch, append_scratch,
                                    overlay_scratch)
        )
    if is_spectranext:
        # SpectraNext replays SHATJRNL.NEW from RECOVERY: one chunk plus the
        # staged path in the same overlay scratch.
        replay_path = Path("asm/overlay/recovery/rcv_replay.asm")
        replay_buf = parse_const(replay_path, "journal_append_buf", defines)
        replay_chunk = parse_const(replay_path, "JA_CHUNK", defines)
        replay_used = replay_chunk + parse_const(replay_path, "JA_PATH_SIZE",
                                                 defines)
        if (replay_buf != overlay_scratch or
                not 0 < replay_chunk < 256 or replay_chunk % 2 or
                replay_used > overlay_scratch_size):
            raise SystemExit(
                "[ERR] RECOVERY replay scratch mismatch: 0x%04x + %d bytes, "
                "map 0x%04x" % (replay_buf, replay_used, overlay_scratch)
            )
    if overlay_scratch_size != MQTT_PACKET_MAX:
        raise SystemExit(
            "[ERR] overlay scratch size %d != MQTT packet size %d"
            % (overlay_scratch_size, MQTT_PACKET_MAX)
        )
    if is_spectranext:
        loader_path = Path("asm/esxdos/overlay_loader.asm")
        stage = parse_const(loader_path, "spxn_overlay_stage", defines)
        chunk = parse_const(loader_path, "spxn_overlay_chunk", defines)
        if (stage < spxn_page_table + page_count or
                stage + chunk + 2 > backend_start):
            raise SystemExit("[ERR] Spectranext overlay staging overlaps low CODE")
    if rules_tmp_size != RULES_TMP_SIZE or rules_tmp_size > overlay_scratch_size:
        raise SystemExit(
            "[ERR] rules scratch size %d invalid for %d-byte overlay scratch"
            % (rules_tmp_size, overlay_scratch_size)
        )
    overlay_scratch_end = overlay_scratch + overlay_scratch_size
    if overlay_scratch_end > lowram_limit:
        raise SystemExit(
            "[ERR] overlay scratch 0x%04x..0x%04x reaches overlay slot"
            % (overlay_scratch, overlay_scratch_end - 1)
        )
    if (not is_next and
            not (about <= overlay_scratch and
                 overlay_scratch_end <= about + about_size)):
        raise SystemExit(
            "[ERR] time-disjoint ABOUT work no longer contains overlay scratch"
        )
    assert_no_overlap("timer roll arena", timer_roll, timer_roll_size,
                      "overlay scratch", overlay_scratch,
                      overlay_scratch_size)
    # Player identity ("MNAME <name>" line + peer name) must survive every
    # time-disjoint user of the surrounding scratch.
    identity = parse_const(
        lowram_path, "NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR", defines)
    identity_size = (
        parse_const(lowram_path, "NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_SIZE") +
        parse_const(lowram_path, "NETCHESSZX_LOWRAM_PEER_NAME_SIZE"))
    stats = identity + identity_size
    stats_size = parse_const(lowram_path, "NETCHESSZX_LOWRAM_GAME_STATS_SIZE")
    if stats != identity + identity_size or stats_size != 33:
        raise SystemExit("[ERR] game stats must occupy 33 bytes after player identity")
    if parse_symbol(text, "_spectrum_game_stats") != stats:
        raise SystemExit("[ERR] game stats do not match the fixed low-RAM map")
    if stats + stats_size > (0x4000 if is_next else lowram_limit):
        raise SystemExit("[ERR] game stats exceed their low-RAM window")
    identity_neighbours = [
        ("mqtt_stream", stream, MQTT_STREAM_MAX),
        ("MQTT packet scratch", packet, MQTT_PACKET_MAX),
        ("overlay scratch", overlay_scratch, overlay_scratch_size),
        ("timer roll arena", timer_roll, timer_roll_size),
    ]
    if is_next:
        next_stages = Path("asm/next/extension_bank_layout.asm")
        identity_neighbours += [
            ("Next sprite stage",
             parse_const(next_stages, "next_sprite_stage", defines),
             NEXT_SPRITE_STAGE_BYTES),
            ("Next palette stage",
             parse_const(next_stages, "next_palette_stage", defines),
             NEXT_PALETTE_STAGE_BYTES),
        ]
        if identity + identity_size > 0x4000:
            raise SystemExit("[ERR] Next player identity leaves its low page")
    else:
        identity_neighbours.append(("about_work", about, about_size))
        if identity + identity_size > lowram_limit:
            raise SystemExit("[ERR] player identity reaches overlay slot")
    if is_spectranext:
        identity_neighbours.append((
            "XFS directory scratch",
            parse_const(lowram_path, "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR",
                        defines),
            parse_const(lowram_path, "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_SIZE")))
        # The cartridge stages one DIRECT recv (cursor, length, packet) in the
        # inactive MQTT stream just below the identity.
        pending = parse_const(
            lowram_path, "NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_CURSOR_ADDR",
            defines)
        pending_data = parse_const(
            lowram_path, "NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_DATA_ADDR",
            defines)
        identity_neighbours.append((
            "DIRECT pending recv", pending,
            pending_data + MQTT_PACKET_MAX - pending))
    for label, start, size in identity_neighbours:
        assert_no_overlap("game stats", stats, stats_size, label, start, size)
        assert_no_overlap("player identity", identity, identity_size,
                          label, start, size)
    if timer_roll_state != overlay_context + overlay_context_size or \
            timer_roll_state + timer_roll_state_size > hinted_rows:
        raise SystemExit("[ERR] timer roll state exceeds its fixed low-RAM gap")
    if gui_state != hinted_rows + hinted_rows_size or gui_state + 7 != 0x5FFF:
        raise SystemExit("[ERR] GUI state does not fill the fixed low-RAM tail")
    gui_symbols = (
        ("_notice_ticks", 0),
        ("_notice_error", 1),
        ("_notice_success", 2),
        ("_last_ply_seen", 3),
        ("_move_line_count", 5),
        ("_chat_line_count", 6),
    )
    for symbol, offset in gui_symbols:
        if parse_symbol(text, symbol) != gui_state + offset:
            raise SystemExit(
                "[ERR] %s does not match its fixed low-RAM address" % symbol
            )
    if linked_last_frame != time_last_frame or linked_move_phase != time_move_phase:
        raise SystemExit(
            "[ERR] frame timing state does not match its fixed low-RAM map"
        )
    if (menu_visible != time_move_phase + 1 or
            menu_focus != menu_visible + 1 or
            menu_item != menu_focus + 1 or menu_item + 1 != hinted_rows):
        raise SystemExit("[ERR] top-menu state does not fill its low-RAM gap")
    if direct_rx_spill != assets_end:
        raise SystemExit(
            "[ERR] DIRECT ASM spill 0x%04x != runtime assets end 0x%04x"
            % (direct_rx_spill, assets_end)
        )
    expected_menu_labels = 0x382C if is_next else 0x632D
    if menu_labels != expected_menu_labels:
        raise SystemExit(
            "[ERR] menu Roller labels 0x%04x != runtime DAT 0x%04x"
            % (menu_labels, expected_menu_labels)
        )
    if editor_input != lowram_editor_input:
        raise SystemExit(
            "[ERR] resident editor input does not match its fixed low-RAM map"
        )
    if (editor_input_max != chat_message_max or
            editor_input_max + 1 != lowram_editor_size):
        raise SystemExit(
            "[ERR] resident editor limit does not match chat/low-RAM contracts"
        )
    if not is_next:
        if timer_roll != overlay_scratch_end or \
                timer_roll + timer_roll_size > lowram_limit:
            raise SystemExit("[ERR] Classic timer roll arena exceeds low RAM")

    next_layout = Path("asm/next/extension_bank_layout.asm")
    if is_next:
        stage = parse_const(next_layout, "next_sprite_stage", defines)
        palette_stage = parse_const(next_layout, "next_palette_stage", defines)
        stage_match = re.search(
            r"^next_sprite_stage\s*=\s*\$([0-9A-Fa-f]+)", text, re.M)
        if stage_match:
            linked_stage = int(stage_match.group(1), 16)
            if linked_stage != stage:
                raise SystemExit(
                    "[ERR] linked/source Next sprite stage mismatch"
                )
            stage = linked_stage
        assert_no_overlap("next_sprite_stage", stage, NEXT_SPRITE_STAGE_BYTES,
                          "mqtt_stream", stream, MQTT_STREAM_MAX)
        assert_no_overlap("next_sprite_stage", stage, NEXT_SPRITE_STAGE_BYTES,
                          "MQTT packet scratch", packet, MQTT_PACKET_MAX)
        if packet + MQTT_PACKET_MAX != stage:
            raise SystemExit(
                "[ERR] MQTT packet scratch must end at Next sprite stage"
            )
        if stage + NEXT_SPRITE_STAGE_BYTES != overlay_scratch:
            raise SystemExit(
                "[ERR] Next sprite stage must end at overlay scratch 0x%04x"
                % overlay_scratch
            )
        if stage + NEXT_SPRITE_STAGE_BYTES > lowram_limit:
            raise SystemExit(
                "[ERR] next_sprite_stage 0x%04x..0x%04x reaches overlay slot"
                % (stage, stage + NEXT_SPRITE_STAGE_BYTES - 1)
            )
        if overlay_scratch_end != palette_stage:
            raise SystemExit(
                "[ERR] Next overlay scratch must end at palette stage"
            )
        if palette_stage + NEXT_PALETTE_STAGE_BYTES > 0x4000:
            raise SystemExit(
                "[ERR] Next palette stage exceeds permanent slot-1 page"
            )
        if timer_roll != palette_stage + NEXT_PALETTE_STAGE_BYTES or \
                timer_roll + timer_roll_size > 0x4000:
            raise SystemExit("[ERR] Next timer roll arena exceeds slot 1")
    bss_candidates = [
        parse_optional_symbol(text, name)
        for name in ("__BSS_END_tail", "__BSS_END", "__bss_end",
                     "__bss_compiler_tail", "__bss_user_tail")
    ]
    bss_end = max(value for value in bss_candidates if value is not None)
    stack_top = parse_first_symbol(text,
                                  ("__register_sp", "TAR__register_sp"))
    if stack_top == 0:
        stack_top = 0x10000
    if not is_next and not is_spectranext:
        bss_end = classic_frame_clock_end(text, bss_end, defines)
        print(
            "[OK] Classic IM2 frame clock: vector 0x%04x..0x%04x; "
            "snow-safe fixed BSS reservation"
            % ((parse_symbol(text, "_spectrum_frame_clock_vector_reservation"),
               parse_symbol(text, "_spectrum_frame_clock_vector_reservation") + 1))
        )
    stack_gap = stack_top - bss_end
    if stack_gap < MIN_SP_GAP:
        raise SystemExit(
            "[ERR] SP 0x%04x minus BSS_END 0x%04x leaves %d bytes, "
            "below %d byte floor"
            % (stack_top, bss_end, stack_gap, MIN_SP_GAP)
        )
    if stack_gap < MIN_SP_GAP_WARN:
        print(
            "[WARN] SP 0x%04x minus BSS_END 0x%04x leaves %d bytes, "
            "below %d byte warning"
            % (stack_top, bss_end, stack_gap, MIN_SP_GAP_WARN)
        )
    print("[OK] SP gap hard floor: %d >= %d bytes"
          % (stack_gap, MIN_SP_GAP))
    print(
        "[OK] lowmem layout: assets end 0x%04x <= mqtt_stream 0x%04x"
        % (assets_end, stream)
    )
    print(
        "[OK] linked overlay scratch: 0x%04x..0x%04x; header/slot match"
        % (overlay_scratch, overlay_scratch_end - 1)
    )
    xfs_state_end = parse_optional_symbol(text, "_spxn_xfs_state_end")
    if xfs_state_end is not None:
        xfs_state = parse_const(lowram_path,
                                "NETCHESSZX_LOWRAM_XFS_STATE_ADDR", defines)
        xfs_state_size = parse_const(lowram_path,
                                     "NETCHESSZX_LOWRAM_XFS_STATE_SIZE", defines)
        xfs_scratch = parse_const(
            lowram_path, "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR", defines)
        linked_scratch = parse_symbol(text, "_spxn_xfs_dir_scratch")
        bss_user = parse_symbol(text, "__bss_user_head")
        bss_user_size = parse_symbol(text, "__bss_user_size")
        low_bss_end = parse_symbol(text, "__BSS_END_tail")
        bss_user_limit = parse_const(
            lowram_path, "NETCHESSZX_LOWRAM_SPXN_BSS_USER_LIMIT", defines)
        net_state = parse_symbol(text, "_active_link")
        keepalive = parse_symbol(text, "_mqtt_broker_keepalive")

        if xfs_state_end != xfs_state + xfs_state_size:
            raise SystemExit("[ERR] linked XFS state binding mismatch")
        if linked_scratch != xfs_scratch:
            raise SystemExit("[ERR] linked XFS directory scratch mismatch")
        if bss_user != parse_const(
                lowram_path, "NETCHESSZX_LOWRAM_SPXN_BSS_USER_ADDR", defines):
            raise SystemExit("[ERR] Spectranext bss_user anchor mismatch")
        if (bss_user_size > bss_user_limit or
                bss_user + bss_user_size > xfs_state or
                low_bss_end > xfs_state):
            raise SystemExit("[ERR] Spectranext bss_user exceeds reserved range")
        if net_state != xfs_state_end or keepalive + 2 > 0x5C00:
            raise SystemExit("[ERR] Spectranext transport-state binding mismatch")
        line_buf = parse_symbol(text, "_line_buf")
        if line_buf != bss_user:
            raise SystemExit("[ERR] Spectranext link-only line buffer moved")
        for forbidden in ("_uart_ring", "_net_uart_init"):
            if parse_optional_symbol(text, forbidden) is not None:
                raise SystemExit(
                    "[ERR] Spectranext image retains inactive UART symbol %s"
                    % forbidden
                )
        print(
            "[OK] Spectranext lowmem: bss_user 0x%04x..0x%04x, "
            "XFS 0x%04x..0x%04x, net 0x%04x..0x%04x"
            % (bss_user, bss_user + bss_user_size - 1,
               xfs_state, xfs_state_end - 1,
               net_state, keepalive + 1)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
