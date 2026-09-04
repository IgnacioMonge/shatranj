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
PUBLIC _spectrum_net_send_text

EXTERN _setup_step_ovl_entry
EXTERN _setup_compute_visible_ovl_entry
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

    ; TIME_CONFIG supplies defined rows; private compute-visible entry must
    ; expose all eleven logical rows, including TIME=4 and ACTION=10.
    ld hl, 0x03ff
    ld (test_visible_out), hl
    ld a, 1
    ld (_setup_choice + 1), a
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ld de, 0x07ff
    or a
    sbc hl, de
    jp nz, test_bad_current_visible

    ld hl, (test_visible_out)
    ld (_setup_visible_mask), hl

    ; CREATE+DIRECT keeps the static IP rendered but never focuses it: LINK
    ; advances to PORT, and navigation in either direction skips logical row 2.
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
    cp 3
    jp nz, test_bad_create_direct_port
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_create_direct_port
    ; Explicit SELECT edits even a valid default port.
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
    ld hl, 0x01ff
    ld (_setup_defined_mask), hl
    ld hl, 0x03ff
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
    ld de, 0x07ff
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
    xor a
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

_spectrum_net_send_text:
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
