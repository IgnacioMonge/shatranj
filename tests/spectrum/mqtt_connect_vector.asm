SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _mqtt_connect_start_ovl
PUBLIC _mqtt_activate_side_ovl
PUBLIC _net_preflight_ovl
PUBLIC _mqtt_probe_seat_ovl
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_local_color
PUBLIC _netchesszx_session_role
PUBLIC _netchesszx_mqtt_session_id

EXTERN _mqtt_connect_packet_ovl

mqtt_packet_ovl EQU 0x672b

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, 0x1234
    ld (0x5c78), hl

    xor a
    ld (_netchesszx_session_role), a
    ld (_netchesszx_local_color), a
    ld hl, 1
    ld (_netchesszx_mqtt_session_id), hl
    call _mqtt_connect_packet_ovl
    ld de, host_white_expected
    ld bc, host_white_expected_end - host_white_expected
    call packet_matches
    jp nz, test_bad_host_white

    xor a
    ld (_netchesszx_session_role), a
    inc a
    ld (_netchesszx_local_color), a
    ld hl, 65535
    ld (_netchesszx_mqtt_session_id), hl
    call _mqtt_connect_packet_ovl
    ld de, host_black_expected
    ld bc, host_black_expected_end - host_black_expected
    call packet_matches
    jp nz, test_bad_host_black

    ld a, 1
    ld (_netchesszx_session_role), a
    xor a
    ld (_netchesszx_local_color), a
    call _mqtt_connect_packet_ovl
    ld de, guest_white_expected
    ld bc, guest_white_expected_end - guest_white_expected
    call packet_matches
    jp nz, test_bad_guest_white

    ld a, 1
    ld (_netchesszx_session_role), a
    ld (_netchesszx_local_color), a
    call _mqtt_connect_packet_ovl
    ld de, guest_black_expected
    ld bc, guest_black_expected_end - guest_black_expected
    call packet_matches
    jp nz, test_bad_guest_black

    xor a
    jr test_store_result

test_bad_host_white:
    ld a, 1
    jr test_store_result
test_bad_host_black:
    ld a, 2
    jr test_store_result
test_bad_guest_white:
    ld a, 3
    jr test_store_result
test_bad_guest_black:
    ld a, 4
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

packet_matches:
    or a
    sbc hl, bc
    ret nz
    ld hl, mqtt_packet_ovl
packet_compare_loop:
    ld a, b
    or c
    ret z
    ld a, (de)
    cp (hl)
    ret nz
    inc de
    inc hl
    dec bc
    jr packet_compare_loop

_mqtt_connect_start_ovl:
_mqtt_activate_side_ovl:
_net_preflight_ovl:
_mqtt_probe_seat_ovl:
    ret

test_result:
    DEFB 0xff
_netchesszx_mqtt_code:
    DEFM "NC1234"
    DEFB 0
_netchesszx_local_color:
    DEFB 0
_netchesszx_session_role:
    DEFB 0
_netchesszx_mqtt_session_id:
    DEFW 0

host_white_expected:
    DEFB 0x10, 0x3d, 0, 4
    DEFM "MQTT"
    DEFB 4, 0x26, 0, 20, 0, 13
    DEFM "ZXNC1234H1234"
    DEFB 0, 27
    DEFM "netchesszx/v1/NC1234/pres_w"
    DEFB 0, 5
    DEFM "F W 1"
host_white_expected_end:

host_black_expected:
    DEFB 0x10, 0x41, 0, 4
    DEFM "MQTT"
    DEFB 4, 0x26, 0, 20, 0, 13
    DEFM "ZXNC1234H1234"
    DEFB 0, 27
    DEFM "netchesszx/v1/NC1234/pres_b"
    DEFB 0, 9
    DEFM "F B 65535"
host_black_expected_end:

guest_white_expected:
    DEFB 0x10, 0x19, 0, 4
    DEFM "MQTT"
    DEFB 4, 0x02, 0, 20, 0, 13
    DEFM "ZXNC1234J1234"
guest_white_expected_end:

guest_black_expected:
    DEFB 0x10, 0x19, 0, 4
    DEFM "MQTT"
    DEFB 4, 0x02, 0, 20, 0, 13
    DEFM "ZXNC1234J1234"
guest_black_expected_end:
