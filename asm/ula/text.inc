draw_char64_at_tmp:
    call draw_char64_pixels_at_tmp
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (current_attr)
    ld (hl), a
    ret

draw_text64_line_attr_fast:
    call draw_text64_line_prepare
    call draw_text64_line_fill_attr
    jr draw_text64_line_pixels_prepared

draw_text64_line_pixels_fast:
    call draw_text64_line_prepare
    jr draw_text64_line_pixels_prepared

draw_text64_line_prepare:
    ld (text_ptr), hl
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, d
    ld (tmp_char), a
    ret

draw_text64_line_fill_attr:
    ld a, (tmp_row)
    call attr_cell_for_tmpcol
    ld a, (tmp_char)
    ld b, a
    ld a, (current_attr)
dt64laf_loop:
    ld (hl), a
    inc hl
    djnz dt64laf_loop
    ret

draw_text64_line_pixels_prepared:
    xor a
    ld (piece_scan), a

dst64_scan_loop:
    ld a, (piece_scan)
    call screen_at_tmp
    ld (sprite_ptr), hl

    ld a, (piece_scan)
    or a
    jr z, dst64_blank_scan
    cp 7
    jr z, dst64_blank_scan

    dec a
    ld (tmp_scan), a
    ld hl, (text_ptr)
    ld a, (tmp_char)
    ld b, a
    ld a, (tmp_col)
    and 1
    xor 1
    ld c, a

dst64_pair_loop:
    ld a, c
    or a
    jr nz, dst64_left_from_text
    inc c
    xor a
    jr dst64_left_ready

dst64_left_from_text:
    call text64_next_glyph
    and 0xf0
dst64_left_ready:
    ld (tmp_attr), a
    call text64_next_glyph
    and 0x0f
    ld e, a
    ld a, (tmp_attr)
    or e
    push hl
    ld hl, (sprite_ptr)
    ld (hl), a
    inc hl
    ld (sprite_ptr), hl
    pop hl
    djnz dst64_pair_loop
    jr dst64_next_scan

dst64_blank_scan:
    ld hl, (sprite_ptr)
    ld a, (tmp_char)
    ld b, a
    xor a
dst64_blank_loop:
    ld (hl), a
    inc hl
    djnz dst64_blank_loop

dst64_next_scan:
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    cp 8
    jr c, dst64_scan_loop
    ret

text64_next_glyph:
    ld a, (hl)
    or a
    ret z
    inc hl
    push hl
    call font_scanline
    pop hl
    ret

draw_char64_pixels_at_tmp:
    ld (tmp_char), a
    xor a
    ld (tmp_scan), a

dc64_scan_loop:
    ld a, (tmp_scan)
    call screen_at_tmp

    ld a, (tmp_scan)
    or a
    jr z, dc64_blank
    cp 7
    jr z, dc64_blank
    dec a
    ld (tmp_scan), a
    push hl
    ld a, (tmp_char)
    call font_scanline
    ld e, a
    pop hl
    ld a, (tmp_scan)
    inc a
    ld (tmp_scan), a
    jr dc64_got_pattern

dc64_blank:
    xor a
    ld e, a

dc64_got_pattern:
    ld a, (tmp_col)
    and 1
    jr nz, dc64_odd
    ld a, (hl)
    and 0x0f
    ld d, a
    ld a, e
    and 0xf0
    or d
    ld (hl), a
    jr dc64_next_scan

dc64_odd:
    ld a, (hl)
    and 0xf0
    ld d, a
    ld a, e
    and 0x0f
    or d
    ld (hl), a

dc64_next_scan:
    inc h
    ld a, (tmp_scan)
    inc a
    ld (tmp_scan), a
    cp 8
    jr c, dc64_scan_loop
    ret

draw_input_cursor_at_tmp:
    ld a, 7
    call screen_at_tmp
    ld a, (tmp_col)
    and 1
    ld b, 0xf0
    jr z, dict_apply
    ld b, 0x0f
dict_apply:
    ld a, (hl)
    or b
    ld (hl), a
    ret

; A = scanline (0-7) in cell row tmp_row; HL = its byte at tmp_col / 2.
; Clobbers A, E.
screen_at_tmp:
    ld e, a
    ld a, (tmp_row)
    add a, a
    add a, a
    add a, a
    add a, e

