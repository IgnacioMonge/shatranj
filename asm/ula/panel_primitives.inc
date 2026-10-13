draw_connection_indicator:
    ld hl, ula_screen_status
    ld a, 31
    add a, l
    ld l, a
    ld de, conn_pattern
    ld b, 8
dci_px:
    ld a, (de)
    ld (hl), a
    inc de
    inc h
    djnz dci_px
    ld hl, ula_attr_status
    ld a, 31
    add a, l
    ld l, a
    ld a, (conn_attr)
    or a
    jr nz, dci_attr_ready
    ld a, ATTR_CONN_OFF
dci_attr_ready:
    ld (hl), a
    ret

draw_right_hline:
    call compute_screen_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld a, (hl)
    or 0x0f
    ld (hl), a
    inc l
    ld b, 13
drh_loop:
    ld (hl), 0xff
    inc l
    djnz drh_loop
    ret

draw_right_hline_full_left:
    call compute_screen_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld b, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
drhfl_loop:
    ld (hl), 0xff
    inc l
    djnz drhfl_loop
    ret

draw_banner_separator:
    ld a, 2
    call compute_screen_base
    ld b, 32
dbs_loop:
    ld (hl), 0xff
    inc hl
    djnz dbs_loop
    ret

INCLUDE "asm/platform/input_scan.inc"

; Presentation advances first; the platform kernel then samples this frame.
_spectrum_input_frame_tick:
    call _spectrum_gui_edit_tick
INCLUDE "asm/platform/input_tick.inc"

clear_right_text_pixels:
    ld (tmp_row), a
    add a, a
    add a, a
    add a, a
    ld b, 8
    jp clear_right_pixel_band_abs

clear_right_pixel_band_abs:
    ld (tmp_scan), a
    ld a, b
    ld (piece_scan), a
    ld a, (tmp_scan)
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
crpba_loop:
    ld a, (piece_scan)
    or a
    ret z
    push hl
    ld b, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
    xor a
crpba_px_loop:
    ld (hl), a
    inc hl
    djnz crpba_px_loop
    pop hl
    call pixel_down_hl
    ld a, (piece_scan)
    dec a
    ld (piece_scan), a
    jr crpba_loop

pixel_down_hl:
    inc h
    ld a, h
    and 7
    ret nz
    ld a, l
    add a, 32
    ld l, a
    ret c
    ld a, h
    sub 8
    ld h, a
    ret

clear_right_text_row:
    call clear_right_text_pixels
    ld a, (tmp_row)
    call compute_attr_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ld b, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
crtr_attr:
    ld (hl), ATTR_TEXT
    inc hl
    djnz crtr_attr
    ret

align_notice_text:
    ld bc, NETCHESSZX_NOTICE_TEXT_SIZE
    xor a
    cpir
    ld a, c
    push af
    dec hl
    push hl
    add hl, bc
    ex de, hl
    pop hl
    ld a, NETCHESSZX_NOTICE_TEXT_SIZE
    sub c
    ld c, a
    lddr
    pop af
    or a
    jr z, ant_return
    ld b, a
    ld h, d
    ld l, e
    ld a, ' '
ant_fill_loop:
    ld (hl), a
    dec hl
    djnz ant_fill_loop
ant_return:
    inc hl
    ret

font_scanline:
    cp 127
    jr nz, fs_not_swatch
    ld a, 0xf0
    ret
fs_not_swatch:
    cp 33
    jr c, fs_blank
    cp 127
    jr nc, fs_blank
    sub 32
    ld l, a
    ld h, 0
    add hl, hl
    ld d, 0
    ld e, a
    add hl, de
    ld de, font_packed
    add hl, de
    ld a, (tmp_scan)
    srl a
    ld e, a
    ld d, 0
    add hl, de
    ld a, (hl)
    ld d, a
    ld a, (tmp_scan)
    and 1
    ld a, d
    jr nz, fs_low
    rrca
    rrca
    rrca
    rrca
fs_low:
    and 0x0f
    ld e, a
    ld d, 0
    ld hl, font_lut
    add hl, de
    ld a, (hl)
    ret
fs_blank:
    xor a
    ret

clear_text_row:
    push af
    push bc
    call clear_row_pixels
    pop bc
    pop af
    jp fill_attr_line

clear_row_pixels:
    call compute_screen_base
    ld d, h
    ld e, l
    ld c, 31
    jp clear_pixels_8

clear_pixels_8:
    ld b, 8
cp8_loop:
    push bc
    push de
    ld h, d
    ld l, e
    ld (hl), 0
    inc de
    ld b, 0
    ldir
    pop de
    pop bc
    inc d
    djnz cp8_loop
    ret

fill_attr_line:
    call compute_attr_base
    ld (hl), c
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ldir
    ret

