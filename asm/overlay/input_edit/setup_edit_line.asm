SECTION code_user

PUBLIC _input_edit_setup_line_ovl
PUBLIC setup_edit_line_buf
PUBLIC setup_edit_line_buf_end

EXTERN _spectrum_info_line
EXTERN _setup_choice
EXTERN _setup_visible_mask
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _setup_port_text
EXTERN _edit_max
EXTERN _netchesszx_mqtt_code
EXTERN _netchesszx_direct_host
EXTERN _last_ip
EXTERN _spectrum_gui_edit_show

ROW_TIME   EQU 4
ROW_COLOR  EQU 5
ROW_ACTION EQU 10

_input_edit_setup_line_ovl:
    ld a, (de)
    cp 0xff
    jr nz, setup_edit_line
    ld hl, (_setup_visible_mask)
    bit 2, l
    jr z, setup_edit_line_all_row_3
    push hl
    call setup_edit_line_row_2
    pop hl
setup_edit_line_all_row_3:
    bit 3, l
    call nz, setup_edit_line_row_3
    ld l, 1
    ret
setup_edit_line_row_2:
    ld a, 2
    jr setup_edit_line
setup_edit_line_row_3:
    ld a, 3
setup_edit_line:
    ld (setup_edit_row), a
    ld a, (_setup_choice + 1)
    or a
    jr z, setup_edit_direct
    ld a, 1
    ld (setup_edit_flags), a
    ld hl, _netchesszx_mqtt_code
    jr setup_edit_selected
setup_edit_direct:
    ld a, 8
    ld (setup_edit_flags), a
    ld a, (_setup_choice)
    or a
    jr nz, setup_edit_join_host
    ld hl, _last_ip
    jr setup_edit_selected
setup_edit_join_host:
    ld hl, _netchesszx_direct_host
setup_edit_selected:
    ld a, (_setup_room_editing)
    or a
    jr z, setup_edit_have_cursor
    ld a, (_setup_edit_row)
    ld c, a
    ld a, (setup_edit_row)
    cp c
    jr nz, setup_edit_have_cursor
    ld a, (setup_edit_flags)
    or 2
    ld (setup_edit_flags), a
setup_edit_have_cursor:
    push hl
    ld hl, setup_edit_line_buf
    ld a, (setup_edit_row)
    call setup_edit_screen_row
    ld (hl), a
    inc hl
    ld a, (setup_edit_flags)
    bit 0, a
    ld de, setup_edit_label_room
    jr nz, setup_edit_copy_label
    ld de, setup_edit_label_ip
setup_edit_copy_label:
    ld b, 6
setup_edit_copy_label_loop:
    ld a, (de)
    ld (hl), a
    inc de
    inc hl
    djnz setup_edit_copy_label_loop
    pop de
    ld a, (setup_edit_flags)
    bit 3, a
    jr nz, setup_edit_direct_copy
    ld b, 0
setup_edit_copy_text_loop:
    ld a, b
    cp 6
    jr nc, setup_edit_after_text
    ld a, (de)
    or a
    jr z, setup_edit_after_text
    ld (hl), a
    inc de
    inc hl
    inc b
    jr setup_edit_copy_text_loop
setup_edit_after_text:
    ld a, (setup_edit_flags)
    bit 0, a
    jr z, setup_edit_terminate
    ld a, 6
    sub b
    jr z, setup_edit_room_gap
    ld b, a
setup_edit_room_pad:
    ld (hl), ' '
    inc hl
    djnz setup_edit_room_pad
setup_edit_room_gap:
    ld b, 4
setup_edit_room_gap_loop:
    ld (hl), ' '
    inc hl
    djnz setup_edit_room_gap_loop
    ld de, setup_edit_text_mqtt_host
setup_edit_room_host:
    ld a, (de)
    ld (hl), a
    inc de
    inc hl
    or a
    jr nz, setup_edit_room_host
    jr setup_edit_draw
setup_edit_direct_copy:
    ld b, 0
    ld a, (de)
    or a
    jr nz, setup_edit_direct_ip
    ld (hl), '_'
    inc hl
    inc b
    jr setup_edit_direct_colon
setup_edit_direct_ip:
    ld a, (de)
    or a
    jr z, setup_edit_direct_colon
    ld (hl), a
    inc de
    inc hl
    inc b
    ld a, b
    cp 15
    jr c, setup_edit_direct_ip
setup_edit_direct_colon:
    ld a, 15
    sub b
    jr z, setup_edit_direct_colon_fixed
    ld b, a
setup_edit_direct_pad:
    ld (hl), ' '
    inc hl
    djnz setup_edit_direct_pad
setup_edit_direct_colon_fixed:
    ld (hl), ':'
    inc hl
    ld de, _setup_port_text
setup_edit_direct_port:
    ld a, (de)
    or a
    jr z, setup_edit_terminate
    ld (hl), a
    inc de
    inc hl
    jr setup_edit_direct_port
setup_edit_terminate:
    ld (hl), 0
setup_edit_draw:
    ld hl, setup_edit_line_buf
    call _spectrum_info_line
    ld a, (setup_edit_flags)
    bit 1, a
    jr z, setup_edit_done
    ld a, (setup_edit_row)
    call setup_edit_screen_row
    ld l, a
    ld h, 43
    ld a, (setup_edit_flags)
    bit 0, a
    jr nz, setup_edit_room_cursor
    bit 3, a
    jr z, setup_edit_show
    ld a, (_edit_max)
    cp 5
    jr nz, setup_edit_show
    ld h, 59
    jr setup_edit_show
setup_edit_room_cursor:
    ld h, 45
setup_edit_show:
    call _spectrum_gui_edit_show
setup_edit_done:
    ld l, 1
    ret

setup_edit_screen_row:
    cp ROW_ACTION
    jr z, setup_edit_screen_row_action
    cp ROW_TIME
    jr z, setup_edit_screen_row_time
    cp 3
    jr nz, setup_edit_screen_row_normal
    dec a
setup_edit_screen_row_normal:
    ld e, a
    cp ROW_COLOR
    ld a, e
    jr c, setup_edit_screen_row_base
    add a, 2
setup_edit_screen_row_base:
    add a, 7
    ret
setup_edit_screen_row_time:
    ld a, 10
    ret
setup_edit_screen_row_action:
    ld a, 20
    ret

setup_edit_label_room:
    DEFB "ROOM  "
setup_edit_label_ip:
    DEFB "IP    "
setup_edit_text_mqtt_host:
    DEFB "HIVEMQ", 0
setup_edit_row:
    DEFB 0
setup_edit_flags:
    DEFB 0
setup_edit_line_buf:
    ; Row + "IP    " + 15-char host + ':' + 5-char port + NUL.
    DEFS 29
setup_edit_line_buf_end:
