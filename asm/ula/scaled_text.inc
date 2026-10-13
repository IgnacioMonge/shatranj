; HL = NUL-terminated banner text. Each glyph's six font rows are drawn
; double height from pixel row 2, one byte column per character from 0.
draw_scaled_text:
    ld c, 0

dst_loop:
    ld a, (hl)
    or a
    ret z
    ld (piece_char), a
    push hl
    ld b, 0

dst_scan_loop:
    ld a, b
    ld (tmp_scan), a
    ld a, (piece_char)
    call font_scanline
    rrca
    rrca
    rrca
    rrca
    and 0x0f
    ld e, a
    ld d, 0
    ld hl, expand_2x
    add hl, de
    ld d, (hl)

    ld a, b
    add a, a
    add a, 2
    call compute_pixel_base
    ld a, c
    add a, l
    ld l, a
    ld (hl), d
    inc h
    ld (hl), d

    inc b
    ld a, b
    cp 6
    jr c, dst_scan_loop
    pop hl
    inc hl
    inc c
    jr dst_loop
