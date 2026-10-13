SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC tmp_row
PUBLIC tmp_col
PUBLIC tmp_scan
PUBLIC current_attr
PUBLIC draw_ikkle_text_at
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _netchesszx_session_send_ack_text

EXTERN netchesszx_load_stack_args2
EXTERN draw_ikkle_inline
EXTERN _netchess_text_is
EXTERN _netchess_after_prefix
EXTERN _netchesszx_asm_direct_parse_role_owner

test_start:
    ld sp, 0xff00
    ld (initial_sp), sp
    ld ix, 0x1234
    ld iy, 0x5c3a

    ; Keep a byte-for-byte copy of AF before moving it to AF'.
    ld a, 0x5a
    scf
    push af
    pop hl
    ld (expected_af_alt), hl
    push hl
    pop af
    ex af, af'

    ld hl, 0x5678
    push hl
    ld hl, 0x1234
    push hl
    call capture_stack_args
    pop hl
    pop hl

    ld hl, (seen_de)
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_bad_first_arg
    ld hl, (seen_bc)
    ld de, 0x5678
    or a
    sbc hl, de
    jp nz, test_bad_second_arg
    ld hl, (capture_sp)
    ld de, 5
    add hl, de
    ld de, (seen_hl)
    or a
    sbc hl, de
    jp nz, test_bad_hl

    call test_inline_calls
    ld a, (render_calls)
    cp 2
    jp nz, test_bad_render_count
    ld a, (continued)
    cp 1
    jp nz, test_bad_continuation

    ld hl, (seen_cont1)
    ld de, inline_continue
    or a
    sbc hl, de
    jp nz, test_bad_continuation
    ld hl, (seen_cont2)
    ld de, inline_final
    or a
    sbc hl, de
    jp nz, test_bad_continuation

    ld hl, (seen_text1)
    ld de, text1
    or a
    sbc hl, de
    jp nz, test_bad_descriptor
    ld hl, (seen_text2)
    ld de, text2
    or a
    sbc hl, de
    jp nz, test_bad_descriptor
    ld hl, seen_spec1
    ld de, expected_spec1
    call compare_three
    jp nz, test_bad_descriptor
    ld hl, seen_spec2
    ld de, expected_spec2
    call compare_three
    jp nz, test_bad_descriptor
    ld a, (tmp_scan)
    cp 2
    jp nz, test_bad_descriptor

    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_bad_index
    push iy
    pop hl
    ld de, 0x5c3a
    or a
    sbc hl, de
    jp nz, test_bad_index
    ld hl, 0
    add hl, sp
    ld de, (initial_sp)
    or a
    sbc hl, de
    jp nz, test_bad_stack

    ex af, af'
    push af
    pop hl
    ld de, (expected_af_alt)
    or a
    sbc hl, de
    jp nz, test_bad_af_alt

    call test_text_kernels
    jp nz, test_bad_text
    call test_role_owner
    jp nz, test_bad_role_owner

    xor a
    jr test_store

test_bad_text:
    ld a, 10
    jr test_store
test_bad_role_owner:
    ld a, 11
    jr test_store
test_bad_first_arg:
    ld a, 1
    jr test_store
test_bad_second_arg:
    ld a, 2
    jr test_store
test_bad_hl:
    ld a, 3
    jr test_store
test_bad_render_count:
    ld a, 4
    jr test_store
test_bad_continuation:
    ld a, 5
    jr test_store
test_bad_descriptor:
    ld a, 6
    jr test_store
test_bad_index:
    ld a, 7
    jr test_store
test_bad_stack:
    ld a, 8
    jr test_store
test_bad_af_alt:
    ld a, 9
test_store:
    ld (test_result), a
test_done:
    jp test_done

; Returns Z when netchess_text_is() and netchess_after_prefix() agree with
; the C contract: whole-text equality, tail pointer, NULL on mismatch.
test_text_kernels:
    ld hl, txt_ping
    ld de, tok_ping
    call call_text_is
    dec a
    ret nz
    ld hl, txt_ping_tail
    ld de, tok_ping
    call call_text_is
    or a
    ret nz
    ld hl, txt_pin
    ld de, tok_ping
    call call_text_is
    or a
    ret nz
    ld hl, txt_empty
    ld de, txt_empty
    call call_text_is
    dec a
    ret nz
    ld hl, txt_ack
    ld de, tok_ack
    call call_after_prefix
    ld de, txt_ack + 4
    or a
    sbc hl, de
    ret nz
    ld hl, txt_pin
    ld de, tok_ack
    call call_after_prefix
    ld a, h
    or l
    ret

