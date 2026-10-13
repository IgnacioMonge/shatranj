SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _setup_choice
PUBLIC _setup_focus_choice
PUBLIC _setup_focus_board_theme
PUBLIC _setup_defined_mask
PUBLIC _setup_visible_mask
PUBLIC _setup_cursor
PUBLIC _setup_room_editing
PUBLIC _setup_edit_row
PUBLIC _setup_port_text
PUBLIC _setup_timezone_text
PUBLIC _setup_timezone_value
PUBLIC _setup_config_dirty
PUBLIC _setup_game_focus
PUBLIC _setup_time_focus
PUBLIC _setup_action_focus
PUBLIC _setup_edit_was_dirty
PUBLIC _setup_edit_backup
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host
PUBLIC _netchesszx_direct_port
PUBLIC _netchesszx_session_role
PUBLIC _netchesszx_transport
PUBLIC _netchesszx_local_color
PUBLIC _netchesszx_host_color
PUBLIC _netchesszx_host_color_ready
PUBLIC _netchesszx_notation
PUBLIC _netchesszx_movement_hints
PUBLIC _netchesszx_board_theme_index
PUBLIC _netchesszx_piece_set_index
PUBLIC _netchesszx_timezone
PUBLIC _netchesszx_timezone_last
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _netchesszx_session_send_ack_text

EXTERN _setup_step_ovl_entry
EXTERN _setup_compute_visible_ovl_entry
EXTERN su_validate_ip
EXTERN su_step_row

INCLUDE "asm/app/player_name_layout.inc"
EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key
EXTERN _spectrum_gui_edit_hide
EXTERN _spectrum_append_u16
EXTERN _netchess_mqtt_session_parse_u16_token
EXTERN _edit_buf
EXTERN _edit_max

CTX_KEY EQU 0x5fe0
CTX_FORCE EQU 0x5fe1
CTX_CLEAR EQU 0x5fe3
CTX_ACTION EQU 0x5fe4
CTX_NOTICE EQU 0x5fe5
CTX_FLAGS EQU 0x5fe6

test_start:
    jp test_current_contract

