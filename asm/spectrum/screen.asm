SECTION code_user

EXTERN _netchesszx_version_banner_msg

PUBLIC _spectrum_render_board
PUBLIC _spectrum_render_board_area
PUBLIC _spectrum_render_status
PUBLIC _spectrum_render_status_error
PUBLIC _spectrum_render_clock
PUBLIC _spectrum_render_game_timer_clear
PUBLIC _spectrum_render_game_timer_char
PUBLIC _spectrum_render_menu_timer_char
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
PUBLIC _spectrum_setup_board_swatches
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
PUBLIC _spectrum_board_show_legal_hints
PUBLIC _spectrum_render_moves
PUBLIC _spectrum_render_move_at
PUBLIC _spectrum_render_moves_scroll
PUBLIC _spectrum_render_chat
PUBLIC _spectrum_render_chat_at
PUBLIC _spectrum_render_chat_scroll
PUBLIC _spectrum_render_input
PUBLIC _spectrum_render_input_cell
PUBLIC _spectrum_key_edit_pressed
PUBLIC _spectrum_key_poll
PUBLIC _netchesszx_board_theme_apply
PUBLIC _spectrum_info_panel_overlay_line
PUBLIC _spectrum_input_parse_move
PUBLIC _netchesszx_setup_update_room_code
PUBLIC _netchesszx_setup_render_edit_line
PUBLIC _netchesszx_setup_compute_visible
PUBLIC _netchesszx_setup_step_row
PUBLIC _netchesszx_setup_paint_attrs
PUBLIC _netchesszx_setup_render_rows
PUBLIC _netchesszx_setup_room_editable
PUBLIC _netchesszx_setup_room_backspace
PUBLIC _netchesszx_setup_room_append
PUBLIC _netchesszx_setup_move_focus
PUBLIC _netchesszx_setup_validate_ip
PUBLIC _spectrum_board_clear_legal_hints
PUBLIC _spectrum_board_view_redraw_square
PUBLIC _spectrum_board_view_flipped
PUBLIC board_theme_hint_inks

EXTERN _spectrum_gui_board_flipped
EXTERN _spectrum_gui_redraw_square
EXTERN _netchesszx_movement_hints
EXTERN _netchesszx_hinted_rows
EXTERN _netchesszx_board_theme_index
EXTERN _netchesszx_board_light_attr
EXTERN _netchesszx_board_dark_attr
EXTERN _spectrum_overlay_exec
EXTERN _spectrum_overlay_exec_cached
EXTERN _side_to_move
EXTERN _castle_rights
EXTERN _ep_square

SCREEN_BASE EQU 0x4000
ATTR_BASE   EQU 0x5800
NETCHESSZX_ASSET_BASE EQU 0x6000
NETCHESSZX_RULES_BOARD_BASE EQU 0x5fa0
NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0
SPECTRUM_OVL_GUI_LOG EQU 2
SPECTRUM_OVL_APP_INPUT EQU 2
SPECTRUM_OVL_APP_INPUT_PARSE_MOVE EQU 2
SPECTRUM_OVL_GUI_LOG_CONNECTION_PANEL EQU 3
SPECTRUM_OVL_MENU_CONFIG EQU 6
SPECTRUM_OVL_MENU_CONFIG_RUN EQU 0
SPECTRUM_OVL_MENU_CONFIG_PAINT_ATTRS EQU 1
SPECTRUM_OVL_MENU_CONFIG_VALIDATE_IP EQU 2
SPECTRUM_OVL_MENU_CONFIG_EDIT_LINE EQU 3
SPECTRUM_OVL_MENU_LOGIC EQU 7
SPECTRUM_OVL_MENU_LOGIC_UPDATE_ROOM EQU 0
SPECTRUM_OVL_MENU_LOGIC_MOVE_FOCUS EQU 1
SPECTRUM_OVL_MENU_LOGIC_ROOM_APPEND EQU 2
SPECTRUM_OVL_MENU_LOGIC_ROOM_EDITABLE EQU 3
SPECTRUM_OVL_MENU_LOGIC_ROOM_BACKSPACE EQU 4
SPECTRUM_OVL_MENU_LOGIC_COMPUTE_VISIBLE EQU 5
SPECTRUM_OVL_MENU_LOGIC_STEP_ROW EQU 6
SPECTRUM_OVL_HINTS EQU 8
SPECTRUM_OVL_HINTS_SHOW EQU 0
SPECTRUM_OVL_HINTS_CLEAR EQU 1
SPECTRUM_OVL_STATUS EQU SPECTRUM_OVL_MENU_LOGIC
SPECTRUM_OVL_STATUS_PHASE EQU 7
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
piece_sprites_16x16 EQU NETCHESSZX_ASSET_BASE + 639

NETCHESSZX_GAME_BOARD_TOP_ROW EQU 5
NETCHESSZX_GAME_BOARD_LEFT_COL EQU 1
NETCHESSZX_INFO_PANEL_COL EQU 18
NETCHESSZX_INFO_TEXT_COL  EQU (NETCHESSZX_INFO_PANEL_COL * 2) + 1
NETCHESSZX_INFO_HEADER_ROW EQU 5
NETCHESSZX_INFO_HEADER_HLINE_ROW EQU 6
NETCHESSZX_INFO_SETUP_GAME_HEADER_ROW EQU 12
NETCHESSZX_INFO_SETUP_GAME_HLINE_ROW EQU 13
NETCHESSZX_INFO_PANEL_CLEAR_FIRST_ROW EQU NETCHESSZX_INFO_HEADER_ROW
NETCHESSZX_INFO_NOTICE_ROW EQU 21
NETCHESSZX_INFO_PANEL_CLEAR_LIMIT_ROW EQU NETCHESSZX_INFO_NOTICE_ROW + 1
NETCHESSZX_INFO_SETUP_LINE_BASE_ROW EQU 7
NETCHESSZX_INFO_MOVES_FIRST_ROW EQU 6
NETCHESSZX_INFO_MOVES_FIRST_Y EQU 51
NETCHESSZX_INFO_MOVES_CLEAR_Y EQU 50
NETCHESSZX_INFO_MOVES_CLEAR_HEIGHT EQU 43
NETCHESSZX_MOVE_ROWS EQU 7
NETCHESSZX_INFO_TIGHT_LINE_STEP EQU 6
NETCHESSZX_INFO_CHAT_TITLE_ROW EQU 12
NETCHESSZX_INFO_CHAT_HLINE_ROW EQU 13
NETCHESSZX_INFO_CHAT_FIRST_ROW EQU 16
NETCHESSZX_INFO_CHAT_FIRST_Y EQU 110
NETCHESSZX_INFO_CHAT_CLEAR_Y EQU 108
NETCHESSZX_INFO_CHAT_CLEAR_HEIGHT EQU 67
NETCHESSZX_CHAT_ROWS EQU 9
NETCHESSZX_STATUS_ROW     EQU 22
NETCHESSZX_INPUT_ROW       EQU 23
NETCHESSZX_MOVE_LINES_BASE EQU 0x5cb6
NETCHESSZX_MOVE_SLOT_SIZE EQU 32
NETCHESSZX_MOVE_BLACK_OFFSET EQU 18
NETCHESSZX_MOVE_BLACK_COL EQU NETCHESSZX_INFO_TEXT_COL + 14
NETCHESSZX_CHAT_SLOT_SIZE EQU 28
NETCHESSZX_CHAT_LINES_BASE EQU 0x5d96
NETCHESSZX_CHAT_TEXT_OFFSET EQU 7
NETCHESSZX_CHAT_ICON_BYTE_COL EQU NETCHESSZX_INFO_PANEL_COL
NETCHESSZX_CHAT_TIME_COL EQU NETCHESSZX_INFO_TEXT_COL + 2
NETCHESSZX_CHAT_TEXT_COL EQU NETCHESSZX_INFO_TEXT_COL + 8
NETCHESSZX_TOP_TIMER_ROW  EQU 2
NETCHESSZX_TOP_TIMER_SCAN EQU 2
NETCHESSZX_TOP_TIMER_COL  EQU 41
NETCHESSZX_TOP_TURN_ROW   EQU 3
NETCHESSZX_TOP_TURN_SCAN  EQU 2
NETCHESSZX_TOP_TURN_CLEAR_COL EQU 51
NETCHESSZX_TOP_TURN_BYTE_COL EQU 25
NETCHESSZX_TOP_TURN_WIDTH_BYTES EQU 7
NETCHESSZX_STATUS_CLOCK_COL EQU 55
NETCHESSZX_STATUS_TEXT_COL EQU 1
NETCHESSZX_STATUS_SCAN EQU 2
NETCHESSZX_STATUS_LEFT_BYTES EQU 27
NETCHESSZX_MOVES_HEADER_WHITE_ICON_BYTE_COL EQU NETCHESSZX_INFO_PANEL_COL
NETCHESSZX_MOVES_HEADER_BLACK_ICON_BYTE_COL EQU NETCHESSZX_INFO_PANEL_COL + 7
NETCHESSZX_MOVES_HEADER_WHITE_TEXT_COL EQU NETCHESSZX_INFO_TEXT_COL + 3
NETCHESSZX_MOVES_HEADER_BLACK_TEXT_COL EQU NETCHESSZX_INFO_TEXT_COL + 17
NETCHESSZX_MOVES_HEADER_DIVIDER_BYTE_COL EQU NETCHESSZX_INFO_PANEL_COL + 7
NETCHESSZX_MENU_ROW EQU 2
NETCHESSZX_MENU_SCAN EQU 2
NETCHESSZX_MENU_CURSOR_SCAN EQU 1
NETCHESSZX_MENU_ABOUT_COL EQU 0
NETCHESSZX_MENU_DISCC_COL EQU 7
NETCHESSZX_MENU_REST_COL EQU 13
NETCHESSZX_MENU_FLIP_COL EQU 18
NETCHESSZX_MENU_THEME_COL EQU 24
NETCHESSZX_BANNER_INFO_COL EQU 18

ATTR_TEXT       EQU 0x07
ATTR_BANNER_TOP EQU 0x47
ATTR_BANNER_BOT EQU 0x07
ATTR_STATUS     EQU 0x38
ATTR_STATUS_ERROR EQU 0x3a
ATTR_TIMER      EQU 0x07
ATTR_TIMER_INV  EQU 0x38
ATTR_TIMER_CHECK_WHITE EQU 0x06
ATTR_TIMER_CHECK_BLACK EQU 0x30
ATTR_NOTICE     EQU 0x06
ATTR_ERROR      EQU 0x42
ATTR_SUCCESS    EQU 0x44
ATTR_CONN_OFF   EQU 0x3a
ATTR_CONN_MQTT  EQU 0x3e
ATTR_CONN_ON    EQU 0x3c
ATTR_MOVES_TITLE EQU 0x06
ATTR_CHAT_TITLE  EQU 0x06
ATTR_CHAT_BLACK  EQU 0x38
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
key_last:     DEFS 1
key_repeat_timer: DEFS 1

SECTION code_user

EXTERN _netchesszx_version_banner_msg

_spectrum_render_board:
    ld (board_ptr), hl
    call clear_screen
    call draw_banner
    call hide_menu
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board
    jp draw_board_frame

_spectrum_render_board_area:
    ld (board_ptr), hl
    call clear_board_coords
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board
    jp draw_board_frame

_spectrum_render_board_coords:
    call clear_board_coords
    call restore_board_frame_attrs
    call draw_board_coords
    jp draw_board_frame

_spectrum_render_board_coord_mark:
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld a, (hl)
    or a
    jr z, srcm_text
    ld a, (_netchesszx_board_light_attr)
    jr srcm_attr_ready
srcm_text:
    call board_light_line_attr
srcm_attr_ready:
    ld (tmp_attr), a

    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    ld c, a
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_attr_base
    ld a, c
    add a, l
    ld l, a
    ld a, (tmp_attr)
    ld (hl), a
    inc hl
    ld (hl), a

    ld a, (board_row)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld a, (tmp_attr)
    ld (hl), a
    ld de, 32
    add hl, de
    ld (hl), a
    ret

_spectrum_render_status:
    ld a, ATTR_STATUS
    jr render_status_common

_spectrum_render_status_error:
    ld a, ATTR_STATUS_ERROR

