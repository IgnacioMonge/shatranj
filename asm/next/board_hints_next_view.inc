next_marker_set_hint_current_square:
    ld a, NEXT_MARKER_FLAG_HINT
    jr next_marker_or_current_square

next_draw_legal_hints:
    ld hl, _netchesszx_hinted_rows
    ld d, 0
ndlh_row:
    ld e, (hl)
    ld c, 0
ndlh_col:
    srl e
    jr nc, ndlh_next_col
    push hl
    push de
    push bc
    ld a, (_spectrum_board_view_flipped)
    or a
    jr z, ndlh_coords_ready
    ld a, 7
    sub d
    ld d, a
    ld a, 7
    sub c
    ld c, a
ndlh_coords_ready:
    ld a, d
    ld (board_row), a
    ld a, c
    ld (board_col), a
    call next_marker_set_hint_current_square
    pop bc
    pop de
    pop hl
ndlh_next_col:
    inc c
    ld a, e
    or a
    jr nz, ndlh_col
    inc hl
    inc d
    ld a, d
    cp 8
    jr nz, ndlh_row
    ret
