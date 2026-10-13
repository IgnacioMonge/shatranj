"""Cheap contracts for the single-screen Spectrum setup flow.

These checks intentionally inspect the small, stable ASM contracts.  They run
without z88dk and catch the old split-screen/menu-row layout before an artifact
build is attempted.
"""

from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from asm_source import read_asm
import re


ROOT = Path(__file__).resolve().parents[2]
MENU = (ROOT / "asm/overlay/menu_config/entry_menu_config.asm").read_text(
    encoding="utf-8"
)
SETUP = (ROOT / "asm/overlay/setup/entry_setup.asm").read_text(encoding="utf-8")
GUI = (ROOT / "src/spectrum/ui/gui.c").read_text(encoding="utf-8")
SCREEN_ZX = (ROOT / "asm/spectrum/screen.asm").read_text(encoding="utf-8")
SCREEN_NEXT = (ROOT / "asm/next/screen.asm").read_text(encoding="utf-8")


def _equates(source: str, names: tuple[str, ...]) -> dict[str, int]:
    values = {}
    for name in names:
        match = re.search(rf"^\s*{name}\s+EQU\s+(\d+)\s*$", source, re.MULTILINE)
        assert match, f"missing {name}"
        values[name] = int(match.group(1))
    return values


def test_board_swatch_source_tracks_runtime_asset_layout() -> None:
    def asset_layout(screen: str) -> tuple[int, int]:
        base = re.search(
            r"^NETCHESSZX_ASSET_BASE EQU (0x[0-9a-f]+)$", screen, re.MULTILINE
        )
        offset = re.search(
            r"^board_theme_light_attrs EQU NETCHESSZX_ASSET_BASE \+ (\d+)$",
            screen,
            re.MULTILINE,
        )
        assert base and offset
        return int(base.group(1), 16), int(offset.group(1))

    sources = [
        (int(base, 16), int(offset))
        for base, offset in re.findall(
            r"^menu_config_dat_light_attrs EQU (0x[0-9a-f]+) \+ (\d+)$",
            MENU,
            re.MULTILINE,
        )
    ]
    assert sources == [asset_layout(SCREEN_NEXT), asset_layout(SCREEN_ZX)]


def test_single_screen_uses_approved_eleven_row_order() -> None:
    names = (
        "ROW_GAME",
        "ROW_LINK1",
        "ROW_LINK2",
        "ROW_TIME",
        "ROW_COLOR",
        "ROW_NOTATION",
        "ROW_BOARD",
        "ROW_SET",
        "ROW_HINTS",
        "ROW_ACTION",
    )
    expected = {
        "ROW_GAME": 0,
        "ROW_LINK1": 1,
        "ROW_LINK2": 2,
        "ROW_TIME": 4,
        "ROW_COLOR": 5,
        "ROW_NOTATION": 6,
        "ROW_BOARD": 7,
        "ROW_SET": 8,
        "ROW_HINTS": 9,
        "ROW_ACTION": 10,
    }
    assert _equates(MENU, names) == expected
    # SETUP deliberately shares only the rows it needs for navigation; GAME
    # and endpoint aliases are owned by the renderer.
    for name in (
        "ROW_TIME",
        "ROW_COLOR",
        "ROW_NOTATION",
        "ROW_BOARD",
        "ROW_SET",
        "ROW_HINTS",
        "ROW_ACTION",
    ):
        assert name in SETUP


