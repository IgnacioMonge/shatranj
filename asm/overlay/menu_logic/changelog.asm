; Changelog page: C on the Classic/SpectraNext About screen. Text only, over
; the right side of the About band; the left player is never touched.
;
; The page has its own glyph writer: it decodes each 64-column glyph once and
; writes its six scanlines down the column, about three times faster than the
; resident line renderer, which looks every scanline up per glyph. The title
; is drawn while its cells are still black and stretched to double height in
; place, as SpecTalkZX draws its headers, so it never shows at normal size.
;
; Edit the text below for each release: player-facing changes only, curated
; from CHANGELOG.md. A list line holds at most 38 characters after ". ".

EXTERN font_packed, font_lut
EXTERN compute_screen_base, _spectrum_render_ikkle_abs_at

chlog_first_row EQU 3
chlog_last_row EQU 21
chlog_wipe_col EQU 11               ; first byte column right of the player
chlog_wipe_cells EQU 21
chlog_text_col EQU 24               ; 64-column units: byte column 12
chlog_text_cells EQU 20
chlog_title_row EQU 4               ; must stay in the top screen third
chlog_title_cells EQU 10
chlog_rule_row EQU 6
chlog_list_row EQU 8
chlog_list_rows EQU 10
chlog_attr_title EQU 0x45           ; app.c recognises the page by this cell
chlog_attr_title_low EQU 0x05
chlog_attr_rule EQU 0x46
chlog_attr_accent EQU 0x43
chlog_attr_text EQU 0x47
chlog_attr_bullet EQU 0x45
chlog_attr_hint EQU 0x06
chlog_text_attr_base EQU 0x5800 + chlog_text_col / 2

_chlog_render_ovl_entry:
    ; Black out and wipe the band right of the player, then the rest of the
    ; hint row and the "GH:" credit cell that reaches into the art columns.
    ld a, chlog_first_row
chlog_wipe_row:
    push af
    ld bc, chlog_wipe_cells * 256 + chlog_wipe_col
    call chlog_wipe
    pop af
    inc a
    cp chlog_last_row + 1
    jr c, chlog_wipe_row
    ld a, chlog_last_row
    ld bc, chlog_wipe_col * 256
    call chlog_wipe
    ld a, 8
    ld bc, 256 + chlog_wipe_col - 1
    call chlog_wipe

    ; Title: drawn unseen, stretched bottom line first so each source line
    ; is still intact when read, then given its colours.
    ld hl, chlog_title
    ld bc, chlog_title_row * 256        ; black cells: drawn unseen
    call chlog_text
    ld b, 16
chlog_stretch:
    ld a, b
    dec a
    call chlog_title_line
    ex de, hl
    ld a, b
    dec a
    srl a
    call chlog_title_line
    push bc
    ld bc, chlog_title_cells
    ldir
    pop bc
    djnz chlog_stretch
    ld hl, chlog_text_attr_base + chlog_title_row * 32
    ld bc, chlog_title_cells * 256 + chlog_attr_title
    call chlog_fill
    ld hl, chlog_text_attr_base + (chlog_title_row + 1) * 32
    ld bc, chlog_title_cells * 256 + chlog_attr_title_low
    call chlog_fill

    ; Rule on scanline 2 of its row (top screen third).
    ld hl, 0x4200 + chlog_rule_row * 32 + chlog_text_col / 2
    ld bc, chlog_text_cells * 256 + 0xff
    call chlog_fill
    ld hl, chlog_text_attr_base + chlog_rule_row * 32
    ld bc, chlog_text_cells * 256 + chlog_attr_rule
    call chlog_fill

    ld hl, chlog_lines
chlog_line:
    ld a, (hl)
    or a
    jr z, chlog_lines_done
    ld b, a                     ; row
    inc hl
    ld c, (hl)                  ; attribute
    inc hl
    call chlog_text
    inc hl
    jr chlog_line
chlog_lines_done:

    ; One cyan cell per list bullet.
    ld hl, chlog_text_attr_base + chlog_list_row * 32
    ld de, 32
    ld b, chlog_list_rows
chlog_bullet:
    ld (hl), chlog_attr_bullet
    add hl, de
    djnz chlog_bullet

    ld hl, chlog_hint
    call _spectrum_render_ikkle_abs_at

    ld hl, 1
    ret