; Legacy init/commit vectors intentionally remain below as historical failure
; labels, but are unreachable.  TIME_CONFIG owns initialization and commit.
test_current_contract:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a

    ; The compact validator reuses the resident decimal parser but preserves
    ; dotted-quad syntax, the 0..255 range and the three-digit segment cap.
    ld hl, test_ip_max
    call test_copy_ip
    call su_validate_ip
    jp z, test_bad_ip_validator
    ld hl, test_ip_leading_zeroes
    call test_copy_ip
    call su_validate_ip
    jp z, test_bad_ip_validator
    ld hl, test_ip_256
    call test_copy_ip
    call su_validate_ip
    jp nz, test_bad_ip_validator
    ld hl, test_ip_four_digits
    call test_copy_ip
    call su_validate_ip
    jp nz, test_bad_ip_validator
    ld hl, test_ip_short
    call test_copy_ip
    call su_validate_ip
    jp nz, test_bad_ip_validator
    ld hl, test_ip_empty_segment
    call test_copy_ip
    call su_validate_ip
    jp nz, test_bad_ip_validator
    ld hl, test_ip_trailing_dot
    call test_copy_ip
    call su_validate_ip
    jp nz, test_bad_ip_validator

    ; TIME_CONFIG supplies defined rows; private compute-visible entry must
    ; expose all twelve logical rows, including TIME=4, ACTION=10, NAME=11.
    ld hl, 0x03ff
    ld (test_visible_out), hl
    ld a, 1
    ld (_setup_choice + 1), a
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld de, 0x0fff
    or a
    sbc hl, de
    jp nz, test_bad_current_visible

    ld hl, (test_visible_out)
    ld (_setup_visible_mask), hl

    ; CREATE+DIRECT keeps the static IP rendered but never focuses it. With a
    ; valid stored port LINK defines IP/PORT and advances to TIME; PORT stays
    ; reachable, and navigation in either direction skips logical row 2.
    xor a
    ld (_setup_choice), a
    ld a, 1
    ld (_setup_choice + 1), a
    xor a
    ld (_setup_focus_choice), a
    ld (_setup_focus_choice + 1), a
    ld a, 1
    ld (_setup_defined_mask), a
    xor a
    ld (_setup_defined_mask + 1), a
    ld a, 1
    ld (_setup_cursor), a
    ld a, 3
    ld (_setup_visible_mask), a
    xor a
    ld (_setup_visible_mask + 1), a
    ld hl, test_port_default
    ld de, _setup_port_text
    ld bc, 5
    ldir
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_create_direct_port
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_create_direct_port
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    jp nz, test_bad_create_direct_port
    ; Explicit SELECT edits even a valid default port.
    ld a, 3
    ld (_setup_cursor), a
    ld hl, 0x000f
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_default_port
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_default_port
    ld hl, (_edit_buf)
    ld de, _setup_port_text
    or a
    sbc hl, de
    jp nz, test_bad_default_port
    ld a, (_edit_max)
    cp 5
    jp nz, test_bad_default_port
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_port
    ; Endpoint arrows navigate only; they never reopen the editor.
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_port
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_port
    ld a, 4
    ld (_setup_cursor), a
    ld hl, 0x001f
    ld (_setup_visible_mask), hl
    ld a, 0x81
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_create_direct_port

    ; Changing a complete CREATE connection from MQTT to DIRECT must use the
    ; same cursor filter: IP is static, so LINK advances straight to PORT.
    xor a
    ld (_setup_choice), a
    ld (_setup_focus_choice + 1), a
    ld (_setup_room_editing), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_complete_create_direct_port
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_complete_create_direct_port

    ; Selecting DIRECT with an empty port must auto-enter PORT editing. The
    ; editable predicate must return setup_port_text, never stale HL.
    xor a
    ld (_setup_choice), a
    ld (_setup_focus_choice + 1), a
    ld (_setup_port_text), a
    ld (_setup_room_editing), a
    ld (_setup_defined_mask + 1), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_defined_mask), a
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_empty_port_edit
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_empty_port_edit
    ld hl, (_edit_buf)
    ld de, _setup_port_text
    or a
    sbc hl, de
    jp nz, test_bad_empty_port_edit
    ld a, (_edit_max)
    cp 5
    jp nz, test_bad_empty_port_edit
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry

    ; JOIN+DIRECT still focuses editable IP first.
    ld a, 1
    ld (_setup_choice), a
    ld a, 4
    ld (_setup_cursor), a
    ld a, 0x81
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_join_direct_ip
    ld a, 0x81
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_bad_join_direct_ip
    ; Outside the editor, LEFT/RIGHT move between the two endpoint fields.
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_join_direct_endpoint_focus
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_join_direct_endpoint_focus
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_bad_join_direct_endpoint_focus
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_join_direct_endpoint_focus

    ; An invalid JOIN+MQTT room enters editing on SELECT.
    xor a
    ld (_netchesszx_mqtt_code), a
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_invalid_room_edit
    xor a
    ld (_setup_room_editing), a

    ; Explicit SELECT edits even a valid generated room.
    ld hl, test_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    xor a
    ld (_setup_choice), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_bad_default_room
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_default_room
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_room
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_room
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_default_room

    ; JOIN+MQTT enters editing with SPACE, not an arrow, and exposes only the
    ; four hexadecimal characters after the immutable NC prefix. Arrows navigate; only
    ; SPACE/ENTER opens a field.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_edit_buf)
    ld de, _netchesszx_mqtt_code + 2
    or a
    sbc hl, de
    jp nz, test_bad_room_format
    ld a, (_edit_max)
    cp 4
    jp nz, test_bad_room_format
    ld a, 'f'
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    cp 4
    jp nz, test_bad_room_letter
    ld a, (test_last_edit_key)
    cp 'F'
    jp nz, test_bad_room_letter
    ld a, 'G'
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    or a
    jp nz, test_bad_room_letter
    ld a, '7'
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    cp 4
    jp nz, test_bad_room_digit
    xor a
    ld (_netchesszx_mqtt_code + 5), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_NOTICE)
    cp 4
    jp nz, test_bad_room_fifth
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_room_fifth
    ld a, '4'
    ld (_netchesszx_mqtt_code + 5), a
    ld a, '5'
    ld (_netchesszx_mqtt_code + 6), a
    xor a
    ld (_netchesszx_mqtt_code + 7), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_NOTICE)
    cp 4
    jp nz, test_bad_room_fifth_notice
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_room_fifth_edit
    xor a
    ld (_netchesszx_mqtt_code + 6), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_room_confirm

    ; Selecting MQTT for JOIN preserves a valid room and focuses it without
    ; entering the editor.
    ld hl, test_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_focus_choice + 1), a
    xor a
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld (_setup_defined_mask + 1), a
    ld a, 1
    ld (_setup_defined_mask), a
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, _netchesszx_mqtt_code
    ld de, test_room
    ld b, 7
