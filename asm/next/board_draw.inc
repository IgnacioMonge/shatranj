
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
    ld a, (_netchesszx_board_theme_index)
    or a
    jr z, blla_classic
    ld a, NEXT_BOARD_COORD_LINE_ATTR
    ret
blla_classic:
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
    call next_marker_release_current_square
    call next_draw_board_square_sprite

    ld a, (piece_char)
    cp ' '
    jr z, next_hide_square_sprite
    ld a, (piece_char)
    jr next_draw_piece_sprite_16x16

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

next_draw_piece_sprite_16x16:
    call next_piece_pattern_for_char
    jr c, next_hide_square_sprite
    ld (tmp_char), a
    call next_square_sprite_slot
    ld (tmp_attr), a
    call next_find_piece_slot
    jr nc, ndps_slot_ready
    call next_alloc_piece_slot
    ret c
ndps_slot_ready:
    add a, NEXT_PIECE_SPRITE_SLOT_BASE
    ld bc, NEXT_SPRITE_SLOT_PORT
    out (c), a
    call next_write_square_sprite_xy
    ld a, (tmp_char)
    or NEXT_SPRITE_VISIBLE
    out (c), a
    ret

next_hide_square_sprite:
    call next_square_sprite_slot
    ld (tmp_attr), a
    call next_find_piece_slot
    ret c
    jp next_hide_piece_slot

next_draw_board_square_sprite:
    call next_square_sprite_slot
    add a, NEXT_BOARD_SPRITE_SLOT_BASE
    ld bc, NEXT_SPRITE_SLOT_PORT
    out (c), a
    call next_write_square_sprite_xy
    call square_parity
    ld (tmp_scan), a
    ld a, (_netchesszx_board_theme_index)
    add a, a
    ld e, a
    ld a, (tmp_scan)
    add a, e
    add a, NEXT_BOARD_PATTERN_BASE
    or NEXT_SPRITE_VISIBLE
    out (c), a
    ret

INCLUDE "asm/next/board_hints_next_view.inc"

next_marker_set_mark_current_square:
    ld a, NEXT_MARKER_FLAG_MARK
    ld c, a
    ld a, (mark_mode)
    or a
    jr z, nmsm_flags_ready
    ld a, c
    or NEXT_MARKER_FLAG_SELECTED
    ld c, a
nmsm_flags_ready:
    ld a, c

next_marker_or_current_square:
    ld (tmp_char), a
    call next_square_sprite_slot
    ld (tmp_attr), a
    call next_find_marker_slot
    jr nc, nmuf_slot_ready
    call next_alloc_marker_slot
    ret c

nmuf_slot_ready:
    ld e, a
    ld d, 0
    ld hl, marker_slot_flags
    add hl, de
    ld a, (tmp_char)
    or (hl)
    ld (hl), a
    ld (tmp_char), a
    ld a, e
    add a, NEXT_MARKER_SPRITE_SLOT_BASE
    ld bc, NEXT_SPRITE_SLOT_PORT
    out (c), a
    call next_write_square_sprite_xy
    ld a, (tmp_char)
    bit 2, a
    jr nz, nmuf_selected
    bit 1, a
    jr z, nmuf_hint
    bit 0, a
    jr z, nmuf_cursor
    ld a, NEXT_MARKER_PATTERN_BASE + 2
    jr nmuf_pattern_ready
nmuf_selected:
    ld a, NEXT_MARKER_PATTERN_BASE + 3
    jr nmuf_pattern_ready
nmuf_cursor:
    ld a, NEXT_MARKER_PATTERN_BASE + 1
    jr nmuf_pattern_ready
nmuf_hint:
    ld a, NEXT_MARKER_PATTERN_BASE
nmuf_pattern_ready:
    or NEXT_SPRITE_VISIBLE
    out (c), a
    ret

next_marker_release_current_square:
    call next_square_sprite_slot
    ld (tmp_attr), a
    call next_find_marker_slot
    ret c
    ld e, a
    ld d, 0
    ld hl, marker_slot_flags
    add hl, de
    ld (hl), 0
    ld a, e
    jr next_hide_marker_slot

