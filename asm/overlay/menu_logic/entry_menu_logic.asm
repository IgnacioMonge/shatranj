SECTION code_user

PUBLIC _menu_logic_update_room_ovl_entry
PUBLIC _menu_logic_move_focus_ovl_entry
PUBLIC _menu_logic_room_append_ovl_entry
PUBLIC _menu_logic_room_editable_ovl_entry
PUBLIC _menu_logic_room_backspace_ovl_entry
PUBLIC _menu_logic_compute_visible_ovl_entry
PUBLIC _menu_logic_step_row_ovl_entry
PUBLIC _status_phase_ovl_entry

EXTERN _netchesszx_direct_host
EXTERN _netchesszx_mqtt_code
EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_cursor
EXTERN _setup_edit_row
EXTERN _setup_port_text
EXTERN _status_phase_ovl

    DW 8
    DW _menu_logic_update_room_ovl_entry
    DW _menu_logic_move_focus_ovl_entry
    DW _menu_logic_room_append_ovl_entry
    DW _menu_logic_room_editable_ovl_entry
    DW _menu_logic_room_backspace_ovl_entry
    DW _menu_logic_compute_visible_ovl_entry
    DW _menu_logic_step_row_ovl_entry
    DW _status_phase_ovl_entry

_status_phase_ovl_entry:
    ex de, hl
    jp _status_phase_ovl

_menu_logic_update_room_ovl_entry:
    ld a, (_setup_choice + 1)
    or a
    jr z, mlur_success
    ld a, (_setup_choice)
    or a
    jr z, mlur_generate
    ld hl, _netchesszx_mqtt_code
    ld (hl), 0
mlur_success:
    ld l, 1
    ret
mlur_generate:
    ld de, _netchesszx_mqtt_code
    ld a, 'N'
    ld (de), a
    inc de
    ld a, 'C'
    ld (de), a
    inc de
    ld hl, (0x5c78)
    ld a, h
    or l
    jr nz, mlur_seed_done
    ld hl, 0x5a3c
mlur_seed_done:
    ld a, h
    rrca
    rrca
    rrca
    rrca
    call mlur_store_hex
    ld a, h
    call mlur_store_hex
    ld a, l
    rrca
    rrca
    rrca
    rrca
    call mlur_store_hex
    ld a, l
    call mlur_store_hex
    xor a
    ld (de), a
    jr mlur_success

mlur_store_hex:
    call mlur_hex
    ld (de), a
    inc de
    ret

mlur_hex:
    and 0x0f
    cp 10
    jr nc, mlur_hex_alpha
    add a, '0'
    ret
mlur_hex_alpha:
    add a, 'A' - 10
    ret

_menu_logic_move_focus_ovl_entry:
    ld a, (de)
    ld (mlmf_key), a
    ld a, (_setup_cursor)
    or a
    jr z, mlmf_toggle_0
    cp 1
    jr z, mlmf_toggle_1
    cp 4
    jr z, mlmf_toggle_2
    cp 5
    jr z, mlmf_toggle_3
    cp 6
    jr z, mlmf_board
    cp 7
    jr z, mlmf_set
    cp 8
    jr z, mlmf_toggle_4
    ld l, 0
    ret
mlmf_toggle_0:
    ld hl, _setup_focus_choice
    jr mlmf_toggle
mlmf_toggle_1:
    ld hl, _setup_focus_choice + 1
    jr mlmf_toggle
mlmf_toggle_2:
    ld hl, _setup_focus_choice + 2
    jr mlmf_toggle
mlmf_toggle_3:
    ld hl, _setup_focus_choice + 3
    jr mlmf_toggle
mlmf_toggle_4:
    ld hl, _setup_focus_choice + 4
mlmf_toggle:
    ld a, (hl)
    xor 1
    ld (hl), a
    jr mlmf_success
mlmf_board:
    ld hl, _setup_focus_board_theme
    ld b, 5
    call mlmf_cycle
    jr mlmf_success
mlmf_set:
    ld hl, _setup_focus_choice + 5
    ld b, 3
    call mlmf_cycle
mlmf_success:
    ld l, 1
    ret
