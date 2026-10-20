SECTION code_user

; Spectranext Setup MATCH contract (assembled with NETCHESSZX_SPECTRANEXT):
; LINK cycles MQTT/DIRECT/MATCH, MATCH stores DIRECT plus the low-RAM flag,
; answers the endpoint rows itself and never focuses them. Without
; matchmaking firmware (LINK_MATCH_OK 0) LINK cycles MQTT and DIRECT only.

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
PUBLIC _setup_config_dirty
PUBLIC _setup_game_focus
PUBLIC _setup_action_focus
PUBLIC _setup_edit_backup
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host
PUBLIC _netchesszx_direct_port
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _netchesszx_session_send_ack_text
PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key
PUBLIC _spectrum_gui_edit_hide
PUBLIC _edit_buf
PUBLIC _edit_max

EXTERN _setup_step_ovl_entry
EXTERN _setup_compute_visible_ovl_entry

INCLUDE "asm/app/link_match_layout.inc"

CTX_KEY EQU 0x5fe0
CTX_CLEAR EQU 0x5fe3
CTX_NOTICE EQU 0x5fe5

KEY_UP EQU 0x81
KEY_LEFT EQU 0x83
KEY_RIGHT EQU 0x84

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, test_port_default
    ld de, _setup_port_text
    ld bc, 5
    ldir
    ld a, 1
    ld (LINK_MATCH_OK), a

    ; LINK arrows cycle three options in screen order; GAME still toggles.
    ld a, 1
    ld (_setup_cursor), a
    ld a, LINK_FOCUS_MATCH
    ld (LINK_FOCUS), a
    ld a, KEY_RIGHT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_MQTT
    jp nz, test_bad_cycle
    ld a, KEY_LEFT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_MATCH
    jp nz, test_bad_cycle
    ld a, KEY_LEFT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_DIRECT
    jp nz, test_bad_cycle
    xor a
    ld (_setup_cursor), a
    ld (_setup_focus_choice), a
    ld a, KEY_RIGHT
    call test_step
    ld a, (_setup_focus_choice)
    cp 1
    jp nz, test_bad_cycle
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_DIRECT
    jp nz, test_bad_cycle

    ; CREATE, first LINK answer MATCH over a stale MQTT choice: DIRECT plus
    ; the flag, both endpoint rows answered, cursor straight to TIME.
    xor a
    ld (_setup_choice), a
    ld (LINK_MATCH), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld a, LINK_FOCUS_MATCH
    call test_select_link_fresh
    ld a, (LINK_MATCH)
    cp 1
    jp nz, test_bad_create_match
    ld a, (_setup_choice + 1)
    or a
    jp nz, test_bad_create_match
    ld a, (_setup_defined_mask)
    and 0x0e
    cp 0x0e
    jp nz, test_bad_create_match
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_create_match
    ld a, (_setup_room_editing)
    or a
    jp nz, test_bad_create_match

    ; Navigation never focuses the fixed endpoint line: TIME up is LINK.
    ld hl, 0x001f
    ld (_setup_visible_mask), hl
    ld a, KEY_UP
    call test_step
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_bad_nav

    ; JOIN+MATCH needs no IP either.
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_netchesszx_direct_host), a
    ld (LINK_MATCH), a
    ld a, LINK_FOCUS_MATCH
    call test_select_link_fresh
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_join_match
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    jp nz, test_bad_join_match

    ; A complete JOIN+MATCH menu reveals ACTION with an empty IP; the same
    ; record as plain DIRECT does not.
    ld a, 1
    ld (LINK_MATCH), a
    ld bc, 0x03df
    call test_visible
    ld a, h
    and 0x04
    jp z, test_bad_join_action
    xor a
    ld (LINK_MATCH), a
    ld bc, 0x03df
    call test_visible
    ld a, h
    and 0x04
    jp nz, test_bad_join_action

    ; CREATE MATCH -> DIRECT changes only the flag. It must still count as
    ; a change: DIRECT re-answers IP/PORT from the stored port and goes to
    ; TIME (an unchanged LINK would step to PORT instead).
    xor a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld a, 1
    ld (LINK_MATCH), a
    ld (_setup_cursor), a
    ld hl, 0x000f
    ld (_setup_defined_mask), hl
    ld hl, 0x001f
    ld (_setup_visible_mask), hl
    ld a, LINK_FOCUS_DIRECT
    ld (LINK_FOCUS), a
    ld a, 32
    call test_step
    ld a, (LINK_MATCH)
    or a
    jp nz, test_bad_flag_change
    ld a, (CTX_CLEAR)
    cp 2
    jp nz, test_bad_flag_change
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_flag_change

    ; Complete JOIN menu, MQTT -> MATCH: the endpoint stays answered without
    ; an IP and the cursor skips it.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    xor a
    ld (LINK_MATCH), a
    ld (_netchesszx_direct_host), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld hl, 0x0fff
    ld (_setup_visible_mask), hl
    ld a, LINK_FOCUS_MATCH
    ld (LINK_FOCUS), a
    ld a, 32
    call test_step
    ld a, (LINK_MATCH)
    cp 1
    jp nz, test_bad_complete_match
    ld a, (_setup_defined_mask)
    and 0x0c
    cp 0x0c
    jp nz, test_bad_complete_match
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_bad_complete_match

    ; MATCH -> MQTT clears the flag and stores MQTT.
    ld a, 1
    ld (_setup_cursor), a
    ld a, LINK_FOCUS_MQTT
    ld (LINK_FOCUS), a
    ld a, 32
    call test_step
    ld a, (LINK_MATCH)
    or a
    jp nz, test_bad_mqtt
    ld a, (_setup_choice + 1)
    cp 1
    jp nz, test_bad_mqtt

    ; Firmware without matchmaking: either arrow alternates MQTT and DIRECT.
    xor a
    ld (LINK_MATCH_OK), a
    ld (_setup_room_editing), a
    inc a
    ld (_setup_cursor), a
    ld a, LINK_FOCUS_DIRECT
    ld (LINK_FOCUS), a
    ld a, KEY_RIGHT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_MQTT
    jp nz, test_bad_no_match
    ld a, KEY_RIGHT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_DIRECT
    jp nz, test_bad_no_match
    ld a, KEY_LEFT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_MQTT
    jp nz, test_bad_no_match
    ld a, KEY_LEFT
    call test_step
    ld a, (LINK_FOCUS)
    cp LINK_FOCUS_DIRECT
    jp nz, test_bad_no_match
    ld a, (CTX_NOTICE)
    or a
    jp nz, test_bad_no_match

    ; Accepting unchanged GAME enters LINK; accepting LINK leaves it.
    ; The LINK painter owns its warning; leaving restores the normal hint.
    xor a
    ld (_setup_cursor), a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld hl, 0x0fff
    ld (_setup_visible_mask), hl
    ld a, 13
    call test_step
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_bad_no_match
    ld a, (CTX_NOTICE)
    or a
    jp nz, test_bad_no_match
    ld a, 32
    call test_step
    ld a, (_setup_cursor)
    cp 1
    jp z, test_bad_no_match
    ld a, (CTX_NOTICE)
    cp 1
    jp nz, test_bad_no_match

    ; Changing GAME also enters LINK: its generic SELECT must not overwrite
    ; the firmware notice after the menu renderer has drawn it.
    xor a
    ld (_setup_cursor), a
    inc a
    ld (_setup_focus_choice), a
    ld a, 13
    call test_step
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_bad_no_match
    ld a, (CTX_NOTICE)
    or a
    jp nz, test_bad_no_match

    xor a
    jr test_store_result