render_status_common:
    ; Caller pads the text with spaces to the full left width; ikkle drawing
    ; self-clears each cell, so no destructive pre-clear (avoids flash).
    ld (status_ptr), hl
    ld (current_attr), a
    ld b, NETCHESSZX_STATUS_ROW
    ld c, NETCHESSZX_STATUS_TEXT_COL
    ld d, NETCHESSZX_STATUS_SCAN
    call store_tmp_rcs
    ld hl, (status_ptr)
    jp draw_ikkle_text_at

_spectrum_render_clock:
    ld a, ATTR_STATUS
    ld (current_attr), a
    ld b, NETCHESSZX_STATUS_ROW
    ld c, NETCHESSZX_STATUS_CLOCK_COL
    ld d, NETCHESSZX_STATUS_SCAN
    call store_tmp_rcs
    jp draw_ikkle_text_at

_spectrum_render_game_timer_clear:
    ld (text_ptr), hl
    ld a, ATTR_TIMER
    ld (current_attr), a
    ld b, NETCHESSZX_TOP_TIMER_ROW
    ld c, NETCHESSZX_TOP_TIMER_COL
    ld de, NETCHESSZX_TOP_TIMER_SCAN * 256 + 12
    call clear_ikkle_region
    ld hl, (text_ptr)
    jp draw_ikkle_text_at

; Game and menu timer chars share position (row 2, scan 2, col base 41);
; only the attribute differs between the closed and taboption states.
_spectrum_render_game_timer_char:
    ld a, ATTR_TIMER
    jr render_timer_char_common
_spectrum_render_menu_timer_char:
    ld a, ATTR_STATUS
render_timer_char_common:
    ld (current_attr), a
    ld a, (hl)
    add a, NETCHESSZX_TOP_TIMER_COL
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (tmp_char), a
    inc hl
    ld a, (hl)
    ld (mark_mode), a
    ld a, NETCHESSZX_TOP_TIMER_ROW
    ld (tmp_row), a
    ld a, NETCHESSZX_TOP_TIMER_SCAN
    ld (tmp_scan), a

    ld a, (tmp_row)
    call compute_screen_base
    ld a, (tmp_scan)
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_col)
    ld c, a
    ld a, (mark_mode)
    or a
    jr z, srtc_no_clear
    call ikkle_clear_cell_hl
srtc_no_clear:
    push hl
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (current_attr)
    ld (hl), a
    pop hl

    ld a, (tmp_char)
    cp 33
    ret c
    cp 128
    ret nc
    cp 96
    jr c, srtc_no_fold
    sub 32
srtc_no_fold:
    sub 32
    add a, a
    ld e, a
    ld d, 0
    push hl
    ld hl, timer_ikkle_packed
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    pop hl
    jp ikkle_blit_de_hl

_spectrum_render_turn_label:
    ld a, l
    ld (mark_mode), a
    ld a, ATTR_TIMER
    ld (current_attr), a
    call clear_turn_strip
    ld a, (mark_mode)
    cp 4
    ret nc
    cp 2
    jr z, srtl_white_check
    cp 3
    jr z, srtl_black_check
    or a
    jr nz, srtl_black
    ld a, ATTR_TIMER
    ld (current_attr), a
    ld a, NETCHESSZX_TOP_TURN_CLEAR_COL
    ld hl, white_turn_msg
    jr srtl_draw
srtl_black:
    jp draw_black_turn_row
srtl_white_check:
    ld a, ATTR_TIMER_CHECK_WHITE
    ld (current_attr), a
    ld a, NETCHESSZX_TOP_TURN_CLEAR_COL + 2
    ld hl, white_check_msg
    jr srtl_draw
srtl_black_check:
    ld a, ATTR_TIMER_CHECK_BLACK
    ld (current_attr), a
    ld a, NETCHESSZX_TOP_TURN_CLEAR_COL + 2
    ld hl, black_check_msg
srtl_draw:
    ld (tmp_col), a
    ld a, NETCHESSZX_TOP_TURN_ROW
    ld (tmp_row), a
    ld a, NETCHESSZX_TOP_TURN_SCAN
    ld (tmp_scan), a
    jp draw_ikkle_text_at

draw_black_turn_row:
    ld a, NETCHESSZX_TOP_TURN_ROW
    call compute_attr_base
    ld a, NETCHESSZX_TOP_TURN_BYTE_COL
    add a, l
    ld l, a
    ld b, NETCHESSZX_TOP_TURN_WIDTH_BYTES
    ld a, ATTR_TIMER
dbtr_attr:
    ld (hl), a
    inc hl
    djnz dbtr_attr

    ld a, NETCHESSZX_TOP_TURN_ROW
    call compute_screen_base
    ld a, NETCHESSZX_TOP_TURN_BYTE_COL
    add a, l
    ld l, a
    ex de, hl
    ld hl, black_turn_row_data
    ld a, 8
dbtr_scan:
    push de
    ld bc, NETCHESSZX_TOP_TURN_WIDTH_BYTES
    ldir
    pop de
    inc d
    dec a
    jr nz, dbtr_scan
    ret

clear_turn_strip:
    ld a, NETCHESSZX_TOP_TURN_ROW
    call compute_screen_base
    ld a, NETCHESSZX_TOP_TURN_BYTE_COL
    add a, l
    ld l, a
    ld c, 8
cts_scan:
    ld b, NETCHESSZX_TOP_TURN_WIDTH_BYTES
    push hl
    xor a
cts_fill:
    ld (hl), a
    inc hl
    djnz cts_fill
    pop hl
    inc h
    dec c
    jr nz, cts_scan
    ret

black_turn_row_data:
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00
    DEFB 0x1f,0xff,0xff,0xff,0xff,0xff,0xff
    DEFB 0x11,0x71,0x35,0xf1,0x1f,0x51,0x51
    DEFB 0x11,0x75,0x73,0xfb,0x5f,0x15,0x53
    DEFB 0x15,0x71,0x75,0xfb,0x5f,0x15,0x57
    DEFB 0x11,0x35,0x35,0xfb,0x1f,0x51,0xb1
    DEFB 0x1f,0xff,0xff,0xff,0xff,0xff,0xff
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00

white_check_msg:
    DEFB "WHITE CHECK",0
black_check_msg:
    DEFB "BLACK CHECK",0

_spectrum_render_notice:
    ld a, ATTR_NOTICE
    jr render_notice_common

_spectrum_render_notice_error:
    ld a, ATTR_ERROR
    jr render_notice_common

_spectrum_render_notice_success:
    ld a, ATTR_SUCCESS

render_notice_common:
    ld (notice_ptr), hl
    ld (current_attr), a
    call clear_notice_row
    ld hl, (notice_ptr)
    call calc_len64
    ld (tmp_col), a
    ld a, NETCHESSZX_INFO_NOTICE_ROW
    ld (tmp_row), a
    ld a, 2
    ld (tmp_scan), a
    ld hl, (notice_ptr)
    jp draw_ikkle_text_at

_spectrum_render_connection:
    ld a, l
    or a
    jr z, src_off
    cp 1
    jr z, src_mqtt
    ld a, ATTR_CONN_ON
    jr src_set
src_mqtt:
    ld a, ATTR_CONN_MQTT
    jr src_set
src_off:
    ld a, ATTR_CONN_OFF
src_set:
    ld (conn_attr), a
    jp draw_connection_indicator

_spectrum_render_menu:
    ld a, l
    or a
    jp z, hide_menu
    bit 7, a
    jp nz, draw_menu_partial
    jp draw_menu

_spectrum_info_show_game:
    call clear_right_panel_rows
    jp draw_static_panels

_spectrum_info_show_setup:
    call clear_right_panel_rows
    jp draw_connection_setup_header

_spectrum_info_show_game_setup:
    ld b, NETCHESSZX_INFO_SETUP_GAME_HEADER_ROW
    ld hl, game_setup_msg
    call draw_setup_header_at_b
    ld a, NETCHESSZX_INFO_SETUP_GAME_HLINE_ROW
    jp draw_right_hline

draw_connection_setup_header:
    ld b, NETCHESSZX_INFO_HEADER_ROW
    ld hl, session_setup_msg
    call draw_setup_header_at_b
    ld a, NETCHESSZX_INFO_HEADER_HLINE_ROW
    jp draw_right_hline

store_tmp_rcs:
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, d
    ld (tmp_scan), a
    ret

attr_cell_for_tmpcol:
    call compute_attr_base
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ret

draw_setup_header_at_b:
    ld a, ATTR_MOVES_TITLE
    ld (current_attr), a
    ld c, NETCHESSZX_INFO_TEXT_COL
    ld d, 2
    call store_tmp_rcs
    jp draw_ikkle_text_at

_spectrum_info_show_preflight:
    call clear_right_panel_rows
    ld a, ATTR_MOVES_TITLE
    ld (current_attr), a
    ld b, NETCHESSZX_INFO_HEADER_ROW
    ld c, NETCHESSZX_INFO_TEXT_COL
    ld d, 2
    call store_tmp_rcs
    ld hl, preflight_setup_msg
    call draw_ikkle_text_at
    ld a, NETCHESSZX_INFO_HEADER_HLINE_ROW
    jp draw_right_hline

clear_right_panel_rows:
    ld a, NETCHESSZX_INFO_PANEL_CLEAR_FIRST_ROW
srsp_clear_loop:
    push af
    call clear_right_text_row
    pop af
    inc a
    cp NETCHESSZX_INFO_PANEL_CLEAR_LIMIT_ROW
    jr nz, srsp_clear_loop
    ret

_spectrum_info_clear_tail:
    ld a, l
    add a, NETCHESSZX_INFO_SETUP_LINE_BASE_ROW
srsct_loop:
    cp NETCHESSZX_INFO_PANEL_CLEAR_LIMIT_ROW
    ret nc
    push af
    call clear_right_text_row
    pop af
    inc a
    jr srsct_loop

_spectrum_info_line:
    ld a, (hl)
    inc hl
    ld b, a
    ld c, NETCHESSZX_INFO_TEXT_COL
    ld d, 14
    jp draw_text64_line_pixels_fast

_spectrum_render_square:
    call read_square_spec
    jp draw_one_board_square

_spectrum_render_square_attr:
    call read_square_spec
    ld d, a
    call compute_square_bc
    ld a, d
    jp set_square_attr_2x2

_spectrum_render_square_with_hint:
    push hl
    call _spectrum_render_square
    pop hl
    ld a, (_netchesszx_movement_hints)
    or a
    ret z
    push hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    cp 8
    jp nc, rsh_no_hint
    ld e, a
    ld d, 0
    inc hl
    ld a, (hl)
    cp 8
    jp nc, rsh_no_hint
    ld c, a
    ld hl, _netchesszx_hinted_rows
    add hl, de
    ld a, (hl)
    ld e, a
    ld b, 0
    ld hl, hint_col_mask
    add hl, bc
    ld a, e
    and (hl)
    pop hl
    ret z
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld c, 0
    ld a, (hl)
    cp '.'
    jr z, rsh_piece_ready
    ld c, 0x80
rsh_piece_ready:
    ld a, (_netchesszx_board_theme_index)
    ld e, a
    ld d, 0
    ld hl, board_theme_hint_inks
    add hl, de
    ld a, (hl)
    or c
    ld (tmp_attr), a

render_square_hint_loaded:
    call compute_square_bc
    ld a, b
    call compute_attr_base
    ld a, c
    add a, l
    ld l, a
    ld a, (hl)
    and 0x78
    ld d, a
    ld a, (tmp_attr)
    and 0x07
    or d
    call set_square_attr_2x2

    ld a, (tmp_attr)
    add a, a
    ret c

    ld a, b
    call compute_screen_base
    ld a, 6
    add a, h
    ld h, a
    ld a, c
    add a, l
    ld l, a

    ld de, 0x8001
    call draw_dot_row
    dec l
    inc h
    ld de, 0xc003
    call draw_dot_row

    ld a, b
    inc a
    call compute_screen_base
    ld a, c
    add a, l
    ld l, a

    ld de, 0xc003
    call draw_dot_row
    dec l
    inc h
    ld de, 0x8001
    jp draw_dot_row

rsh_no_hint:
    pop hl
    ret

