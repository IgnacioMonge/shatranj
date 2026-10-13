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

    ld d, 7
    ld e, 0
    call dsm_draw_horizontal_pair

    ld d, 0x80
    ld e, 0x01
    ld c, 8
    call dsm_draw_side_pair

    ld a, (mark_mode)
    or a
    ret z
    ld d, 6
    ld e, 1
    call dsm_draw_horizontal_pair

    ld d, 0x40
    ld e, 0x02
    ld c, 7
    jp dsm_draw_side_pair

dsm_draw_horizontal_pair:
    ld a, (piece_row)
    call dsm_draw_horizontal
    ld a, (piece_row)
    inc a
    ld e, d
    jp dsm_draw_horizontal

dsm_draw_side_pair:
    ld a, (piece_row)
    call dsm_prepare_side_row
    bit 6, d
    jr z, dsm_side_pair_no_inner_adjust
    inc h
dsm_side_pair_no_inner_adjust:
    ld b, c
    call dsm_draw_side_span
    ld a, (piece_row)
    inc a
    call dsm_prepare_side_row
    ld b, c
    jp dsm_draw_side_span

dsm_draw_horizontal:
    call compute_screen_base
    ld a, e
    add a, h
    ld h, a
    ld a, (piece_col)
    add a, l
    ld l, a
    ld (hl), 0xff
    inc l
    ld (hl), 0xff
    ret

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
