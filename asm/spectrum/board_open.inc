_spectrum_render_board:
    ld (board_ptr), hl
    call clear_screen
    call draw_banner
    call draw_banner_separator
    call restore_board_frame_attrs
    call draw_board_coords
    ; clear_screen already blanked every Classic board bitmap byte.
    call draw_board_clean
    jp draw_board_frame

_spectrum_render_board_area:
    ld (board_ptr), hl
    call clear_board_coords
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board
    jp draw_board_frame

; Rebuild the Classic About footprint (rows 3-21, all 32 columns) the same
; way a board paint does after clear_screen: blank the band, then coords,
; squares and frame. Frame drawing ORs bits, so the canvas must be zero.
; Row 3 only holds the turn label, which the GUI repaints afterwards.
_spectrum_restore_game_center:
    ld (board_ptr), hl
    call restore_game_center_canvas
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board_clean
    jp draw_board_frame

restore_game_center_canvas:
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 2
rgcc_row:
    push af
    call clear_full_row_pixels
    pop af
    push af
    ld c, 0
    call fill_attr_line
    pop af
    inc a
    cp NETCHESSZX_GAME_BOARD_TOP_ROW + 17
    jr c, rgcc_row
    ret

; A=row. Zero 32 bytes on each of the 8 scanlines. Same fill as fill_attr_line.
clear_full_row_pixels:
    call compute_screen_base
    ld b, 8
cfrp_scan:
    push bc
    push hl
    xor a
    ld (hl), a
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ldir
    pop hl
    inc h
    pop bc
    djnz cfrp_scan
    ret

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
    ld hl, ula_attr_board_top_m1
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

