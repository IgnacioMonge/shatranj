_netchesszx_board_theme_apply:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    cp 5
    jr c, nbta_index_ok
    xor a
nbta_index_ok:
    ld (_netchesszx_board_theme_index), a
    call next_board_coord_palette_sync
    ld a, (_netchesszx_board_theme_index)
    ld e, a
    ld d, 0
    ld hl, board_theme_light_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_light_attr), a
    ld hl, board_theme_dark_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_dark_attr), a
    call restore_board_frame_attrs
    jp draw_board_frame

; Theme 1 uses the classic colours mirrored in ULA+ groups 0/1. Themes 2-5
; use private group-2 entries: unselected INK1/PAPER0 and selected
; INK2/PAPER1. Generated menu attributes and board/piece sprites are unchanged.
next_board_coord_palette_sync:
    ld a, NEXTREG_PALETTE_CONTROL
    call nextreg_read
    and 0xfe                    ; ULANext off; preserve active palette choices
    ld (tmp_scan), a
    ld a, (_netchesszx_board_theme_index)
    or a
    jr nz, nbcps_apply

    ld a, (tmp_scan)
    DEFB 0xed, 0x92, NEXTREG_PALETTE_CONTROL
    ret

nbcps_apply:
    dec a
    add a, a
    add a, a
    ld e, a
    ld d, 0
    ld hl, next_board_coord_rgb333
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    inc hl
    push de                     ; COLOR1
    ld d, (hl)
    inc hl
    ld e, (hl)                  ; COLOR2
    push de

    xor a                       ; Next init owns ULA palette 0
    DEFB 0xed, 0x92, NEXTREG_PALETTE_CONTROL

    pop hl
    ld e, NEXT_BOARD_COORD_INK2_INDEX
    call next_board_write_ula_pair
    pop hl
    push hl
    ld e, NEXT_BOARD_COORD_INK1_INDEX
    call next_board_write_ula_pair
    pop hl
    ld e, NEXT_BOARD_COORD_PAPER1_INDEX
    call next_board_write_ula_pair

    ld a, (tmp_scan)
    DEFB 0xed, 0x92, NEXTREG_PALETTE_CONTROL
    ret

; E = ULA palette index, HL = exact NextReg 0x44 pair.
next_board_write_ula_pair:
    ld a, e
    DEFB 0xed, 0x92, NEXTREG_PALETTE_INDEX
    ld bc, NEXTREG_SELECT_PORT
    ld a, NEXTREG_PALETTE_VALUE_9
    out (c), a
    ld bc, NEXTREG_DATA_PORT
    ld a, h
    out (c), a
    ld a, l
    out (c), a
    ret

INCLUDE "assets/next/coord_palette.asm"

