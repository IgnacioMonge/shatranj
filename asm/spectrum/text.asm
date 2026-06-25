SECTION code_user

PUBLIC _spectrum_append_text
PUBLIC _spectrum_append_u16

; char *spectrum_append_text(char *dst, const char *src)
; SDCC/IY stack: SP+2 = dst, SP+4 = src.
_spectrum_append_text:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
append_text_loop:
    ld a, (bc)
    inc bc
    ld (de), a
    inc de
    or a
    jr nz, append_text_loop
    dec de
    ex de, hl
    ret

; char *spectrum_append_u16(char *dst, uint16_t value)
; SDCC/IY stack: SP+2 = dst, SP+4 = value.
_spectrum_append_u16:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a

    ld bc, 10000
    call append_count_digit
    or a
    jr z, append_no_10000
    call append_emit_digit
    jr append_emit_1000

append_no_10000:
    ld bc, 1000
    call append_count_digit
    or a
    jr z, append_no_1000
    call append_emit_digit
    jr append_emit_100

append_no_1000:
    ld bc, 100
    call append_count_digit
    or a
    jr z, append_no_100
    call append_emit_digit
    jr append_emit_10

append_no_100:
    ld bc, 10
    call append_count_digit
    or a
    jr z, append_emit_1
    call append_emit_digit
    jr append_emit_1

append_emit_1000:
    ld bc, 1000
    call append_count_digit
    call append_emit_digit

append_emit_100:
    ld bc, 100
    call append_count_digit
    call append_emit_digit

append_emit_10:
    ld bc, 10
    call append_count_digit
    call append_emit_digit

append_emit_1:
    ld a, l
    call append_emit_digit
    xor a
    ld (de), a
    ex de, hl
    ret

append_count_digit:
    xor a
append_count_loop:
    or a
    sbc hl, bc
    jr c, append_count_done
    inc a
    jr append_count_loop
append_count_done:
    add hl, bc
    ret

append_emit_digit:
    add a, '0'
    ld (de), a
    inc de
    ret