draw_dot_row:
    ld a, (hl)
    or e
    ld (hl), a
    inc l
    ld a, (hl)
    or d
    ld (hl), a
    ret

hint_col_mask:
    DEFB 1, 2, 4, 8, 16, 32, 64, 128

board_theme_hint_inks:
    DEFB 3, 2, 1, 3, 7
board_theme_mark_inks:
    DEFB 2, 5, 0, 7, 6

_spectrum_render_square_mark:
    call read_square_spec
    ld (tmp_attr), a
    jp draw_square_mark

_spectrum_render_square_mark_with_hint:
    push hl
    call read_square_spec
    ld (tmp_attr), a
    call draw_square_mark
    pop hl
    jp render_hint_from_mark_spec

render_hint_from_mark_spec:
    ld a, (_netchesszx_movement_hints)
    or a
    ret z
    push hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    cp 8
    jp nc, rsh_mark_no_hint
    ld e, a
    ld d, 0
    inc hl
    ld a, (hl)
    cp 8
    jp nc, rsh_mark_no_hint
    ld c, a
    ld hl, _netchesszx_hinted_rows
    add hl, de
    ld a, (hl)
    ld e, a
    ld b, 0
    ld hl, hint_col_mask
    add hl, bc
    ld a, e
    and (hl)
    pop hl
    ret z
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    inc hl
    inc hl
    inc hl
    ld c, 0
    ld a, (hl)
    cp '.'
    jr z, rsh_mark_piece_ready
    ld c, 0x80
rsh_mark_piece_ready:
    ld a, (_netchesszx_board_theme_index)
    ld e, a
    ld d, 0
    ld hl, board_theme_mark_inks
    add hl, de
    ld a, (hl)
    or c
    ld (tmp_attr), a
    jp render_square_hint_loaded

rsh_mark_no_hint:
    pop hl
    ret



_spectrum_board_show_legal_hints:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    inc hl
    ld c, (hl)
    add a, a
    add a, a
    add a, a
    add a, c
    ld d, a
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), NETCHESSZX_RULES_BOARD_BASE & 0xff
    inc hl
    ld (hl), NETCHESSZX_RULES_BOARD_BASE >> 8
    inc hl
    ld a, (_side_to_move)
    ld (hl), a
    inc hl
    ld (hl), d
    inc hl
    ld a, (_castle_rights)
    ld (hl), a
    inc hl
    ld a, (_ep_square)
    ld (hl), a
    ld hl, 8
    push hl
    call _spectrum_overlay_exec_cached
    pop bc
    ret

call_overlay_ae:
    ld h, e
    ld l, a
    push hl
    call _spectrum_overlay_exec
    pop bc
    ret

call_overlay_cached_ae:
    ld h, e
    ld l, a
    push hl
    call _spectrum_overlay_exec_cached
    pop bc
    ret

_spectrum_info_panel_overlay_line:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), c
    ld a, SPECTRUM_OVL_GUI_LOG
    ld e, SPECTRUM_OVL_GUI_LOG_CONNECTION_PANEL
    jp call_overlay_ae

_spectrum_input_parse_move:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), c
    inc hl
    ld (hl), b
    ld a, SPECTRUM_OVL_APP_INPUT
    ld e, SPECTRUM_OVL_APP_INPUT_PARSE_MOVE
    jp call_overlay_ae

_netchesszx_setup_update_room_code:
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_UPDATE_ROOM
    jp call_overlay_cached_ae

_netchesszx_setup_render_edit_line:
    ld hl, 2
    add hl, sp
    ld b, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld a, (hl)
    ld (NETCHESSZX_OVERLAY_CONTEXT + 4), a
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), b
    inc hl
    ld (hl), c
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_EDIT_LINE
    jp call_overlay_cached_ae

_netchesszx_setup_compute_visible:
    ld (NETCHESSZX_OVERLAY_CONTEXT), hl
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_COMPUTE_VISIBLE
    jp call_overlay_cached_ae

_netchesszx_setup_step_row:
    ld hl, 2
    add hl, sp
    ld b, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), b
    inc hl
    ld (hl), c
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_STEP_ROW
    call call_overlay_cached_ae
    ld a, (NETCHESSZX_OVERLAY_CONTEXT)
    ld h, 0
    ld l, a
    ret

_netchesszx_setup_paint_attrs:
    ld hl, 2
    add hl, sp
    ld b, (hl)
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld a, (hl)
    ld (NETCHESSZX_OVERLAY_CONTEXT + 4), a
    inc hl
    ld a, (hl)
    ld (NETCHESSZX_OVERLAY_CONTEXT + 5), a
    inc hl
    ld a, (hl)
    ld (NETCHESSZX_OVERLAY_CONTEXT + 6), a
    inc hl
    ld a, (hl)
    ld (NETCHESSZX_OVERLAY_CONTEXT + 7), a
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), b
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), c
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_PAINT_ATTRS
    jp call_overlay_cached_ae

_netchesszx_setup_render_rows:
    ld hl, 2
    add hl, sp
    ld b, (hl)
    inc hl
    ld c, (hl)
    inc hl
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), b
    inc hl
    ld (hl), c
    inc hl
    ld (hl), e
    inc hl
    ld (hl), d
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_RUN
    jp call_overlay_cached_ae

_netchesszx_setup_room_editable:
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_ROOM_EDITABLE
    jp call_overlay_cached_ae

_netchesszx_setup_room_backspace:
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_ROOM_BACKSPACE
    jp call_overlay_cached_ae

_netchesszx_setup_room_append:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_ROOM_APPEND
    jp call_overlay_cached_ae

_netchesszx_setup_move_focus:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_MENU_LOGIC
    ld e, SPECTRUM_OVL_MENU_LOGIC_MOVE_FOCUS
    jp call_overlay_cached_ae

_netchesszx_setup_validate_ip:
    ld (NETCHESSZX_OVERLAY_CONTEXT), hl
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_VALIDATE_IP
    jp call_overlay_ae

_spectrum_board_clear_legal_hints:
    ld a, SPECTRUM_OVL_HINTS
    ld e, SPECTRUM_OVL_HINTS_CLEAR
    jp call_overlay_cached_ae

read_square_spec:
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld a, (hl)
    ret

compute_square_bc:
    ld a, (board_row)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW
    ld b, a
    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    ld c, a
    ret

square_parity:
    ld a, (board_row)
    ld d, a
    ld a, (board_col)
    add a, d
    and 1
    ret

_spectrum_render_moves:
    ld (move_ptr), hl
    ld a, NETCHESSZX_INFO_MOVES_CLEAR_Y
    ld b, NETCHESSZX_INFO_MOVES_CLEAR_HEIGHT
    call clear_right_pixel_band_abs
    ld a, NETCHESSZX_INFO_MOVES_FIRST_Y
    ld (tmp_scan), a
    ld b, NETCHESSZX_MOVE_ROWS
rm_loop:
    push bc
    call render_move_current_line
    ld hl, (move_ptr)
    ld de, NETCHESSZX_MOVE_SLOT_SIZE
    add hl, de
    ld (move_ptr), hl
    ld a, (tmp_scan)
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    ld (tmp_scan), a
    pop bc
    djnz rm_loop
    ret

_spectrum_render_move_at:
    ld (move_ptr), hl
    ld de, NETCHESSZX_MOVE_LINES_BASE
    or a
    sbc hl, de
    ld b, 0
rma_row_loop:
    ld a, l
    cp NETCHESSZX_MOVE_SLOT_SIZE
    jr c, rma_row_ready
    sub NETCHESSZX_MOVE_SLOT_SIZE
    ld l, a
    inc b
    jr rma_row_loop
rma_row_ready:
    ld a, NETCHESSZX_INFO_MOVES_FIRST_Y
rma_y_loop:
    inc b
    dec b
    jr z, rma_y_ready
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    djnz rma_y_loop
rma_y_ready:
    push af
    dec a
    ld b, NETCHESSZX_INFO_TIGHT_LINE_STEP
    call clear_right_pixel_band_abs
    pop af
    ld (tmp_scan), a

render_move_current_line:
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, (move_ptr)
    bit 7, (hl)
    jr z, rmc_attr_ready
    res 7, (hl)
    ld a, ATTR_ERROR
    ld (current_attr), a
rmc_attr_ready:
    ld a, (tmp_scan)
    ld b, a
    ld c, NETCHESSZX_INFO_TEXT_COL
    call draw_ikkle_text_abs_y
    ld a, (current_attr)
    cp ATTR_ERROR
    jr nz, rmc_no_restore
    ld hl, (move_ptr)
    set 7, (hl)
rmc_no_restore:
    ld hl, (move_ptr)
    ld de, NETCHESSZX_MOVE_BLACK_OFFSET
    add hl, de
    ld a, (tmp_scan)
    ld b, a
    ld c, NETCHESSZX_MOVE_BLACK_COL
    jp draw_ikkle_text_abs_y