def test_physical_layout_preserves_game_and_endpoint_alias() -> None:
    # GAME/LINK are composite rows.  Endpoint values render through the LINK
    # row; TIME remains the physical row 10 despite its logical row shift.
    expected = {
        "menu_config_line_game": '"GAME  CREATE    JOIN"',
        "menu_config_line_link": '"LINK  MQTT      DIRECT"',
    }
    for label, text in expected.items():
        assert re.search(
            rf"^{label}:\s*DEFB\s+NETCHESSZX_SETUP_SCREEN_ROW_\w+,\s*{re.escape(text)}", MENU, re.MULTILINE
        ), label
    assert "menu_config_game_create_mqtt" not in MENU
    assert "menu_config_game_create_direct" not in MENU
    assert "menu_config_game_join_mqtt" not in MENU
    assert "menu_config_game_join_direct" not in MENU
    assert "menu_config_setup_screen_row" in MENU
    helper = MENU[MENU.index("menu_config_setup_screen_row:") :]
    assert "ROW_TIME" in helper and "ROW_ACTION" in helper
    assert "cp 3" in helper and "dec a" in helper
    assert "add a, NETCHESSZX_SETUP_SCREEN_ROW_BASE" in helper
    assert "ld a, NETCHESSZX_SETUP_SCREEN_ROW_TIME" in helper
    assert "ld a, NETCHESSZX_SETUP_SCREEN_ROW_ACTION" in helper


def test_game_is_preserved_when_incrementally_painting_non_game_rows() -> None:
    assert "menu_config_run_dirty" in MENU
    assert "_menu_config_render_ovl_entry" in MENU
    assert "menu_config_setup_screen_row" in MENU
    incremental = MENU[
        MENU.index("menu_config_render_incremental:") : MENU.index(
            "menu_config_render_force:"
        )
    ]
    assert "_spectrum_info_show_setup" not in incremental
    assert "menu_config_render_dirty" in incremental
    assert "menu_config_draw_game_keep" not in MENU


def test_persistence_dirty_mask_excludes_per_game_choices() -> None:
    # Setup may dirty connection rows, but game-only choices must carry a zero
    # force mask.  Keep this as a source contract until a target harness exists.
    app = (ROOT / "src/spectrum/ui/setup.h").read_text(encoding="utf-8")
    assert "SETUP_CHOICE_COLOR 2u" in app
    assert "SETUP_CHOICE_NOTATION 3u" in app
    assert "SETUP_CHOICE_TIME 6u" in app
    assert "config_dirty" in app


def test_time_and_action_repaints_are_independently_routed() -> None:
    app = (ROOT / "src/spectrum/ui/setup.c").read_text(encoding="utf-8")
    edit = app[
        app.index("if (flags & NETCHESSZX_SETUP_FLAG_EDIT)") : app.index(
            "session_setup_sync_board_view();"
        )
    ]
    time = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text(
        encoding="utf-8"
    )
    assert "NETCHESSZX_SETUP_FLAG_TIME_UI |" in edit
    assert "NETCHESSZX_SETUP_FLAG_ACTION_UI" in edit
    assert edit.count("netchesszx_setup_time_ui();") == 1
    assert "FLAG_TIME_UI    EQU 0x10" in time
    assert "FLAG_ACTION_UI  EQU 0x20" in time
    ui = time[time.index("_time_config_ui_ovl_entry:") : time.index("tc_ui_time:")]
    assert "bit 4, a" in ui
    assert "bit 5, a" in ui


def test_piece_preview_does_not_wait_between_pairs() -> None:
    assert "reveal_board_piece_pairs(1u, 0u);" in GUI
    assert "reveal_board_piece_pairs(0u, 0u);" in GUI


def test_rtc_is_probed_by_default_with_utc_available() -> None:
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    load = app.index("config_state = netchesszx_config_load_overlay();")
    setup = app.index("connection_setup:", load)
    assert "netchesszx_timezone = NETCHESSZX_TIME_RTC;" in app[load:setup]

    time = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text(
        encoding="utf-8"
    )
    assert 'DEFB NETCHESSZX_SETUP_SCREEN_ROW_TIME, "TIME  RTC       UTC "' in time
    select = time[time.index("tc_time_select:") : time.index("tc_begin_edit:")]
    assert select.count("tc_begin_edit") == 2
    assert "tc_parse_timezone" not in select


def test_rtc_probe_does_not_patch_its_own_code() -> None:
    time = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text(
        encoding="utf-8"
    )
    assert "ld (tc_rtc_rx_mask + 1), a" not in time
    assert "ld (CTX_FLAGS), a" in time
    assert "and d" in time


