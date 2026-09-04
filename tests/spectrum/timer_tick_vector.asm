SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

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
    jr nz, test_bad_progress
    ld a, (test_minute)
    cp 59
    jr nz, test_bad_progress
    ld a, (test_second)
    cp 1
    jr nz, test_bad_progress

    ld a, 58
    ld (test_second), a
    call test_tick
    ld a, (test_second)
    cp 59
    jr nz, test_bad_maximum

    call test_tick
    ld a, (test_hour)
    cp 99
    jr nz, test_bad_saturation
    ld a, (test_minute)
    cp 59
    jr nz, test_bad_saturation
    ld a, (test_second)
    cp 59
    jr nz, test_bad_saturation

    ld a, 98
    ld (test_hour), a
    ld a, 59
    ld (test_minute), a
    ld (test_second), a
    call test_tick
    ld a, (test_hour)
    cp 99
    jr nz, test_bad_carry
    ld a, (test_minute)
    or a
    jr nz, test_bad_carry
    ld a, (test_second)
    or a
    jr nz, test_bad_carry

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

_spectrum_net_send_text:
    ld l, 1
    ret
