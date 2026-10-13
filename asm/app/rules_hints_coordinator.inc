PUBLIC _rules_hints_ovl
PUBLIC _rules_hints_clear_ovl
EXTERN _netchesszx_board_theme_index
EXTERN _netchesszx_hinted_rows
EXTERN _spectrum_board_view_flipped
EXTERN _spectrum_board_view_redraw_square
EXTERN board_theme_hint_inks

INCLUDE "asm/app/board_hints_contract.inc"

; DE addresses the named request contract above. The coordinator queries the
; rules primitive and delivers each legal square to draw_square_hint; the
; primitive neither reads presentation state nor calls the view.
_rules_hints_ovl:
    push ix
    push iy
    call rules_import_board
    ld de, r_side
    ldi
    ldi
    inc de
    ldi
    ldi
    ld a, (_netchesszx_board_theme_index)
    ld hl, board_theme_hint_inks
    ld e, a
    ld d, 0
    add hl, de
    ld a, (hl)
    ld (r_hint_ink), a
    xor a
rh_loop:
    ld (r_to), a
    push af
    call rules_current_legal
    or a
    call nz, rh_draw_to
    pop af
    inc a
    cp 64
    jr nz, rh_loop
    ld l, 1
    pop iy
    pop ix
    ret

rh_draw_to:
    ld a, (r_to)
    ld c, a
    rrca
    rrca
    rrca
    and 7
    ld b, a
    ld a, c
    and 7
    ld c, a
    ld hl, _netchesszx_hinted_rows
    ld e, b
    ld d, 0
    add hl, de
    ld d, c
    inc d
    scf
    ld a, 0
rh_rot_loop:
    rla
    dec d
    jr nz, rh_rot_loop
    or (hl)
    ld (hl), a
    ld a, (_spectrum_board_view_flipped)
    or a
    jr z, rh_store_spec
    ld a, 7
    sub b
    ld b, a
    ld a, 7
    sub c
    ld c, a
rh_store_spec:
    ld hl, r_tmp
    ld (hl), b
    inc hl
    ld (hl), c
    inc hl
    ld a, (r_hint_ink)
    ld d, a
    ld a, (r_dst)
    or a
    ld a, d
    jr z, rh_store_hint_mode
    or 0x80
rh_store_hint_mode:
    ld (hl), a
    dec hl
    dec hl
    jp draw_square_hint

_rules_hints_clear_ovl:
    ld hl, _netchesszx_hinted_rows
    ld d, 0
rhc_row_loop:
    ld c, (hl)
    ld (hl), 0
    ld a, c
    or a
    jr z, rhc_next_row
    ld e, 0
rhc_col_loop:
    srl c
    jr nc, rhc_skip_square
    push hl
    push de
    push bc
    ld l, d
    ld h, e
    push hl
    call _spectrum_board_view_redraw_square
    pop bc
    pop bc
    pop de
    pop hl
rhc_skip_square:
    inc e
    ld a, c
    or a
    jr nz, rhc_col_loop
rhc_next_row:
    inc hl
    inc d
    ld a, d
    cp 8
    jr nz, rhc_row_loop
    ld l, 1
    ret