test_preserved_room_loop:
    ld a, (de)
    cp (hl)
    jp nz, test_bad_room_preserve
    inc de
    inc hl
    djnz test_preserved_room_loop
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_bad_room_preserve
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_room_preserve

    ; An invalid stored JOIN room is normalised to NC plus an empty editable
    ; suffix, then editing starts immediately after LINK is selected.
    xor a
    ld (_setup_choice + 1), a
    ld (_netchesszx_mqtt_code), a
    ld (_setup_defined_mask + 1), a
    ld a, 1
    ld (_setup_defined_mask), a
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_netchesszx_mqtt_code)
    ld de, 0x434e
    or a
    sbc hl, de
    jp nz, test_bad_room_init
    ld a, (_netchesszx_mqtt_code + 2)
    or a
    jp nz, test_bad_room_init
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_bad_room_init
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_room_init
    ld hl, (_edit_buf)
    ld de, _netchesszx_mqtt_code + 2
    or a
    sbc hl, de
    jp nz, test_bad_room_init
    ld a, (_edit_max)
    cp 4
    jp nz, test_bad_room_init
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry

    ; CREATE+MQTT maps all seed nibbles into hexadecimal room characters.
    ld hl, 0xabcd
    ld (0x5c78), hl
    xor a
    ld (_setup_focus_choice), a
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, _netchesszx_mqtt_code
    ld de, test_created_room
    ld b, 7
test_created_room_loop:
    ld a, (de)
    cp (hl)
    jp nz, test_bad_room_create
    inc de
    inc hl
    djnz test_created_room_loop

    ; Navigation skips no visible row and reaches ACTION after ten DOWNs.
    ld a, 1
    ld (_setup_choice + 1), a
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_cursor), a
    ; The overlay owns B internally, so keep the ten navigation steps explicit.
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 10
    jp nz, test_bad_current_navigation

    ; TIME transition is the only ordinary navigation path with TIME_UI.
    ld a, 3
    ld (_setup_cursor), a
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_current_time
    ld a, (CTX_FLAGS)
    and 0x12
    cp 0x12
    jp nz, test_bad_current_time
    ld hl, (CTX_FORCE)
    ld de, 0x0018
    or a
    sbc hl, de
    jp nz, test_bad_current_time

    ; Entering ACTION repaints only the old normal row and ACTION itself.
    ld a, 9
    ld (_setup_cursor), a
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    and 0x22
    cp 0x22
    jp nz, test_bad_current_action
    ld hl, (CTX_FORCE)
    ld de, 0x0600
    or a
    sbc hl, de
    jp nz, test_bad_current_action

    ; Confirming HINTS reveals and paints SAVE/START immediately; ACTION's
    ; text belongs to TIME_CONFIG and therefore needs its dedicated UI flag.
    ; GAME setup now rides in the config record, so confirming a row there
    ; also dirties it and SAVE has something to write.
    ld hl, 0x09ff
    ld (_setup_defined_mask), hl
    ld hl, 0x0bff
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_config_dirty), a
    ld a, 9
    ld (_setup_cursor), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 10
    jp nz, test_bad_current_hints_cursor
    ld bc, (_setup_defined_mask)
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld de, 0x0fff
    or a
    sbc hl, de
    jp nz, test_bad_current_hints_visible
    ld a, (CTX_FLAGS)
    and 0x20
    jp z, test_bad_current_hints_action
    ld a, (_setup_config_dirty)
    or a
    jp z, test_bad_current_hints_action

    ; BOARD previews every theme in order, but leaving without SELECT restores
    ; the last confirmed theme. SELECT promotes the preview to confirmed.
    ld a, 7
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_board_theme), a
    ld (_setup_game_focus), a
    ld (_netchesszx_board_theme_index), a
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    cp 1
    jp nz, test_bad_board_preview
    ld a, (CTX_FLAGS)
    and 0x08
    jp z, test_bad_board_preview
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    cp 2
    jp nz, test_bad_board_preview
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    cp 3
    jp nz, test_bad_board_preview
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    cp 4
    jp nz, test_bad_board_preview
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    or a
    jp nz, test_bad_board_preview

    ld a, 3
    ld (_setup_focus_board_theme), a
    ld (_netchesszx_board_theme_index), a
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 8
    jp nz, test_bad_board_restore
    ld a, (_setup_focus_board_theme)
    or a
    jp nz, test_bad_board_restore
    ld a, (CTX_ACTION)
    or a
    jp nz, test_bad_board_restore
    ld a, (CTX_FORCE)
    and 0x80
    jp z, test_bad_board_restore

    ld a, 7
    ld (_setup_cursor), a
    ld a, 4
    ld (_setup_focus_board_theme), a
    ld (_netchesszx_board_theme_index), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_game_focus)
    cp 4
    jp nz, test_bad_board_select

    ; A finished CREATE menu (HINTS defined) that switches to JOIN must keep
    ; GAME SETUP visible. IP/PORT are invalidated; COLOR is JOIN-hidden.
    xor a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    ld (_netchesszx_direct_host), a
    ld a, 1
    ld (_setup_focus_choice), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_setup_defined_mask)
    ld (test_visible_out), hl
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld a, l
    and 0xc0
    cp 0xc0
    jp nz, test_bad_join_keeps_game_setup
    ld a, h
    and 0x03
    cp 0x03
    jp nz, test_bad_join_keeps_game_setup
    ld hl, (test_visible_out)
    ld (_setup_visible_mask), hl

    ; Returning to CREATE reveals COLOR during endpoint refresh. The reducer
    ; marks that newly visible row explicitly for MENU_CONFIG.
    xor a
    ld (_setup_focus_choice), a
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_choice)
    or a
    jp nz, test_bad_create_restores_color
    ld a, (CTX_CLEAR)
    cp 0xfe
    jp nz, test_bad_create_restores_color
    ld hl, (CTX_FORCE)
    ld de, 0x0024
    or a
    sbc hl, de
    jp nz, test_bad_create_restores_color
    jp test_player_name

