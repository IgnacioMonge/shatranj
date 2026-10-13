SECTION code_user

PUBLIC _setup_step_ovl_entry

EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_game_focus
EXTERN _setup_defined_mask
EXTERN _setup_visible_mask
EXTERN _setup_cursor
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _setup_port_text
EXTERN _setup_config_dirty
EXTERN _setup_action_focus
EXTERN _setup_edit_backup
EXTERN _netchesszx_mqtt_code
EXTERN _netchesszx_direct_host
EXTERN _netchesszx_direct_port
EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key
EXTERN _spectrum_gui_edit_hide
EXTERN _netchess_mqtt_session_parse_u16_token
EXTERN _edit_buf
EXTERN _edit_max

CTX             EQU 0x5FE0
CTX_KEY         EQU CTX + 0
CTX_FORCE_LO    EQU CTX + 1
CTX_FORCE_HI    EQU CTX + 2
CTX_CLEAR_FROM  EQU CTX + 3
CTX_ACTION      EQU CTX + 4
CTX_NOTICE      EQU CTX + 5
CTX_FLAGS       EQU CTX + 6

; Selection and edit-result paths are mutually exclusive within one entry.
SU_MARKS_CONFIG EQU CTX_NOTICE

KEY_UP          EQU 0x81
KEY_DOWN        EQU 0x82
KEY_LEFT        EQU 0x83
KEY_RIGHT       EQU 0x84
KEY_HOME        EQU 0x88
KEY_END         EQU 0x89
KEY_CANCEL      EQU 0x8a
SETUP_CLEAR_NONE EQU 0xff
SETUP_CLEAR_ENDPOINT EQU 0xfe

FLAG_RENDER     EQU 0x01
FLAG_PAINT      EQU 0x02
FLAG_EDIT       EQU 0x04
FLAG_SUPPRESS   EQU 0x08
FLAG_TIME_UI    EQU 0x10
FLAG_ACTION_UI  EQU 0x20
ACTION_SET      EQU 2
NOTICE_SELECT   EQU 1
NOTICE_BAD_IP   EQU 3
NOTICE_BAD_ROOM EQU 4
NOTICE_BAD_PORT EQU 5

ROW_GAME        EQU 0
ROW_LINK1       EQU 1
ROW_LINK2       EQU 2
ROW_TIME        EQU 4
ROW_COLOR       EQU 5
ROW_NOTATION    EQU 6
ROW_BOARD       EQU 7
ROW_SET         EQU 8
ROW_HINTS       EQU 9
ROW_ACTION      EQU 10
; NAME is logical row 11 (mask bit 11) but sits between TIME and COLOR on
; screen and in navigation: see su_order_step.
ROW_NAME        EQU 11

INCLUDE "asm/app/player_name_layout.inc"

    DEFB 2
    DW _setup_step_ovl_entry
    DW _setup_compute_visible_ovl_entry

_setup_compute_visible_ovl_entry:
    ex de, hl
    push hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    call su_compute_visible
    pop de
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a
    ret

_setup_step_ovl_entry:
    call su_clear_ctx
    ld a, (_setup_room_editing)
    or a
    jp nz, su_edit_key
    ld a, (CTX_KEY)
    cp KEY_UP
    jr z, su_nav_prev
    cp KEY_DOWN
    jr z, su_nav_next
    cp KEY_LEFT
    jr z, su_focus
    cp KEY_RIGHT
    jr z, su_focus
    cp 32
    jr z, su_select_key
    cp 13
    jr nz, su_ret1
su_select_key:
    jp su_select

; A = flag bits; ORs into CTX_FLAGS then falls through to return 1.
su_or_flags:
    ld hl, CTX_FLAGS
    or (hl)
    ld (hl), a
su_ret1:
    ld l, 1
    ret

su_clear_ctx:
    xor a
    ld hl, CTX_FORCE_LO
    ld b, 6
su_clear_loop:
    ld (hl), a
    inc hl
    djnz su_clear_loop
    dec a
    ld (CTX_CLEAR_FROM), a
    ret