mlmf_cycle:
    ld a, (mlmf_key)
    cp 0x83
    jr z, mlmf_cycle_left
    cp 'o'
    jr z, mlmf_cycle_left
    ld a, (hl)
    inc a
    cp b
    jr c, mlmf_cycle_store
    xor a
    jr mlmf_cycle_store
mlmf_cycle_left:
    ld a, (hl)
    dec a
    cp b
    jr c, mlmf_cycle_store
    ld a, b
    dec a
mlmf_cycle_store:
    ld (hl), a
    ret

_menu_logic_room_append_ovl_entry:
    ld a, (de)
    call ml_room_char
    or a
    jr z, mlra_false
    ld (mlra_ch), a
    call ml_endpoint_text
    ld b, 0
mlra_len_loop:
    ld a, (hl)
    or a
    jr z, mlra_len_done
    inc hl
    inc b
    jr mlra_len_loop
mlra_len_done:
    call ml_endpoint_max
    ld c, a
    ld a, b
    cp c
    jr nc, mlra_false
    ld a, (mlra_ch)
    ld (hl), a
    inc hl
    ld (hl), 0
    ld l, 1
    ret
mlra_false:
    ld l, 0
    ret

ml_room_char:
    cp '0'
    jr c, mlrc_not_digit
    cp '9' + 1
    ret c
mlrc_not_digit:
    ld c, a
    ld a, (_setup_choice + 1)
    or a
    jr nz, mlrc_mqtt
    ld a, (_setup_edit_row)
    cp 2
    jr nz, mlrc_bad
    ld a, c
    cp '.'
    jr z, mlrc_dot
    cp ','
    jr z, mlrc_dot
    cp ';'
    jr z, mlrc_dot
    cp ':'
    jr z, mlrc_dot
mlrc_bad:
    xor a
    ret
mlrc_dot:
    ld a, '.'
    ret
mlrc_mqtt:
    ld a, c
    cp 'a'
    jr c, mlrc_upper
    cp 'z' + 1
    jr nc, mlrc_upper
    sub 'a' - 'A'
    ret
mlrc_upper:
    cp 'A'
    jr c, mlrc_bad
    cp 'Z' + 1
    jr nc, mlrc_bad
    ret

ml_endpoint_max:
    ld a, (_setup_choice + 1)
    or a
    jr z, ml_endpoint_max_direct
    ld a, 6
    ret
ml_endpoint_max_direct:
    ld a, (_setup_edit_row)
    cp 2
    jr nz, ml_endpoint_max_port
    ld a, 15
    ret
ml_endpoint_max_port:
    ld a, 5
    ret

_menu_logic_room_editable_ovl_entry:
    ld a, (_setup_choice + 1)
    or a
    jr nz, mlre_mqtt
    ld a, (_setup_cursor)
    cp 3
    jr z, mlre_true
mlre_mqtt:
    ld a, (_setup_choice)
    cp 1
    jr nz, mlre_false
    ld a, (_setup_cursor)
    cp 2
    jr nz, mlre_false
mlre_true:
    ld l, 1
    ret
mlre_false:
    ld l, 0
    ret

_menu_logic_room_backspace_ovl_entry:
    call ml_endpoint_text
    ld b, 0
mlrb_len_loop:
    ld a, (hl)
    or a
    jr z, mlrb_len_done
    inc hl
    inc b
    jr mlrb_len_loop
mlrb_len_done:
    ld a, b
    or a
    jr z, mlrb_false
    dec hl
    ld (hl), 0
    ld l, 1
    ret
mlrb_false:
    ld l, 0
    ret

ml_endpoint_text:
    ld a, (_setup_choice + 1)
    or a
    jr z, ml_endpoint_direct
    ld hl, _netchesszx_mqtt_code
    ret
ml_endpoint_direct:
    ld a, (_setup_edit_row)
    cp 2
    jr nz, ml_endpoint_port
    ld hl, _netchesszx_direct_host
    ret
ml_endpoint_port:
    ld hl, _setup_port_text
    ret

_menu_logic_compute_visible_ovl_entry:
    push de
    ld h, d
    ld l, e
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld de, 1

    bit 0, c
    jr z, mlcv_write
    set 1, e

    bit 1, c
    jr z, mlcv_write
    ld hl, _setup_choice
    ld a, (hl)
    inc hl
    or (hl)
    jr nz, mlcv_link_room_only
    set 2, e
    set 3, e
    jr mlcv_after_link