; NAME (logical row 11) appears once TIME is defined, holds GAME SETUP back
; until it is answered, and sits between TIME and COLOR in navigation.
test_player_name:
    xor a
    ld (_setup_choice), a
    ld (_setup_room_editing), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld hl, 0x001f
    ld (test_visible_out), hl
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld de, 0x081f
    or a
    sbc hl, de
    jp nz, test_bad_player_name_visible
    ld hl, 0x081f
    ld (test_visible_out), hl
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld de, 0x083f
    or a
    sbc hl, de
    jp nz, test_bad_player_name_visible

    ; TIME <-> NAME <-> COLOR, never through logical order.
    ld hl, 0x0bff
    ld (_setup_visible_mask), hl
    ld hl, 0x0bff
    ld (_setup_defined_mask), hl
    ld a, 4
    ld (_setup_cursor), a
    ld a, 1
    call test_step_row
    cp 11
    jp nz, test_bad_player_name_order
    ld (_setup_cursor), a
    ld a, 1
    call test_step_row
    cp 5
    jp nz, test_bad_player_name_order
    ld (_setup_cursor), a
    xor a
    call test_step_row
    cp 11
    jp nz, test_bad_player_name_order
    ld (_setup_cursor), a
    xor a
    call test_step_row
    cp 4
    jp nz, test_bad_player_name_order
    ld a, 10
    ld (_setup_cursor), a
    ld a, 1
    call test_step_row
    cp 10
    jp nz, test_bad_player_name_order
    ; JOIN hides COLOR: NAME steps straight to NOTATION and back.
    ld hl, 0x0bdf
    ld (_setup_visible_mask), hl
    ld a, 11
    ld (_setup_cursor), a
    ld a, 1
    call test_step_row
    cp 6
    jp nz, test_bad_player_name_order
    ld (_setup_cursor), a
    xor a
    call test_step_row
    cp 11
    jp nz, test_bad_player_name_order

    ; SELECT on NAME binds the ten-character editor to the low-RAM name.
    ld hl, test_name_before
    ld de, PLAYER_NAME
    ld bc, 11
    ldir
    ld a, 11
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_player_name_edit
    ld a, (_setup_edit_row)
    cp 11
    jp nz, test_bad_player_name_edit
    ld hl, (_edit_buf)
    ld de, PLAYER_NAME
    or a
    sbc hl, de
    jp nz, test_bad_player_name_edit
    ld a, (_edit_max)
    cp 10
    jp nz, test_bad_player_name_edit
    ; Lower case is folded; punctuation other than '-' and '_' is refused.
    ld a, 'n'
    call test_name_key
    cp 'N'
    jp nz, test_bad_player_name_keys
    ld a, '_'
    call test_name_key
    cp '_'
    jp nz, test_bad_player_name_keys
    ld a, '7'
    call test_name_key
    cp '7'
    jp nz, test_bad_player_name_keys
    ld a, '!'
    call test_name_key
    or a
    jp nz, test_bad_player_name_keys
    ld a, ' '
    ld (test_last_edit_key), a
    ; An empty name is a valid answer: confirm defines NAME and moves on.
    xor a
    ld (PLAYER_NAME), a
    ld hl, 0x001f
    ld (_setup_defined_mask), hl
    ld hl, 0x081f
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_player_name_confirm
    ld a, (_setup_defined_mask + 1)
    and 0x08
    jp z, test_bad_player_name_confirm
    ld a, (_setup_cursor)
    cp 5
    jp nz, test_bad_player_name_confirm
    ld a, (_setup_config_dirty)
    or a
    jp z, test_bad_player_name_confirm
    ; ESC restores the previous name.
    ld hl, test_name_before
    ld de, PLAYER_NAME
    ld bc, 11
    ldir
    ld a, 11
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    xor a
    ld (PLAYER_NAME), a
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (PLAYER_NAME)
    cp 'O'
    jp nz, test_bad_player_name_confirm

    ; JOIN+DIRECT: an accepted IP keeps the valid stored port defined and
    ; continues at TIME instead of walking the user through PORT.
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld (_setup_defined_mask + 1), a
    ld a, 3
    ld (_setup_defined_mask), a
    ld hl, 0x000f
    ld (_setup_visible_mask), hl
    ld hl, test_port_default
    ld de, _setup_port_text
    ld bc, 5
    ldir
    ld hl, test_ip_max
    call test_copy_ip
    ld a, 2
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_join_ip_port
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_join_ip_port
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    jp nz, test_bad_join_ip_port
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_join_ip_port
    ; Without a valid port the accepted IP still stops on PORT.
    xor a
    ld (_setup_port_text), a
    ld (_setup_room_editing), a
    ld a, 3
    ld (_setup_defined_mask), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x04
    jp nz, test_bad_join_ip_port
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_join_ip_port

    ; A GAME change on an unfinished menu keeps only GAME defined and clears
    ; from LINK. The CREATE+DIRECT zero word must not define IP: that
    ; auto-room rule belongs to LINK alone.
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld (_setup_focus_choice + 1), a
    ld (_setup_cursor), a
    ld (_setup_room_editing), a
    ld hl, 0x081f
    ld (_setup_defined_mask), hl
    ld hl, 0x001f
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_setup_defined_mask)
    ld de, 0x0001
    or a
    sbc hl, de
    jp nz, test_bad_game_wipe
    ld a, (CTX_CLEAR)
    cp 1
    jp nz, test_bad_game_wipe
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_bad_game_wipe
    ; The first GAME answer with that zero word clears nothing either.
    xor a
    ld (_setup_choice), a
    ld (_setup_cursor), a
    ld h, a
    ld l, a
    ld (_setup_defined_mask), hl
    inc l
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_setup_defined_mask)
    ld de, 0x0001
    or a
    sbc hl, de
    jp nz, test_bad_game_first
    ld a, (CTX_CLEAR)
    inc a
    jp nz, test_bad_game_first
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_bad_game_first

    ; Re-selecting an unchanged GAME/LINK on a complete menu only steps on:
    ; GAME to LINK, MQTT LINK to ROOM, CREATE+DIRECT LINK past IP to PORT.
    xor a
    ld (_setup_choice), a
    ld (_setup_focus_choice), a
    inc a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice + 1), a
    xor a
    call test_reselect
    jp nz, test_bad_reselect_game
    cp 1
    jp nz, test_bad_reselect_game
    ld de, 0x0003
    call test_force_is
    jp nz, test_bad_reselect_game
    ld a, 1
    call test_reselect
    jp nz, test_bad_reselect_mqtt
    cp 2
    jp nz, test_bad_reselect_mqtt
    ld de, 0x0006
    call test_force_is
    jp nz, test_bad_reselect_mqtt
    xor a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice + 1), a
    inc a
    call test_reselect
    jp nz, test_bad_reselect_direct
    cp 3
    jp nz, test_bad_reselect_direct
    ld de, 0x000a
    call test_force_is
    jp nz, test_bad_reselect_direct

    ; Confirming emptied endpoint text is refused with that field's notice
    ; and reopens the editor on it: ROOM, IP, then PORT.
    ld hl, test_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld a, 2
    ld de, _netchesszx_mqtt_code + 2
    call test_empty_confirm
    jp nz, test_bad_empty_room
    cp 4
    jp nz, test_bad_empty_room
    xor a
    ld (_setup_choice + 1), a
    ld hl, test_ip_max
    call test_copy_ip
    ld a, 2
    ld de, _netchesszx_direct_host
    call test_empty_confirm
    jp nz, test_bad_empty_ip
    cp 3
    jp nz, test_bad_empty_ip
    ld hl, test_port_default
    ld de, _setup_port_text
    ld bc, 5
    ldir
    ld a, 3
    ld de, _setup_port_text
    call test_empty_confirm
    jp nz, test_bad_empty_port
    cp 5
    jp nz, test_bad_empty_port
    xor a
    ld (_setup_room_editing), a
    jp test_store_result