; A = LINK focus. Fresh menu with GAME answered, cursor on LINK, then SELECT.
test_select_link_fresh:
    ld (LINK_FOCUS), a
    ld hl, 0x0001
    ld (_setup_defined_mask), hl
    ld hl, 0x0003
    ld (_setup_visible_mask), hl
    ld a, 1
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    ld a, 32
; A = key.
test_step:
    ld (CTX_KEY), a
    jp _setup_step_ovl_entry

; BC = defined mask. Returns HL = visible mask.
test_visible:
    ld (test_visible_out), bc
    ld de, test_visible_out
    call _setup_compute_visible_ovl_entry
    ld hl, (test_visible_out)
    ret

test_bad_cycle:
    ld a, 1
    jr test_store_result
test_bad_create_match:
    ld a, 2
    jr test_store_result
test_bad_nav:
    ld a, 3
    jr test_store_result
test_bad_join_match:
    ld a, 4
    jr test_store_result
test_bad_join_action:
    ld a, 5
    jr test_store_result
test_bad_flag_change:
    ld a, 6
    jr test_store_result
test_bad_complete_match:
    ld a, 7
    jr test_store_result
test_bad_mqtt:
    ld a, 8
    jr test_store_result
test_bad_no_match:
    ld a, 9
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_result:
    defb 0xff
test_port_default:
    defb "5000", 0
test_visible_out:
    defw 0

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
_setup_config_dirty:
    defb 0
_setup_game_focus:
    defb 0
_setup_action_focus:
    defb 0
_setup_edit_backup:
    defs 17, 0
_netchesszx_mqtt_code:
    defs 8, 0
_netchesszx_direct_host:
    defs 18, 0
_netchesszx_direct_port:
    defw 5000

_netchesszx_session_send_ack_text:
    ret
_NETCHESS_PROTO_ACK_PREFIX:
    defb "ACK ", 0
_NETCHESS_PROTO_NACK_PREFIX:
    defb "NACK ", 0
_line_buf:
    defs 128, 0
_spectrum_net_payload_scratch:
    defs 128, 0

_edit_buf:
    defw 0
_edit_max:
    defb 0
_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    ret
_spectrum_gui_edit_key:
    ret
_spectrum_gui_edit_hide:
    ret
