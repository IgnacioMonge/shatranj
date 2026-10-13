; Old-vs-new pin for asm/overlay/edit/edit_buf.asm. Every case runs the
; frozen reference editor below and the linked production editor from the
; same state, then compares guard + buffer + editor variables + returned L.
; Covers every valid state (len <= max, pos <= max(len, 1) - 1) for each
; production edit_max (TIME 2, ROOM 4, PORT 5, NAME 10, HOST 15) plus 16,
; with 16 keys, and bind over lengths 0..17. On failure the four bytes before
; test_result hold max/len/pos/key (key 0 = bind case).

SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _edit_buf
PUBLIC _edit_max
PUBLIC _edit_len
PUBLIC _edit_pos

EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key

KEY_LEFT  EQU 0x83
KEY_RIGHT EQU 0x84
KEY_HOME  EQU 0x88
KEY_END   EQU 0x89

TEST_FILL_SIZE EQU 4 + 24
TEST_SNAP_SIZE EQU TEST_FILL_SIZE + 5 + 1

test_start:
    ld sp, 0xff00
    ld hl, test_max_table
test_max_loop:
    ld a, (hl)
    or a
    jr z, test_bind_cases
    ld (test_max), a
    inc hl
    push hl
    xor a
    ld (test_len), a
test_len_loop:
    xor a
    ld (test_pos), a
test_pos_loop:
    ld hl, test_keys
test_key_loop:
    ld a, (hl)
    or a
    jr z, test_next_pos
    ld (test_key), a
    push hl
    call test_key_case
    pop hl
    jr nz, test_fail_key
    inc hl
    jr test_key_loop
test_next_pos:
    ld a, (test_len)
    or a
    jr nz, test_pos_limit
    inc a
test_pos_limit:
    ld b, a
    ld a, (test_pos)
    inc a
    ld (test_pos), a
    cp b
    jr c, test_pos_loop
    ld a, (test_max)
    ld b, a
    ld a, (test_len)
    cp b
    inc a
    ld (test_len), a
    jr c, test_len_loop
    pop hl
    jr test_max_loop

test_bind_cases:
    xor a
    ld (test_key), a
    ld (test_len), a
    ld a, 16
    ld (test_max), a
    ld a, 0xee
    ld (test_pos), a
test_bind_loop:
    call test_fill_bind
    call ref_bind
    ld de, test_snap_a
    call test_snap_void
    call test_fill_bind
    call _spectrum_gui_edit_bind
    ld de, test_snap_b
    call test_snap_void
    call test_compare
    jr nz, test_fail_bind
    ld a, (test_len)
    inc a
    ld (test_len), a
    cp 18
    jr c, test_bind_loop
    xor a
    jr test_store_result

test_fail_key:
    ld a, 1
    jr test_store_result
test_fail_bind:
    ld a, 2
test_store_result:
    ld (test_result), a
test_done:
    jr test_done

test_key_case:
    call test_fill
    ld a, (test_key)
    ld l, a
    call ref_key
    ld de, test_snap_a
    call test_snap
    call test_fill
    ld a, (test_key)
    ld l, a
    call _spectrum_gui_edit_key
    ld de, test_snap_b
    call test_snap
test_compare:
    ld hl, test_snap_a
    ld de, test_snap_b
    ld b, TEST_SNAP_SIZE
test_compare_loop:
    ld a, (de)
    cp (hl)
    ret nz
    inc hl
    inc de
    djnz test_compare_loop
    ret

; Bind must write every variable it owns, so seed them with garbage.
test_fill_bind:
    call test_fill
    ld hl, 0
    ld (_edit_buf), hl
    ld hl, test_buf
    ret

; Guard and buffer = 0xEE; buffer = test_len letters + NUL; vars from test_*.
test_fill:
    ld hl, test_region
    ld (hl), 0xee
    ld de, test_region + 1
    ld bc, TEST_FILL_SIZE - 1
    ldir
    ld hl, test_buf
    ld (_edit_buf), hl
    ld a, (test_max)
    ld (_edit_max), a
    ld a, (test_pos)
    ld (_edit_pos), a
    ld a, (test_len)
    ld (_edit_len), a
    ld b, a
    ld c, 'a'
    or a
    jr z, test_fill_end
