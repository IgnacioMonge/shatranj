draw_board_clean:
    ld hl, render_skip_clear
    inc (hl)
    push hl
    call draw_board
    pop hl
    dec (hl)
    ret

draw_board:
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
    call draw_board_square_view

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
    ld hl, ula_screen_board_top_m1
    ld a, 7
    add a, h
    ld h, a
    call draw_board_hline

    ld hl, ula_screen_board_bottom
    call draw_board_hline

    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW
    ld (tmp_row), a
    ld b, 16
dbf_row:
    push bc
    ld a, (tmp_row)
    call compute_screen_base_de8
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
    ld hl, ula_attr_board_top
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
    ld a, d
    call set_square_attr_2x2
    ld a, (render_skip_clear)
    cp RENDER_ATTRS_ONLY
    ret z
    ld a, (piece_char)
    cp ' '
    jp nz, draw_piece_sprite_16x16
    ld a, (render_skip_clear)
    or a
    ret nz
    jp clear_square_pixels_2x2

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


