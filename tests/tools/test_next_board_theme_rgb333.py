#!/usr/bin/env python3
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCREEN = ROOT / "asm" / "spectrum" / "screen.asm"
LOADER = ROOT / "asm" / "next" / "overlay_loader_next.asm"
GRAPHICS_BANK = ROOT / "asm" / "next" / "graphics_bank_next.asm"
GRAPHICS_LAYOUT = ROOT / "asm" / "next" / "extension_bank_layout.asm"
GEN_ASSETS = ROOT / "tools" / "gen_assets.py"
BUILD_SPRITES = ROOT / "tools" / "build_next_piece_sprites.py"
SPRITE_PALETTE_BIN = ROOT / "assets" / "next" / "lichess_sprite_palette.bin"
SPRITE_BIN = ROOT / "assets" / "next" / "lichess_piece_sprites.bin"
MENU_CONFIG = ROOT / "asm" / "overlay" / "menu_config" / "entry_menu_config.asm"
SETUP = ROOT / "asm" / "overlay" / "setup" / "entry_setup.asm"
APP = ROOT / "src" / "spectrum" / "app" / "app.c"


def block(text: str, start: str, end: str) -> str:
    return text[text.index(start) : text.index(end, text.index(start))]


class NextBoardThemeRgb333Tests(unittest.TestCase):
    def setUp(self) -> None:
        self.screen = SCREEN.read_text(encoding="utf-8")
        self.loader = LOADER.read_text(encoding="utf-8")
        self.graphics_bank = GRAPHICS_BANK.read_text(encoding="utf-8")
        self.graphics_layout = GRAPHICS_LAYOUT.read_text(encoding="utf-8")
        self.gen_assets = GEN_ASSETS.read_text(encoding="utf-8")
        self.build_sprites = BUILD_SPRITES.read_text(encoding="utf-8")
        self.sprite_palette_bin = SPRITE_PALETTE_BIN.read_bytes()
        self.sprite_bin = SPRITE_BIN.read_bytes()
        self.menu_config = MENU_CONFIG.read_text(encoding="utf-8")
        self.setup = SETUP.read_text(encoding="utf-8")
        self.app = APP.read_text(encoding="utf-8")

    def test_exact_rgb333_pairs_and_private_attributes(self) -> None:
        self.assertIn("NEXT_BOARD_COORD_LINE_ATTR EQU 0x81", self.screen)
        self.assertIn("NEXT_BOARD_COORD_SELECTED_ATTR EQU 0x8a", self.screen)
        table = block(
            self.screen,
            "next_board_coord_rgb333:",
            "next_board_coord_rgb333_end:",
        )
        values = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", table)]
        self.assertEqual(
            values,
            [
                0xBB,
                0x00,
                0x4E,
                0x01,
                0xFF,
                0x00,
                0x95,
                0x01,
                0xFA,
                0x01,
                0xB1,
                0x01,
                0xD1,
                0x00,
                0x88,
                0x01,
            ],
        )

        frame = block(
            self.screen,
            "restore_board_frame_attrs:",
            "draw_one_board_square:",
        )
        self.assertIn("call board_light_line_attr", frame)
        self.assertIn("ld a, NEXT_BOARD_COORD_LINE_ATTR", frame)

    def test_next_hint_dots_drop_bright_without_changing_ink(self) -> None:
        hint = block(
            self.screen,
            "render_square_hint_loaded:",
            "_spectrum_render_square_mark:",
        )
        self.assertIn("and 0x38", hint)
        self.assertIn("ld a, (mark_mode)", hint)
        self.assertIn("HINT_RGB = (204, 186, 48)", self.build_sprites)
        self.assertIn('MARKER_HINT_KEY = "hint-dark-1"', self.build_sprites)

    def test_marker_refresh_does_not_overwrite_piece_patterns(self) -> None:
        self.assertIn(
            "SOURCE_MARKER_PATTERN_BASE = (\n"
            "    PIECE_SETS * PIECES_PER_SET + BOARD_THEMES * BOARD_TILES_PER_THEME\n"
            ")",
            self.build_sprites,
        )
        self.assertIn(
            "offset = SOURCE_MARKER_PATTERN_BASE * SPRITE_SIZE * SPRITE_SIZE",
            self.build_sprites,
        )
        for piece_index, marker_index in zip(range(22, 26), range(46, 50)):
            with self.subTest(piece_index=piece_index):
                self.assertNotEqual(
                    self.sprite_bin[piece_index * 256 : (piece_index + 1) * 256],
                    self.sprite_bin[marker_index * 256 : (marker_index + 1) * 256],
                )

    def test_palette_is_live_before_private_attributes_are_painted(self) -> None:
        initial = block(
            self.screen,
            "_spectrum_render_board:",
            "_spectrum_render_board_area:",
        )
        self.assertLess(
            initial.index("call next_board_coord_palette_sync"),
            initial.index("call draw_board_coords"),
        )
        apply_theme = block(
            self.screen,
            "_netchesszx_board_theme_apply:",
            "compute_screen_base:",
        )
        self.assertLess(
            apply_theme.index("call next_board_coord_palette_sync"),
            apply_theme.index("call restore_board_frame_attrs"),
        )
        self.assertNotIn("NEXTREG_ULA_CONTROL", apply_theme)

    def test_clean_classic_canvas_skips_redundant_square_clears(self) -> None:
        initial = block(
            self.screen,
            "_spectrum_render_board:",
            "_spectrum_render_board_area:",
        )
        restore = block(
            self.screen,
            "_spectrum_restore_game_center:",
            "restore_game_center_canvas:",
        )
        clean = block(self.screen, "draw_board_clean:", "draw_board:")

        self.assertIn("call draw_board_clean", initial)
        self.assertIn("call draw_board_clean", restore)
        self.assertIn("inc (hl)", clean)
        self.assertIn("call draw_board", clean)
        self.assertIn("dec (hl)", clean)

    def test_sent_cursor_move_does_not_clear_hints_twice(self) -> None:
        send = block(
            self.app,
            "static uint8_t send_local_move(const char *move)",
            "static uint8_t retry_pending_outgoing(void)",
        )
        cursor = block(
            self.app,
            "static uint8_t cursor_select_or_move(uint8_t key)",
            "static uint8_t restore_transfer_pending(void)",
        )
        sent = cursor[cursor.index("move_rc = send_local_move(move);") :]

        self.assertEqual(send.count("movement_hints_clear();"), 1)
        self.assertNotIn("movement_hints_clear();", sent)

    def test_standard_ula_groups_and_menu_tables_stay_on_main_contract(self) -> None:
        self.assertIn(
            "NEXT_BOARD_LIGHT_ATTRS = [0x78, 0x6F, 0x66, 0x77, 0x37]",
            self.gen_assets,
        )
        self.assertIn(
            "NEXT_BOARD_DARK_ATTRS = [0x07, 0x4D, 0x20, 0x56, 0x52]",
            self.gen_assets,
        )
        table = block(
            self.build_sprites,
            "STANDARD_ULA_PALETTE = bytes(",
            "PIPELINE_VERSION",
        )
        values = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", table)]
        self.assertEqual(
            values,
            [
                0x00,
                0x00,
                0x02,
                0x01,
                0xA0,
                0x00,
                0xA2,
                0x01,
                0x14,
                0x00,
                0x16,
                0x01,
                0xB4,
                0x00,
                0xB6,
                0x01,
                0x00,
                0x00,
                0x02,
                0x01,
                0xA0,
                0x00,
                0xA2,
                0x01,
                0x14,
                0x00,
                0x16,
                0x01,
                0xB4,
                0x00,
                0xB6,
                0x01,
                0x00,
                0x00,
                0x03,
                0x01,
                0xE0,
                0x00,
                0xE3,
                0x01,
                0x1C,
                0x00,
                0x1F,
                0x01,
                0xFC,
                0x00,
                0xFF,
                0x01,
                0x00,
                0x00,
                0x03,
                0x01,
                0xE0,
                0x00,
                0xE3,
                0x01,
                0x1C,
                0x00,
                0x1F,
                0x01,
                0xFC,
                0x00,
                0xFF,
                0x01,
                *([0x00, 0x00] * 16),
                0xD1,
                0x00,
                0x88,
                0x01,
                *([0x00, 0x00] * 6),
                0xD1,
                0x00,
                0x88,
                0x01,
            ],
        )
        self.assertEqual(len(self.sprite_palette_bin), 160 * 2 + len(values))
        self.assertEqual(self.sprite_palette_bin[-len(values) :], bytes(values))
        self.assertIn("ld e, 192", self.graphics_bank)
        init = block(
            self.graphics_bank,
            "ngb_sprite_system_init:",
            "ngb_palette_upload_pairs:",
        )
        self.assertLess(
            init.index("call ngb_palette_upload_pairs"),
            init.index("ld a, nextreg_ula_control"),
        )
        self.assertIn("or 0x08", init)
        self.assertRegex(
            self.graphics_layout,
            r"(?m)^nextreg_ula_control\s+EQU 0x68$",
        )
        self.assertRegex(
            self.graphics_layout,
            r"(?m)^next_ula_standard_palette_size\s+EQU 116$",
        )
        self.assertIn("PUBLIC nextreg_read", self.loader)
        self.assertIn("PUBLIC nextreg_write", self.loader)

    def test_next_menu_swatches_preserve_ula_palette_groups(self) -> None:
        swatch = block(
            self.menu_config,
            "menu_config_board_swatch:",
            "menu_config_board_swatch_store:",
        )
        self.assertRegex(
            swatch,
            r"IFDEF NETCHESSZX_NEXT\s+;[^\n]*\s+ELSE\s+or 0x40\s+ENDIF",
        )
        self.assertIn(
            "cp 0x37\n    jr nz, menu_config_board_swatch_attr_ready\n    ld a, 0xc1",
            self.menu_config,
        )
        paint = block(
            self.menu_config,
            "_menu_config_paint_attrs_ovl_entry:",
            "_menu_config_edit_line_ovl_entry:",
        )
        self.assertNotIn("ld a, 0x68", paint)
        self.assertIn(
            'DEFB 16, "BOARD  ", 127, "   ", 127, "   ", 127, "   ", '
            '127, "   ", 127, 0',
            self.menu_config,
        )

    def test_board_focus_previews_without_reapplying_selection(self) -> None:
        focus = block(self.setup, "su_focus_board:", "su_focus_set:")
        self.assertIn("call su_cycle_choice", focus)
        self.assertIn("set 3, (hl)", focus)
        self.assertNotIn("IFDEF NETCHESSZX_NEXT", focus)

        select = block(self.setup, "su_select_board:", "su_select_set:")
        self.assertIn("ld (_setup_game_focus), a", select)
        self.assertIn("call su_set_defined_row", select)
        self.assertIn("jp su_after_game_select", select)

        apply_action = block(
            self.app,
            "if (force_dirty & SETUP_MASK_BOARD)",
            "if (action == NETCHESSZX_SETUP_ACTION_SET)",
        )
        self.assertIn(
            "volatile uint8_t board_theme = setup_focus_board_theme;",
            apply_action,
        )
        self.assertIn(
            "netchesszx_board_theme_index != board_theme",
            apply_action,
        )
        self.assertIn(
            "netchesszx_board_theme_apply(board_theme);",
            apply_action,
        )
        self.assertIn("spectrum_gui_restore_board_area();", apply_action)
        self.assertNotIn("spectrum_gui_redraw_board_squares();", apply_action)


if __name__ == "__main__":
    unittest.main()
