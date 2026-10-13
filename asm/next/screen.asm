SECTION code_user

IFNDEF NETCHESSZX_NEXT_EXTENSION
PUBLIC _spectrum_render_board
PUBLIC _spectrum_render_board_area
PUBLIC _spectrum_render_status
PUBLIC _spectrum_render_status_error
PUBLIC _spectrum_render_clock
PUBLIC _spectrum_render_game_timer_clear
PUBLIC _spectrum_render_turn_label
PUBLIC _spectrum_render_notice
PUBLIC _spectrum_render_notice_error
PUBLIC _spectrum_render_notice_success
PUBLIC _spectrum_render_connection
PUBLIC _spectrum_render_menu
PUBLIC _spectrum_info_show_game
PUBLIC _spectrum_render_board_coord_mark
PUBLIC _spectrum_info_show_setup
PUBLIC _spectrum_info_show_game_setup
PUBLIC _spectrum_info_show_preflight
PUBLIC _spectrum_info_clear_tail
PUBLIC _spectrum_info_line
PUBLIC _spectrum_render_board_coords
PUBLIC _spectrum_render_square
PUBLIC _spectrum_render_square_attr
PUBLIC _spectrum_render_square_with_hint
PUBLIC _spectrum_render_square_mark
PUBLIC _spectrum_render_square_mark_with_hint
PUBLIC compute_square_bc
PUBLIC compute_attr_base
PUBLIC set_square_attr_2x2
PUBLIC compute_screen_base
PUBLIC board_row
PUBLIC board_col
PUBLIC tmp_attr
PUBLIC tmp_row
PUBLIC tmp_col
PUBLIC tmp_char
PUBLIC tmp_scan
PUBLIC draw_char64_pixels_at_tmp
PUBLIC _spectrum_render_moves
PUBLIC _spectrum_render_move_at
PUBLIC _spectrum_render_chat
PUBLIC _spectrum_render_chat_at
PUBLIC _spectrum_render_input
PUBLIC _spectrum_render_input_cell
PUBLIC _spectrum_input_frame_tick
PUBLIC _netchesszx_board_theme_apply
IFNDEF NETCHESSZX_NEXT_EXTENSION
PUBLIC _spectrum_input_parse_move
ENDIF
PUBLIC _netchesszx_setup_render_edit_line
PUBLIC _netchesszx_setup_compute_visible
PUBLIC _netchesszx_setup_paint_attrs
PUBLIC _netchesszx_setup_render_overlay
PUBLIC _netchesszx_setup_step_overlay
PUBLIC _netchesszx_setup_dispatch_overlay
PUBLIC _netchesszx_setup_time_ui
PUBLIC _netchesszx_setup_time_init
PUBLIC _netchesszx_setup_time_commit
PUBLIC _netchesszx_setup_nav_key_alias
PUBLIC _netchesszx_confirm_key_alias
PUBLIC _netchesszx_asm_timer_game
PUBLIC _netchesszx_asm_timer_clock
PUBLIC _netchesszx_asm_menu_roll
PUBLIC _netchesszx_asm_menu_key
PUBLIC _netchesszx_config_load_overlay
PUBLIC _netchesszx_config_save_overlay
PUBLIC _netchesszx_config_defaults_overlay
PUBLIC _spectrum_board_view_redraw_square
PUBLIC _spectrum_board_view_flipped
PUBLIC _spectrum_next_sprites_hide_all
PUBLIC piece_sprites_16x16
EXTERN nextreg_read
EXTERN _spectrum_gui_about_visible_state
EXTERN _spectrum_moves_header_shown
EXTERN _spectrum_frame_wait
EXTERN _spectrum_uart_background_pump
ENDIF

