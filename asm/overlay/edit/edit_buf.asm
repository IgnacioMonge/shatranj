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
    ld b, 0
eb_len:
    ld a, (hl)
    or a
    jr z, eb_done
    inc hl
    inc b
    jr eb_len
eb_done:
    ld a, b
    ld (_edit_len), a
    or a
    jr z, eb_st
    dec a
eb_st:
    ld (_edit_pos), a
    ret

_spectrum_gui_edit_key:
    ld a, l
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

ek_del:
    ld a, (_edit_len)
    or a
    jr z, ek_no
    ld a, (_edit_pos)
    ld c, a
    ld hl, (_edit_buf)
    ld b, 0
    add hl, bc
ek_sl:
    inc hl
    ld a, (hl)
    dec hl
    ld (hl), a
    inc hl
    or a
    jr nz, ek_sl
    ld hl, _edit_len
    dec (hl)
    ld a, (hl)
    or a
    jr z, ek_mt
    dec a
    ld b, a
    ld a, (_edit_pos)
    cp b
    jr c, ek_ch2
    ld a, b
    ld (_edit_pos), a
    jr ek_ch2
ek_mt:
    xor a
    ld (_edit_pos), a
ek_ch2:
    ld l, 2
    ret

ek_left:
    ld a, (_edit_pos)
    or a
    jr z, ek_no
    dec a
ek_sp:
    ld (_edit_pos), a
    ld l, 1
    ret

ek_right:
    ld a, (_edit_len)
    or a
    jr z, ek_no
    dec a
    ld b, a
    ld a, (_edit_pos)
    cp b
    jr nc, ek_no
    inc a
    jr ek_sp

ek_home:
    xor a
    jr ek_sp

ek_end:
    ld a, (_edit_len)
    or a
    jr z, ek_no
    dec a
    jr ek_sp

ek_type:
    push af
    ld a, (_edit_len)
    ld b, a
    ld a, (_edit_max)
    cp b
    jr z, ek_rep
    ld a, (_edit_pos)
    inc a
    cp b
    jr z, ek_append
    ld c, b
    ld b, 0
    ld hl, (_edit_buf)
    add hl, bc
    ld a, (_edit_pos)
    ld e, a
    ld a, c
    sub e
    inc a
    ld c, a
    ld d, h
    ld e, l
    inc de
    lddr
    pop af
    ld (de), a
    ld hl, _edit_len
    inc (hl)
    jr ek_ch2

ek_append:
    ld c, b
    ld b, 0
    ld hl, (_edit_buf)
    add hl, bc
    pop af
    ld (hl), a
    inc hl
    xor a
    ld (hl), a
    ld hl, _edit_len
    inc (hl)
    ld a, (hl)
    dec a
    ld (_edit_pos), a
    jr ek_ch2

ek_rep:
    ld hl, (_edit_buf)
    ld a, (_edit_pos)
    ld e, a
    ld d, 0
    add hl, de
    pop af
    ld (hl), a
    jr ek_ch2