test_fill_loop:
    ld (hl), c
    inc hl
    inc c
    djnz test_fill_loop
test_fill_end:
    ld (hl), 0
    ret

; Bind returns nothing and its callers ignore HL.
test_snap_void:
    ld l, 0
; L = editor result, DE = snapshot.
test_snap:
    ld a, l
    ld (test_ret), a
    ld hl, test_region
    ld bc, TEST_SNAP_SIZE
    ldir
    ret

; Frozen reference: edit_buf.asm before the compaction, labels renamed.
ref_bind:
    ld (_edit_buf), hl
    ld b, 0
rb_len:
    ld a, (hl)
    or a
    jr z, rb_done
    inc hl
    inc b
    jr rb_len
rb_done:
    ld a, b
    ld (_edit_len), a
    or a
    jr z, rb_st
    dec a
rb_st:
    ld (_edit_pos), a
    ret

ref_key:
    ld a, l
    cp 8
    jr z, rk_del
    cp KEY_LEFT
    jr z, rk_left
    cp KEY_RIGHT
    jr z, rk_right
    cp KEY_HOME
    jr z, rk_home
    cp KEY_END
    jr z, rk_end
    cp 32
    jr c, rk_no
    cp 127
    jr c, rk_type
rk_no:
    ld l, 0
    ret

rk_del:
    ld a, (_edit_len)
    or a
    jr z, rk_no
    ld a, (_edit_pos)
    ld c, a
    ld hl, (_edit_buf)
    ld b, 0
    add hl, bc
rk_sl:
    inc hl
    ld a, (hl)
    dec hl
    ld (hl), a
    inc hl
    or a
    jr nz, rk_sl
    ld hl, _edit_len
    dec (hl)
    ld a, (hl)
    or a
    jr z, rk_mt
    dec a
    ld b, a
    ld a, (_edit_pos)
    cp b
    jr c, rk_ch2
    ld a, b
    ld (_edit_pos), a
    jr rk_ch2
rk_mt:
    xor a
    ld (_edit_pos), a
rk_ch2:
    ld l, 2
    ret

rk_left:
    ld a, (_edit_pos)
    or a
    jr z, rk_no
    dec a
rk_sp:
    ld (_edit_pos), a
    ld l, 1
    ret

rk_right:
    ld a, (_edit_len)
    or a
    jr z, rk_no
    dec a
    ld b, a
    ld a, (_edit_pos)
    cp b
    jr nc, rk_no
    inc a
    jr rk_sp

rk_home:
    xor a
    jr rk_sp

rk_end:
    ld a, (_edit_len)
    or a
    jr z, rk_no
    dec a
    jr rk_sp

rk_type:
    push af
    ld a, (_edit_len)
    ld b, a
    ld a, (_edit_max)
    cp b
    jr z, rk_rep
    ld a, (_edit_pos)
    inc a
    cp b
    jr z, rk_append
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
    jr rk_ch2

rk_append:
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
    jr rk_ch2

rk_rep:
    ld hl, (_edit_buf)
    ld a, (_edit_pos)
    ld e, a
    ld d, 0
    add hl, de
    pop af
    ld (hl), a
    jr rk_ch2

test_max_table:
    defb 2, 4, 5, 10, 15, 16, 0
test_keys:
    defb 8, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, 0x81, 0x85, 0x8a
    defb 31, 32, '0', 'A', 126, 127, 0xff, 13, 0

; Guard, buffer and the editor variables (edit_field.asm BSS order) are one
; contiguous snapshot, followed by the returned L.
test_region:
    defs 4
test_buf:
    defs 24
_edit_buf:
    defw 0
_edit_max:
    defb 0
_edit_len:
    defb 0
_edit_pos:
    defb 0
test_ret:
    defb 0
test_snap_a:
    defs TEST_SNAP_SIZE
test_snap_b:
    defs TEST_SNAP_SIZE
test_max:
    defb 0
test_len:
    defb 0
test_pos:
    defb 0
test_key:
    defb 0
test_result:
    defb 0xff