def test_time_change_uses_local_shift_or_resyncs_the_selected_source() -> None:
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    time = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text(
        encoding="utf-8"
    )
    define = time[time.index("tc_define_time:") : time.index("tc_action_key:")]
    handler = app[
        app.index("void spectrum_setup_apply_timezone(") :
        app.index("static uint8_t session_setup_run(")
    ]
    assert "IFDEF NETCHESSZX_NEXT" not in define
    assert "ld (CTX_ACTION), a" in define
    # NAME follows TIME on screen; it is valid for both CREATE and JOIN.
    assert "ld a, ROW_NAME" in define
    assert "netchesszx_setup_time_commit()" in handler
    assert "spectrum_app_shift_clock(" in handler
    assert "(void)clock_sync_run();" in handler
    assert "spectrum_link_clock_retry_start();" in handler
    assert "else {\n                spectrum_link_clock_retry_start();\n            }" in handler
    assert "preflight_clock_run" not in handler

    setup_call = app.rindex("config_state = session_setup_run(config_state);")
    before_setup = app[app.rindex("while (!connection_preflight_run", 0,
                                   setup_call) : setup_call]
    after_setup = app[setup_call : app.rindex("if (netchesszx_session_identity_host_color_ready())")]
    assert "netchesszx_timezone != warm_restart" not in after_setup
    assert "preflight_clock_run" not in after_setup
    assert "spectrum_link_clock_retry_start();" in before_setup
    assert "spectrum_link_clock_retry_cancel();" in after_setup

    setup_loop = app[
        app.index("static uint8_t session_setup_run") :
        app.index("static uint8_t input_has_text")
    ]
    assert "spectrum_net_runtime_wait_frame();" in setup_loop


def test_clock_preflight_messages_are_classic_and_next_only() -> None:
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    clock_ui_start = app.index(
        "#ifndef NETCHESSZX_SPECTRANEXT\nstatic void preflight_clock_line"
    )
    clock_ui = app[clock_ui_start : app.index("#endif", clock_ui_start)]
    assert clock_ui.index("CLOCK WAIT") < clock_ui.index("clock_sync_run()")
    assert clock_ui.index("clock_sync_run()") < clock_ui.index("CLOCK OK")
    assert "CLOCK FAIL" in clock_ui

    connection = app[
        app.index("static uint8_t connection_preflight_run") :
        app.index("static void status_show_phase")
    ]
    assert "#ifdef NETCHESSZX_SPECTRANEXT" in connection
    assert "preflight_clock_run();" in connection


def test_board_preview_skips_only_the_unchanged_theme() -> None:
    app = (ROOT / "src/spectrum/ui/setup.c").read_text(encoding="utf-8")
    handler = app[
        app.index("if (force_dirty & SETUP_MASK_BOARD)") :
        app.index("if (action == NETCHESSZX_SETUP_ACTION_SET)")
    ]
    assert "volatile uint8_t board_theme = setup_focus_board_theme;" in handler
    assert "netchesszx_board_theme_index != board_theme" in handler
    assert "spectrum_presentation_apply_theme(board_theme);" in handler
    assert "NETCHESSZX_NEXT" not in handler


def test_classic_theme_switch_repaints_only_attributes() -> None:
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    screen = read_asm(ROOT / "asm/spectrum/screen.asm")
    handler_start = app.index(
        "if (key == SPECTRUM_GUI_KEY_MENU_THEME ||\n"
        "        key == SPECTRUM_GUI_KEY_MENU_ASSET)"
    )
    handler = app[
        handler_start : app.index("key = nav_key_alias(key)", handler_start)
    ]
    draw_square = screen[
        screen.index("draw_one_board_square:") :
        screen.index("clear_square_pixels_2x2:")
    ]
    theme_start = screen.index("_netchesszx_board_theme_apply:")
    classic_apply = (ROOT / "asm/spectrum/board_theme.inc").read_text()

    assert handler.index("movement_hints_clear();") < handler.index(
        "spectrum_presentation_apply_theme("
    )
    assert handler.index("spectrum_presentation_apply_theme(") < handler.index(
        "cursor_request_hints();"
    )
    policy = (ROOT / "src/spectrum/ui/presentation_zx.h").read_text()
    assert "#define spectrum_presentation_apply_theme(theme) netchesszx_board_theme_apply(theme)" in policy
    assert "call z, clear_square_pixels_2x2" not in draw_square
    assert draw_square.index("call set_square_attr_2x2") < draw_square.index(
        "cp RENDER_ATTRS_ONLY"
    )
    assert draw_square.index("jp nz, draw_piece_sprite_16x16") < draw_square.index(
        "jp clear_square_pixels_2x2"
    )
    assert "ld a, RENDER_ATTRS_ONLY" in classic_apply
    assert "call draw_board" in classic_apply
    assert "jp restore_board_frame_attrs" in classic_apply
    assert "draw_board_frame" not in classic_apply


