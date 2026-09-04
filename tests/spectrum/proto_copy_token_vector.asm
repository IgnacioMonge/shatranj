SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

EXTERN _netchesszx_asm_proto_copy_token

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, test_source
    ld (test_source_ptr), hl

    ; SDCC/IY normal ABI: const char **p, char *out, uint8_t cap.
    ld hl, 6
    push hl
    ld hl, test_output
    push hl
    ld hl, test_source_ptr
    push hl
    call _netchesszx_asm_proto_copy_token
    pop bc
    pop bc
    pop bc

    ld a, l
    cp 5
    jr nz, test_bad_length
    ld hl, (test_source_ptr)
    ld de, test_source + 5
    or a
    sbc hl, de
    jr nz, test_bad_pointer
    ld hl, test_output
    ld de, test_expected
    ld b, 6
test_compare:
    ld a, (de)
    cp (hl)
    jr nz, test_bad_output
    inc de
    inc hl
    djnz test_compare
    xor a
    jr test_store_result

test_bad_length:
    ld a, 1
    jr test_store_result
test_bad_pointer:
    ld a, 2
    jr test_store_result
test_bad_output:
    ld a, 3
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_result:
    defb 0xff
test_source_ptr:
    defw 0
test_source:
    defm "e7e8q"
    defb 0
test_expected:
    defm "e7e8q"
    defb 0
test_output:
    defs 8, 0xa5

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