su_nav_prev:
    xor a
    jr su_nav_go
su_nav_next:
    ld a, 1
su_nav_go:
    ld b, a
    ld a, (_setup_cursor)
    push af
    ld a, b
    call su_step_row
    ld (_setup_cursor), a
    cp ROW_ACTION
    jr nz, su_nav_paint
    ld a, (_setup_config_dirty)
    xor 1
    ld (_setup_action_focus), a
su_nav_paint:
    pop af
    ld e, a
    call su_mark_paint_row
    ld a, e
    cp ROW_BOARD
    jr nz, su_nav_next_mark
    ld a, (_setup_game_focus)
    ld (_setup_focus_board_theme), a
su_nav_next_mark:
    ld a, (_setup_cursor)
    call su_mark_paint_row
    jr su_ret1

su_focus:
    ld a, (_setup_cursor)
    cp 2
    jr nc, su_focus_later
    ld e, a
    ld d, 0
    ld hl, _setup_focus_choice
    add hl, de
    jr su_focus_toggle
su_focus_later:
    cp ROW_TIME
    jr nc, su_focus_option
    ; Like CREATE/JOIN, either horizontal arrow alternates the two controls.
    ld hl, (_setup_choice)
    ld a, l
    dec a
    or h
    jr nz, su_ret1
    ld a, (_setup_cursor)
    xor 3
    jr su_nav_go
su_focus_option:
    cp ROW_COLOR
    jr z, su_focus_2
    cp ROW_NOTATION
    jr z, su_focus_3
    cp ROW_BOARD
    jr z, su_focus_board
    cp ROW_SET
    jr z, su_focus_set
    cp ROW_HINTS
    jr z, su_focus_4
    jp su_ret1
su_focus_2:
    ld hl, _setup_focus_choice + 2
    jr su_focus_toggle
su_focus_3:
    ld hl, _setup_focus_choice + 3
    jr su_focus_toggle
su_focus_4:
    ld hl, _setup_focus_choice + 4
su_focus_toggle:
    ld a, (hl)
    xor 1
    ld (hl), a
    jr su_focus_paint
su_focus_board:
    ld hl, _setup_focus_board_theme
    ld b, 5
    call su_cycle_choice
    ; Classic and Next both preview the focused theme immediately.
    ld hl, CTX_FLAGS
    set 3, (hl)
    jr su_focus_paint
su_focus_set:
    ld hl, _setup_focus_choice + 5
    ld b, 3
    call su_cycle_choice
su_focus_paint:
    ld a, (_setup_cursor)
    jr su_mark_paint_row

; A = logical row. Accumulates its bit in CTX_FORCE and routes its painter.
su_mark_paint_row:
    push af
    call su_row_bit_bc
    ld hl, (CTX_FORCE_LO)
    ld a, l
    or c
    ld l, a
    ld a, h
    or b
    ld h, a
    ld (CTX_FORCE_LO), hl
    pop af
    ld d, FLAG_TIME_UI
    cp ROW_TIME
    jr z, su_mark_ui
    ld d, FLAG_ACTION_UI
    cp ROW_ACTION
    jr z, su_mark_ui
    ld d, FLAG_PAINT
su_mark_ui:
    ld a, d
    jp su_or_flags

su_cycle_choice:
    ld a, (CTX_KEY)
    cp KEY_LEFT
    jr z, su_cycle_left
    ld a, (hl)
    inc a
    cp b
    jr c, su_cycle_store
    xor a
su_cycle_store:
    ld (hl), a
    ret
su_cycle_left:
    ld a, (hl)
    dec a
    cp b
    jr c, su_cycle_store
    ld a, b
    dec a
    jr su_cycle_store

