SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _spectrum_board_cells
PUBLIC _spectrum_board_is_legal_move_coords
PUBLIC _spectrum_board_check_state
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

EXTERN _spectrum_board_move_san_base
EXTERN _spectrum_board_san_append_suffix
EXTERN _netchesszx_asm_move_parse_coords

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, start_board
    ld (board_ptr), hl
    call clear_legal

    ; Startpos g1f3 must be "Nf3". The other knight (b1) is illegal to f3.
    ; The P0 pop-hl bug treats any same-piece square as a conflict → "Ngf3".
    ld hl, out_nf3
    push hl
    ld hl, move_g1f3
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld a, l
    or a
    ld a, 1
    jp z, test_store_result
    ld hl, out_nf3
    ld de, want_nf3
    call cmpstr
    ld a, 1
    jp nz, test_store_result

    ; Two-rook board, move a1d1. legal_tab[from] = other rook can reach d1.
    ld hl, rook_board
    ld (board_ptr), hl

    ; A: a5 and h1 both legal → Ra1d1
    call clear_legal
    ld a, 1
    ld (legal_tab + 24), a
    ld (legal_tab + 63), a
    ld hl, out_ra
    push hl
    ld hl, move_a1d1
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld hl, out_ra
    ld de, want_ra
    call cmpstr
    ld a, 2
    jp nz, test_store_result

    ; B: only h1 legal → Rad1
    call clear_legal
    ld a, 1
    ld (legal_tab + 63), a
    ld hl, out_rb
    push hl
    ld hl, move_a1d1
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld hl, out_rb
    ld de, want_rb
    call cmpstr
    ld a, 3
    jp nz, test_store_result

    ; C: only a5 legal → R1d1
    call clear_legal
    ld a, 1
    ld (legal_tab + 24), a
    ld hl, out_rc
    push hl
    ld hl, move_a1d1
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld hl, out_rc
    ld de, want_rc
    call cmpstr
    ld a, 4
    jp nz, test_store_result

    ; D: none legal → Rd1
    call clear_legal
    ld hl, out_rd
    push hl
    ld hl, move_a1d1
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld hl, out_rd
    ld de, want_rd
    call cmpstr
    ld a, 5
    jp nz, test_store_result

    ; Suffix: check → '+' appended once.
    ld a, 1
    ld (sfx_state), a
    ld hl, sfx_buf
    call _spectrum_board_san_append_suffix
    ld hl, sfx_buf
    ld de, want_sfx
    call cmpstr
    ld a, 6
    jp nz, test_store_result

    ; Failed parse must leave out[0] = 0 (C wrote NUL before parsing).
    ld hl, start_board
    ld (board_ptr), hl
    ld hl, out_fail
    ld (hl), 'X'
    push hl
    ld hl, move_bad
    push hl
    call _spectrum_board_move_san_base
    pop bc
    pop bc
    ld a, l
    or a
    ld a, 7
    jp nz, test_store_result
    ld a, (out_fail)
    or a
    ld a, 7
    jp nz, test_store_result

    xor a
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

clear_legal:
    ld hl, legal_tab
    ld de, legal_tab + 1
    ld bc, 63
    ld (hl), 0
    ldir
    ret

; DE=want, HL=got. ZF set if equal including the terminating NUL.
cmpstr:
    ld a, (de)
    cp (hl)
    ret nz
    or a
    ret z
    inc de
    inc hl
    jr cmpstr

_spectrum_board_cells:
    ld hl, (board_ptr)
    ret

_spectrum_board_check_state:
    ld a, (sfx_state)
    ld l, a
    ret

; Packed SDCC uint8,uint8: [sp+2]=from, [sp+3]=to. Table is keyed by from.
_spectrum_board_is_legal_move_coords:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    ld e, a
    ld d, 0
    ld hl, legal_tab
    add hl, de
    ld l, (hl)
    ret

_spectrum_net_payload_scratch:
    ld hl, dummy_payload
    ret

_spectrum_net_send_text:
    ld l, 1
    ret

board_ptr:
    defw start_board
sfx_state:
    defb 0
test_result:
    defb 0xff

move_g1f3:
    defm "g1f3"
    defb 0
want_nf3:
    defm "Nf3"
    defb 0
move_a1d1:
    defm "a1d1"
    defb 0
want_ra:
    defm "Ra1d1"
    defb 0
want_rb:
    defm "Rad1"
    defb 0
want_rc:
    defm "R1d1"
    defb 0
want_rd:
    defm "Rd1"
    defb 0
want_sfx:
    defm "Nf3+"
    defb 0
move_bad:
    defm "e1e1"
    defb 0
sfx_buf:
    defm "Nf3"
    defb 0, 0, 0, 0

start_board:
    defm "rnbqkbnr"
    defm "pppppppp"
    defm "........"
    defm "........"
    defm "........"
    defm "........"
    defm "PPPPPPPP"
    defm "RNBQKBNR"

rook_board:
    defm "....k..."
    defm "........"
    defm "........"
    defm "R......."
    defm "........"
    defm "........"
    defm "........"
    defm "R......R"

out_nf3:
    defs 16, 0
out_ra:
    defs 16, 0
out_rb:
    defs 16, 0
out_rc:
    defs 16, 0
out_rd:
    defs 16, 0
out_fail:
    defs 16, 0
legal_tab:
    defs 64, 0

_line_buf:
    defs 64, 0
_NETCHESS_PROTO_ACK_PREFIX:
    defm "ACK "
    defb 0
_NETCHESS_PROTO_NACK_PREFIX:
    defm "NACK "
    defb 0
dummy_payload:
    defs 64, 0
