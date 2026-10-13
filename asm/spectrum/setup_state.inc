; Setup interpretation of the phase-shared arena. Raw aliases are local to
; this UI ASM unit; only the Setup overlays receive generated imports.
EXTERN _app_workspace
    defc _setup_choice = _app_workspace
    defc _setup_focus_choice = _app_workspace + 7
    defc _setup_focus_board_theme = _app_workspace + 14
    defc _setup_defined_mask = _app_workspace + 15
    defc _setup_visible_mask = _app_workspace + 17
    defc _setup_cursor = _app_workspace + 19
    defc _setup_room_editing = _app_workspace + 20
    defc _setup_edit_row = _app_workspace + 21
    defc _setup_port_text = _app_workspace + 22
    defc _setup_timezone_text = _app_workspace + 28
    defc _setup_timezone_value = _app_workspace + 32
    defc _setup_config_dirty = _app_workspace + 33
    defc _setup_game_focus = _app_workspace + 34
    defc _setup_time_focus = _app_workspace + 35
    defc _setup_action_focus = _app_workspace + 36
    defc _setup_edit_was_dirty = _app_workspace + 37
    defc _setup_edit_backup = _app_workspace + 38
PUBLIC _spectrum_setup_view
defc _spectrum_setup_view = _app_workspace

; Fastcall HL mask. No paging, allocation or additional mutable storage.
PUBLIC _spectrum_setup_set_visible
_spectrum_setup_set_visible:
    ld (_setup_visible_mask), hl
    ret

; Commit a successfully loaded SET and reveal HINTS, preserving the old order.
PUBLIC _spectrum_setup_accept_set
_spectrum_setup_accept_set:
    ld a, (_setup_focus_choice + 5)
    ld hl, _setup_choice + 5
    cp (hl)
    ld (hl), a
    jr z, setup_accept_set_same
    ; A different SET is an unsaved change like any other row: EDIT -> SAVE.
    ld a, 1
    ld (_setup_config_dirty), a
setup_accept_set_same:
    ld hl, _setup_defined_mask + 1
    set 0, (hl)
    xor a
    ld (_setup_action_focus), a
    ld a, 9
    ld (_setup_cursor), a
    ret