su_select:
    ld a, (_setup_cursor)
    cp 2
    jr c, su_select_conn
    cp 4
    jr c, su_select_endpoint
    cp ROW_NAME
    jr z, su_select_name
    ld b, a
    cp ROW_BOARD
    jr z, su_select_board
    cp ROW_SET
    jr z, su_select_set
    cp ROW_COLOR
    jp c, su_ret1
    cp ROW_BOARD
    jr c, su_select_binary_low
    cp ROW_HINTS
    jp nz, su_ret1
    ld e, 4
    jr su_select_binary_ptr
su_select_binary_low:
    sub 3
    ld e, a
su_select_binary_ptr:
    ld d, 0
    ld hl, _setup_focus_choice
    add hl, de
    ld a, (hl)
    ld hl, _setup_choice
    add hl, de
    ld (hl), a
    ld a, b
su_define_game_row_and_finish:
    call su_set_defined_row
    jp su_after_game_select

su_define_row_and_finish:
    call su_set_defined_row
    jp su_after_select

su_select_board:
    ld a, (_setup_focus_board_theme)
    ld (_setup_game_focus), a
    ld a, ROW_BOARD
    call su_set_defined_row
    jp su_after_game_select

su_select_set:
    ld a, ACTION_SET
    ld (CTX_ACTION), a
    ; Accepting a different SET turns EDIT into SAVE: repaint ACTION now,
    ; not when the cursor next reaches it.
    ld a, FLAG_ACTION_UI
    jp su_or_flags

su_select_endpoint:
    call su_room_editable
    jr z, su_endpoint_define
su_select_name:
su_endpoint_edit:
    call su_begin_room_edit
su_mark_current_suppress:
    ld a, (_setup_cursor)
    call su_mark_paint_row
    ld a, FLAG_SUPPRESS
    jp su_or_flags
su_endpoint_define:
    ld a, (_setup_cursor)
    jr su_define_row_and_finish

; A = cursor: 0 GAME, 1 LINK. Its defined bit is cursor + 1.
su_select_conn:
    ld e, a
    ld d, 0
    ld hl, _setup_focus_choice
    add hl, de
    ld a, (hl)
    ld hl, _setup_choice
    add hl, de
    inc e
    ld d, e
    call su_select_primary
    jr z, su_select_unchanged
    ld a, b
    or a
    jr z, su_conn_kept
    call su_prepare_complete_connection
    jp nz, su_after_select
    ; Unfinished menu: keep GAME (and LINK) defined, clear from the next row.
    ld a, (_setup_cursor)
    inc a
    ld (CTX_CLEAR_FROM), a
    add a, a
    dec a
    ld hl, _setup_defined_mask
    and (hl)
    ld (hl), a
    inc hl
    ld (hl), 0
su_conn_kept:
    ; Only LINK auto-defines row 2: the static CREATE+DIRECT IP.
    ld a, (_setup_cursor)
    or a
    jr z, su_after_select
su_link_auto_room:
    ld hl, (_setup_choice)
    ld a, h
    or l
    jr nz, su_after_select
    ld a, 2
    ld (CTX_CLEAR_FROM), a
    jp su_define_row_and_finish

; A = focused choice, D = defined bit, HL = stored choice.
; Returns B = value changed, Z only when value and defined bit are unchanged.
su_select_primary:
    ld e, a
    ld a, (hl)
    cp e
    ld b, 0
    jr z, su_sp_same
    inc b
su_sp_same:
    ld (hl), e
    ld a, (_setup_defined_mask)
    ld c, a
    or d
    ld (_setup_defined_mask), a
    xor c
    or b
    ret z
    push bc
    call su_update_room
    pop bc
    or 1
    ret

su_select_unchanged:
    ; From GAME/LINK the next visible row is LINK, IP or PORT: any visible
    ; mask with ACTION also has rows 1-3, so nav's ACTION/BOARD hooks idle.
    ld a, FLAG_SUPPRESS
    call su_or_flags
    jp su_nav_next

