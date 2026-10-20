SECTION code_user

EXTERN _direct_listen_ovl
EXTERN _direct_connect_ovl
EXTERN _direct_wait_pc_connect_ovl
EXTERN _direct_read_payload_ovl
EXTERN _direct_send_text_ovl
EXTERN _direct_close_ovl
EXTERN _spxn_poll
EXTERN _spxn_recv
EXTERN _net_wait_frame
PUBLIC _direct_close_wait_ovl

    DEFB 6
    DW _direct_listen_ovl
    DW _direct_connect_ovl
    DW _direct_wait_pc_connect_ovl
    DW _direct_read_payload_ovl
    DW _direct_send_text_ovl
    DW _direct_close_wait_ovl

; ctx[0] is the bounded BYE close wait. The recv staging area is the same
; inactive MQTT buffer as direct_ovl.c; test_direct_close_wait.py binds it.
_direct_close_wait_ovl:
    ld b, (hl)
    ld a, b
    or a
    jr z, direct_close_wait_done
direct_close_wait_poll:
    push bc
    call _spxn_poll
    bit 7, h
    jr nz, direct_close_wait_stop
    ld a, l
    and 0x82                 ; POLLNVAL | POLLHUP
    jr nz, direct_close_wait_stop
    bit 2, l                 ; POLLIN grants one recv
    jr z, direct_close_wait_frame
    ld hl, 160
    push hl
    ld hl, 0x652d
    push hl
    call _spxn_recv          ; callee pops (buffer, capacity)
    ld a, h
    or l
    jr z, direct_close_wait_stop
    bit 7, h
    jr nz, direct_close_wait_stop
direct_close_wait_frame:
    call _net_wait_frame
    pop bc
    djnz direct_close_wait_poll
direct_close_wait_done:
    jp _direct_close_ovl
direct_close_wait_stop:
    pop bc
    jr direct_close_wait_done
direct_close_wait_end:
