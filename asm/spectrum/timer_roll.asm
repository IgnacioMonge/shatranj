INCLUDE "asm/spectrum/timer_roll_layout.inc"

; UI delta renderer. Only changed cells paint; the blit receives packed
; pixels, a VRAM address and a nibble mask. One ephemeral strip is reused.
; Cache commits at q=4. q=0 only arms the phase: those pixels are already
; on screen. Destinations stay on scanline 2 of a character cell, so four
; rows are INC H.
_netchesszx_asm_timer_game:
    push ix
    ld ix,timer_roll_state_addr
    ld de,0x5e9a
    ld bc,23*256+41
    ld a,0x42
    jr tr_start
_netchesszx_asm_timer_clock:
    push ix
    ld ix,timer_roll_state_addr+1
    ld de,0x5e92
    ld bc,5*256+56
    ld a,0x52
tr_start:
    ld (timer_roll_buffer+4),a
tr_compare:
    ld a,(de)
    bit 5,(ix+0)
    jr nz,tr_changed
    cp (hl)
    jr z,tr_unchanged
tr_changed:
    ex af,af'
    push bc
    push de
    push hl
    ld a,(ix+0)
    or 0x80
    ld (ix+0),a
    and 7
    jr z,tr_skip
    cp 4
    jr nz,tr_keep_old
    ld a,(hl)
    ld (de),a
tr_keep_old:
    ld a,c
    srl a
    ld l,a
    ld a,(timer_roll_buffer+4)
    and 16
    rlca
    rlca
    rlca
    or 0x40
    or l
    ld (timer_roll_buffer+3),a
    xor a
    srl c
    sbc a,a
    xor 0xf0
    ld (timer_roll_buffer+2),a
    ld de,timer_roll_buffer+10
    bit 6,(ix+0)
    jr nz,tr_pack_reverse
    ex af,af'
    call tr_pack_glyph
    pop hl
    push hl
    ld a,(hl)
    call tr_pack_glyph
    jr tr_packed
tr_pack_reverse:
    pop hl
    push hl
    ld a,(hl)
    call tr_pack_glyph
    ex af,af'
    call tr_pack_glyph
tr_packed:
    ld a,(ix+0)
    and 7
    bit 6,(ix+0)
    jr z,tr_paint_phase
    cpl
    add a,5
tr_paint_phase:
    call tr_paint
    pop hl
    pop de
    pop bc
    jr tr_unchanged
tr_skip:
    pop hl
    pop de
    pop bc
tr_unchanged:
    inc hl
    inc de
    inc c
    djnz tr_compare
    pop ix
    ret

; Table-driven top-menu selection and rolling labels.
INCLUDE "asm/spectrum/taboption.asm"

; A=q (1..4). Source is strip+q; mask at +2, dest at +3.
tr_paint:
    ld c,a
    ld b,0
    ld hl,timer_roll_buffer+10
    add hl,bc
    ex de,hl
    ld hl,(timer_roll_buffer+3)
    ld a,(timer_roll_buffer+2)
    ld c,a
    ld b,4
tr_paint_row:
    ld a,(de)
    bit 5,(ix+0)
    jr z,tr_paint_merge
    xor c
tr_paint_merge:
    xor (hl)
    and c
    xor (hl)
    ld (hl),a
    inc de
    inc h
    djnz tr_paint_row
    ret

; Existing packed font -> four repeated-nibble rows. Caller owns rendering.
tr_pack_glyph:
    cp 33
    jr c,tr_space
    cp 128
    jr nc,tr_space
    cp 96
    jr c,tr_upper
    sub 32
tr_upper:
    sub 32
    add a,a
    ld l,a
    ld h,0
    ld bc,timer_roll_font
    add hl,bc
    ld b,2
tr_pack_pair:
    ld a,(hl)
    ld c,a
    rrca
    rrca
    rrca
    rrca
    xor c
    ld c,a
    and 15
    xor (hl)
    ld (de),a
    inc de
    xor c
    ld (de),a
    inc de
    inc hl
    djnz tr_pack_pair
    ret
tr_space:
    xor a
    ld b,4
tr_blank_row:
    ld (de),a
    inc de
    djnz tr_blank_row
    ret
