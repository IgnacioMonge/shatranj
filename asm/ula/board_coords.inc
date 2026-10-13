clear_board_coords:
    ld a, NETCHESSZX_GAME_BOARD_TOP_ROW - 1
    call compute_screen_base_de8
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
    call compute_screen_base_de8
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

; Cell i gets file index i (7 - i flipped) and rank index file xor 7;
; board_col/board_row end at 8 as the separate loops left them.
draw_board_coords:
    call board_light_line_attr
    ld (current_attr), a
    xor a

coord_loop:
    ld (board_col), a
    ld (board_row), a
    cp 8
    ret nc
    ld b, a
    ld a, (_spectrum_gui_board_flipped)
    or a
    ld a, b
    jr z, coord_draw
    xor 7
coord_draw:
    push af
    call draw_file_letter_centered
    pop af
    xor 7
    call draw_rank_digit_centered
    ld a, (board_col)
    inc a
    jr coord_loop

; A = rank pattern index (0-7, '1'-'8').
draw_rank_digit_centered:
    push af

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

    pop af
    ld e, a
    add a, a
    add a, a
    add a, e
    ld e, a
    ld d, 0
    ld hl, rank_digit_patterns
    add hl, de
    ld (sprite_ptr), hl

    ; compute_screen_base preserves B; callers do not retain a scan counter.
    ld b, 5

drdc_loop:
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
    ld a, 5
    sub b
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

    djnz drdc_loop
    ret

; A = file pattern index (0-7, 'A'-'H').
draw_file_letter_centered:
    push af

    ld hl, ula_attr_board_top_m1
    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    add a, l
    ld l, a
    ld a, (current_attr)
    ld (hl), a

    pop af
    ld e, a
    add a, a
    add a, a
    add a, e
    ld e, a
    ld d, 0
    ld hl, file_letter_patterns
    add hl, de
    ld (sprite_ptr), hl

    ld b, 5

dflc_loop:
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

    ld hl, ula_screen_board_top_m1
    ld a, 5
    sub b
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

    djnz dflc_loop
    ret

; Invert board_row/board_col in place. Used to paint the logical cell at
; its flipped display square and then restore the loop counters.
flip_board_row_col:
    ld a, 7
    ld hl, board_row
    sub (hl)
    ld (hl), a
    ld a, 7
    ld hl, board_col
    sub (hl)
    ld (hl), a
    ret

; A = piece at the current logical board_row/board_col. Honor flip so a
; full-board blit never appears in the unflipped orientation first.
draw_board_square_view:
    ld c, a
    ld a, (_spectrum_gui_board_pieces_visible)
    or a
    jr nz, dbsv_piece_ready
    ld c, ' '
dbsv_piece_ready:
    ld a, (_spectrum_gui_board_flipped)
    or a
    ld a, c
    jp z, draw_one_board_square
    push bc
    call flip_board_row_col
    pop bc
    ld a, c
    call draw_one_board_square
    jp flip_board_row_col