; A complete menu remains an editor when GAME/LINK changes. Invalidate only
; the endpoint, then immediately restore it when the stored values are valid.
; Bit 10 is the existing ACTION/completion marker set by TIME_CONFIG init.
su_prepare_complete_connection:
    ; GAME setup is finished once HINTS is defined, which is the same test
    ; su_cv_check_action uses to reveal ACTION. Row 10 is never marked
    ; defined anywhere, so testing it here made this path dead and every
    ; seat or link change wiped the section.
    ld hl, _setup_defined_mask + 1
    bit 1, (hl)
    ret z
    dec hl
    ld a, (hl)
    and 0xf3
    ld (hl), a
    ld a, SETUP_CLEAR_ENDPOINT
    ld (CTX_CLEAR_FROM), a
    call su_endpoint_valid
    jr z, su_pcc_done
    call su_set_defined_rows_2_3
su_pcc_done:
    or 1
    ret

su_after_select:
    ld a, 1
    jr su_after_select_dirty
su_after_game_select:
    xor a
su_after_select_dirty:
    ld (SU_MARKS_CONFIG), a
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (su_scratch), hl
    ld a, (CTX_CLEAR_FROM)
    call su_write_force_from_row
    ld a, (CTX_CLEAR_FROM)
    cp SETUP_CLEAR_ENDPOINT
    jr nz, su_as_endpoint_progress
    ; Advance through the common filter: GAME goes to LINK; LINK goes to the
    ; first editable endpoint, skipping static CREATE-DIRECT IP.
    ld a, 1
    call su_step_row
    jr su_as_store_next
su_as_endpoint_progress:
    cp 2
    jr nz, su_as_clear_visible
su_as_endpoint_next:
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    ld a, 4
    jr z, su_as_store_next
    ld hl, (_setup_choice)
    ld a, l
    or a
    jr nz, su_as_clear_visible
    ; CREATE: MQTT generates its room and DIRECT keeps a valid stored port
    ; (5000 unless changed), so both continue at TIME; PORT stays reachable.
    ; Only an invalid port still stops on PORT.
su_as_create_endpoint:
    or h
    call z, su_parse_port
    ld a, 3
    jr z, su_as_store_next
    call su_set_defined_rows_2_3
    ld a, 4
    jr su_as_store_next
su_as_clear_visible:
    ld a, (CTX_CLEAR_FROM)
    cp SETUP_CLEAR_NONE
    jr z, su_as_first_new
    push af
    call su_row_bit_bc
    ld hl, (su_scratch)
    call su_mask_has_bc
    jr nz, su_as_use_clear_pop
    pop af
    jr su_as_first_new
su_as_use_clear_pop:
    pop af
    jr su_as_store_next
su_as_first_new:
    ld hl, (su_scratch)
    ld de, (_setup_visible_mask)
    ld a, e
    cpl
    and l
    ld l, a
    ld a, d
    cpl
    and h
    ld h, a
    call su_first_row
    cp SETUP_CLEAR_NONE
    jr nz, su_as_store_next
    ld a, 1
    call su_step_row
su_as_store_next:
    ld (_setup_cursor), a
    call su_room_editable
    jr z, su_as_flags
    ld a, (hl)
    or a
    call z, su_begin_room_edit
su_as_flags:
    call su_mark_dirty
    ld a, (SU_MARKS_CONFIG)
    or a
    ld a, FLAG_ACTION_UI | FLAG_RENDER | FLAG_SUPPRESS
    jr z, su_as_ui
    ld a, FLAG_TIME_UI | FLAG_ACTION_UI | FLAG_RENDER | FLAG_SUPPRESS
su_as_ui:
    jp su_or_flags

su_mark_dirty:
    ld a, 1
    ld (_setup_config_dirty), a
    xor a
    ld (_setup_action_focus), a
    ret

su_edit_key:
    ld a, (CTX_KEY)
    cp KEY_CANCEL
    jr z, su_edit_cancel
    cp 8
    jr z, su_edit_apply
    cp 13
    jr z, su_edit_confirm
    cp 32
    jr z, su_edit_confirm
    cp KEY_LEFT
    jr c, su_edit_char
    cp KEY_END + 1
    jr c, su_edit_apply
