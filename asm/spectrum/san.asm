SECTION bss_user

san_from:   defs 1
san_to:     defs 1
san_piece:  defs 1
san_from_c: defs 1
san_to_c:   defs 1
san_cap:    defs 1

SECTION code_user

PUBLIC _spectrum_board_move_san_base
PUBLIC _spectrum_board_san_append_suffix

EXTERN _netchesszx_asm_move_parse_coords
EXTERN _spectrum_board_cells
EXTERN _spectrum_board_is_legal_move_coords
EXTERN _spectrum_board_check_state

; uint8_t spectrum_board_move_san_base(const char *move, char *out)
; SP+2=move, SP+4=out. Return L=1 success / 0 fail. IY untouched.
_spectrum_board_move_san_base:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    ld a, d
    or e
    jp z, san_ret0
    ld a, h
    or l
    jp z, san_ret0
    xor a
    ld (hl), a
    push de
    ex de, hl
    call _netchesszx_asm_move_parse_coords
    ld a, h
    and l
    inc a
    jp z, san_fail_move
    ld a, l
    ld (san_from), a
    and 7
    ld (san_from_c), a
    ld a, h
    ld (san_to), a
    and 7
    ld (san_to_c), a
    pop hl
    ld bc, 4
    add hl, bc
    ld a, (hl)
    and a
    jr z, san_len_ok
    inc hl
    ld a, (hl)
    and a
    jp nz, san_fail_ix
    dec hl
san_len_ok:
    ld bc, $fffc
    add hl, bc
    push hl
    push de
    call _spectrum_board_cells
    pop de
    ld a, (san_from)
    add a, l
    ld l, a
    ld a, 0
    adc a, h
    ld h, a
    ld a, (hl)
    cp '.'
    jp z, san_fail_move
    ld (san_piece), a
    or $20
    cp 'k'
    jr nz, san_not_castle
    ld a, (san_from_c)
    cp 4
    jr nz, san_not_castle
    ld a, (san_to_c)
    cp 6
    jr z, san_oo
    cp 2
    jr nz, san_not_castle
    ld a, 'O'
    call san_put
    ld a, '-'
    call san_put
san_oo:
    ld a, 'O'
    call san_put
    ld a, '-'
    call san_put
    ld a, 'O'
    call san_put
    jp san_ok
san_not_castle:
    xor a
    ld (san_cap), a
    push de
    call _spectrum_board_cells
    pop de
    ld a, (san_to)
    add a, l
    ld l, a
    ld a, 0
    adc a, h
    ld h, a
    ld a, (hl)
    cp '.'
    jr nz, san_mark_cap
    ld a, (san_piece)
    or $20
    cp 'p'
    jr nz, san_cap_ready
    ld a, (san_from_c)
    ld hl, san_to_c
    cp (hl)
    jr z, san_cap_ready
san_mark_cap:
    ld a, 1
    ld (san_cap), a
san_cap_ready:
    ld a, (san_piece)
    or $20
    cp 'p'
    jr nz, san_piece_move
    ld a, (san_cap)
    or a
    jr z, san_dest
    ld a, (san_from_c)
    add a, 'a'
    call san_put
    jr san_dest
san_piece_move:
    ld a, (san_piece)
    call san_piece_letter
    or a
    jr z, san_fail_move
    call san_put
    ld a, (san_piece)
    or $20
    cp 'k'
    call nz, san_disambig
san_dest:
    ld a, (san_cap)
    or a
    jr z, san_dest_sq
    ld a, 'x'
    call san_put
san_dest_sq:
    ld a, (san_to_c)
    add a, 'a'
    call san_put
    ld a, (san_to)
    rrca
    rrca
    rrca
    and 7
    ld b, a
    ld a, '8'
    sub b
    call san_put
    pop hl
    push hl
    ld bc, 4
    add hl, bc
    ld a, (hl)
    or a
    jr z, san_ok
    call san_piece_letter
    or a
    jr z, san_fail_move
    push af
    ld a, '='
    call san_put
    pop af
    call san_put
san_ok:
    pop hl
    xor a
    ld (de), a
    ld l, 1
    ret

san_fail_move:
    pop hl
san_fail_ix:
san_ret0:
    ld l, 0
    ret

san_put:
    ld (de), a
    inc de
    ret

san_piece_letter:
    and $df
    cp 'N'
    ret z
    cp 'B'
    ret z
    cp 'R'
    ret z
    cp 'Q'
    ret z
    cp 'K'
    ret z
    xor a
    ret

san_disambig:
    ld c, 0
    ld b, 0
san_d_loop:
    ld a, b
    ld hl, san_from
    cp (hl)
    jr z, san_d_next
    push bc
    push de
    call _spectrum_board_cells
    pop de
    pop bc
    ld a, b
    add a, l
    ld l, a
    ld a, 0
    adc a, h
    ld h, a
    ld a, (san_piece)
    cp (hl)
    jr nz, san_d_next
    push bc
    push de
    ld a, (san_to)
    ld h, a
    ld l, b
    push hl
    call _spectrum_board_is_legal_move_coords
    ld a, l
    pop hl
    pop de
    pop bc
    or a
    jr z, san_d_next
    set 0, c
    ld a, b
    and 7
    ld hl, san_from_c
    cp (hl)
    jr nz, san_d_rank
    set 1, c
san_d_rank:
    ld a, b
    rrca
    rrca
    rrca
    and 7
    ld l, a
    ld a, (san_from)
    rrca
    rrca
    rrca
    and 7
    cp l
    jr nz, san_d_next
    set 2, c
san_d_next:
    inc b
    ld a, b
    cp 64
    jr nz, san_d_loop
    bit 0, c
    ret z
    bit 1, c
    jr nz, san_d_file_conflict
    ld a, (san_from_c)
    add a, 'a'
    jp san_put
san_d_file_conflict:
    bit 2, c
    jr z, san_d_rank_only
    ld a, (san_from_c)
    add a, 'a'
    call san_put
san_d_rank_only:
    ld a, (san_from)
    rrca
    rrca
    rrca
    and 7
    ld b, a
    ld a, '8'
    sub b
    jp san_put

; uint8_t spectrum_board_san_append_suffix(char *san) __z88dk_fastcall
_spectrum_board_san_append_suffix:
    ld a, h
    or l
    jp z, san_ret0
    push hl
    call _spectrum_board_check_state
    pop de
    ld c, l
    ld a, c
    cp 1
    jr z, san_sfx_need
    cp 2
    jr z, san_sfx_need
    ld l, c
    ret
san_sfx_need:
    ld h, d
    ld l, e
san_sfx_scan:
    ld a, (hl)
    or a
    jr z, san_sfx_nul
    inc hl
    jr san_sfx_scan
san_sfx_nul:
    ld a, l
    cp e
    jr nz, san_sfx_prev
    ld a, h
    cp d
    jr z, san_sfx_write
san_sfx_prev:
    dec hl
    ld a, (hl)
    inc hl
    cp '+'
    jr z, san_sfx_done
    cp '#'
    jr z, san_sfx_done
san_sfx_write:
    ld a, c
    cp 2
    ld a, '#'
    jr z, san_sfx_store
    ld a, '+'
san_sfx_store:
    ld (hl), a
    inc hl
    ld (hl), 0
san_sfx_done:
    ld l, c
    ret
