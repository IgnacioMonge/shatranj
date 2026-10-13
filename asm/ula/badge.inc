draw_badge:
    ld hl, NETCHESSZX_BADGE_TOP_PIXELS
    ld c, 4
    call draw_badge_cells
    ld hl, NETCHESSZX_BADGE_BOTTOM_PIXELS
    ld c, 5
    call draw_badge_cells

    ld hl, NETCHESSZX_BADGE_TOP_ATTRS
    ld (hl), NETCHESSZX_BADGE_ATTR_0
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_1
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_2
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_3

    ld hl, NETCHESSZX_BADGE_BOTTOM_ATTRS
    ld (hl), NETCHESSZX_BADGE_ATTR_0
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_1
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_2
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_3
    inc hl
    ld (hl), NETCHESSZX_BADGE_ATTR_4
    ret

draw_badge_cells:
    ld de, badge_pattern
    ld b, 8
dbc_scan:
    push bc
    push hl
    ld a, (de)
    ld b, c
dbc_byte:
    ld (hl), a
    inc l
    djnz dbc_byte
    pop hl
    inc h
    inc de
    pop bc
    djnz dbc_scan
    ret

