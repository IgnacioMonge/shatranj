PUBLIC board_theme_hint_inks

_spectrum_render_square_with_hint:
    push hl
    call _spectrum_render_square
    pop hl

    xor a
render_hint_from_spec:
    ld (mark_mode), a
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
    ld a, (mark_mode)
    or a
    jr z, rsh_piece_ptr_ready
    inc hl
    inc hl
    inc hl
rsh_piece_ptr_ready:
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
    ld a, (mark_mode)
    or a
    jr z, rsh_inks_ready
    ld hl, board_theme_mark_inks
rsh_inks_ready:
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
    jr draw_dot_row

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

render_hint_from_mark_spec:
    ld a, 1
    jp render_hint_from_spec