test_bad_join_ip_port:
    ld a, 99
    jp test_store_result
test_bad_game_wipe:
    ld a, 70
    jp test_store_result
test_bad_game_first:
    ld a, 71
    jp test_store_result
test_bad_reselect_game:
    ld a, 72
    jp test_store_result
test_bad_reselect_mqtt:
    ld a, 73
    jp test_store_result
test_bad_reselect_direct:
    ld a, 74
    jp test_store_result
test_bad_empty_room:
    ld a, 75
    jp test_store_result
test_bad_empty_ip:
    ld a, 76
    jp test_store_result
test_bad_empty_port:
    ld a, 77
    jp test_store_result

; A = endpoint cursor, DE = its editable text. SELECT opens the editor on DE,
; then ENTER on emptied text must reopen it there. Returns NZ on any
; structural failure, else A = the notice.
test_empty_confirm:
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    push de
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    pop de
    call test_editing_de
    ret nz
    xor a
    ld (de), a
    push de
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    pop de
    call test_editing_de
    ret nz
    ld a, (CTX_NOTICE)
    ret

; Z when the editor is open on DE. Preserves DE.
test_editing_de:
    ld a, (_setup_room_editing)
    dec a
    ret nz
    ld hl, (_edit_buf)
    or a
    sbc hl, de
    ret