next_find_piece_slot:
    ld hl, piece_slot_squares
    jr next_find_slot

next_find_marker_slot:
    ld hl, marker_slot_squares

next_find_slot:
    ld b, 32
    ld c, 0
nfs_loop:
    ld a, (tmp_attr)
    cp (hl)
    jr z, nfs_found
    inc hl
    inc c
    djnz nfs_loop
    scf
    ret
nfs_found:
    ld a, c
    or a
    ret

next_alloc_piece_slot:
    ld hl, piece_slot_squares
    jr next_alloc_slot

next_alloc_marker_slot:
    ld hl, marker_slot_squares

next_alloc_slot:
    ld b, 32
    ld c, 0
nas_loop:
    ld a, (hl)
    cp NEXT_EMPTY_SLOT
    jr z, nas_found
    inc hl
    inc c
    djnz nas_loop
    scf
    ret
nas_found:
    ld a, (tmp_attr)
    ld (hl), a
    ld a, c
    or a
    ret

next_hide_piece_slot:
    ld e, a
    ld d, 0
    ld hl, piece_slot_squares
    add hl, de
    ld (hl), NEXT_EMPTY_SLOT
    ld a, e
    add a, NEXT_PIECE_SPRITE_SLOT_BASE
    jr next_hide_hardware_sprite

next_hide_marker_slot:
    ld e, a
    ld d, 0
    ld hl, marker_slot_squares
    add hl, de
    ld (hl), NEXT_EMPTY_SLOT
    ld a, e
    add a, NEXT_MARKER_SPRITE_SLOT_BASE

next_hide_hardware_sprite:
    ld bc, NEXT_SPRITE_SLOT_PORT
    out (c), a
    ld bc, NEXT_SPRITE_ATTR_PORT
    xor a
    out (c), a
    out (c), a
    out (c), a
    out (c), a
    ret

next_write_square_sprite_xy:
    ld bc, NEXT_SPRITE_ATTR_PORT
    ld a, (board_col)
    add a, a
    add a, a
    add a, a
    add a, a
    add a, NEXT_SPRITE_X_BASE
    out (c), a
    ld a, (board_row)
    add a, a
    add a, a
    add a, a
    add a, a
    add a, NEXT_SPRITE_Y_BASE
    out (c), a
    xor a
    out (c), a
    ret

_spectrum_next_sprites_hide_all:
    ld d, 0
next_hide_all_loop:
    ld a, d
    call next_hide_hardware_sprite
    inc d
    ld a, d
    cp 128
    jr nz, next_hide_all_loop

next_sprite_tables_reset:
    ld hl, piece_slot_squares
    ld b, 32
    ld a, NEXT_EMPTY_SLOT
nstr_piece_loop:
    ld (hl), a
    inc hl
    djnz nstr_piece_loop
    ld hl, marker_slot_squares
    ld b, 32
    ld a, NEXT_EMPTY_SLOT
nstr_marker_loop:
    ld (hl), a
    inc hl
    djnz nstr_marker_loop
    ld hl, marker_slot_flags
    ld b, 32
    xor a
nstr_flags_loop:
    ld (hl), a
    inc hl
    djnz nstr_flags_loop
    ret

next_square_sprite_slot:
    ld a, (board_row)
    add a, a
    add a, a
    add a, a
    ld d, a
    ld a, (board_col)
    add a, d
    ret

next_piece_pattern_for_char:
    ld c, 0
    cp 'a'
    jr c, npp_upper_ready
    sub 32
    ld c, 6
npp_upper_ready:
    ld (tmp_char), a
    ld hl, psfc_piece_chars
    ld b, 6
npp_match:
    cp (hl)
    jr z, npp_found
    inc hl
    djnz npp_match
    scf
    ret
npp_found:
    ld a, 6
    sub b
    add a, c
    or a
    ret