EXTERN _spectrum_gui_board_flipped
EXTERN _spectrum_gui_board_pieces_visible
EXTERN _spectrum_gui_redraw_square
EXTERN _netchesszx_movement_hints
EXTERN _netchesszx_hinted_rows
EXTERN _netchesszx_board_theme_index
EXTERN _netchesszx_board_light_attr
EXTERN _netchesszx_board_dark_attr
EXTERN _spectrum_overlay_exec
EXTERN _spectrum_overlay_exec_cached
EXTERN _spectrum_gui_edit_tick
IFNDEF NETCHESSZX_NEXT_EXTENSION
EXTERN netchesszx_load_stack_args2
ENDIF

SCREEN_BASE EQU 0x4000
ATTR_BASE   EQU 0x5800
NETCHESSZX_ASSET_BASE EQU 0x3500
NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0
SPECTRUM_OVL_INPUT_EDIT EQU 9
SPECTRUM_OVL_INPUT_EDIT_PARSE_MOVE_PRIVATE EQU 5
SPECTRUM_OVL_INPUT_EDIT_SETUP_LINE_PRIVATE EQU 6
SPECTRUM_OVL_MENU_CONFIG EQU 6
SPECTRUM_OVL_MENU_CONFIG_RUN EQU 0
SPECTRUM_OVL_MENU_CONFIG_PAINT_ATTRS EQU 1
SPECTRUM_OVL_MENU_CONFIG_VALIDATE_IP EQU 2
SPECTRUM_OVL_MENU_CONFIG_EDIT_LINE EQU 3
SPECTRUM_OVL_MENU_CONFIG_RENDER EQU 4
SPECTRUM_OVL_MENU_CONFIG_NAV_PRIVATE EQU 5
SPECTRUM_OVL_SETUP EQU 8
SPECTRUM_OVL_SETUP_STEP EQU 0
SPECTRUM_OVL_SETUP_COMPUTE_VISIBLE_PRIVATE EQU 1
SPECTRUM_OVL_CONTROL_FORMAT_BUSY_PRIVATE EQU 1
SPECTRUM_OVL_GUI_LOG_ANIMATE_PRIVATE EQU 4
SPECTRUM_OVL_GUI_LOG_MORPH_PRIVATE EQU 5
SPECTRUM_OVL_GUI_LOG_RESTORE_PANELS_PRIVATE EQU 6
SPECTRUM_OVL_GUI_LOG_APPLY_MOVE_PRIVATE EQU 7
SPECTRUM_OVL_TIME_CONFIG EQU 16
SPECTRUM_OVL_TIME_CONFIG_UI EQU 0
SPECTRUM_OVL_TIME_CONFIG_STEP EQU 1
SPECTRUM_OVL_TIME_CONFIG_INIT EQU 2
SPECTRUM_OVL_TIME_CONFIG_COMMIT EQU 3
SPECTRUM_OVL_CONFIG EQU 15
SPECTRUM_OVL_CONFIG_LOAD EQU 0
SPECTRUM_OVL_CONFIG_SAVE EQU 1
SPECTRUM_OVL_CONFIG_DEFAULTS EQU 2
DEFC _spectrum_board_view_redraw_square = _spectrum_gui_redraw_square
DEFC _spectrum_board_view_flipped = _spectrum_gui_board_flipped
expand_2x EQU NETCHESSZX_ASSET_BASE
font_lut EQU NETCHESSZX_ASSET_BASE + 16
chat_icon_white EQU NETCHESSZX_ASSET_BASE + 26
chat_icon_black EQU NETCHESSZX_ASSET_BASE + 32
timer_ikkle_packed EQU NETCHESSZX_ASSET_BASE + 38
font_packed EQU NETCHESSZX_ASSET_BASE + 166
badge_pattern EQU NETCHESSZX_ASSET_BASE + 454
conn_pattern EQU NETCHESSZX_ASSET_BASE + 462
rank_digit_patterns EQU NETCHESSZX_ASSET_BASE + 470
file_letter_patterns EQU NETCHESSZX_ASSET_BASE + 510
psfc_piece_chars EQU NETCHESSZX_ASSET_BASE + 550
title_msg EQU NETCHESSZX_ASSET_BASE + 556
chat_msg EQU NETCHESSZX_ASSET_BASE + 567
session_setup_msg EQU NETCHESSZX_ASSET_BASE + 572
game_setup_msg EQU NETCHESSZX_ASSET_BASE + 589
preflight_setup_msg EQU NETCHESSZX_ASSET_BASE + 600
input_prompt_msg EQU NETCHESSZX_ASSET_BASE + 622
white_turn_msg EQU NETCHESSZX_ASSET_BASE + 625
black_turn_msg EQU NETCHESSZX_ASSET_BASE + 639
white_check_msg EQU NETCHESSZX_ASSET_BASE + 653
black_check_msg EQU NETCHESSZX_ASSET_BASE + 665
menu_cursor_masks EQU NETCHESSZX_ASSET_BASE + 677
tab_label_file EQU NETCHESSZX_ASSET_BASE + 701
tab_label_link EQU NETCHESSZX_ASSET_BASE + 706
tab_label_game EQU NETCHESSZX_ASSET_BASE + 712
tab_label_flip EQU NETCHESSZX_ASSET_BASE + 718
tab_label_theme EQU NETCHESSZX_ASSET_BASE + 723
tab_label_about EQU NETCHESSZX_ASSET_BASE + 729
moves_white_msg EQU NETCHESSZX_ASSET_BASE + 735
moves_black_msg EQU NETCHESSZX_ASSET_BASE + 741
banner_info_top_msg EQU NETCHESSZX_ASSET_BASE + 747
board_theme_mark_inks EQU NETCHESSZX_ASSET_BASE + 781
board_theme_light_attrs EQU NETCHESSZX_ASSET_BASE + 786
board_theme_dark_attrs EQU NETCHESSZX_ASSET_BASE + 791
version_banner_msg EQU NETCHESSZX_ASSET_BASE + 796
taboption_labels EQU NETCHESSZX_ASSET_BASE + 812
taboption_transitions EQU NETCHESSZX_ASSET_BASE + 867
taboption_actions EQU NETCHESSZX_ASSET_BASE + 878
piece_sprites_16x16 EQU NETCHESSZX_ASSET_BASE + 889