su_edit_char:
    ld c, a
    call su_editing_name
    ld a, c
    jr nz, su_edit_char_endpoint
    call su_name_char
    jr su_edit_char_checked
su_edit_char_endpoint:
    call su_room_char
su_edit_char_checked:
    or a
    jp z, su_ret1
su_edit_apply:
    ld l, a
    call _spectrum_gui_edit_key
    ld a, l
    or a
    jp z, su_ret1
    ld a, FLAG_EDIT
    jp su_or_flags

su_edit_cancel:
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    call su_restore_edit
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, (_setup_edit_row)
    call su_mark_paint_row
    ld a, FLAG_EDIT | FLAG_SUPPRESS
    jp su_or_flags

su_edit_confirm:
    ; NAME is optional: an empty or any filtered entry confirms as typed.
    call su_editing_name
    jr z, su_ec_not_direct
    ; Empty text needs no pre-check: each validator below rejects it.
    ld a, (_setup_choice + 1)
    or a
    jr z, su_ec_direct
    call su_validate_room
    jr nz, su_ec_not_direct
su_ec_bad_room:
    ld a, NOTICE_BAD_ROOM
    jr su_ec_bad
su_ec_direct:
    ld a, (_edit_max)
    cp 5
    jr z, su_ec_check_port
    call su_validate_ip
    jr z, su_ec_bad_ip
    jr su_ec_not_direct
su_ec_bad_ip:
    ld a, NOTICE_BAD_IP
    jr su_ec_bad
su_ec_check_port:
    call su_parse_port
    jr nz, su_ec_not_direct
su_ec_bad_port:
    ld a, NOTICE_BAD_PORT
su_ec_bad:
    ; A rejected entry must not survive in the live config: _edit_buf
    ; points straight at the setting, so the invalid text was already
    ; committed and the row stayed marked defined. Put the last accepted
    ; value back and rebind the editor so the user can retype it.
    ld (CTX_NOTICE), a
    call su_restore_edit
    call su_begin_room_edit
    jp su_ret1
su_ec_not_direct:
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    ld a, (_setup_edit_row)
    call su_set_defined_row
    call su_editing_name
    jr z, su_ec_after_port
    ; MQTT has no port; DIRECT keeps a valid stored port defined, so an
    ; accepted IP continues at TIME instead of walking through PORT.
    ld a, (_setup_choice + 1)
    or a
    call z, su_parse_port
    jr z, su_ec_after_port
    ld a, 3
    call su_set_defined_row
su_ec_after_port:
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ; With PORT defined, an accepted IP steps on from PORT's position.
    ld a, (_setup_defined_mask)
    and 0x08
    jr z, su_ec_step
    ld a, (_setup_cursor)
    cp 2
    jr nz, su_ec_step
    inc a
    ld (_setup_cursor), a
su_ec_step:
    ld a, 1
    call su_step_row
    ld (_setup_cursor), a
    call su_mark_dirty
    ld a, FLAG_TIME_UI | FLAG_ACTION_UI
    call su_or_flags
    ld a, (_setup_edit_row)
    call su_write_row_bit_to_force
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, FLAG_RENDER | FLAG_SUPPRESS
    jp su_or_flags

su_update_room:
    ld a, (_setup_choice + 1)
    or a
    ret z
    ld a, (_setup_choice)
    or a
    jr z, su_ur_prepare
    call su_validate_room
    ret nz
    scf
su_ur_prepare:
    ld hl, 0x434e
    ld (_netchesszx_mqtt_code), hl
    ld de, _netchesszx_mqtt_code + 2
    ld (de), a
    ret c
    ld hl, (0x5c78)
    ld a, h
    or l
    jr nz, su_ur_seed_done
    ld hl, 0x5a3c
su_ur_seed_done:
    ld b, 2
su_ur_byte:
    ld a, h
    rrca
    rrca
    rrca
    rrca
    call su_ur_store_hex
    ld a, h
    call su_ur_store_hex
    ld h, l
    djnz su_ur_byte
    xor a
    ld (de), a
    ret
