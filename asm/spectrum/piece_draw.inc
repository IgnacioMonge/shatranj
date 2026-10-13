draw_piece_sprite_16x16:
    push af
    ld a, b
    ld (piece_row), a
    ld a, c
    ld (piece_col), a
    pop af
    call piece_sprite_for_char
    ld a, h
    or l
    ret z
    ex de, hl                  ; DE = sprite data pointer (32 bytes, sequential)

    ; --- top char row: pixel scanlines 0..7 ---
    ld a, (piece_row)
    call compute_screen_base   ; HL = scan 0 of char row; clobbers A/HL, keeps DE
    ld a, (piece_col)
    add a, l
    ld l, a                    ; HL = top-left pixel address
    ld b, 8
    call dps_blit_8

    ; --- bottom char row: pixel scanlines 8..15 ---
    ld a, (piece_row)
    inc a
    call compute_screen_base
    ld a, (piece_col)
    add a, l
    ld l, a
    ld b, 8
    ; falls through into dps_blit_8

; Blit B pixel lines of a 16x16 sprite into one char row.
; DE = sprite pointer (advances 2 bytes/line), HL = left pixel address.
; inc h walks down within the char block; no per-line address recompute.
dps_blit_8:
    ld a, (de)
    ld (hl), a
    inc l
    inc de
    ld a, (de)
    ld (hl), a
    dec l
    inc de
    inc h
    djnz dps_blit_8
    ret

piece_sprite_for_char:
    ld c, 0
    cp 'a'
    jr c, psfc_upper_ready
    sub 32
    inc c
psfc_upper_ready:
    push af
    call square_parity
    xor c
    ld c, a
    pop af
    ld hl, psfc_piece_chars
    ld b, 6
psfc_match:
    cp (hl)
    jr z, psfc_found
    inc hl
    djnz psfc_match
    ld hl, 0
    ret

psfc_found:
    ; Runtime sets interleave two 32-byte masks per piece (KQRBNP).
    ld a, 6
    sub b
    add a, a
    add a, c
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, piece_sprites_16x16
    add hl, de
    ret