; A = cursor (0 GAME, 1 LINK). SELECT with the focused value already stored
; on a complete menu (ACTION visible). Returns NZ if anything but the cursor
; step and its PAINT|SUPPRESS repaint happened, else A = new cursor.
test_reselect:
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    ld a, 0x5a
    ld (_setup_action_focus), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    cp 0x0a
    ret nz
    ld a, (CTX_CLEAR)
    inc a
    ret nz
    ld a, (_setup_room_editing)
    or a
    ret nz
    ld a, (_setup_action_focus)
    cp 0x5a
    ret nz
    ld de, 0x03ff
    call test_mask_is
    ret nz
    ld a, (_setup_cursor)
    ret

; DE = expected CTX_FORCE / defined mask. Z when equal.
test_force_is:
    ld hl, (CTX_FORCE)
    jr test_hl_is
test_mask_is:
    ld hl, (_setup_defined_mask)
test_hl_is:
    or a
    sbc hl, de
    ret

; A = direction (1 down, 0 up). Exercises the Setup visual-order stepper.
test_step_row:
    jp su_step_row

; A = typed key while editing NAME. Returns the key forwarded to the editor.
test_name_key:
    ld (CTX_KEY), a
    xor a
    ld (test_last_edit_key), a
    call _setup_step_ovl_entry
    ld a, (test_last_edit_key)
    ret

test_bad_player_name_visible:
    ld a, 60
    jp test_store_result
test_bad_player_name_order:
    ld a, 61
    jp test_store_result
test_bad_player_name_edit:
    ld a, 62
    jp test_store_result
test_bad_player_name_keys:
    ld a, 63
    jp test_store_result
test_bad_player_name_confirm:
    ld a, 64
    jp test_store_result

test_bad_current_visible:
    ld a, 21
    jp test_store_result
test_bad_current_navigation:
    ld a, 22
    jp test_store_result
test_bad_current_time:
    ld a, 23
    jp test_store_result
test_bad_current_action:
    ld a, 26
    jp test_store_result
test_bad_current_hints_action:
    ld a, 42
    jp test_store_result
test_bad_current_hints_cursor:
    ld a, 40
    jp test_store_result
test_bad_current_hints_visible:
    ld a, 41
    jp test_store_result
test_bad_default_port:
    ld a, 27
    jp test_store_result
test_bad_default_room:
    ld a, 28
    jp test_store_result
test_bad_invalid_room_edit:
    ld a, 29
    jp test_store_result
test_bad_room_preserve:
    ld a, 43
    jp test_store_result
test_bad_empty_port_edit:
    ld a, 44
    jp test_store_result
test_bad_complete_create_direct_port:
    ld a, 45
    jp test_store_result
test_bad_create_direct_port:
    ld a, 24
    jp test_store_result
test_bad_join_direct_ip:
    ld a, 25
    jp test_store_result
test_bad_join_keeps_game_setup:
    ld a, 46
    jp test_store_result
test_bad_create_restores_color:
    ld a, 47
    jp test_store_result
test_bad_ip_validator:
    ld a, 48
    jp test_store_result

    ; First run: dirty, GAME focused, SAVE active.
    ld a, 0xfc
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_config_dirty)
    cp 1
    jp nz, test_bad_first_run
    ld a, (_setup_cursor)
    or a
    jp nz, test_bad_first_run
    ld hl, (_setup_visible_mask)
    ld de, 0x000f
    or a
    sbc hl, de
    jp nz, test_bad_first_run

    ; Completing CONNECTION SETUP reveals GAME SETUP once, with a full render.
    ld a, 3
    ld (_setup_cursor), a
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_setup_visible_mask)
    ld de, 0x03ff
    or a
    sbc hl, de
    jp nz, test_bad_reveal
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_reveal
    ld a, (CTX_CLEAR)
    cp 0xfe
    jp nz, test_bad_reveal
    ld a, (CTX_FLAGS)
    cp 0x09
    jp nz, test_bad_reveal

    ; Dirty action invokes SAVE.
    ld a, 9
    ld (_setup_cursor), a
    xor a
    ld (_setup_action_focus), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 4
    jp nz, test_bad_save

    ; Saving is optional: RIGHT reaches START while dirty and invokes it.
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_action_focus)
    cp 1
    jp nz, test_bad_dirty_start
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 3
    jp nz, test_bad_dirty_start

    ; DOWN on the action row is inert; EDIT is the explicit return to GAME.
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 9
    jp nz, test_bad_action_wrap

    ; Loaded/saved config focuses START and invokes it directly.
    ld a, 0xfe
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_clean
    ld a, (_setup_cursor)
    cp 9
    jp nz, test_bad_clean
    ld a, (_setup_action_focus)
    cp 1
    jp nz, test_bad_clean
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 3
    jp nz, test_bad_start

    ; EDIT returns to GAME without making the configuration dirty.
    xor a
    ld (_setup_action_focus), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    or a
    jp nz, test_bad_edit
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_edit

    ; BOARD navigation previews only. Leaving restores the confirmed theme;
    ; SELECT confirms the per-game theme without dirtying connection config.
    ld a, 6
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_board_theme), a
    ld (_netchesszx_board_theme_index), a
    ld (_setup_config_dirty), a
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_netchesszx_board_theme_index)
    cp 1
    jp nz, test_bad_board_preview
    ld a, (_setup_focus_board_theme)
    or a
    jp nz, test_bad_board_preview
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_board_preview
    ld a, (CTX_FLAGS)
    cp 0x0a
    jp nz, test_bad_board_preview
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 7
    jp nz, test_bad_board_restore
    ld a, (_netchesszx_board_theme_index)
    or a
    jp nz, test_bad_board_restore
    ld a, 6
    ld (_setup_cursor), a
    ld a, 1
    ld (_netchesszx_board_theme_index), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_focus_board_theme)
    cp 1
    jp nz, test_bad_board_select
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_board_select