_spectrum_render_moves_scroll:
    ld a, NETCHESSZX_INFO_MOVES_FIRST_Y
    ld b, (NETCHESSZX_MOVE_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP
    call scroll_right_tight_band_up
    ld a, NETCHESSZX_INFO_MOVES_FIRST_Y + ((NETCHESSZX_MOVE_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP)
    ld b, NETCHESSZX_INFO_TIGHT_LINE_STEP
    jp clear_right_pixel_band_abs

_spectrum_render_chat_scroll:
    ld a, NETCHESSZX_INFO_CHAT_FIRST_Y
    ld b, (NETCHESSZX_CHAT_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP
    call scroll_right_tight_band_up
    ld a, NETCHESSZX_INFO_CHAT_FIRST_Y + ((NETCHESSZX_CHAT_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP)
    ld b, NETCHESSZX_INFO_TIGHT_LINE_STEP
    jp clear_right_pixel_band_abs

scroll_right_tight_band_up:
    ld (tmp_scan), a
    ld a, b
    ld (piece_scan), a
    ld a, (tmp_scan)
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ex de, hl
    ld a, (tmp_scan)
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
srub_loop:
    ld a, (piece_scan)
    or a
    ret z
    push hl
    push de
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    ldi
    pop de
    pop hl
    call pixel_down_hl
    ex de, hl
    call pixel_down_hl
    ex de, hl
    ld a, (piece_scan)
    dec a
    ld (piece_scan), a
    jr srub_loop

_spectrum_render_chat:
    ld (move_ptr), hl
    ld a, NETCHESSZX_INFO_CHAT_CLEAR_Y
    ld b, NETCHESSZX_INFO_CHAT_CLEAR_HEIGHT
    call clear_right_pixel_band_abs
    ld a, NETCHESSZX_INFO_CHAT_FIRST_Y
    ld (tmp_scan), a
    ld b, NETCHESSZX_CHAT_ROWS
rc_loop:
    push bc
    call render_chat_current_line
    ld hl, (move_ptr)
    ld de, NETCHESSZX_CHAT_SLOT_SIZE
    add hl, de
    ld (move_ptr), hl
    ld a, (tmp_scan)
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    ld (tmp_scan), a
    pop bc
    djnz rc_loop
    ret

_spectrum_render_chat_at:
    ld (move_ptr), hl
    ld de, NETCHESSZX_CHAT_LINES_BASE
    or a
    sbc hl, de
    ld b, 0
rca_row_loop:
    ld a, l
    cp NETCHESSZX_CHAT_SLOT_SIZE
    jr c, rca_row_ready
    sub NETCHESSZX_CHAT_SLOT_SIZE
    ld l, a
    inc b
    jr rca_row_loop
rca_row_ready:
    ld a, NETCHESSZX_INFO_CHAT_FIRST_Y
rca_y_loop:
    inc b
    dec b
    jr z, rca_y_ready
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    djnz rca_y_loop
rca_y_ready:
    push af
    dec a
    ld b, NETCHESSZX_INFO_TIGHT_LINE_STEP
    call clear_right_pixel_band_abs
    pop af
    ld (tmp_scan), a

render_chat_current_line:
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, (move_ptr)
    ld a, (hl)
    ; Chat is black-paper UI: these glyph choices are intentionally inverted
    ; versus the white-paper move header.
    cp 'B'
    ld hl, chat_icon_white
    jr z, rc_draw_label
    cp 'W'
    ld hl, chat_icon_black
    jr z, rc_draw_label
    jr rc_draw_ikkle_line
rc_draw_label:
    call draw_chat_icon_at
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, (move_ptr)
    inc hl
    ld a, (tmp_scan)
    ld b, a
    ld c, NETCHESSZX_CHAT_TIME_COL
    jp draw_ikkle_text_abs_y
rc_draw_ikkle_line:
    ld hl, (move_ptr)
    ld a, (hl)
    or a
    jr nz, rc_draw_legacy_line
    inc hl
    ld a, NETCHESSZX_CHAT_TIME_COL
    jr rc_draw_plain_line
rc_draw_legacy_line:
    ld hl, (move_ptr)
    ld de, NETCHESSZX_CHAT_TEXT_OFFSET
    add hl, de
    ld a, NETCHESSZX_CHAT_TEXT_COL
rc_draw_plain_line:
    ld c, a
    ld a, (tmp_scan)
    ld b, a
    jp draw_ikkle_text_abs_y

draw_chat_icon_at:
    ld (sprite_ptr), hl
    ld a, NETCHESSZX_CHAT_ICON_BYTE_COL
    ld (tmp_col), a
    ld a, 5
    ld (tmp_char), a
    jp draw_icon_byte_abs_at_tmp

draw_icon_byte_abs_at_tmp:
    xor a
    ld (piece_scan), a
diba_loop:
    ld hl, (sprite_ptr)
    ld a, (hl)
    inc hl
    ld (sprite_ptr), hl
    ld c, a
    ld a, (tmp_scan)
    ld d, a
    ld a, (piece_scan)
    add a, d
    call compute_pixel_base
    ld a, (tmp_col)
    add a, l
    ld l, a
    ld (hl), c
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    ld c, a
    ld a, (tmp_char)
    cp c
    jr nz, diba_loop
    ret

draw_icon_sprite_at_tmp:
dci_loop:
    ld a, (tmp_row)
    call compute_screen_base
    ld a, (tmp_scan)
    add a, h
    ld h, a
    ld a, (tmp_col)
    add a, l
    ld l, a
    ex de, hl
    ld hl, (sprite_ptr)
    ld a, (hl)
    inc hl
    ld (sprite_ptr), hl
    ld c, a
    ex de, hl
    ld a, c
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
    inc hl
    ld a, c
    and 0x0f
    rlca
    rlca
    rlca
    rlca
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    ld a, (tmp_scan)
    inc a
    ld (tmp_scan), a
    ld a, (tmp_char)
    ld c, a
    ld a, (tmp_scan)
    cp c
    jr c, dci_loop
    ret

_spectrum_render_input:
    ld (notice_ptr), hl
    ld a, NETCHESSZX_INPUT_ROW
    ld c, ATTR_TEXT
    call clear_text_row
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld b, NETCHESSZX_INPUT_ROW
    ld c, 0
    ld d, 1
    ld hl, input_prompt_msg
    call draw_text64_line_attr_fast
    ld b, NETCHESSZX_INPUT_ROW
    ld c, 2
    ld d, 31
    ld hl, (notice_ptr)
    jp draw_text64_line_attr_fast

_spectrum_render_input_cell:
    ld a, (hl)
    add a, 2
    cp 64
    ret nc
    ld d, a
    inc hl
    ld a, (hl)
    ld (tmp_char), a
    inc hl
    ld a, (hl)
    ld (mark_mode), a

    ld a, d
    ld (tmp_col), a
    ld a, NETCHESSZX_INPUT_ROW
    ld (tmp_row), a

    ld a, ATTR_TEXT
    ld (current_attr), a
    ld a, (tmp_char)
    call draw_char64_at_tmp

    ld a, (mark_mode)
    or a
    ret z
    jp draw_input_cursor_at_tmp

clear_screen:
    xor a
    out (0xfe), a

    ld hl, SCREEN_BASE
    ld de, SCREEN_BASE + 1
    ld bc, 6143
    ld (hl), a
    ldir

    ld h, d
    ld l, e
    inc de
    ld a, ATTR_TEXT
    ld (hl), a
    ld bc, 767
    ldir
    ret

draw_banner:
    xor a
    ld c, ATTR_BANNER_TOP
    call fill_attr_line
    ld a, 1
    ld c, ATTR_BANNER_BOT
    call fill_attr_line

    ld hl, title_msg
    ld b, 0
    ld c, 0
    call draw_scaled_text

    call draw_banner_info
    jp draw_badge

draw_banner_info:
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, _netchesszx_version_banner_msg
    ld b, 2
    ld c, NETCHESSZX_BANNER_INFO_COL
    call draw_ikkle_text_abs_y
    ld hl, banner_info_top_msg
    ld b, 10
    ld c, NETCHESSZX_BANNER_INFO_COL
    jp draw_ikkle_text_abs_y

draw_menu:
    ld (mark_mode), a
    ; Timer pixels (cols 20-31, scans 2-5) are identical open/closed; keep
    ; them and only retint attrs, so the timer does not blink on open.
    ld a, NETCHESSZX_MENU_ROW
    ld c, ATTR_STATUS
    call fill_attr_line
    ld a, NETCHESSZX_MENU_ROW
    call compute_screen_base
    ld a, 16
    add a, l
    ld l, a
    ld b, 16
dm_sep_clear:
    ld (hl), 0
    inc l
    djnz dm_sep_clear
    ld a, (mark_mode)
    dec a
    cp 5
    jr c, dm_selection_ok
    xor a
dm_selection_ok:
    ld (tmp_row), a
    ld a, NETCHESSZX_MENU_ROW
    call compute_screen_base
    ex de, hl
    ld hl, taboption_base
    ld a, 8
dm_copy_scan_loop:
    push de
    ld bc, 16
    ldir
    pop de
    inc d
    dec a
    jr nz, dm_copy_scan_loop
dm_base_done:
    ld a, (tmp_row)
    jp draw_taboption_patch

draw_menu_partial:
    ld (mark_mode), a
    and 0x38
    srl a
    srl a
    srl a
    call draw_menu_partial_option
    ld a, (mark_mode)
    and 0x07
    jp draw_menu_partial_option

draw_menu_partial_option:
    cp 5
    jr c, dmp_option_ok
    xor a
dmp_option_ok:
    ld (tmp_row), a
    add a, a
    ld e, a
    ld d, 0
    ld hl, menu_option_ranges
    add hl, de
    ld a, (hl)
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (tmp_scan), a

    ld a, (mark_mode)
    and 0x07
    cp 5
    jr c, dmp_source_ok
    xor a
dmp_source_ok:
    ld b, a
    ld a, (tmp_row)
    cp b
    jp z, draw_taboption_patch
    ld a, NETCHESSZX_MENU_ROW
    call compute_screen_base
    ld a, (tmp_col)
    add a, l
    ld l, a
    ex de, hl
    ld hl, taboption_base
    ld a, 8
dmp_scan_loop:
    push af
    push de
    push hl
    ld a, (tmp_col)
    add a, l
    ld l, a
    jr nc, dmp_src_ok
    inc h
dmp_src_ok:
    ld b, 0
    ld a, (tmp_scan)
    ld c, a
    ldir
    pop hl
    ld bc, 16
    add hl, bc
    pop de
    inc d
    pop af
    dec a
    jr nz, dmp_scan_loop
    ret

draw_taboption_patch:
    cp 5
    jr c, dtp_option_ok
    xor a
dtp_option_ok:
    ld (tmp_row), a
    add a, a
    ld e, a
    ld d, 0
    ld hl, menu_option_ranges
    add hl, de
    ld a, (hl)
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (tmp_scan), a
    ld a, (tmp_row)
    ld b, a
    ld hl, taboption_patches
    ld de, 18
    or a
    jr z, dtp_patch_ready
dtp_patch_offset_loop:
    add hl, de
    djnz dtp_patch_offset_loop
dtp_patch_ready:
    push hl
    ld a, NETCHESSZX_MENU_ROW
    call compute_screen_base
    inc h
    ld a, (tmp_col)
    add a, l
    ld l, a
    ex de, hl
    pop hl
    ld a, 6
dtp_scan_loop:
    push af
    push de
    ld a, (tmp_scan)
    ld c, a
    ld b, 0
    ldir
    pop de
    inc d
    pop af
    dec a
    jr nz, dtp_scan_loop
    ret

menu_option_ranges:
    DEFB 0,3
    DEFB 3,3
    DEFB 6,3
    DEFB 9,3
    DEFB 11,4

taboption_base:
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    DEFB 0x3b,0xba,0xb8,0x31,0x3b,0x30,0x0e,0xee,0xe0,0x3a,0x13,0x80,0xea,0xea,0xe0,0x00
    DEFB 0x2b,0xaa,0x90,0x29,0x32,0x20,0x0a,0xcc,0x40,0x22,0x12,0x80,0x4e,0xce,0xc0,0x00
    DEFB 0x3a,0xaa,0x90,0x29,0x0a,0x20,0x0c,0x82,0x40,0x32,0x13,0x80,0x4a,0x8e,0x80,0x00
    DEFB 0x2b,0xbb,0x90,0x39,0x3b,0x30,0x0a,0xee,0x40,0x23,0x12,0x00,0x4a,0xea,0xe0,0x00
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    DEFB 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00

taboption_patches:
    DEFB 0x7f,0xff,0xfc
    DEFB 0x44,0x45,0x44
    DEFB 0x54,0x55,0x6c
    DEFB 0x45,0x55,0x6c
    DEFB 0x54,0x44,0x6c
    DEFB 0x7f,0xff,0xfc

    DEFB 0x7f,0xff,0xfc
    DEFB 0x4e,0xc4,0xcc
    DEFB 0x56,0xcd,0xdc
    DEFB 0x56,0xf5,0xdc
    DEFB 0x46,0xc4,0xcc
    DEFB 0x7f,0xff,0xfc

    DEFB 0x1f,0xff,0xf0
    DEFB 0x11,0x11,0x10
    DEFB 0x15,0x33,0xb0
    DEFB 0x13,0x7d,0xb0
    DEFB 0x15,0x11,0xb0
    DEFB 0x1f,0xff,0xf0

    DEFB 0x7f,0xff,0xc0
    DEFB 0x45,0xec,0x40
    DEFB 0x5d,0xed,0x40
    DEFB 0x4d,0xec,0x40
    DEFB 0x5c,0xed,0xc0
    DEFB 0x7f,0xff,0xc0

    DEFB 0x01,0xff,0xff,0xf0
    DEFB 0x01,0x15,0x15,0x10
    DEFB 0x01,0xb1,0x31,0x30
    DEFB 0x01,0xb5,0x71,0x70
    DEFB 0x01,0xb5,0x15,0x10
    DEFB 0x01,0xff,0xff,0xf0

hide_menu:
    ; Clear only the taboption block (cols 0-15); timer pixels stay put so
    ; closing the menu does not blink the timer. ATTR_TIMER matches ATTR_TEXT.
    ld a, NETCHESSZX_MENU_ROW
    call compute_screen_base
    ld c, 8
    xor a
hm_scan_loop:
    ld b, 16
    push hl
hm_px_loop:
    ld (hl), a
    inc l
    djnz hm_px_loop
    pop hl
    inc h
    dec c
    jr nz, hm_scan_loop
hm_attrs:
    ld a, NETCHESSZX_MENU_ROW
    ld c, ATTR_TEXT
    call fill_attr_line
    jp draw_banner_separator

draw_static_panels:
    call draw_moves_header

    ld a, ATTR_CHAT_TITLE
    ld (current_attr), a
    ld b, NETCHESSZX_INFO_CHAT_TITLE_ROW
    ld c, NETCHESSZX_INFO_TEXT_COL
    ld d, 2
    call store_tmp_rcs
    ld hl, chat_msg
    call draw_ikkle_text_at
    ld a, NETCHESSZX_INFO_CHAT_HLINE_ROW
    jp draw_right_hline_full_left

draw_moves_header:
    ld a, NETCHESSZX_INFO_HEADER_ROW
    call compute_screen_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld d, h
    ld e, l
    ld c, 13
    call clear_pixels_8

    ld a, NETCHESSZX_INFO_HEADER_ROW
    call compute_attr_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld b, 14
dmh_attr_loop:
    ld (hl), ATTR_STATUS
    inc hl
    djnz dmh_attr_loop

    call draw_moves_header_divider

    ld a, NETCHESSZX_MOVES_HEADER_WHITE_ICON_BYTE_COL
    ld hl, moves_header_white_icon
    call draw_moves_header_icon
    ld a, NETCHESSZX_MOVES_HEADER_BLACK_ICON_BYTE_COL
    ld hl, moves_header_black_icon
    call draw_moves_header_icon

    ld a, ATTR_STATUS
    ld (current_attr), a
    ld b, NETCHESSZX_INFO_HEADER_ROW
    ld c, NETCHESSZX_MOVES_HEADER_WHITE_TEXT_COL
    ld d, 2
    call store_tmp_rcs
    ld hl, moves_white_msg
    call draw_ikkle_text_at
    ld b, NETCHESSZX_INFO_HEADER_ROW
    ld c, NETCHESSZX_MOVES_HEADER_BLACK_TEXT_COL
    ld d, 2
    call store_tmp_rcs
    ld hl, moves_black_msg
    jp draw_ikkle_text_at

draw_moves_header_icon:
    ld (sprite_ptr), hl
    ld (tmp_col), a
    ld a, NETCHESSZX_INFO_HEADER_ROW
    ld (tmp_row), a
    ld a, 1
    ld (tmp_scan), a
    ld a, 6
    ld (tmp_char), a
    jp draw_icon_sprite_at_tmp

draw_moves_header_divider:
    ld a, NETCHESSZX_INFO_HEADER_ROW
    call compute_screen_base
    ld a, NETCHESSZX_MOVES_HEADER_DIVIDER_BYTE_COL
    add a, l
    ld l, a
    ld b, 8
dmhd_loop:
    ld a, (hl)
    or 0x80
    ld (hl), a
    inc h
    djnz dmhd_loop
    ret

moves_white_msg:
    DEFM "WHITE",0
moves_black_msg:
    DEFM "BLACK",0
moves_header_white_icon:
    DEFB 0x3c,0x42,0x42,0x42,0x3c
moves_header_black_icon:
    DEFB 0x3c,0x7e,0x7e,0x7e,0x3c

banner_info_top_msg:
    DEFM "ONLINE CHESS FOR ZX SPECTRUM",0

clear_board_coords:
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_screen_base
    ld d, h
    ld e, l
    ld b, 8
cbc_file_scan:
    push bc
    push de
    ld h, d
    ld l, e
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld b, 18
    xor a
cbc_file_byte:
    ld (hl), a
    inc hl
    djnz cbc_file_byte
    pop de
    pop bc
    inc d
    djnz cbc_file_scan

    ld c, NETCHESSZX_GAME_BOARD_TOP_ROW
    ld b, 16
cbc_rank_row:
    push bc
    ld a, c
    call compute_screen_base
    ld d, h
    ld e, l
    ld b, 8
cbc_rank_scan:
    ld h, d
    ld l, e
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    xor a
    ld (hl), a
    inc d
    djnz cbc_rank_scan
    pop bc
    inc c
    djnz cbc_rank_row
    ret

draw_board_coords:
    call board_light_line_attr
    ld (current_attr), a
    xor a
    ld (board_col), a

coord_file_loop:
    ld a, (board_col)
    cp 8
    jr nc, coord_rank_start

    ld a, (_spectrum_gui_board_flipped)
    or a
    jr z, coord_file_white
    ld a, 'H'
    ld d, a
    ld a, (board_col)
    ld e, a
    ld a, d
    sub e
    jr coord_file_draw
coord_file_white:
    ld a, 'A'
    ld d, a
    ld a, (board_col)
    add a, d
coord_file_draw:
    call draw_file_letter_centered

    ld a, (board_col)
    inc a
    ld (board_col), a
    jr coord_file_loop

coord_rank_start:
    xor a
    ld (board_row), a

coord_rank_loop:
    ld a, (board_row)
    cp 8
    ret nc

    ld a, (_spectrum_gui_board_flipped)
    or a
    jr z, coord_rank_white
    ld a, '1'
    ld d, a
    ld a, (board_row)
    add a, d
    jr coord_rank_draw
coord_rank_white:
    ld a, '8'
    ld d, a
    ld a, (board_row)
    ld e, a
    ld a, d
    sub e
coord_rank_draw:
    call draw_rank_digit_centered

    ld a, (board_row)
    inc a
    ld (board_row), a
    jr coord_rank_loop

draw_rank_digit_centered:
    ld (tmp_char), a

    ld a, (board_row)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld a, (current_attr)
    ld (hl), a
    ld de, 32
    add hl, de
    ld (hl), a

    ld a, (tmp_char)
    sub '1'
    ret c
    cp 8
    ret nc
    ld e, a
    add a, a
    add a, a
    add a, e
    ld e, a
    ld d, 0
    ld hl, rank_digit_patterns
    add hl, de
    ld (sprite_ptr), hl

    xor a
    ld (piece_scan), a

drdc_loop:
    ld a, (piece_scan)
    cp 5
    ret nc

    ld hl, (sprite_ptr)
    ld c, (hl)
    inc hl
    ld (sprite_ptr), hl

    ld a, (board_row)
    add a, a
    add a, a
    add a, a
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW * 8 + 5
    ld d, a
    ld a, (piece_scan)
    add a, d
    ld d, a
    srl a
    srl a
    srl a
    call compute_screen_base
    ld a, d
    and 7
    add a, h
    ld h, a
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld (hl), c

    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    jr drdc_loop

draw_file_letter_centered:
    ld (tmp_char), a

    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_attr_base
    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    add a, l
    ld l, a
    ld a, (current_attr)
    ld (hl), a

    ld a, (tmp_char)
    sub 'A'
    ret c
    cp 8
    ret nc
    ld e, a
    add a, a
    add a, a
    add a, e
    ld e, a
    ld d, 0
    ld hl, file_letter_patterns
    add hl, de
    ld (sprite_ptr), hl

    xor a
    ld (piece_scan), a

dflc_loop:
    ld a, (piece_scan)
    cp 5
    ret nc

    ld hl, (sprite_ptr)
    ld a, (hl)
    rrca
    rrca
    rrca
    rrca
    ld c, a
    and 0x01
    ld (tmp_attr), a
    ld a, c
    srl a
    ld c, a
    inc hl
    ld (sprite_ptr), hl

    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_screen_base
    ld a, (piece_scan)
    inc a
    add a, h
    ld h, a
    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    add a, l
    ld l, a
    ld (hl), c
    ld a, (tmp_attr)
    or a
    jr z, dflc_no_carry
    inc hl
    ld a, (hl)
    or 0x80
    ld (hl), a
dflc_no_carry:

    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    jr dflc_loop

draw_board:
    call fill_board_attrs

    ld hl, (board_ptr)
    ld (board_iter), hl
    xor a
    ld (board_row), a

board_piece_row_loop:
    ld a, (board_row)
    cp 8
    ret nc

    xor a
    ld (board_col), a

board_piece_col_loop:
    ld a, (board_col)
    cp 8
    jr nc, board_piece_next_row

    ld hl, (board_iter)
    ld a, (hl)
    inc hl
    ld (board_iter), hl
    call draw_one_board_square

board_piece_skip:
    ld a, (board_col)
    inc a
    ld (board_col), a
    jr board_piece_col_loop

board_piece_next_row:
    ld a, (board_row)
    inc a
    ld (board_row), a
    jr board_piece_row_loop

draw_board_frame:
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_screen_base
    ld a, 7
    add a, h
    ld h, a
    call draw_board_hline

    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW + 16
    call compute_screen_base
    call draw_board_hline

    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW
    ld (tmp_row), a
    ld b, 16
dbf_row:
    push bc
    ld a, (tmp_row)
    call compute_screen_base
    ld d, h
    ld e, l
    ld b, 8
dbf_scan:
    ld h, d
    ld l, e
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld a, (hl)
    or 0x01
    ld (hl), a

    ld h, d
    ld l, e
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL + 16
    add a, l
    ld l, a
    ld a, (hl)
    or 0x80
    ld (hl), a

    inc d
    djnz dbf_scan
    ld a, (tmp_row)
    inc a
    ld (tmp_row), a
    pop bc
    djnz dbf_row
    ret

draw_board_hline:
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld a, (hl)
    or 0x01
    ld (hl), a
    inc l
    ld b, 16
dbh_mid:
    ld (hl), 0xff
    inc l
    djnz dbh_mid
    ld a, (hl)
    or 0x80
    ld (hl), a
    ret

restore_board_frame_attrs:
    ; ABOUT asset leaves only frame bits in the right/bottom border bytes.
    call board_light_line_attr
    ld (tmp_attr), a
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call restore_board_attr_hline
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW + 16
    call restore_board_attr_hline
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld b, 16
    ld a, (tmp_attr)
rbfa_side_loop:
    ld (hl), a
    ld de, 17
    add hl, de
    ld (hl), a
    ld de, 15
    add hl, de
    djnz rbfa_side_loop
    ret

restore_board_attr_hline:
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld b, 18
    ld a, (tmp_attr)
rbah_loop:
    ld (hl), a
    inc hl
    djnz rbah_loop
    ret

board_light_line_attr:
    ld a, (_netchesszx_board_light_attr)
    ld d, a
    and 0x40
    ld e, a
    ld a, d
    and 0x38
    rrca
    rrca
    rrca
    or e
    ret

draw_one_board_square:
    cp '.'
    jr nz, dobs_piece_ready
    ld a, ' '
dobs_piece_ready:
    ld (piece_char), a
    call square_parity
    jr z, dobs_light
    ld a, (_netchesszx_board_dark_attr)
    jr dobs_attr
dobs_light:
    ld a, (_netchesszx_board_light_attr)
dobs_attr:
    ld d, a
    call compute_square_bc
    push de
    push bc
    call clear_square_pixels_2x2
    pop bc
    pop de
    ld a, d
    call set_square_attr_2x2

    ld a, (piece_char)
    cp ' '
    ret z
    ld a, (piece_char)
    jp draw_piece_sprite_16x16

fill_board_attrs:
    xor a
    ld (board_row), a

fba_row_loop:
    ld a, (board_row)
    cp 8
    ret nc
    xor a
    ld (board_col), a

fba_col_loop:
    ld a, (board_col)
    cp 8
    jr nc, fba_next_row

    call square_parity
    jr z, fba_light
    ld a, (_netchesszx_board_dark_attr)
    jr fba_attr_ready
fba_light:
    ld a, (_netchesszx_board_light_attr)
fba_attr_ready:
    ld d, a
    call compute_square_bc
    ld a, d
    call set_square_attr_2x2

    ld a, (board_col)
    inc a
    ld (board_col), a
    jr fba_col_loop

fba_next_row:
    ld a, (board_row)
    inc a
    ld (board_row), a
    jr fba_row_loop

clear_square_pixels_2x2:
    ld e, c
    ld c, b
    ld d, 2
csp_row_loop:
    ld a, c
    call compute_screen_base
    ld a, e
    add a, l
    ld l, a
    ld b, 8
    xor a
csp_scan_loop:
    ld (hl), a
    inc l
    ld (hl), a
    dec l
    inc h
    djnz csp_scan_loop
    inc c
    dec d
    jr nz, csp_row_loop
    ret

set_square_attr_2x2:
    ld d, a
    ld a, b
    call compute_attr_base
    ld a, c
    add a, l
    ld l, a
    ld a, d
    ld (hl), a
    inc hl
    ld (hl), a
    ld de, 31
    add hl, de
    ld (hl), a
    inc hl
    ld (hl), a
    ret

draw_scaled_text:
    ld (text_ptr), hl
    ld a, b
    ld (scaled_row), a
    ld a, c
    ld (scaled_col), a

dst_loop:
    ld hl, (text_ptr)
    ld a, (hl)
    or a
    ret z
    ld b, a
    ld a, (scaled_row)
    ld d, a
    ld a, (scaled_col)
    ld c, a
    ld a, b
    ld b, d
    call draw_scaled_glyph
    ld hl, (text_ptr)
    inc hl
    ld (text_ptr), hl
    ld a, (scaled_col)
    inc a
    ld (scaled_col), a
    jr dst_loop

draw_scaled_glyph:
    ld (piece_char), a
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    xor a
    ld (piece_scan), a

dsg_loop:
    ld a, (piece_scan)
    cp 6
    ret nc

    ld (tmp_scan), a
    ld a, (piece_char)
    call font_scanline
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    ld e, a
    ld d, 0
    ld hl, expand_2x
    add hl, de
    ld d, (hl)

    ld a, (piece_scan)
    add a, a
    add a, 2
    ld e, a
    and 7
    ld (tmp_scan), a
    ld a, e
    rrca
    rrca
    rrca
    and 0x1f
    ld e, a
    ld a, (piece_row)
    add a, e
    call compute_screen_base
    ld a, (tmp_scan)
    add a, h
    ld h, a
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), d
    inc h
    ld (hl), d

    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    jr dsg_loop

draw_piece_sprite_16x16:
    ld (piece_char), a
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a

    ld a, (piece_char)
    call piece_sprite_for_char
    ld a, h
    or l
    ret z
    ex de, hl                  ; DE = sprite data pointer (32 bytes, sequential)

    ; --- top char row: pixel scanlines 0..7 ---
    ld a, (piece_row)
    call compute_screen_base   ; HL = scan 0 of char row; clobbers A/HL, keeps DE
    ld a, (piece_col)
    add a, l
    ld l, a                    ; HL = top-left pixel address
    ld b, 8
    call dps_blit_8

    ; --- bottom char row: pixel scanlines 8..15 ---
    ld a, (piece_row)
    inc a
    call compute_screen_base
    ld a, (piece_col)
    add a, l
    ld l, a
    ld b, 8
    ; falls through into dps_blit_8

; Blit B pixel lines of a 16x16 sprite into one char row.
; DE = sprite pointer (advances 2 bytes/line), HL = left pixel address.
; inc h walks down within the char block; no per-line address recompute.
dps_blit_8:
    ld a, (de)
    ld (hl), a
    inc l
    inc de
    ld a, (de)
    ld (hl), a
    dec l
    inc de
    inc h
    djnz dps_blit_8
    ret

piece_sprite_for_char:
    ld c, 0
    cp 'a'
    jr c, psfc_upper_ready
    sub 32
    ld c, 12
psfc_upper_ready:
    ld (tmp_char), a

    call square_parity
    jr z, psfc_black_light
    ld a, c
    xor 12
    ld c, a
psfc_black_light:
    ld a, (tmp_char)
    ld hl, psfc_piece_chars
    ld b, 6
psfc_match:
    cp (hl)
    jr z, psfc_found
    inc hl
    djnz psfc_match
    ld hl, 0
    ret

psfc_found:
    ld a, 6
    sub b
    add a, a
    add a, c
    ld e, a
    ld d, 0
    ld hl, psfc_table
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ret

psfc_table:
    DW piece_sprites_16x16 + 0
    DW piece_sprites_16x16 + 64
    DW piece_sprites_16x16 + 128
    DW piece_sprites_16x16 + 192
    DW piece_sprites_16x16 + 256
    DW piece_sprites_16x16 + 320
    DW piece_sprites_16x16 + 32
    DW piece_sprites_16x16 + 96
    DW piece_sprites_16x16 + 160
    DW piece_sprites_16x16 + 224
    DW piece_sprites_16x16 + 288
    DW piece_sprites_16x16 + 352

draw_text64_at:
    ld (text_ptr), hl
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a

dt64_loop:
    ld hl, (text_ptr)
    ld a, (hl)
    or a
    ret z
    call draw_char64_at_tmp
    ld hl, (text_ptr)
    inc hl
    ld (text_ptr), hl
    ld a, (tmp_col)
    inc a
    cp 64
    ret nc
    ld (tmp_col), a
    jr dt64_loop

draw_char64_at_tmp:
    call draw_char64_pixels_at_tmp
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (current_attr)
    ld (hl), a
    ret

draw_text64_line_attr_fast:
    call draw_text64_line_prepare
    call draw_text64_line_fill_attr
    jp draw_text64_line_pixels_prepared

draw_text64_line_text_attr_fast:
    call draw_text64_line_prepare
    call draw_text64_line_fill_text_attr
    jp draw_text64_line_pixels_prepared

draw_text64_line_pixels_fast:
    call draw_text64_line_prepare
    jp draw_text64_line_pixels_prepared

draw_text64_line_prepare:
    ld (text_ptr), hl
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, d
    ld (tmp_char), a
    ret

draw_text64_line_fill_attr:
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (tmp_char)
    ld b, a
    ld a, (current_attr)
dt64laf_loop:
    ld (hl), a
    inc hl
    djnz dt64laf_loop
    ret

draw_text64_line_fill_text_attr:
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld de, (text_ptr)
    ld a, (tmp_char)
    ld b, a
    ld a, (tmp_col)
    and 1
    ld c, a
dt64lta_loop:
    ld a, (de)
    or a
    ret z
    ld a, (current_attr)
    ld (hl), a
    ld a, c
    or a
    jr z, dt64lta_even
    xor a
    ld c, a
    inc de
    jr dt64lta_next
dt64lta_even:
    inc de
    ld a, (de)
    or a
    jr z, dt64lta_next
    inc de
dt64lta_next:
    inc hl
    djnz dt64lta_loop
    ret

draw_text64_line_pixels_prepared:
    xor a
    ld (piece_scan), a

dst64_scan_loop:
    ld a, (tmp_row)
    call compute_screen_base
    ld a, (piece_scan)
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld (sprite_ptr), hl

    ld a, (piece_scan)
    or a
    jr z, dst64_blank_scan
    cp 7
    jr z, dst64_blank_scan

    dec a
    ld (tmp_scan), a
    ld hl, (text_ptr)
    ld a, (tmp_char)
    ld b, a
    ld a, (tmp_col)
    and 1
    xor 1
    ld c, a

dst64_pair_loop:
    ld a, c
    or a
    jr nz, dst64_left_from_text
    inc c
    xor a
    jr dst64_left_ready

dst64_left_from_text:
    call text64_next_glyph
    and 0xf0
dst64_left_ready:
    ld (tmp_attr), a
    call text64_next_glyph
    and 0x0f
    ld e, a
    ld a, (tmp_attr)
    or e
    push hl
    ld hl, (sprite_ptr)
    ld (hl), a
    inc hl
    ld (sprite_ptr), hl
    pop hl
    djnz dst64_pair_loop
    jr dst64_next_scan

dst64_blank_scan:
    ld hl, (sprite_ptr)
    ld a, (tmp_char)
    ld b, a
    xor a
dst64_blank_loop:
    ld (hl), a
    inc hl
    djnz dst64_blank_loop

dst64_next_scan:
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    cp 8
    jr c, dst64_scan_loop
    ret

text64_next_glyph:
    ld a, (hl)
    or a
    ret z
    inc hl
    push hl
    call font_scanline
    pop hl
    ret

draw_char64_pixels_at_tmp:
    ld (tmp_char), a
    xor a
    ld (tmp_scan), a

dc64_scan_loop:
    ld a, (tmp_row)
    call compute_screen_base
    ld a, (tmp_scan)
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a

    ld a, (tmp_scan)
    or a
    jr z, dc64_blank
    cp 7
    jr z, dc64_blank
    dec a
    ld (tmp_scan), a
    push hl
    ld a, (tmp_char)
    call font_scanline
    ld e, a
    pop hl
    ld a, (tmp_scan)
    inc a
    ld (tmp_scan), a
    jr dc64_got_pattern

dc64_blank:
    xor a
    ld e, a

dc64_got_pattern:
    ld a, (tmp_col)
    and 1
    jr nz, dc64_odd
    ld a, (hl)
    and 0x0f
    ld d, a
    ld a, e
    and 0xf0
    or d
    ld (hl), a
    jr dc64_next_scan

dc64_odd:
    ld a, (hl)
    and 0xf0
    ld d, a
    ld a, e
    and 0x0f
    or d
    ld (hl), a

dc64_next_scan:
    inc h
    ld a, (tmp_scan)
    inc a
    ld (tmp_scan), a
    cp 8
    jr c, dc64_scan_loop
    ret

draw_input_cursor_at_tmp:
    ld a, (tmp_row)
    call compute_screen_base
    ld a, 7
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_col)
    and 1
    ld b, 0xf0
    jr z, dict_apply
    ld b, 0x0f
dict_apply:
    ld a, (hl)
    or b
    ld (hl), a
    ret

clear_ikkle_region:
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, d
    ld (tmp_scan), a
    ld a, e
    ld (tmp_char), a
    xor a
    ld (piece_scan), a

cir_scan_loop:
    ld a, (piece_scan)
    cp 4
    jr nc, cir_attrs

    ld a, (tmp_row)
    call compute_screen_base
    ld a, (tmp_scan)
    ld d, a
    ld a, (piece_scan)
    add a, d
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_char)
    ld b, a
    xor a
cir_px_loop:
    ld (hl), a
    inc hl
    djnz cir_px_loop

    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    jr cir_scan_loop

cir_attrs:
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (tmp_char)
    ld b, a
    ld a, (current_attr)
cir_attr_loop:
    ld (hl), a
    inc hl
    djnz cir_attr_loop
    ret

draw_ikkle_text_at:
    ld (text_ptr), hl

dit_loop:
    ld a, (tmp_col)
    cp 64
    ret nc

    ld hl, (text_ptr)
    ld a, (hl)
    or a
    ret z
    ld (tmp_char), a
    push hl

    ld a, (tmp_row)
    call compute_screen_base
    ld a, (tmp_scan)
    add a, h
    ld h, a
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_col)
    ld c, a
    call ikkle_clear_cell_hl

    push hl
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (current_attr)
    ld (hl), a
    pop hl

    ld a, (tmp_char)
    cp 33
    jr c, dit_space
    cp 128
    jr nc, dit_space
    cp 96
    jr c, dit_no_fold
    sub 32
