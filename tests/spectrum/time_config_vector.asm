SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _setup_choice
PUBLIC _setup_focus_choice
PUBLIC _setup_focus_board_theme
PUBLIC _setup_game_focus
PUBLIC _setup_defined_mask
PUBLIC _setup_visible_mask
PUBLIC _setup_cursor
PUBLIC _setup_room_editing
PUBLIC _setup_edit_row
PUBLIC _setup_timezone_text
PUBLIC _setup_timezone_value
PUBLIC _setup_config_dirty
PUBLIC _setup_time_focus
PUBLIC _setup_action_focus
PUBLIC _setup_port_text
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
PUBLIC _netchesszx_rtc_available
PUBLIC _netchesszx_direct_port
PUBLIC _edit_buf
PUBLIC _edit_max
PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key
PUBLIC _spectrum_gui_edit_hide
PUBLIC _spectrum_gui_edit_show
PUBLIC _spectrum_append_u16
PUBLIC _spectrum_info_line

EXTERN _time_config_step_ovl_entry
EXTERN _time_config_init_ovl_entry
EXTERN _time_config_ui_ovl_entry

CTX_KEY    EQU 0x5fe0
CTX_FORCE  EQU 0x5fe1
CTX_ACTION EQU 0x5fe4
CTX_NOTICE EQU 0x5fe5
CTX_FLAGS  EQU 0x5fe6
ROW_TIME   EQU 4

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a

    ; SAVE success reinitialises a complete menu as clean EDIT/START with
    ; START focused. The app restores visible_mask before invoking this UI.
    ld a, 3
    ld (CTX_KEY), a
    call _time_config_init_ovl_entry
    ld hl, (_setup_defined_mask)
    ld de, 0x07ff
    or a
    sbc hl, de
    jp nz, test_bad_save_success
    ld hl, (_setup_visible_mask)
    ld a, h
    or l
    jp nz, test_bad_save_success
    ld a, (_setup_cursor)
    cp 10
    jp nz, test_bad_save_success
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_save_success
    ld a, (_setup_action_focus)
    cp 1
    jp nz, test_bad_save_success
    ld a, (CTX_FLAGS)
    cp 0x30
    jp nz, test_bad_save_success
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 0x20
    ld (CTX_FLAGS), a
    call _time_config_ui_ovl_entry
    ld hl, (last_info_line)
    ld de, 15
    add hl, de
    ld a, (hl)
    cp 'E'
    jp nz, test_bad_save_success
    ld a, (0x5a9d)
    cp 0x38
    jp nz, test_bad_save_success

    ; SAVE failure reinitialises the same complete menu as dirty SAVE/START
    ; with SAVE focused, so retry and START remain available.
    ld a, 2
    ld (CTX_KEY), a
    call _time_config_init_ovl_entry
    ld hl, (_setup_defined_mask)
    ld de, 0x07ff
    or a
    sbc hl, de
    jp nz, test_bad_save_failure
    ld a, (_setup_cursor)
    cp 10
    jp nz, test_bad_save_failure
    ld a, (_setup_config_dirty)
    cp 1
    jp nz, test_bad_save_failure
    ld a, (_setup_action_focus)
    or a
    jp nz, test_bad_save_failure
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 0x20
    ld (CTX_FLAGS), a
    call _time_config_ui_ovl_entry
    ld hl, (last_info_line)
    ld de, 15
    add hl, de
    ld a, (hl)
    cp 'S'
    jp nz, test_bad_save_failure
    ld a, (0x5a99)
    cp 0x38
    jp nz, test_bad_save_failure
    ld a, 32
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 4
    jp nz, test_bad_save_action

    ; Clean complete state invokes START directly; choosing EDIT returns to
    ; GAME without dirtying the saved configuration.
    ld a, 3
    ld (CTX_KEY), a
    call _time_config_init_ovl_entry
    ld a, 32
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 3
    jp nz, test_bad_save_action
    xor a
    ld (_setup_action_focus), a
    ld a, 32
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_cursor)
    or a
    jp nz, test_bad_save_action
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_bad_save_action
    ld a, (CTX_FORCE)
    cp 1
    jp nz, test_bad_save_action
    ld a, (CTX_FLAGS)
    cp 0x2a
    jp nz, test_bad_save_action

    ; TIME draws the marker beside the focused source and moves it without
    ; replacing the existing BRIGHT focus attributes.
    ld hl, test_timezone_valid
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, ROW_TIME
    ld (_setup_cursor), a
    ld hl, 0x07ff
    ld (_setup_visible_mask), hl
    ld a, 1
    ld (_netchesszx_rtc_available), a
    xor a
    ld (_setup_time_focus), a
    ld a, 0x10
    ld (CTX_FLAGS), a
    call _time_config_ui_ovl_entry
    ld hl, (last_info_line)
    ld de, 6
    add hl, de
    ld a, (hl)
    cp 92
    jp nz, test_bad_time_marker
    ld de, 10
    add hl, de
    ld a, (hl)
    cp ' '
    jp nz, test_bad_time_marker
    ld a, 1
    ld (_setup_time_focus), a
    call _time_config_ui_ovl_entry
    ld hl, (last_info_line)
    ld de, 6
    add hl, de
    ld a, (hl)
    cp ' '
    jp nz, test_bad_time_marker
    ld de, 10
    add hl, de
    ld a, (hl)
    cp 92
    jp nz, test_bad_time_marker

    ; ENTER on a valid displayed UTC value opens it for editing. This must
    ; work even when RTC is available and UTC is the selected fallback.
    ld hl, test_timezone_valid
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, ROW_TIME
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_time_focus), a
    ld a, 1
    ld (_netchesszx_rtc_available), a
    xor a
    ld (_setup_room_editing), a
    ; Arrows switch TIME values without opening the editor.
    ld a, 0x83
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_time_focus)
    or a
    jp nz, test_bad_accept
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_accept
    ld a, 0x84
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_time_focus)
    cp 1
    jp nz, test_bad_accept
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_accept
    ld a, 13
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_accept
    ld a, (_setup_cursor)
    cp ROW_TIME
    jp nz, test_bad_accept
    ld a, (_setup_edit_row)
    cp ROW_TIME
    jp nz, test_bad_accept
    ld a, 13
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (CTX_ACTION)
    cp 5
    jp nz, test_bad_timezone_action

    ; An invalid displayed UTC value still opens the editor.
    ld hl, test_timezone_invalid
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, ROW_TIME
    ld (_setup_cursor), a
    xor a
    ld (_netchesszx_rtc_available), a
    ld (_setup_room_editing), a
    ld a, 32
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_bad_invalid
    xor a
    ld (_setup_room_editing), a

    ; Outside editing, ordinary characters cannot open the TIME editor.
    ld hl, test_timezone_valid
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, ROW_TIME
    ld (_setup_cursor), a
    ld a, '-'
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_direct_edit
    ld a, (_setup_timezone_text)
    cp '+'
    jp nz, test_bad_direct_edit

    ; Digits are ignored too; SPACE/ENTER is the only entry to editing.
    xor a
    ld (edit_key_seen), a
    ld a, '7'
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_direct_edit
    ld a, (edit_key_seen)
    or a
    jp nz, test_bad_direct_edit

    ; With RTC focused, typing is ignored and cannot open UTC editing.
    xor a
    ld (_setup_room_editing), a
    ld (_setup_time_focus), a
    ld a, 1
    ld (_netchesszx_rtc_available), a
    ld a, '8'
    ld (CTX_KEY), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_rtc_typing
    ld a, (CTX_FLAGS)
    or a
    jp nz, test_bad_rtc_typing

    xor a
    jr test_store_result