IFNDEF NETCHESSZX_NEXT
    ; Classic can select RTC just like Next; probing happens after Setup.
    ld a, 3
    ld (_setup_cursor), a
    xor a
    ld (_setup_time_focus), a
    ld (_setup_focus_choice + 6), a
    ld (_setup_room_editing), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_classic_time
    ld a, (_setup_focus_choice + 6)
    cp 1
    jp nz, test_bad_classic_time
    ld a, (CTX_FLAGS)
    cp 0x09
    jp nz, test_bad_classic_time
    xor a
    ld (_setup_focus_choice + 6), a
    ld (_setup_config_dirty), a
ENDIF

    ; Three timezone digits must not wrap the 8-bit accumulator into range.
    ld hl, test_timezone_overflow
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, 3
    ld (_setup_edit_row), a
    ld a, 1
    ld (_setup_room_editing), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_timezone_overflow
    ld a, (CTX_NOTICE)
    cp 4
    jp nz, test_bad_timezone_overflow
    xor a
    ld (_setup_room_editing), a

    ; Decimal port editing rejects 16-bit overflow and accepts 65535.
    xor a
    ld (_setup_focus_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld (_setup_edit_row), a
    ld a, 1
    ld (_setup_room_editing), a
    ld hl, test_port_overflow
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_port_overflow
    ld hl, test_port_max
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_port_max
    ld a, 0xfd
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (_netchesszx_direct_port)
    ld de, 0xffff
    or a
    sbc hl, de
    jp nz, test_bad_port_max

    ; Per-game binary choices never dirty the connection configuration.
    xor a
    ld (_setup_config_dirty), a
    ld (_setup_focus_choice + 2), a
    ld a, 4
    ld (_setup_cursor), a
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (CTX_FORCE)
    ld de, 0
    or a
    sbc hl, de
    jp nz, test_bad_dirty_mask
    ld a, (CTX_FLAGS)
    cp 0x0a
    jp nz, test_bad_dirty_mask
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (CTX_FORCE)
    ld de, 0
    or a
    sbc hl, de
    jp nz, test_bad_dirty_mask
    ld a, (CTX_FLAGS)
    cp 0x0a
    jp nz, test_bad_dirty_mask
    xor a
    ld (_setup_cursor), a
    ld (_setup_game_focus), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, (CTX_FORCE)
    ld de, 0x0207
    or a
    sbc hl, de
    jp nz, test_bad_dirty_mask

    ; START rejects an empty JOIN+DIRECT host even if IP was never edited.
    ld a, 1
    ld (_setup_focus_choice), a
    xor a
    ld (_setup_focus_choice + 1), a
    ld (_netchesszx_direct_host), a
    ld a, 9
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_action_focus), a
    ld a, 32
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_ACTION)
    or a
    jp nz, test_bad_start_ip
    ld a, (CTX_NOTICE)
    cp 3
    jp nz, test_bad_start_ip

    ; Repeating an already-selected horizontal focus is a true no-op.
    xor a
    ld (_setup_cursor), a
    ld (_setup_game_focus), a
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (CTX_FLAGS)
    or a
    jp nz, test_bad_focus_repeat
    xor a
    jr test_store_result

test_bad_first_run:
    ld a, 3
    jr test_store_result
test_bad_save:
    ld a, 4
    jr test_store_result
test_bad_clean:
    ld a, 5
    jr test_store_result
test_bad_start:
    ld a, 6
    jr test_store_result