dit_no_fold:
    sub 32
    add a, a
    ld e, a
    ld d, 0
    push hl
    ld hl, timer_ikkle_packed
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    pop hl
    call ikkle_blit_de_hl

dit_space:
    pop hl
    inc hl
    ld (text_ptr), hl
    ld a, (tmp_col)
    inc a
    ld (tmp_col), a
    jr dit_loop

draw_ikkle_text_abs_y:
    ld (text_ptr), hl
    ld a, b
    ld (tmp_scan), a
    ld a, c
    ld (tmp_col), a

dita_loop:
    ld a, (tmp_col)
    cp 64
    ret nc

    ld hl, (text_ptr)
    ld a, (hl)
    or a
    ret z
    ld (tmp_char), a
    push hl

    call ikkle_clear_abs_cell
    call ikkle_attr_abs_cell

    ld a, (tmp_char)
    cp 33
    jr c, dita_space
    cp 128
    jr nc, dita_space
    cp 96
    jr c, dita_no_fold
    sub 32
dita_no_fold:
    sub 32
    add a, a
    ld e, a
    ld d, 0
    ld hl, timer_ikkle_packed
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    ld a, d
    ld (piece_row), a
    ld a, e
    ld (piece_col), a
    xor a
    ld (piece_scan), a
    call ikkle_blit_abs_saved

