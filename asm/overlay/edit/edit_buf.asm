; Overlay-local buffer editor. Semantics frozen:
; blinking cell is the editable cell (pos in 0..len-1).
; Type appends after last, inserts before a middle cell, replaces if full.
; Backspace deletes the blinking cell.

SECTION code_user

PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key

EXTERN _edit_buf
EXTERN _edit_max
EXTERN _edit_len
EXTERN _edit_pos

KEY_LEFT  EQU 0x83
KEY_RIGHT EQU 0x84
KEY_HOME  EQU 0x88
KEY_END   EQU 0x89

_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    xor a
    ld b, a
    ld c, a
    ; BC=0 scans up to 64K; C ends as ~len (mod 256).
    cpir
    ld a, c
    cpl
    ld (_edit_len), a
    ; pos = len ? len - 1 : 0
    sub 1
    adc a, 0
    ld (_edit_pos), a
    ret

; _edit_pos must follow _edit_len (edit_field.asm BSS order).
_spectrum_gui_edit_key:
    ld a, l
    ld hl, _edit_len
    ld c, (hl)
    inc hl
    cp 8
    jr z, ek_del
    cp KEY_LEFT
    jr z, ek_left
    cp KEY_RIGHT
    jr z, ek_right
    cp KEY_HOME
    jr z, ek_home
    cp KEY_END
    jr z, ek_end
    cp 32
    jr c, ek_no
    cp 127
    jr c, ek_type
ek_no:
    ld l, 0
    ret

ek_left:
    ld a, (hl)
    or a
    jr z, ek_no
    dec (hl)
ek_ret1:
    ld l, 1
    ret

ek_right:
    ld a, (hl)
    inc a
    cp c
    jr nc, ek_no
    ld (hl), a
    jr ek_ret1

ek_home:
    ld (hl), 0
    jr ek_ret1

ek_end:
    ld a, c
    or a
    jr z, ek_no
    dec a
    ld (hl), a
    jr ek_ret1

ek_del:
    ld a, c
    or a
    jr z, ek_no
    dec c
    ld b, (hl)
    ld a, b
    cp c
    jr nz, ek_del_keep
    or a
    jr z, ek_del_keep
    dec (hl)
ek_del_keep:
    dec hl
    ld (hl), c
    ld hl, (_edit_buf)
    ld e, b
    ld d, 0
    add hl, de
    ld d, h
    ld e, l
ek_sl:
    inc hl
    ld a, (hl)
    ld (de), a
    inc de
    or a
    jr nz, ek_sl
ek_ret2:
    ld l, 2
    ret

ek_type:
    push af
    ld a, (_edit_max)
    cp c
    ld a, (hl)
    jr z, ek_rep
    ; Append is an insert at len; the cursor follows it. A middle insert
    ; keeps pos. Either way shift buf[i..len] up by one and store at i.
    inc a
    cp c
    jr z, ek_at
    dec a
ek_at:
    ld (hl), a
    dec hl
    inc (hl)
    neg
    add a, c
    inc a
    ld hl, (_edit_buf)
    ld b, 0
    add hl, bc
    ld c, a
    ld d, h
    ld e, l
    inc de
    lddr
ek_store:
    pop af
    ld (de), a
    jr ek_ret2
ek_rep:
    ld hl, (_edit_buf)
    ld e, a
    ld d, 0
    add hl, de
    ex de, hl
    jr ek_store
