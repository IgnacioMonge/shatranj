SECTION code_user

PUBLIC _spectrum_append_text
PUBLIC _spectrum_append_u16
PUBLIC _spectrum_streq

; uint8_t spectrum_streq(const char *a, const char *b) __z88dk_callee
; SDCC/IY stack: [return][a][b]; leave only return for RET.
_spectrum_streq:
    pop af
    pop de
    pop hl
    push af
streq_loop:
    ld a, (de)
    cp (hl)
    jr nz, streq_no
    inc de
    inc hl
    or a
    jr nz, streq_loop
    ld l, 1
    ret
streq_no:
    ld l, 0
    ret

; char *spectrum_append_text(char *dst, const char *src) __z88dk_callee
; SDCC/IY stack: [return][dst][src]; leave only return for RET.
_spectrum_append_text:
    pop af
    pop de
    pop bc
    push af
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

; char *spectrum_append_u16(char *dst, uint16_t value) __z88dk_callee
; SDCC/IY stack: [return][dst][value]; leave only return for RET.
_spectrum_append_u16:
    pop af
    pop de
    pop hl
    push af
    ld a, 0xc8
    ld (append_lead_skip), a
    ld bc, 10000
    call append_place
    ld bc, 1000
    call append_place
    ld bc, 100
    call append_place
    ld bc, 10
    call append_place
    ld a, l
    call append_emit_digit
    xor a
    ld (de), a
    ex de, hl
    ret

append_place:
    xor a
append_count_loop:
    sbc hl, bc
    jr c, append_count_done
    inc a
    jr append_count_loop
append_count_done:
    add hl, bc
    or a
append_lead_skip:
    ret z
    call append_emit_digit
    xor a
    ld (append_lead_skip), a
    ret

append_emit_digit:
    add a, '0'
    ld (de), a
    inc de
    ret