dita_space:
    pop hl
    inc hl
    ld (text_ptr), hl
    ld a, (tmp_col)
    inc a
    ld (tmp_col), a
    jr dita_loop

ikkle_clear_abs_cell:
    xor a
    ld (piece_scan), a
icac_loop:
    ld a, (tmp_scan)
    ld d, a
    ld a, (piece_scan)
    add a, d
    call compute_pixel_base
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_col)
    bit 0, a
    jr nz, icac_odd
    ld a, (hl)
    and 0x0f
    ld (hl), a
    jr icac_next
icac_odd:
    ld a, (hl)
    and 0xf0
    ld (hl), a
icac_next:
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    cp 4
    jr c, icac_loop
    ret

ikkle_attr_abs_cell:
    ld a, (tmp_scan)
    srl a
    srl a
    srl a
    call ikkle_set_abs_attr_row
    ld a, (tmp_scan)
    add a, 3
    srl a
    srl a
    srl a
    ld d, a
    ld a, (tmp_scan)
    srl a
    srl a
    srl a
    cp d
    ret z
    ld a, d

ikkle_set_abs_attr_row:
    call compute_attr_base
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (current_attr)
    ld (hl), a
    ret

ikkle_blit_abs_saved:
    ld a, (piece_row)
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    call ikkle_draw_abs_nibble
    ld a, (piece_row)
    and 0x0f
    call ikkle_draw_abs_nibble
    ld a, (piece_col)
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    call ikkle_draw_abs_nibble
    ld a, (piece_col)
    and 0x0f