test_bad_accept:      ld a, 1
    jr test_store_result
test_bad_invalid:     ld a, 2
    jr test_store_result
test_bad_direct_edit: ld a, 3
    jr test_store_result
test_bad_rtc_typing:  ld a, 4
    jr test_store_result
test_bad_save_success: ld a, 5
    jr test_store_result
test_bad_save_failure: ld a, 6
    jr test_store_result
test_bad_save_action:  ld a, 7
    jr test_store_result
test_bad_timezone_action: ld a, 8
    jr test_store_result
test_bad_time_marker: ld a, 9
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    ret
_spectrum_gui_edit_key:
    ld a, l
    ld (edit_key_seen), a
    ld l, 1
    ret
_spectrum_gui_edit_hide:
_spectrum_gui_edit_show:
    ret
_spectrum_append_u16:
    pop af
    pop de
    pop hl
    push af
    ret
_spectrum_info_line:
    ld (last_info_line), hl
    ret

test_timezone_valid:   DEFB "+02",0
test_timezone_invalid: DEFB "+14",0
test_result: DEFB 0xff
edit_key_seen: DEFB 0
last_info_line: DEFW 0
_setup_choice: DEFS 7,0
_setup_focus_choice: DEFS 7,0
_setup_focus_board_theme: DEFB 0
_setup_game_focus: DEFB 0
_setup_defined_mask: DEFW 0
_setup_visible_mask: DEFW 0x07ff
_setup_cursor: DEFB 0
_setup_room_editing: DEFB 0
_setup_edit_row: DEFB 0
_setup_timezone_text: DEFS 4,0
_setup_timezone_value: DEFB 0
_setup_config_dirty: DEFB 0
_setup_time_focus: DEFB 0
_setup_action_focus: DEFB 0
_setup_port_text: DEFS 6,0
_netchesszx_session_role: DEFB 0
_netchesszx_transport: DEFB 0
_netchesszx_local_color: DEFB 0
_netchesszx_host_color: DEFB 0
_netchesszx_host_color_ready: DEFB 0
_netchesszx_notation: DEFB 0
_netchesszx_movement_hints: DEFB 0
_netchesszx_board_theme_index: DEFB 0
_netchesszx_piece_set_index: DEFB 0
_netchesszx_timezone: DEFB 0
_netchesszx_timezone_last: DEFB 0
_netchesszx_rtc_available: DEFB 0
_netchesszx_direct_port: DEFW 5000
_edit_buf: DEFW 0
_edit_max: DEFB 0
