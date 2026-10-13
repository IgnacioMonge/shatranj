SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _netchesszx_session_send_ack_text

EXTERN _netchesszx_asm_timer_tick_one_second

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a

    ld a, 99
    ld (test_hour), a
    ld a, 59
    ld (test_minute), a
    xor a
    ld (test_second), a
    call test_tick
    ld a, (test_hour)
    cp 99
    jp nz, test_bad_progress
    ld a, (test_minute)
    cp 59
    jp nz, test_bad_progress
    ld a, (test_second)
    cp 1
    jp nz, test_bad_progress

    ld a, 58
    ld (test_second), a
    call test_tick
    ld a, (test_second)
    cp 59
    jp nz, test_bad_maximum

    call test_tick
    ld a, (test_hour)
    cp 99
    jp nz, test_bad_saturation
    ld a, (test_minute)
    cp 59
    jp nz, test_bad_saturation
    ld a, (test_second)
    cp 59
    jp nz, test_bad_saturation

    ld a, 98
    ld (test_hour), a
    ld a, 59
    ld (test_minute), a
    ld (test_second), a
    call test_tick
    ld a, (test_hour)
    cp 99
    jp nz, test_bad_carry
    ld a, (test_minute)
    or a
    jp nz, test_bad_carry
    ld a, (test_second)
    or a
    jp nz, test_bad_carry

    ld ix, 0x1234
    ld iy, 0x5c3a
    ld (sp_save), sp
    call test_tick

    push ix
    pop hl
    ld a, l
    cp 0x34
    jp nz, test_bad_regs
    ld a, h
    cp 0x12
    jp nz, test_bad_regs
    push iy
    pop hl
    ld a, l
    cp 0x3a
    jp nz, test_bad_regs
    ld a, h
    cp 0x5c
    jp nz, test_bad_regs
    ld hl, 0
    add hl, sp
    ld a, l
    ld de, (sp_save)
    cp e
    jp nz, test_bad_regs
    ld a, h
    cp d
    jp nz, test_bad_regs

    xor a
    jr test_store_result

test_bad_progress:
    ld a, 1
    jr test_store_result
test_bad_maximum:
    ld a, 2
    jr test_store_result
test_bad_saturation:
    ld a, 3
    jr test_store_result
test_bad_carry:
    ld a, 4
    jr test_store_result
test_bad_regs:
    ld a, 7
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_tick:
    ld hl, test_second
    push hl
    ld hl, test_minute
    push hl
    ld hl, test_hour
    push hl
    call _netchesszx_asm_timer_tick_one_second
    pop bc
    pop bc
    pop bc
    ret

test_result:
    defb 0xff
test_hour:
    defb 0
test_minute:
    defb 0
test_second:
    defb 0
sp_save:
    defw 0

_line_buf:
    defs 64, 0
_NETCHESS_PROTO_ACK_PREFIX:
    defm "ACK "
    defb 0
_NETCHESS_PROTO_NACK_PREFIX:
    defm "NACK "
    defb 0
dummy_payload:
    defs 64, 0

_spectrum_net_payload_scratch:
    ld hl, dummy_payload
    ret

_netchesszx_session_send_ack_text:
    ld l, 1
    ret
