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
    ld de, NETCHESSZX_MOVE_LINES_BASE
    ld bc, NETCHESSZX_INFO_MOVES_FIRST_Y * 256 + NETCHESSZX_MOVE_SLOT_SIZE
    jr render_log_at

render_log_at:
    ld (move_ptr), hl
    or a
    sbc hl, de
    ld d, b
    ld b, 0
render_log_at_row_loop:
    ld a, l
    cp c
    jr c, render_log_at_row_ready
    sub c
    ld l, a
    inc b
    jr render_log_at_row_loop
render_log_at_row_ready:
    ld a, d
render_log_at_y_loop:
    inc b
    dec b
    jr z, render_log_at_y_ready
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    djnz render_log_at_y_loop
render_log_at_y_ready:
    push af
    dec a
    ld b, NETCHESSZX_INFO_TIGHT_LINE_STEP
    call clear_right_pixel_band_abs
    pop af
    ld (tmp_scan), a
    ld a, 1
    ld (render_skip_clear), a
    ld a, (tmp_scan)
    cp NETCHESSZX_INFO_CHAT_FIRST_Y
    jp nc, render_chat_current_line

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

; ULA builds assemble the log scroll into GUI_LOG, its only caller.
IFDEF NETCHESSZX_NEXT_BANKING
INCLUDE "asm/ula/log_scroll.inc"
ENDIF

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
    ld de, NETCHESSZX_CHAT_LINES_BASE
    ld bc, NETCHESSZX_INFO_CHAT_FIRST_Y * 256 + NETCHESSZX_CHAT_SLOT_SIZE
    jp render_log_at

render_chat_current_line:
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, (move_ptr)
    ld a, (hl)
    and 0x7f
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
    inc hl
    ld a, (tmp_scan)
    ld b, a
    ld c, NETCHESSZX_CHAT_TIME_COL
    jp draw_ikkle_text_abs_y

draw_chat_icon_at:
    ld (sprite_ptr), hl
    ld a, NETCHESSZX_CHAT_ICON_BYTE_COL
    ld (tmp_col), a
    ld a, 5
    ld (tmp_char), a

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
    ld d, 31                  ; bytes: 62 half-width glyphs, bounded to 0x57ff
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
