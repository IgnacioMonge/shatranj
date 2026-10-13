PUBLIC _spectrum_board_show_legal_hints
EXTERN _side_to_move
EXTERN _castle_rights
EXTERN _ep_square

INCLUDE "asm/app/board_hints_contract.inc"

_spectrum_board_show_legal_hints:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    inc hl
    ld c, (hl)
    add a, a
    add a, a
    add a, a
    add a, c
    ld d, a
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), NETCHESSZX_RULES_BOARD_BASE & 0xff
    inc hl
    ld (hl), NETCHESSZX_RULES_BOARD_BASE >> 8
    inc hl
    ld a, (_side_to_move)
    ld (hl), a
    inc hl
    ld (hl), d
    inc hl
    ld a, (_castle_rights)
    ld (hl), a
    inc hl
    ld a, (_ep_square)
    ld (hl), a
    ld hl, SPECTRUM_OVL_HINTS + (SPECTRUM_OVL_HINTS_SHOW * 256)
    push hl
    call _spectrum_overlay_exec_cached
IFDEF NETCHESSZX_NEXT
    call next_draw_legal_hints
ENDIF
    ret