ikkle_draw_abs_nibble:
    ld (tmp_attr), a
    ld a, (tmp_scan)
    ld d, a
    ld a, (piece_scan)
    add a, d
    call compute_pixel_base
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ld a, (tmp_col)
    bit 0, a
    jr nz, idan_odd
    ld a, (tmp_attr)
    rlca
    rlca
    rlca
    rlca
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    jr idan_next
idan_odd:
    ld a, (tmp_attr)
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
idan_next:
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    ret

ikkle_clear_cell_hl:
    push hl
    bit 0, c
    jr nz, icc_odd

    ld b, 4
icc_even_loop:
    ld a, (hl)
    and 0x0f
    ld (hl), a
    inc h
    djnz icc_even_loop
    pop hl
    ret

icc_odd:
    ld b, 4
icc_odd_loop:
    ld a, (hl)
    and 0xf0
    ld (hl), a
    inc h
    djnz icc_odd_loop
    pop hl
    ret

ikkle_blit_de_hl:
    bit 0, c
    jr nz, ib_odd_col

    ld a, d
    and 0xf0
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    ld a, d
    and 0x0f
    rlca
    rlca
    rlca
    rlca
    inc h
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    ld a, e
    and 0xf0
    inc h
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    ld a, e
    and 0x0f
    rlca
    rlca
    rlca
    rlca
    inc h
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    ret

ib_odd_col:
    ld a, d
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
    ld a, d
    and 0x0f
    inc h
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
    ld a, e
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    inc h
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
    ld a, e
    and 0x0f
    inc h
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
    ret

draw_connection_indicator:
    ld a, NETCHESSZX_STATUS_ROW
    call compute_screen_base
    ld a, 31
    add a, l
    ld l, a
    ld de, conn_pattern
    ld b, 8
dci_px:
    ld a, (de)
    ld (hl), a
    inc de
    inc h
    djnz dci_px
    ld a, NETCHESSZX_STATUS_ROW
    call compute_attr_base
    ld a, 31
    add a, l
    ld l, a
    ld a, (conn_attr)
    or a
    jr nz, dci_attr_ready
    ld a, ATTR_CONN_OFF
dci_attr_ready:
    ld (hl), a
    ret

draw_right_hline:
    call compute_screen_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld a, (hl)
    or 0x0f
    ld (hl), a
    inc l
    ld b, 13
drh_loop:
    ld (hl), 0xff
    inc l
    djnz drh_loop
    ret

draw_right_hline_full_left:
    call compute_screen_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld b, 14
drhfl_loop:
    ld (hl), 0xff
    inc l
    djnz drhfl_loop
    ret

draw_banner_separator:
    ld a, 2
    call compute_screen_base
    ld b, 32
dbs_loop:
    ld (hl), 0xff
    inc hl
    djnz dbs_loop
    ret

_spectrum_key_edit_pressed:
    ld hl, 0
    ld bc, 0xfefe
    in a, (c)
    rra
    ret c
    ld b, 0xf7
    in a, (c)
    rra
    ret c
    inc l
    ret

spectrum_key_scan_raw:
    ld bc, 0xfefe
    in a, (c)
    bit 0, a
    jr nz, skp_no_break
    ld b, 0x7f
    in a, (c)
    bit 0, a
    jr nz, skp_no_break
    ld hl, 0x8a
    ret

skp_no_break:
    ld b, 0xbf
    in a, (c)
    bit 0, a
    jr nz, skp_space
    ld hl, 13
    ret

skp_space:
    ld b, 0x7f
    in a, (c)
    bit 0, a
    jr nz, skp_shift
    ld hl, 32
    ret

skp_shift:
    ld b, 0xfe
    in a, (c)
    bit 0, a
    jr nz, skp_symbol_check

    ld b, 0xef
    in a, (c)
    bit 0, a
    jr nz, skp_shift_9
    ld hl, 8
    ret
skp_shift_9:
    bit 1, a
    jr nz, skp_shift_8
    jp skp_none
skp_shift_8:
    bit 2, a
    jr nz, skp_shift_7
    ld hl, 0x84
    ret
skp_shift_7:
    bit 3, a
    jr nz, skp_shift_6
    ld hl, 0x81
    ret
skp_shift_6:
    bit 4, a
    jr nz, skp_shift_5
    ld hl, 0x82
    ret
skp_shift_5:
    ld b, 0xf7
    in a, (c)
    bit 0, a
    jr nz, skp_shift_2
    ld hl, 0x88
    ret
skp_shift_2:
    bit 1, a
    jr nz, skp_shift_5_only
    ld hl, 0x89
    ret
skp_shift_5_only:
    bit 4, a
    jr nz, skp_symbol_check
    ld hl, 0x83
    ret

skp_symbol_check:
    ld b, 0x7f
    in a, (c)
    bit 1, a
    jp nz, skp_letters

skp_symbol:
    ld b, 0xfe
    in a, (c)
    bit 3, a
    jr nz, skp_sym_z
    ld hl, '?'
    ret
skp_sym_z:
    bit 1, a
    jr nz, skp_sym_x
    ld hl, ':'
    ret
skp_sym_x:
    bit 2, a
    jr nz, skp_sym_v
    ld hl, '`'
    ret
skp_sym_v:
    bit 4, a
    jr nz, skp_sym_arow
    ld hl, '/'
    ret

skp_sym_arow:
    ld b, 0xfd
    in a, (c)
    bit 0, a
    jr nz, skp_sym_s
    ld hl, '~'
    ret
skp_sym_s:
    bit 1, a
    jr nz, skp_sym_d
    ld hl, '|'
    ret
skp_sym_d:
    bit 2, a
    jr nz, skp_sym_f
    ld hl, 92
    ret
skp_sym_f:
    bit 3, a
    jr nz, skp_sym_g
    ld hl, '{'
    ret
skp_sym_g:
    bit 4, a
    jr nz, skp_sym_qrow
    ld hl, '}'
    ret

skp_sym_qrow:
    ld b, 0xfb
    in a, (c)
    bit 0, a
    jr nz, skp_sym_w
    jp skp_none
skp_sym_w:
    bit 1, a
    jr nz, skp_sym_e
    jp skp_none
skp_sym_e:
    bit 2, a
    jr nz, skp_sym_r
    jp skp_none
skp_sym_r:
    bit 3, a
    jr nz, skp_sym_t
    ld hl, '<'
    ret
skp_sym_t:
    bit 4, a
    jr nz, skp_sym_digits_1
    ld hl, '>'
    ret

skp_sym_digits_1:
    ld b, 0xf7
    in a, (c)
    bit 0, a
    jr nz, skp_sym_d2
    ld hl, '!'
    ret
skp_sym_d2:
    bit 1, a
    jr nz, skp_sym_d3
    ld hl, '@'
    ret
skp_sym_d3:
    bit 2, a
    jr nz, skp_sym_d4
    ld hl, '#'
    ret
skp_sym_d4:
    bit 3, a
    jr nz, skp_sym_d5
    ld hl, '$'
    ret
skp_sym_d5:
    bit 4, a
    jr nz, skp_sym_digits_6
    ld hl, '%'
    ret

skp_sym_digits_6:
    ld b, 0xef
    in a, (c)
    bit 4, a
    jr nz, skp_sym_d7
    ld hl, '&'
    ret
skp_sym_d7:
    bit 3, a
    jr nz, skp_sym_d8
    ld hl, 39
    ret
skp_sym_d8:
    bit 2, a
    jr nz, skp_sym_d9
    ld hl, '('
    ret
skp_sym_d9:
    bit 1, a
    jr nz, skp_sym_d0
    ld hl, ')'
    ret
skp_sym_d0:
    bit 0, a
    jr nz, skp_sym_o_p
    ld hl, '_'
    ret

skp_sym_o_p:
    ld b, 0xdf
    in a, (c)
    bit 0, a
    jr nz, skp_sym_o
    ld hl, 34
    ret
skp_sym_o:
    bit 1, a
    jr nz, skp_sym_i
    ld hl, ';'
    ret
skp_sym_i:
    bit 2, a
    jr nz, skp_sym_u
    jp skp_none
skp_sym_u:
    bit 3, a
    jr nz, skp_sym_y
    ld hl, ']'
    ret
skp_sym_y:
    bit 4, a
    jr nz, skp_sym_hrow
    ld hl, '['
    ret

skp_sym_hrow:
    ld b, 0xbf
    in a, (c)
    bit 4, a
    jr nz, skp_sym_j
    ld hl, '^'
    ret
skp_sym_j:
    bit 3, a
    jr nz, skp_sym_k
    ld hl, '-'
    ret
skp_sym_k:
    bit 2, a
    jr nz, skp_sym_l
    ld hl, '+'
    ret
skp_sym_l:
    bit 1, a
    jr nz, skp_sym_bottom
    ld hl, '='
    ret

skp_sym_bottom:
    ld b, 0x7f
    in a, (c)
    bit 2, a
    jr nz, skp_sym_n
    ld hl, '.'
    ret
skp_sym_n:
    bit 3, a
    jr nz, skp_sym_b
    ld hl, ','
    ret
skp_sym_b:
    bit 4, a
    jp nz, skp_none
    ld hl, '*'
    ret

skp_letters:
    ld b, 0xfe
    in a, (c)
    bit 3, a
    jr nz, skp_z
    ld hl, 'c'
    ret
skp_z:
    bit 1, a
    jr nz, skp_x
    ld hl, 'z'
    ret
skp_x:
    bit 2, a
    jr nz, skp_v
    ld hl, 'x'
    ret
skp_v:
    bit 4, a
    jr nz, skp_qrow
    ld hl, 'v'
    ret

skp_qrow:
    ld b, 0xfb
    in a, (c)
    bit 0, a
    jr nz, skp_w
    ld hl, 'q'
    ret
skp_w:
    bit 1, a
    jr nz, skp_e
    ld hl, 'w'
    ret
skp_e:
    bit 2, a
    jr nz, skp_r
    ld hl, 'e'
    ret
skp_r:
    bit 3, a
    jr nz, skp_t
    ld hl, 'r'
    ret
skp_t:
    bit 4, a
    jr nz, skp_arow
    ld hl, 't'
    ret

skp_arow:
    ld b, 0xfd
    in a, (c)
    bit 0, a
    jr nz, skp_s
    ld hl, 'a'
    ret
skp_s:
    bit 1, a
    jr nz, skp_d
    ld hl, 's'
    ret
skp_d:
    bit 2, a
    jr nz, skp_f
    ld hl, 'd'
    ret
skp_f:
    bit 3, a
    jr nz, skp_g
    ld hl, 'f'
    ret
skp_g:
    bit 4, a
    jr nz, skp_bottom
    ld hl, 'g'
    ret

skp_bottom:
    ld b, 0x7f
    in a, (c)
    bit 4, a
    jr nz, skp_m
    ld hl, 'b'
    ret
skp_m:
    bit 2, a
    jr nz, skp_n
    ld hl, 'm'
    ret
skp_n:
    bit 3, a
    jr nz, skp_hrow
    ld hl, 'n'
    ret

skp_hrow:
    ld b, 0xbf
    in a, (c)
    bit 4, a
    jr nz, skp_j
    ld hl, 'h'
    ret
skp_j:
    bit 3, a
    jr nz, skp_k
    ld hl, 'j'
    ret
skp_k:
    bit 2, a
    jr nz, skp_l
    ld hl, 'k'
    ret
skp_l:
    bit 1, a
    jr nz, skp_o_p
    ld hl, 'l'
    ret

skp_o_p:
    ld b, 0xdf
    in a, (c)
    bit 0, a
    jr nz, skp_o
    ld hl, 'p'
    ret
skp_o:
    bit 1, a
    jr nz, skp_i
    ld hl, 'o'
    ret
skp_i:
    bit 2, a
    jr nz, skp_u
    ld hl, 'i'
    ret
skp_u:
    bit 3, a
    jr nz, skp_y
    ld hl, 'u'
    ret