su_ur_store_hex:
    and 0x0f
    add a, '0'
    cp '9' + 1
    jr c, su_ur_digit
    add a, 'A' - '9' - 1
su_ur_digit:
    ld (de), a
    inc de
    ret

su_begin_room_edit:
    ld a, (_setup_cursor)
    ld (_setup_edit_row), a
    call su_edit_target_hl_bc
    ld a, c
    push hl
    ld de, _setup_edit_backup
    ldir
    pop hl
    dec a
    ld c, a
    ld a, 1
    ld (_setup_room_editing), a
    call su_editing_name
    jr z, su_bre_bind
    ld a, (_setup_choice + 1)
    or a
    jr z, su_bre_bind
    inc hl
    inc hl
    ld c, 4
su_bre_bind:
    ld a, c
    ld (_edit_max), a
    call _spectrum_gui_edit_bind
    ; Explicit selection adds PAINT; auto-entry already performs full RENDER.
    ld a, FLAG_EDIT
    jp su_or_flags

su_edit_target_hl_bc:
    call su_editing_name
    jr nz, su_edit_target_endpoint
    ld hl, PLAYER_NAME
    ld bc, PLAYER_NAME_MAX + 1
    ret
su_edit_target_endpoint:
    ld hl, _netchesszx_direct_host
    ld bc, 16
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_edit_target_mqtt
    ld a, (_setup_edit_row)
    cp 3
    ret nz
    ld hl, _setup_port_text
    ld c, 6
    ret
su_edit_target_mqtt:
    ld hl, _netchesszx_mqtt_code
    inc c
    ret

su_restore_edit:
    call su_edit_target_hl_bc
    ex de, hl
    ld hl, _setup_edit_backup
    ldir
    ld a, (_setup_edit_row)
    cp 3
    ret nz
    jp su_parse_port

su_room_editable:
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_re_mqtt
    ld a, (_setup_cursor)
    cp 3
    jr z, su_re_port
    cp 2
    jr nz, su_re_false
    ld hl, _netchesszx_direct_host
    ld a, (_setup_choice)
    or a
    ret
su_re_port:
    ld hl, _setup_port_text
    inc a
    ret
su_re_mqtt:
    ld a, (_setup_cursor)
    cp 2
    jr nz, su_re_false
    ld hl, _netchesszx_mqtt_code + 2
    inc a
    ret
su_re_false:
    xor a
    ret

su_endpoint_valid:
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_validate_room
    ld hl, (_netchesszx_direct_port)
    ld a, h
    or l
    ret z
    ld a, (_setup_choice)
    or a
    jp nz, su_validate_ip
    inc a
    ret

su_room_char:
    cp '0'
    jr c, su_rc_not_digit
    cp '9' + 1
    ret c
su_rc_not_digit:
    ld c, a
    ld a, (_setup_choice + 1)
    or a
    jr z, su_rc_direct
    ld a, c
    and 0xdf
    cp 'A'
    jr c, su_rc_bad
    cp 'F' + 1
    ret c
    jr su_rc_bad
su_rc_direct:
    ld a, (_edit_max)
    cp 5
    jr z, su_rc_bad
    ld a, c
    and 0xfd
    cp ','
    jr z, su_rc_dot
    ld a, c
    sub ':'
    cp 2
    jr c, su_rc_dot
su_rc_bad:
    xor a
    ret
su_rc_dot:
    ld a, '.'
    ret
; Z when the edited row is NAME.
su_editing_name:
    ld a, (_setup_edit_row)
    cp ROW_NAME
    ret

; A = typed key. Returns A = the accepted upper-case name character, else 0.
su_name_char:
    cp '-'
    ret z
    cp '_'
    ret z
    cp '0'
    jr c, su_name_char_bad
    cp '9' + 1
    ret c
    and 0xdf
    cp 'A'
    jr c, su_name_char_bad
    cp 'Z' + 1
    ret c
su_name_char_bad:
    xor a
    ret