; A = absolute pixel Y; HL = its byte at tmp_col / 2.
pixel_at_tmpcol:
    call compute_pixel_base

; L += tmp_col / 2 (callers' L never carries into H).
tmpcol_add_l:
    ld a, (tmp_col)
    srl a
    add a, l
    ld l, a
    ret

draw_ikkle_text_abs_y:
    ld a, c
    ld (tmp_col), a
    ld a, b
    jr dita_start

; Cell-row entry: absolute Y = tmp_row * 8 + tmp_scan, column tmp_col.
draw_ikkle_text_at:
    ld a, (tmp_scan)
    ld b, a
    ld a, (tmp_row)
    add a, a
    add a, a
    add a, a
    add a, b

dita_start:
    ld (tmp_scan), a
    ld (text_ptr), hl

dita_loop:
    ld a, (tmp_col)
    cp 64
    jr nc, dita_done

    ld hl, (text_ptr)
    ld a, (hl)
    or a
    jr z, dita_done
    ld (tmp_char), a
    push hl

    ld a, (render_skip_clear)
    or a
    call z, ikkle_clear_abs_cell
    call ikkle_attr_abs_cell

    ld a, (tmp_char)
    cp 33
    jr c, dita_space
    cp 128
    jr nc, dita_space
    cp 96
    jr c, dita_no_fold
    sub 32
dita_no_fold:
    sub 32
    add a, a
    ld e, a
    ld d, 0
    ld hl, timer_ikkle_packed
    add hl, de
    ld d, (hl)
    inc hl
    ld e, (hl)
    ld a, d
    ld (piece_row), a
    ld a, e
    ld (piece_col), a
    xor a
    ld (piece_scan), a
    call ikkle_blit_abs_saved

dita_space:
    pop hl
    inc hl
    ld (text_ptr), hl
    ld a, (tmp_col)
    inc a
    ld (tmp_col), a
    jr dita_loop

dita_done:
    xor a
    ld (render_skip_clear), a
    ret

ikkle_clear_abs_cell:
    ld a, (tmp_scan)
    call pixel_at_tmpcol
    ld b, 4
    ld a, (tmp_col)
    bit 0, a
    jr nz, icac_odd
icac_even:
    ld a, (hl)
    and 0x0f
    ld (hl), a
    call pixel_down_hl
    djnz icac_even
    ret
icac_odd:
    ld a, (hl)
    and 0xf0
    ld (hl), a
    call pixel_down_hl
    djnz icac_odd
    ret

ikkle_attr_abs_cell:
    ld a, (tmp_scan)
    srl a
    srl a
    srl a
    call ikkle_set_abs_attr_row
    ld a, (tmp_scan)
    add a, 3
    srl a
    srl a
    srl a
    ld d, a
    ld a, (tmp_scan)
    srl a
    srl a
    srl a
    cp d
    ret z
    ld a, d

ikkle_set_abs_attr_row:
    call compute_attr_base
    call tmpcol_add_l
    ld a, (current_attr)
    ld (hl), a
    ret

ikkle_blit_abs_saved:
    ld a, (piece_row)
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    call ikkle_draw_abs_nibble
    ld a, (piece_row)
    and 0x0f
    call ikkle_draw_abs_nibble
    ld a, (piece_col)
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    call ikkle_draw_abs_nibble
    ld a, (piece_col)
    and 0x0f

ikkle_draw_abs_nibble:
    ld (tmp_attr), a
    ld a, (piece_scan)
    or a
    jr nz, idan_down
    ld a, (tmp_scan)
    call pixel_at_tmpcol
    jr idan_have_hl
idan_down:
    call pixel_down_hl
idan_have_hl:
    ld a, (tmp_col)
    bit 0, a
    jr nz, idan_odd
    ld a, (tmp_attr)
    rlca
    rlca
    rlca
    rlca
    ld b, a
    ld a, (hl)
    and 0x0f
    or b
    ld (hl), a
    jr idan_next
idan_odd:
    ld a, (tmp_attr)
    ld b, a
    ld a, (hl)
    and 0xf0
    or b
    ld (hl), a
idan_next:
    ld a, (piece_scan)
    inc a
    ld (piece_scan), a
    ret
