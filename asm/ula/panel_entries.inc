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
    ; The 23-character line self-clears half-columns 41-63; a blank cell at
    ; 40 completes the 12-byte strip (pixels and attributes).
    push hl
    ld a, ATTR_TIMER
    ld (current_attr), a
    ld hl, input_prompt_msg + 1
    ld bc, (NETCHESSZX_TOP_TIMER_ROW * 8 + NETCHESSZX_TOP_TIMER_SCAN) * 256 + NETCHESSZX_TOP_TIMER_COL - 1
    call draw_ikkle_text_abs_y
    pop hl
    ld bc, (NETCHESSZX_TOP_TIMER_ROW * 8 + NETCHESSZX_TOP_TIMER_SCAN) * 256 + NETCHESSZX_TOP_TIMER_COL
    jp draw_ikkle_text_abs_y

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
    ld a, NETCHESSZX_TOP_TURN_CLEAR_COL
    jr nz, srtl_black
    ld hl, white_turn_msg
    jr srtl_draw
srtl_black:
    ld hl, black_turn_msg
    jr srtl_draw
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

clear_turn_strip:
    ld hl, ula_screen_top_turn + NETCHESSZX_TOP_TURN_BYTE_COL
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

_spectrum_render_notice:
    ld a, ATTR_NOTICE
    jr render_notice_common

_spectrum_render_notice_error:
    ld a, ATTR_ERROR
    jr render_notice_common

_spectrum_render_notice_success:
    ld a, ATTR_SUCCESS

render_notice_common:
    ld (current_attr), a
    call align_notice_text
    ld a, NETCHESSZX_INFO_PANEL_COL * 2
    ld (tmp_col), a
    ld a, NETCHESSZX_INFO_NOTICE_ROW
    ld (tmp_row), a
    ld a, 2
    ld (tmp_scan), a
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

; Classic calls these only inside MENU_CONFIG; other targets keep their owner.
; The Next extension also receives the outer Classic assembler flags.
IFDEF NETCHESSZX_CLASSIC_BOOT
IFNDEF NETCHESSZX_NEXT
DEFC setup_headers_in_menu_config = 1
ENDIF
ENDIF
IFNDEF setup_headers_in_menu_config
INCLUDE "asm/overlay/menu_config/setup_headers.inc"
ENDIF

store_tmp_rcs:
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, d
    ld (tmp_scan), a
    ret

; CALL is followed by row, half-column, attribute and a text pointer. Replace
; the return address with the byte after that five-byte descriptor.
draw_ikkle_inline:
    pop hl
    ld a, (hl)
    ld (tmp_row), a
    inc hl
    ld a, (hl)
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (current_attr), a
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    push hl
    ex de, hl
    jp render_ikkle_text_at

PUBLIC _spectrum_render_ikkle_at
PUBLIC _spectrum_render_ikkle_abs_at
; HL -> spec: row cell, ikkle half-column (0-63), attr, NUL-terminated text.
_spectrum_render_ikkle_at:
    ld a, (hl)
    ld (tmp_row), a
    inc hl
    ld a, (hl)
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (current_attr), a
    inc hl
render_ikkle_text_at:
    ld a, 2
    ld (tmp_scan), a
    jp draw_ikkle_text_at

; HL -> spec: absolute Y, ikkle half-column (0-63), attr, NUL-terminated text.
_spectrum_render_ikkle_abs_at:
    ld a, (hl)
    ld b, a
    inc hl
    ld a, (hl)
    ld c, a
    inc hl
    ld a, (hl)
    ld (current_attr), a
    inc hl
    jp draw_ikkle_text_abs_y

PUBLIC _spectrum_render_fileui_select
EXTERN _spectrum_fileui_count
; L = list slot (0-9), H = 1 select / 0 deselect. Paints the item's 12
; attribute cells (items at ikkle half-col 4, rows 9-18): normal ink for
; deselect, inverted (taboption-style) for select; saved rows cyan, free
; rows white.
_spectrum_render_fileui_select:
    ld de, NETCHESSZX_FILEUI_ITEM_ATTRS_OFF        ; D = saved off (cyan), E = free off (white)
    ld a, h
    or a
    jr z, srfs_pick
    ld de, NETCHESSZX_FILEUI_ITEM_ATTRS_ON        ; D = saved on, E = free on (inverted)
srfs_pick:
    ld a, (_spectrum_fileui_count)
    ld b, a
    ld a, l
    cp b
    jr c, srfs_saved
    ld d, e
srfs_saved:
    add a, NETCHESSZX_FILEUI_ROW_FIRST
    push de
    call compute_attr_base
    pop de
    ld a, NETCHESSZX_FILEUI_ITEM_BYTE_COL
    add a, l
    ld l, a
    ld a, d
    ld b, NETCHESSZX_FILEUI_ITEM_WIDTH_BYTES
srfs_loop:
    ld (hl), a
    inc l
    djnz srfs_loop
    ret

PUBLIC _spectrum_render_fileui_frame
; Clear the board interior (rows 5-20, cells 1-16) to ATTR_TEXT while
; leaving the board frame pixels in the border cells untouched.
_spectrum_render_fileui_frame:
    call clear_board_coords
    ld c, NETCHESSZX_GAME_BOARD_TOP_ROW
srff_row:
    ld a, c
    call compute_screen_base_de8
srff_scan:
    ld h, d
    ld l, e
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL
    add a, l
    ld l, a
    push bc
    ld b, 16
    xor a
srff_px:
    ld (hl), a
    inc l
    djnz srff_px
    pop bc
    inc d
    djnz srff_scan
    ld a, c
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL
    add a, l
    ld l, a
    ld b, 16
    ld a, ATTR_TEXT
srff_attr:
    ld (hl), a
    inc l
    djnz srff_attr
    inc c
    ld a, c
    cp NETCHESSZX_GAME_BOARD_TOP_ROW + 16
    jr c, srff_row
    ld hl, ula_screen_fileui_separator + NETCHESSZX_GAME_BOARD_LEFT_COL
    ld b, 16
    ld a, 0xff
srff_sep:
    ld (hl), a
    inc l
    djnz srff_sep
    jp draw_board_frame

attr_cell_for_tmpcol:
    call compute_attr_base
    jp tmpcol_add_l

_spectrum_info_show_preflight:
    call clear_right_panel_rows
    call draw_ikkle_inline
    DEFB NETCHESSZX_INFO_SETUP_HEADER_ROW, NETCHESSZX_INFO_TEXT_COL, ATTR_MOVES_TITLE
    DEFW preflight_setup_msg
    ld a, NETCHESSZX_INFO_SETUP_HLINE_ROW
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
    ld d, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
    jp draw_text64_line_pixels_fast