su_validate_room:
    ld hl, (_netchesszx_mqtt_code)
    ld de, 0x434e
    or a
    sbc hl, de
    jr nz, su_vr_bad
    ld hl, _netchesszx_mqtt_code + 2
    ld b, 4
su_vr_hex:
    ld a, (hl)
    call su_room_char
    or a
    jr z, su_vr_bad
    ld (hl), a
    inc hl
    djnz su_vr_hex
    ld a, (hl)
    or a
    jr nz, su_vr_bad
    inc a
    ret
su_vr_bad:
    xor a
    ret

su_compute_visible:
    ld de, 1
    bit 0, c
    jr z, su_cv_done
    set 1, e
    bit 1, c
    jr z, su_cv_done
    set 2, e
    set 3, e
su_cv_after_link:
    ; HINTS (bit 9), not ACTION (bit 10): ACTION is never marked defined, so
    ; testing it hid GAME SETUP after a completed CREATE→JOIN seat change.
    bit 1, b
    jr z, su_cv_progressive
    ld de, 0x0bdf
    ld a, (_setup_choice)
    or a
    jr nz, su_cv_check_action
    set 5, e
    jr su_cv_check_action
su_cv_progressive:
su_cv_after_room:
    bit 3, c
    jr z, su_cv_done
    set 4, e
    bit 4, c
    jr z, su_cv_done
    ; NAME follows TIME; GAME SETUP waits until NAME is answered (an empty
    ; name is a valid answer).
    set 3, d
    bit 3, b
    jr z, su_cv_done
    ld a, (_setup_choice)
    or a
    jr z, su_cv_host_color
    set 6, e
    jr su_cv_after_color
su_cv_host_color:
    set 5, e
    bit 5, c
    jr z, su_cv_done
    set 6, e
su_cv_after_color:
    bit 6, c
    jr z, su_cv_done
    set 7, e
    bit 7, c
    jr z, su_cv_done
    set 0, d
    bit 0, b
    jr z, su_cv_done
    set 1, d
su_cv_check_action:
    bit 1, b
    jr z, su_cv_done
    ld hl, (_setup_choice)
    ld a, l
    or a
    jr z, su_cv_create_ready
    ld a, c
    or 0x20
    jr su_cv_required
su_cv_create_ready:
    ld a, h
    or a
    ld a, c
    jr nz, su_cv_required
    or 0x04
su_cv_required:
    inc a
    jr nz, su_cv_done
    ld a, l
    or a
    jr z, su_cv_action
    ld a, h
    or a
    jr nz, su_cv_action
    ld a, (_netchesszx_direct_host)
    or a
    jr z, su_cv_done
su_cv_action:
    set 2, d
su_cv_done:
    ex de, hl
    ret

; A = 0 previous / 1 next. Returns the adjacent visible row, or the cursor.
su_step_row:
    ld d, a
    ld a, (_setup_cursor)
su_step_loop:
    call su_order_step
    jr c, su_step_restore
    call su_visible_a
    ld a, e
    ret nz
    jr su_step_loop
su_step_restore:
    ld a, (_setup_cursor)
    ret

; A = row, D = 0 previous / 1 next. Returns A = neighbouring row in screen
; order (GAME..TIME, NAME, COLOR..ACTION); CF set past either end.
su_order_step:
    dec d
    inc d
    jr z, su_order_prev
    cp ROW_TIME
    jr z, su_order_name
    cp ROW_NAME
    jr z, su_order_color
    inc a
    cp ROW_ACTION + 1
    ccf
    ret
su_order_prev:
    cp ROW_COLOR
    jr z, su_order_name
    cp ROW_NAME
    jr z, su_order_time
    sub 1
    ret
; Reached only through an equal CP, so CF is already clear.
su_order_name:
    ld a, ROW_NAME
    ret
su_order_color:
    ld a, ROW_COLOR
    ret
su_order_time:
    ld a, ROW_TIME
    ret

su_visible_a:
    ld e, a