test_bad_edit:
    ld a, 7
    jr test_store_result
test_bad_port_overflow:
    ld a, 8
    jr test_store_result
test_bad_port_max:
    ld a, 9
    jr test_store_result
test_bad_dirty_mask:
    ld a, 10
    jr test_store_result
test_bad_dirty_start:
    ld a, 11
    jr test_store_result
test_bad_action_wrap:
    ld a, 12
    jr test_store_result
test_bad_board_preview:
    ld a, 13
    jr test_store_result
test_bad_board_restore:
    ld a, 14
    jr test_store_result
test_bad_board_select:
    ld a, 15
    jr test_store_result
test_bad_reveal:
    ld a, 16
    jr test_store_result
test_bad_classic_time:
    ld a, 17
    jr test_store_result
test_bad_timezone_overflow:
    ld a, 18
    jr test_store_result
test_bad_start_ip:
    ld a, 19
    jr test_store_result
test_bad_focus_repeat:
    ld a, 20
    jr test_store_result
test_bad_room_format:
    ld a, 30
    jr test_store_result
test_bad_room_init:
    ld a, 31
    jr test_store_result
test_bad_room_letter:
    ld a, 32
    jr test_store_result
test_bad_room_digit:
    ld a, 33
    jr test_store_result
test_bad_room_partial:
    ld a, 34
    jr test_store_result
test_bad_room_confirm:
    ld a, 35
    jr test_store_result
test_bad_room_create:
    ld a, 36
    jr test_store_result
test_bad_room_fifth:
    ld a, 37
    jr test_store_result
test_bad_room_fifth_notice:
    ld a, 38
    jr test_store_result
test_bad_room_fifth_edit:
    ld a, 39
    jr test_store_result
test_bad_join_direct_endpoint_focus:
    ld a, 40
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_result:
    defb 0xff
test_port_overflow:
    defb "65536",0
test_port_max:
    defb "65535",0
test_port_default:
    defb "1883",0
test_timezone_overflow:
    defb "260",0
test_room:
    defb "NC1234",0
test_created_room:
    defb "NCABCD",0
test_ip_max:
    defb "255.255.255.255",0
test_ip_leading_zeroes:
    defb "001.002.003.004",0
test_ip_256:
    defb "256.0.0.1",0
test_ip_four_digits:
    defb "0000.0.0.1",0
test_ip_short:
    defb "1.2.3",0
test_ip_empty_segment:
    defb "1..2.3",0
test_ip_trailing_dot:
    defb "1.2.3.4.",0

test_copy_ip:
    ld de, _netchesszx_direct_host
test_copy_ip_loop:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, test_copy_ip_loop
    ret

test_name_before:
    defb "OLD", 0, 0, 0, 0, 0, 0, 0, 0

_setup_choice:
    defs 7, 0
_setup_focus_choice:
    defs 8, 0
_setup_focus_board_theme:
    defb 0
_setup_defined_mask:
    defw 0
_setup_visible_mask:
    defw 0
_setup_cursor:
    defb 0
_setup_room_editing:
    defb 0
_setup_edit_row:
    defb 0
_setup_port_text:
    defs 8, 0
_setup_timezone_text:
    defs 4, 0
_setup_timezone_value:
    defb 0
_setup_config_dirty:
    defb 0
_setup_game_focus:
    defb 0
_setup_time_focus:
    defb 0
_setup_action_focus:
    defb 0
_setup_edit_was_dirty:
    defb 0
_setup_edit_backup:
    defs 17, 0
_netchesszx_mqtt_code:
    defs 8, 0
_netchesszx_direct_host:
    defs 18, 0
_netchesszx_direct_port:
    defw 1883
_netchesszx_session_role:
    defb 0
_netchesszx_transport:
    defb 1
_netchesszx_local_color:
    defb 0
_netchesszx_host_color:
    defb 0
_netchesszx_host_color_ready:
    defb 1
_netchesszx_notation:
    defb 0
_netchesszx_movement_hints:
    defb 0
_netchesszx_board_theme_index:
    defb 0
_netchesszx_piece_set_index:
    defb 0
_netchesszx_timezone:
    defb 0
_netchesszx_timezone_last:
    defb 0

_netchesszx_session_send_ack_text:
    ret

_NETCHESS_PROTO_ACK_PREFIX:
    defb "ACK ",0
_NETCHESS_PROTO_NACK_PREFIX:
    defb "NACK ",0
_line_buf:
    defs 128, 0
_spectrum_net_payload_scratch:
    defs 128, 0

test_visible_out:
    defw 0
_edit_buf:
    defw 0
_edit_max:
    defb 0
test_last_edit_key:
    defb 0
_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    ret
_spectrum_gui_edit_key:
    ld a, l
    ld (test_last_edit_key), a
    ret
_spectrum_gui_edit_hide:
    ret