mlcv_link_room_only:
    set 2, e

mlcv_after_link:
    bit 2, c
    jr z, mlcv_after_room
    set 3, e

mlcv_after_room:
    bit 3, c
    jr z, mlcv_after_mqtt
    ld a, (_setup_choice)
    or a
    jr z, mlcv_host_side
    set 5, e
    jr mlcv_after_mqtt
mlcv_host_side:
    set 4, e
    bit 4, c
    jr z, mlcv_after_mqtt
    set 5, e

mlcv_after_mqtt:
    bit 5, c
    jr z, mlcv_after_notation
    set 6, e

mlcv_after_notation:
    bit 6, c
    jr z, mlcv_after_board
    set 7, e

mlcv_after_board:
    bit 7, c
    jr z, mlcv_after_set
    set 0, d

mlcv_after_set:
    bit 0, b
    jr z, mlcv_write
    ld a, (_setup_choice)
    or a
    jr nz, mlcv_join_ready
    ld hl, _setup_choice + 1
    ld a, (hl)
    or a
    jr z, mlcv_host_direct_ready
    ld a, c
    cp 0xff
    jr z, mlcv_action
    jr mlcv_write
mlcv_host_direct_ready:
    ld a, c
    and 0xfb
    cp 0xfb
    jr z, mlcv_action
    jr mlcv_write
mlcv_join_ready:
    ld a, c
    and 0xef
    cp 0xef
    jr nz, mlcv_write
    ld hl, _setup_choice + 1
    ld a, (hl)
    or a
    jr nz, mlcv_action
    ld a, (_netchesszx_direct_host)
    or a
    jr z, mlcv_write
mlcv_action:
    set 1, d

mlcv_write:
    pop hl
    ld (hl), e
    inc hl
    ld (hl), d
    ld h, d
    ld l, e
    ret

_menu_logic_step_row_ovl_entry:
    ld (mlsr_ctx), de
    ld h, d
    ld l, e
    ld a, (hl)
    ld (mlsr_row), a
    inc hl
    ld a, (hl)
    ld (mlsr_dir), a
    inc hl
    ld a, (hl)
    ld (mlsr_visible_lo), a
    inc hl
    ld a, (hl)
    ld (mlsr_visible_hi), a
    ld a, (mlsr_dir)
    or a
    jr z, mlsr_prev
    ld a, (mlsr_row)
    inc a
mlsr_next_loop:
    cp 10
    jr nc, mlsr_restore
    push af
    call mlsr_visible
    jr nz, mlsr_found
    pop af
    inc a
    jr mlsr_next_loop
mlsr_prev:
    ld a, (mlsr_row)
    or a
    jr z, mlsr_restore
    dec a
mlsr_prev_loop:
    push af
    call mlsr_visible
    jr nz, mlsr_found
    pop af
    or a
    jr z, mlsr_restore
    dec a
    jr mlsr_prev_loop
mlsr_found:
    pop af
    jr mlsr_store
mlsr_restore:
    ld a, (_setup_cursor)
mlsr_store:
    ld hl, (mlsr_ctx)
    ld (hl), a
    ld l, 1
    ret

mlsr_visible:
    ld e, a
    ld d, 0
    ld hl, mlsr_masks_lo
    add hl, de
    ld a, (mlsr_visible_lo)
    and (hl)
    ret nz
    ld hl, mlsr_masks_hi
    add hl, de
    ld a, (mlsr_visible_hi)
    and (hl)
    ret

mlmf_key:
    DEFB 0
mlra_ch:
    DEFB 0
mlsr_ctx:
    DEFW 0
mlsr_row:
    DEFB 0
mlsr_dir:
    DEFB 0
mlsr_visible_lo:
    DEFB 0
mlsr_visible_hi:
    DEFB 0
mlsr_masks_lo:
    DEFB 1, 2, 4, 8, 16, 32, 64, 128, 0, 0
mlsr_masks_hi:
    DEFB 0, 0, 0, 0, 0, 0, 0, 0, 1, 2
