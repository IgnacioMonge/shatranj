SECTION code_user

PUBLIC _mqtt_connect_packet_ovl
PUBLIC _mqtt_will_topic_prefix
EXTERN _mqtt_connect_start_ovl
EXTERN _mqtt_activate_side_ovl
EXTERN _net_preflight_ovl
EXTERN _mqtt_probe_seat_ovl
; Must equal the linked _overlay_scratch_base; check_lowmem_layout.py proves it.
IFDEF NETCHESSZX_NEXT_BANKING
mqtt_packet_ovl EQU 0x3C2B
ELSE
mqtt_packet_ovl EQU 0x672B
ENDIF
EXTERN _netchesszx_mqtt_code
EXTERN _netchesszx_local_color
EXTERN _netchesszx_session_role
EXTERN _netchesszx_mqtt_session_id
EXTERN _spectrum_append_u16

mqtt_client_id_fixed EQU 7

    DEFB 4
    DW _mqtt_connect_start_ovl
    DW _mqtt_activate_side_ovl
    DW _net_preflight_ovl
    DW _mqtt_probe_seat_ovl

mqtt_conn_fixed_header:
    ; Guest default: clean session, no Will. Hosts patch flags to $26 and add
    ; their correlated retained F <side> <sid> Will below.
    ; keepalive 20s; peer death detected by app-level PING miss (~26s).
    DEFB 0, 4, "MQTT", 4, $02, 0, 20, 0

_mqtt_connect_packet_ovl:
    ld hl, _netchesszx_mqtt_code
    ld b, 0
mqtt_code_len_loop:
    ld a, (hl)
    inc hl
    or a
    jr z, mqtt_code_len_done
    inc b
    jr mqtt_code_len_loop
mqtt_code_len_done:
    ld a, (_netchesszx_local_color)
    or a
    ld c, 'W'
    jr z, color_is_white
    ld c, 'B'
color_is_white:
    ld de, mqtt_packet_ovl
    ld a, $10
    ld (de), a
    inc de
    xor a
    ld (de), a
    inc de
    push bc
    ld hl, mqtt_conn_fixed_header
    ld bc, 11
    ldir
    pop bc
    ld a, (_netchesszx_session_role)
    or a
    jr nz, mqtt_conn_flags_done
    ld a, $26
    ld (mqtt_packet_ovl + 9), a
mqtt_conn_flags_done:
    ld a, b
    add a, mqtt_client_id_fixed
    ld (de), a
    inc de
mqtt_client_id_begin:
    ld a, 'Z'
    ld (de), a
    inc de
    ld a, 'X'
    ld (de), a
    inc de
    push bc
    ld hl, _netchesszx_mqtt_code
    ld c, b
    ld b, 0
    ldir
    pop bc
    ld a, (_netchesszx_session_role)
    add a, a
    add a, 'H'
    ld (de), a
    inc de
    push bc
    ld hl, ($5c78)
    ld b, 2
mqtt_client_nonce_loop:
    ld a, h
    ld h, l
    push af
    rrca
    rrca
    rrca
    rrca
    call mqtt_conn_hex_digit
    pop af
    call mqtt_conn_hex_digit
    djnz mqtt_client_nonce_loop
    pop bc
mqtt_client_id_end:
    ld a, (_netchesszx_session_role)
    or a
    jr nz, mqtt_conn_finish
    xor a
    ld (de), a
    inc de
    ld a, b
    add a, 21
    ld (de), a
    inc de
    ld hl, _mqtt_will_topic_prefix
    call mqtt_conn_copy_z
    push bc
    ld hl, _netchesszx_mqtt_code
    ld c, b
    ld b, 0
    ldir
    pop bc
    ld hl, mqtt_will_topic_tail
    call mqtt_conn_copy_z
    ld a, c
    or $20               ; topic suffix is lowercase (pres_w/pres_b); subscribers never see pres_W
    ld (de), a
    inc de
    xor a
    ld (de), a
    inc de
    push de
    inc de
    ld hl, mqtt_will_payload
    call mqtt_conn_copy_z
    ld a, c
    ld (de), a
    inc de
    ld a, ' '
    ld (de), a
    inc de
    ld hl, (_netchesszx_mqtt_session_id)
    push hl
    push de
    call _spectrum_append_u16
    ex de, hl
    pop hl
    ld a, e
    sub l
    dec a
    ld (hl), a
mqtt_conn_finish:
    ; The configured room cap keeps this packet below 128 bytes, so one-byte
    ; Remaining Length is the canonical MQTT encoding.
    ex de, hl
    ld de, mqtt_packet_ovl
    or a
    sbc hl, de
    ld a, l
    sub 2
    ld (mqtt_packet_ovl + 1), a
    ret

mqtt_conn_hex_digit:
    and $0f
    add a, '0'
    cp $3a
    jr c, mqtt_conn_hex_store
    add a, 7
mqtt_conn_hex_store:
    ld (de), a
    inc de
    ret

mqtt_conn_copy_z:
    ld a, (hl)
    or a
    ret z
    ld (de), a
    inc hl
    inc de
    jr mqtt_conn_copy_z

_mqtt_will_topic_prefix:
    DEFB "netchesszx/v1/", 0
mqtt_will_topic_tail:
    DEFB "/pres_", 0
mqtt_will_payload:
    DEFB "F ", 0