su_va_mask:
    ld a, e
    cp 2
    jr nz, su_va_check_port
    ld a, (_setup_choice + 1)
    or a
    jr nz, su_va_bit_restore
    ld a, (_setup_choice)
    or a
    jp z, su_re_false
su_va_bit_restore:
    ld a, e
su_va_check_port:
    cp 3
    jr nz, su_va_bit
    ld a, (_setup_choice + 1)
    or a
    jp nz, su_re_false
    ld a, e
su_va_bit:
    call su_row_bit_bc
    ld hl, (_setup_visible_mask)
    jr su_mask_has_bc

; HL = mask. Returns the first row of HL in screen order, or CLEAR_NONE.
su_first_row:
    xor a
    ld d, 1
su_fr_loop:
    ld e, a
    call su_row_bit_bc
    call su_mask_has_bc
    ld a, e
    ret nz
    call su_order_step
    jr nc, su_fr_loop
    ld a, SETUP_CLEAR_NONE
    ret

su_mask_has_bc:
    ld a, l
    and c
    ret nz
    ld a, h
    and b
    ret

su_row_bit_bc:
    ld bc, 1
    or a
    ret z
su_rbbc_shift:
    sla c
    rl b
    dec a
    jr nz, su_rbbc_shift
    ret

su_set_defined_rows_2_3:
    ld hl, _setup_defined_mask
    ld a, (hl)
    or 0x0c
    ld (hl), a
    ret

su_set_defined_row:
    call su_row_bit_bc
    ld hl, _setup_defined_mask
    ld a, (hl)
    or c
    ld (hl), a
    inc hl
    ld a, (hl)
    or b
    ld (hl), a
    ret

su_write_force_from_row:
    cp SETUP_CLEAR_ENDPOINT
    jr nz, su_wff_not_endpoint
    ; IP only. COLOR appear/hide stays in MENU_CONFIG so GAME SETUP is
    ; not dirtied when CONNECTION changes after HINTS is already defined.
    ld a, (_setup_visible_mask)
    cpl
    and l
    and 0x20
    or 1 << 2
    ld c, a
    ld b, 0
    jr su_wff_store
su_wff_not_endpoint:
    cp SETUP_CLEAR_NONE
    ret z
su_wff_row:
    call su_row_bit_bc
    dec bc
    ld a, c
    cpl
    ld c, a
    ld a, b
    cpl
    ld b, a
su_wff_store:
    ld (CTX_FORCE_LO), bc
    ret

su_write_row_bit_to_force:
    call su_row_bit_bc
    jr su_wff_store

su_validate_ip:
    ld hl, _netchesszx_direct_host
    ld b, 4
su_ip_segment:
    push bc
    ld de, su_scratch
    push de
    push hl
    call _netchess_mqtt_session_parse_u16_token
    pop de
    pop bc
    pop bc
    ld a, h
    or l
    jr z, su_ip_fail
    sbc hl, de
    ld a, h
    or a
    jr nz, su_ip_fail
    ld a, l
    cp 4
    jr nc, su_ip_fail
    add hl, de
    ld a, (su_scratch + 1)
    or a
    jr nz, su_ip_fail
    dec b
    jr z, su_ip_end
    ld a, (hl)
    cp '.'
    jr nz, su_ip_fail
    inc hl
    jr su_ip_segment
su_ip_end:
    ld a, (hl)
    or a
    jr nz, su_ip_fail
    inc a
    ret
su_ip_fail:
    xor a
    ret

su_parse_port:
    ld hl, su_scratch
    push hl
    ld hl, _setup_port_text
    push hl
    call _netchess_mqtt_session_parse_u16_token
    pop de
    pop de
    ld a, h
    or l
    ret z
    ld a, (hl)
    or a
    jr nz, su_pp_fail
    ld hl, (su_scratch)
    ld a, h
    or l
    ret z
    ld (_netchesszx_direct_port), hl
    ret
su_pp_fail:
    xor a
    ret

; Mutually exclusive visible-mask, IP-digit and parsed-port scratch.
su_scratch: DEFW 0
