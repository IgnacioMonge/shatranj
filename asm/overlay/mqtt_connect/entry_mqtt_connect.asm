SECTION code_user

PUBLIC _mqtt_connect_packet_ovl
PUBLIC _mqtt_will_topic_prefix
EXTERN _mqtt_connect_start_ovl
EXTERN _mqtt_activate_side_ovl
EXTERN _mqtt_packet_ovl
EXTERN _netchesszx_mqtt_code
EXTERN _netchesszx_local_color

    DW 2
    DW _mqtt_connect_start_ovl
    DW _mqtt_activate_side_ovl

mqtt_conn_fixed_header:
    DEFB 0, 4, "MQTT", 4, $26, 0, 45, 0

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
    ld de, _mqtt_packet_ovl
    ld a, $10
    ld (de), a
    inc de
    ld a, b
    add a, a
    add a, 45
    ld (de), a
    inc de
    push bc
    ld hl, mqtt_conn_fixed_header
    ld bc, 11
    ldir
    pop bc
    ld a, b
    add a, 5
    ld (de), a
    inc de
    ld a, 'Z'
    ld (de), a
    inc de
    ld a, 'X'
    ld (de), a
    inc de
    ld a, '-'
    ld (de), a
    inc de
    push bc
    ld hl, _netchesszx_mqtt_code
    ld c, b
    ld b, 0
    ldir
    pop bc
    ld a, '-'
    ld (de), a
    inc de
    ld a, c
    ld (de), a
    inc de
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
    ld (de), a
    inc de
    xor a
    ld (de), a
    inc de
    ld a, 3
    ld (de), a
    inc de
    ld hl, mqtt_will_payload
    call mqtt_conn_copy_z
    ld a, c
    ld (de), a
    inc de
    ld a, b
    add a, a
    add a, 47
    ld l, a
    ld h, 0
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