INCLUDE "ui_layout.inc"
RENDER_ATTRS_ONLY EQU 2
NEXT_SPRITE_Y_BASE EQU 32 + (NETCHESSZX_GAME_BOARD_TOP_ROW * 8)
NEXT_SPRITE_X_BASE EQU 32 + (NETCHESSZX_GAME_BOARD_LEFT_COL * 8)
NEXT_SPRITE_SLOT_PORT EQU 0x303B
NEXT_SPRITE_ATTR_PORT EQU 0x0057
NEXT_SPRITE_VISIBLE EQU 0x80
NEXT_BOARD_SPRITE_SLOT_BASE EQU 0
NEXT_PIECE_SPRITE_SLOT_BASE EQU 64
NEXT_MARKER_SPRITE_SLOT_BASE EQU 96
NEXT_BOARD_PATTERN_BASE EQU 12
NEXT_MARKER_PATTERN_BASE EQU 22
NEXT_MARKER_FLAG_HINT EQU 1
NEXT_MARKER_FLAG_MARK EQU 2
NEXT_MARKER_FLAG_SELECTED EQU 4
NEXT_EMPTY_SLOT EQU 0xff
NEXTREG_SELECT_PORT EQU 0x243b
NEXTREG_DATA_PORT EQU 0x253b
NEXTREG_PALETTE_INDEX EQU 0x40
NEXTREG_PALETTE_CONTROL EQU 0x43
NEXTREG_PALETTE_VALUE_9 EQU 0x44
INCLUDE "assets/next/theme_constants.asm"
; 9 * 6px lines + 2px lead-in, stop before notice row 21 (Y=168).
NETCHESSZX_MOVE_LINES_BASE EQU 0x5cb6
NETCHESSZX_CHAT_LINES_BASE EQU 0x5d96