def test_mirrorshift_loader_cache_guard_precedes_io() -> None:
    for relative in (
        "asm/esxdos/overlay_loader.asm",
        "asm/next/overlay_loader_next.asm",
    ):
        source = (ROOT / relative).read_text(encoding="utf-8")
        guard = source.index("ovl_ensure_loaded:")
        load = source.index("ovl_load:", guard)
        prefix = source[guard:load]
        assert "_spectrum_overlay_loaded_id" in prefix
        assert "jr nz, ovl_load" in prefix
        assert "ret" in prefix
        assert "ovl_read" in source[load:] or "next_copy_bundle_ei" in source[load:]


def test_file_erase_requires_confirmation_and_refreshes_browser() -> None:
    app = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    pick = app[
        app.index("static uint8_t fileui_process_key") :
        app.index("static const char *move_display_text")
    ]
    key_handler = app.index("static uint8_t process_local_key")
    confirm_start = app.index("if (confirm_action != CONFIRM_NONE)", key_handler)
    confirm = app[
        confirm_start : app.index("SESSION_PEER_GATE(",
                                  confirm_start)
    ]

    assert "confirm_action = CONFIRM_FILE_ERASE;" in pick
    assert "spectrum_saveload_erase" not in pick
    assert "confirm_action == CONFIRM_FILE_ERASE" in confirm
    normalize = confirm.index("if (confirm_action == CONFIRM_FILE_ERASE)")
    accept = confirm.index("if (key == 'y')")
    reject = confirm.index("else if (key == 'n')")
    assert normalize < accept < reject
    assert "key = netchesszx_confirm_key_alias(key);" in confirm[normalize:accept]
    host_alias = app[
        app.index("static uint8_t netchesszx_confirm_key_alias") :
        app.index("#else", app.index("static uint8_t netchesszx_confirm_key_alias"))
    ]
    assert "key == 13u || key == ' '" in host_alias
    assert "key == 0x8au" in host_alias
    erase = confirm[
        confirm.index("if (confirm_action == CONFIRM_FILE_ERASE)", accept) :
        confirm.index("if (confirm_action == CONFIRM_DISCONNECT)", accept)
    ]
    assert "spectrum_saveload_erase" in erase
    assert "spectrum_fileui_rerender" in erase
    assert "goto reject_confirm;" in erase
    assert "about_restore_game" not in erase


def test_file_browser_allows_notice_row_rendering() -> None:
    gui = (ROOT / "src/spectrum/ui/gui.c").read_text(encoding="utf-8")
    notify = gui[
        gui.index("static void notify_internal") :
        gui.index("void spectrum_gui_notify(")
    ]
    tick = gui[
        gui.index("void spectrum_gui_present_tick") :
        gui.index("void spectrum_gui_reset_moves")
    ]

    assert "if (about_visible == 1u)" in notify
    assert "if (about_visible != 0u)" not in notify
    assert "if (about_visible != 1u)" in tick
    assert "if (about_visible == 0u)" not in tick
    assert gui.count("about_visible == 1u") == 1
    # The second use keeps the turn label off the About band (row 3).
    assert gui.count("about_visible != 1u") == 2