; Returns Z when the DIRECT role parser maps HOST/GUEST, rejects a glued
; suffix and rejects a null owner even when address 0 reads "HOST".
test_role_owner:
    ld hl, txt_host
    call call_role_owner
    dec a
    ret nz
    ld a, (role_out)
    or a
    ret nz
    ld hl, txt_guest
    call call_role_owner
    dec a
    ret nz
    ld a, (role_out)
    dec a
    ret nz
    ld hl, txt_hostx
    call call_role_owner
    or a
    ret nz
    ld hl, txt_host
    ld de, 0
    ld bc, 5
    ldir
    ld hl, 0
    call call_role_owner
    or a
    ret

; HL = owner text; writes role_out. Returns A = L.
call_role_owner:
    ld de, role_out
    ld a, 0xff
    ld (de), a
    push de
    push hl
    call _netchesszx_asm_direct_parse_role_owner
    pop de
    pop de
    ld a, l
    ret

role_out: defb 0
txt_host: defm "HOST"
    defb 0
txt_guest: defm "GUEST"
    defb 0
txt_hostx: defm "HOSTX"
    defb 0

; HL = text, DE = token; C stack ABI. Returns A = L.
call_text_is:
    push de
    push hl
    call _netchess_text_is
    pop de
    pop de
    ld a, l
    ret

call_after_prefix:
    push de
    push hl
    call _netchess_after_prefix
    pop de
    pop de
    ret

txt_ping: defm "PING"
    defb 0
txt_ping_tail: defm "PING "
    defb 0
txt_pin: defm "PIN"
    defb 0
txt_empty: defb 0
tok_ping: defm "PING"
    defb 0
txt_ack: defm "ACK 12"
    defb 0
tok_ack: defm "ACK "
    defb 0

capture_stack_args:
    call netchesszx_load_stack_args2
    ld (seen_de), de
    ld (seen_bc), bc
    ld (seen_hl), hl
    ld hl, 0
    add hl, sp
    ld (capture_sp), hl
    ret

test_inline_calls:
    call draw_ikkle_inline
    DEFB 7, 19, 0x4e
    DEFW text1
inline_continue:
    ld a, 1
    ld (continued), a
    call draw_ikkle_inline
    DEFB 9, 22, 0x4f
    DEFW text2
inline_final:
    ret

draw_ikkle_text_at:
    ld a, (render_calls)
    inc a
    ld (render_calls), a
    dec a
    jr nz, capture_second_render
    ld (seen_text1), hl
    ld a, (tmp_row)
    ld (seen_spec1), a
    ld a, (tmp_col)
    ld (seen_spec1 + 1), a
    ld a, (current_attr)
    ld (seen_spec1 + 2), a
    ld hl, 0
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld (seen_cont1), de
    ret
capture_second_render:
    ld (seen_text2), hl
    ld a, (tmp_row)
    ld (seen_spec2), a
    ld a, (tmp_col)
    ld (seen_spec2 + 1), a
    ld a, (current_attr)
    ld (seen_spec2 + 2), a
    ld hl, 0
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld (seen_cont2), de
    ret

compare_three:
    ld b, 3
compare_three_loop:
    ld a, (de)
    cp (hl)
    ret nz
    inc de
    inc hl
    djnz compare_three_loop
    ret

test_result: defb 0xff
initial_sp: defw 0
capture_sp: defw 0
seen_de: defw 0
seen_bc: defw 0
seen_hl: defw 0
expected_af_alt: defw 0
render_calls: defb 0
continued: defb 0
seen_cont1: defw 0
seen_cont2: defw 0
seen_text1: defw 0
seen_text2: defw 0
seen_spec1: defs 3, 0
seen_spec2: defs 3, 0
expected_spec1: defb 7, 19, 0x4e
expected_spec2: defb 9, 22, 0x4f
text1: defm "CHAT"
       defb 0
text2: defm "BLACK"
       defb 0

tmp_row: defb 0
tmp_col: defb 0
tmp_scan: defb 0
current_attr: defb 0

_line_buf: defs 64, 0
_NETCHESS_PROTO_ACK_PREFIX:
    defm "ACK "
    defb 0
_NETCHESS_PROTO_NACK_PREFIX:
    defm "NACK "
    defb 0
dummy_payload: defs 64, 0

_spectrum_net_payload_scratch:
    ld hl, dummy_payload
    ret

_netchesszx_session_send_ack_text:
    ld l, 1
    ret