skp_y:
    bit 4, a
    jr nz, skp_digits_1
    ld hl, 'y'
    ret

skp_digits_1:
    ld b, 0xf7
    in a, (c)
    bit 0, a
    jr nz, skp_d2
    ld hl, '1'
    ret
skp_d2:
    bit 1, a
    jr nz, skp_d3
    ld hl, '2'
    ret
skp_d3:
    bit 2, a
    jr nz, skp_d4
    ld hl, '3'
    ret
skp_d4:
    bit 3, a
    jr nz, skp_d5
    ld hl, '4'
    ret
skp_d5:
    bit 4, a
    jr nz, skp_digits_6
    ld hl, '5'
    ret

skp_digits_6:
    ld b, 0xef
    in a, (c)
    bit 4, a
    jr nz, skp_d7
    ld hl, '6'
    ret
skp_d7:
    bit 3, a
    jr nz, skp_d8
    ld hl, '7'
    ret
skp_d8:
    bit 2, a
    jr nz, skp_d9
    ld hl, '8'
    ret
skp_d9:
    bit 1, a
    jr nz, skp_d0
    ld hl, '9'
    ret
skp_d0:
    bit 0, a
    jr nz, skp_none
    ld hl, '0'
    ret

skp_none:
    ld hl, 0
    ret

_spectrum_key_poll:
    call spectrum_key_scan_raw
    ld a, l
    or a
    jr nz, skp_repeat_got_key
    ld (key_last), a
    ld (key_repeat_timer), a
    ret

skp_repeat_got_key:
    ld b, a
    ld a, (key_last)
    cp b
    jr z, skp_repeat_same_key

    ld a, b
    ld (key_last), a
    call skp_repeat_start_delay
    ld (key_repeat_timer), a
    ld l, b
    ld h, 0
    ret

skp_repeat_same_key:
    ld a, b
    call skp_repeatable
    or a
    jr z, skp_repeat_zero

    ld hl, key_repeat_timer
    ld a, (hl)
    or a
    jr z, skp_repeat_fire
    dec (hl)
    jr skp_repeat_zero

skp_repeat_fire:
    ld a, b
    call skp_repeat_next_delay
    ld (key_repeat_timer), a
    ld l, b
    ld h, 0
    ret

skp_repeat_zero:
    jp skp_none

skp_repeatable:
    cp 8
    jr z, skp_repeatable_yes
    cp 0x81
    jr c, skp_repeatable_text
    cp 0x8a
    jr c, skp_repeatable_yes

skp_repeatable_text:
    cp 33
    jr c, skp_repeatable_no
    cp 127
    jr c, skp_repeatable_yes

skp_repeatable_no:
    xor a
    ret

skp_repeatable_yes:
    ld a, 1
    ret

skp_repeat_start_delay:
    cp 8
    jr z, skp_repeat_start_backspace
    cp 0x81
    jr c, skp_repeat_start_text
    cp 0x8a
    jr c, skp_repeat_start_nav

skp_repeat_start_text:
    ld a, 20
    ret

skp_repeat_start_backspace:
    ld a, 12
    ret

skp_repeat_start_nav:
    ld a, 15
    ret

skp_repeat_next_delay:
    cp 8
    jr z, skp_repeat_next_backspace
    cp 0x81
    jr z, skp_repeat_next_vertical
    cp 0x82
    jr z, skp_repeat_next_vertical
    cp 0x83
    jr c, skp_repeat_next_text
    cp 0x8a
    jr c, skp_repeat_next_nav

skp_repeat_next_text:
    ld a, 3
    ret

skp_repeat_next_backspace:
    ld a, 1
    ret

skp_repeat_next_nav:
    ld a, 2
    ret

skp_repeat_next_vertical:
    ld a, 5
    ret

clear_right_text_pixels:
    ld (tmp_row), a
    call compute_screen_base
    ld d, h
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld e, a
    ld c, 13
    jp clear_pixels_8

clear_right_pixel_band_abs:
    ld (tmp_scan), a
    ld a, b
    ld (piece_scan), a
    ld a, (tmp_scan)
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
crpba_loop:
    ld a, (piece_scan)
    or a
    ret z
    push hl
    ld b, 14
    xor a
crpba_px_loop:
    ld (hl), a
    inc hl
    djnz crpba_px_loop
    pop hl
    call pixel_down_hl
    ld a, (piece_scan)
    dec a
    ld (piece_scan), a
    jr crpba_loop

pixel_down_hl:
    inc h
    ld a, h
    and 7
    ret nz
    ld a, l
    add a, 32
    ld l, a
    ret c
    ld a, h
    sub 8
    ld h, a
    ret

clear_right_text_row:
    call clear_right_text_pixels
    ld a, (tmp_row)
    call compute_attr_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld b, 14
crtr_attr:
    ld (hl), ATTR_TEXT
    inc hl
    djnz crtr_attr
    ret

clear_notice_row:
    ld a, NETCHESSZX_INFO_NOTICE_ROW
    jp clear_right_text_row

calc_len64:
    ld b, 64
cl64_loop:
    ld a, (hl)
    or a
    jr z, cl64_done
    inc hl
    djnz cl64_loop
cl64_done:
    ld a, b
    ret

font_scanline:
    cp 127
    jr nz, fs_not_swatch
    ld a, 0xf0
    ret
fs_not_swatch:
    cp 32
    jr c, fs_blank
    cp 127
    jr nc, fs_blank
    sub 32
    ld l, a
    ld h, 0
    add hl, hl
    ld d, 0
    ld e, a
    add hl, de
    ld de, font_packed
    add hl, de
    ld a, (tmp_scan)
    srl a
    ld e, a
    ld d, 0
    add hl, de
    ld a, (hl)
    ld d, a
    ld a, (tmp_scan)
    and 1
    ld a, d
    jr nz, fs_low
    rrca
    rrca
    rrca
    rrca
fs_low:
    and 0x0f
    ld e, a
    ld d, 0
    ld hl, font_lut
    add hl, de
    ld a, (hl)
    ret
fs_blank:
    xor a
    ret

clear_text_row:
    push af
    push bc
    call clear_row_pixels
    pop bc
    pop af
    jp fill_attr_line

clear_row_pixels:
    call compute_screen_base
    ld d, h
    ld e, l
    ld c, 31
    jp clear_pixels_8

clear_pixels_8:
    ld b, 8
cp8_loop:
    push bc
    push de
    ld h, d
    ld l, e
    ld (hl), 0
    inc de
    ld b, 0
    ldir
    pop de
    pop bc
    inc d
    djnz cp8_loop
    ret

fill_attr_line:
    call compute_attr_base
    ld (hl), c
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ldir
    ret

_netchesszx_board_theme_apply:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    cp 5
    jr c, nbta_index_ok
    xor a
nbta_index_ok:
    ld (_netchesszx_board_theme_index), a
    ld e, a
    ld d, 0
    ld hl, board_theme_light_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_light_attr), a
    ld hl, board_theme_dark_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_dark_attr), a
    call restore_board_frame_attrs
    jp draw_board_frame

board_theme_light_attrs:
    DEFB 0x38, 0x31, 0x3a, 0x29, 0x62
board_theme_dark_attrs:
    DEFB 0x07, 0x0e, 0x17, 0x0d, 0x54

_spectrum_setup_board_swatches:
    ld c, l
    ld hl, 0x5a16
    ld b, 0
    ld de, board_theme_light_attrs
setup_board_swatch_loop:
    ld a, (de)
    push de
    call setup_board_swatch
    pop de
    inc de
    inc b
    ld a, b
    cp 5
    jr nz, setup_board_swatch_loop
    ret
setup_board_swatch:
    ld e, a
    ld a, c
    cp b
    ld a, e
    jr z, setup_board_swatch_flash
    ld a, b
    add a, 5
    cp c
    ld a, e
    jr nz, setup_board_swatch_store
    or 0x40
    jr setup_board_swatch_store
setup_board_swatch_flash:
    ld a, e
    and 0xc0
    ld d, a
    ld a, e
    and 0x07
    rlca
    rlca
    rlca
    or d
    ld d, a
    ld a, e
    and 0x38
    rrca
    rrca
    rrca
    or d
    or 0x80
setup_board_swatch_store:
    ld (hl), a
    inc hl
    ld (hl), ATTR_TEXT
    inc hl
    ret

compute_screen_base:
    ld l, a
    and 0x18
    or 0x40
    ld h, a
    ld a, l
    and 0x07
    rrca
    rrca
    rrca
    ld l, a
    ret

compute_pixel_base:
    ld l, a
    and 0xc0
    rrca
    rrca
    rrca
    ld h, a
    ld a, l
    and 0x07
    or h
    or 0x40
    ld h, a
    ld a, l
    and 0x38
    rlca
    rlca
    ld l, a
    ret

compute_attr_base:
    rrca
    rrca
    rrca
    ld l, a
    and 0x03
    or 0x58
    ld h, a
    ld a, l
    and 0xe0
    ld l, a
    ret

draw_badge:
    ld hl, 0x401c
    ld c, 4
    call draw_badge_cells
    ld hl, 0x403b
    ld c, 5
    call draw_badge_cells

    ld hl, 0x581c
    ld (hl), 0x42
    inc hl
    ld (hl), 0x56
    inc hl
    ld (hl), 0x74
    inc hl
    ld (hl), 0x61

    ld hl, 0x583b
    ld (hl), 0x42
    inc hl
    ld (hl), 0x56
    inc hl
    ld (hl), 0x74
    inc hl
    ld (hl), 0x61
    inc hl
    ld (hl), 0x48
    ret

draw_badge_cells:
    ld de, badge_pattern
    ld b, 8
dbc_scan:
    push bc
    push hl
    ld a, (de)
    ld b, c
dbc_byte:
    ld (hl), a
    inc l
    djnz dbc_byte
    pop hl
    inc h
    inc de
    pop bc
    djnz dbc_scan
    ret

draw_square_mark:
    call compute_square_bc
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a

    ld a, (tmp_attr)
    ld (mark_mode), a
    ld a, (piece_row)
    call compute_attr_base
    ld a, (piece_col)
    add a, l
    ld l, a
    ld a, (hl)
    and 0x78
    ld (tmp_attr), a
    ld a, (_netchesszx_board_theme_index)
    ld e, a
    ld d, 0
    ld hl, board_theme_mark_inks
    add hl, de
    ld a, (tmp_attr)
    or (hl)
    ld d, a
    ld a, (piece_row)
    ld b, a
    ld a, (piece_col)
    ld c, a
    ld a, d
    call set_square_attr_2x2

    ld a, (piece_row)
    call compute_screen_base
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), 0xff
    inc l
    ld (hl), 0xff

    ld a, (piece_row)
    inc a
    call compute_screen_base
    ld a, 7
    add a, h
    ld h, a
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), 0xff
    inc l
    ld (hl), 0xff

    ld d, 0x80
    ld e, 0x01
    ld a, (piece_row)
    call dsm_prepare_side_row
    ld b, 8
    call dsm_draw_side_span
    ld a, (piece_row)
    inc a
    call dsm_prepare_side_row
    ld b, 8
    call dsm_draw_side_span

    ld a, (mark_mode)
    or a
    ret z
    call compute_square_bc
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    ld a, (piece_row)
    call compute_screen_base
    ld a, 1
    add a, h
    ld h, a
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), 0xff
    inc l
    ld (hl), 0xff

    ld a, (piece_row)
    inc a
    call compute_screen_base
    ld a, 6
    add a, h
    ld h, a
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), 0xff
    inc l
    ld (hl), 0xff

    ld d, 0x40
    ld e, 0x02
    ld a, (piece_row)
    call dsm_prepare_side_row
    inc h
    ld b, 7
    call dsm_draw_side_span
    ld a, (piece_row)
    inc a
    call dsm_prepare_side_row
    ld b, 7
    jp dsm_draw_side_span

dsm_prepare_side_row:
    call compute_screen_base
    ld a, (piece_col)
    add a, l
    ld l, a
    ret

dsm_draw_side_span:
    ld a, (hl)
    or d
    ld (hl), a
    inc l
    ld a, (hl)
    or e
    ld (hl), a
    dec l
    inc h
    djnz dsm_draw_side_span
    ret