; HL=text, B=row, C=attribute. Colours the row's text cells, ORs the glyphs
; onto the wiped pixels and returns HL at the terminator.
chlog_text:
    push hl
    ld a, b
    call compute_screen_base
    ld a, l
    add a, chlog_text_col / 2
    ld l, a
    push hl
    call chlog_attr_addr
    ld b, chlog_text_cells
    call chlog_fill
    pop de
    inc d                       ; DE = top glyph scanline (1) of the column
    ld a, 0xf0                  ; chlog_text_col is even: start on the left half
    ld (chlog_mask), a
    pop hl
chlog_char:
    ld a, (hl)
    or a
    ret z
    inc hl
    push hl
    sub 32
    ld l, a
    ld h, 0
    ld b, h
    ld c, l
    add hl, hl
    add hl, bc
    ld bc, font_packed
    add hl, bc                  ; three bytes, six scanline indexes
    push de
    ld b, 3
chlog_glyph:
    ld c, (hl)
    inc hl
    ld a, c
    rrca
    rrca
    rrca
    rrca
    call chlog_put
    ld a, c
    call chlog_put
    djnz chlog_glyph
    pop de
    ld a, (chlog_mask)
    cpl
    ld (chlog_mask), a
    rlca                        ; back on the left half: next byte column
    jr nc, chlog_same_byte
    inc e
chlog_same_byte:
    pop hl
    jr chlog_char

; A=scanline index. OR that scanline into (DE) under the half mask; DE down.
chlog_put:
    push hl
    and 15
    add a, font_lut & 0xff
    ld l, a
    adc a, font_lut / 256
    sub l
    ld h, a
    ld a, (hl)
chlog_mask EQU $ + 1
    and 0xf0
    ex de, hl
    or (hl)
    ld (hl), a
    inc h
    ex de, hl
    pop hl
    ret

; A=row, C=byte column, B=cells. Black attributes first, so the old picture
; vanishes at once, then zero the pixels.
chlog_wipe:
    call compute_screen_base
    ld a, l
    add a, c
    ld l, a
    push hl
    push bc
    call chlog_attr_addr
    ld c, 0
    call chlog_fill
    pop bc
    pop hl
    ld e, 8
chlog_wipe_scan:
    push hl
    push bc
    ld c, 0
    call chlog_fill
    pop bc
    pop hl
    inc h
    dec e
    jr nz, chlog_wipe_scan
    ret

; HL=bitmap address on scanline 0. Returns its attribute address in HL.
chlog_attr_addr:
    ld a, h
    rrca
    rrca
    rrca
    and 3
    or 0x58
    ld h, a
    ret

; HL=first byte of one screen or attribute row, B=count, C=value.
chlog_fill:
    ld (hl), c
    inc l
    djnz chlog_fill
    ret

; A=line 0..15 of the two title rows. Returns its bitmap address in HL.
chlog_title_line:
    ld l, a
    and 7
    add a, 0x40
    ld h, a
    ld a, l
    and 8
    add a, a
    add a, a
    add a, chlog_title_row * 32 + chlog_text_col / 2
    ld l, a
    ret

chlog_title:
    DEFM "What's new in 1.3", 0

; row, attribute, text; a zero row ends the list.
chlog_lines:
    DEFB 7, chlog_attr_accent
    DEFM "Changes:", 0
    DEFB 8, chlog_attr_text
    DEFM ". Play against Stockfish via Qt", 0
    DEFB 9, chlog_attr_text
    DEFM ". End-of-game statistics (S)", 0
    DEFB 10, chlog_attr_text
    DEFM ". Player names in the MOVES headers", 0
    DEFB 11, chlog_attr_text
    DEFM ". Resume a game after a crash or reset", 0
    DEFB 12, chlog_attr_text
    DEFM ". Saves keep the full move list", 0
    DEFB 13, chlog_attr_text
    DEFM ". Menu groups open on their first action", 0
    DEFB 14, chlog_attr_text
    DEFM ". No auto rematch: the host asks", 0
    DEFB 15, chlog_attr_text
    DEFM ". Faster moves, setup and start", 0
    DEFB 16, chlog_attr_text
    DEFM ". Clocks keep time during disk use", 0
    DEFB 17, chlog_attr_text
    DEFM ". Spectranext: launch from the web", 0
    DEFB 18, chlog_attr_accent
    DEFM "And many, many more fixes!", 0
    DEFB 0

; Key hint in the small ikkle font, like About's: pixel y, 64-column
; position (centred on the full screen width), attribute, text.
chlog_hint:
    DEFB chlog_last_row * 8 + 2, 24, chlog_attr_hint
    DEFM "ANY KEY TO CLOSE", 0