IFDEF NETCHESSZX_NEXT_EXTENSION
EXTERN board_ptr
EXTERN status_ptr
EXTERN notice_ptr
EXTERN text_ptr
EXTERN move_ptr
EXTERN current_attr
EXTERN board_iter
EXTERN piece_row
EXTERN piece_col
EXTERN piece_char
EXTERN piece_scan
EXTERN scaled_row
EXTERN scaled_col
EXTERN conn_attr
EXTERN sprite_ptr
EXTERN mark_mode
INCLUDE "asm/platform/input_state_extern.inc"
EXTERN render_skip_clear
EXTERN tmp_attr
EXTERN tmp_char
EXTERN tmp_col
EXTERN tmp_row
EXTERN tmp_scan
EXTERN piece_slot_squares
EXTERN marker_slot_squares
EXTERN marker_slot_flags
EXTERN compute_square_bc
EXTERN draw_icon_sprite_at_tmp
EXTERN square_parity
ELSE
SECTION bss_user

board_ptr:    DEFS 2
status_ptr:   DEFS 2
notice_ptr:   DEFS 2
text_ptr:     DEFS 2
move_ptr:     DEFS 2
tmp_row:      DEFS 1
tmp_col:      DEFS 1
tmp_char:     DEFS 1
tmp_attr:     DEFS 1
tmp_scan:     DEFS 1
current_attr: DEFS 1
board_row:    DEFS 1
board_col:    DEFS 1
board_iter:   DEFS 2
piece_row:    DEFS 1
piece_col:    DEFS 1
piece_char:   DEFS 1
piece_scan:   DEFS 1
scaled_row:   DEFS 1
scaled_col:   DEFS 1
conn_attr:    DEFS 1
sprite_ptr:   DEFS 2
mark_mode:    DEFS 1
INCLUDE "asm/platform/input_state.inc"
render_skip_clear: DEFS 1
piece_slot_squares: DEFS 32
marker_slot_squares: DEFS 32
marker_slot_flags: DEFS 32
ENDIF

SECTION code_user

INCLUDE "asm/platform/input_queue.inc"

IFDEF NETCHESSZX_NEXT_RESIDENT
EXTERN next_extension_restore
INCLUDE "asm/next/extension_bank_layout.asm"
INCLUDE "asm/next/screen_extension_stubs_top.asm"
ELSE
INCLUDE "asm/next/board_open.inc"
INCLUDE "asm/ula/panel_entries.inc"

ENDIF

IFNDEF NETCHESSZX_NEXT_EXTENSION

_spectrum_render_square:
    call read_square_spec
    jp draw_one_board_square

_spectrum_render_square_attr:
    call read_square_spec
    ld d, a
    call compute_square_bc
    ld a, d
    jp set_square_attr_2x2

INCLUDE "asm/next/board_hints_view.inc"

INCLUDE "asm/app/board_hints_show.inc"

INCLUDE "asm/app/screen_dispatch.inc"
INCLUDE "asm/ula/log_view.inc"

ENDIF

IFDEF NETCHESSZX_NEXT_RESIDENT
INCLUDE "asm/next/screen_extension_stubs_tail.asm"
ELSE

INCLUDE "asm/ula/panel_draw.inc"
INCLUDE "asm/ula/board_coords.inc"
INCLUDE "asm/next/board_draw.inc"
INCLUDE "asm/ula/scaled_text.inc"
INCLUDE "asm/ula/text.inc"
INCLUDE "asm/ula/panel_primitives.inc"
INCLUDE "asm/next/board_theme.inc"
INCLUDE "asm/ula/address.inc"
INCLUDE "asm/ula/badge.inc"
INCLUDE "asm/next/board_mark.inc"

ENDIF

IFNDEF NETCHESSZX_NEXT_EXTENSION
INCLUDE "asm/spectrum/setup_state.inc"
INCLUDE "asm/spectrum/timer_roll.asm"
ENDIF
