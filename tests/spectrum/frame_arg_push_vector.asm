SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

EXTERN _netchesszx_asm_push_frame_arg

test_start:
    ld sp, 0xff00
    ld (sp_before), sp
    ld ix, frame
    ld iy, 0x5678
    ld bc, 0x9abc
    scf
    call _netchesszx_asm_push_frame_arg
    call test_callee
    ld a, l
    cp 1
    ld a, 1
    jr nz, test_store_result
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    ld a, 2
    jr nz, test_store_result
    ld hl, 0
    add hl, sp
    ld de, (sp_before)
    or a
    sbc hl, de
    ld a, 3
    jr nz, test_store_result
    xor a
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_callee:
    jp nc, test_callee_bad
    ld a, b
    cp 0x9a
    jr nz, test_callee_bad
    ld a, c
    cp 0xbc
    jr nz, test_callee_bad
    push ix
    pop de
    ld a, d
    cp frame / 256
    jr nz, test_callee_bad
    ld a, e
    cp frame & 255
    jr nz, test_callee_bad
    push iy
    pop de
    ld a, d
    cp 0x56
    jr nz, test_callee_bad
    ld a, e
    cp 0x78
    jr nz, test_callee_bad
    ld a, h
    cp 0x12
    jr nz, test_callee_bad
    ld a, l
    cp 0x34
    jr nz, test_callee_bad
    ld hl, 2
    add hl, sp
    ld a, (hl)
    cp 0x34
    jr nz, test_callee_bad
    inc hl
    ld a, (hl)
    cp 0x12
    jr nz, test_callee_bad
    ld l, 1
    ret
test_callee_bad:
    ld l, 0
    ret

sp_before: DEFW 0
test_result: DEFB 0xff
frame: DEFS 4, 0
    DEFW 0x1234
_line_buf: DEFB 0
_NETCHESS_PROTO_ACK_PREFIX: DEFB 0
_NETCHESS_PROTO_NACK_PREFIX: DEFB 0
_spectrum_net_payload_scratch:
_spectrum_net_send_text:
    ret
