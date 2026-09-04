SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _gui_log_add_move_ovl
PUBLIC _gui_log_add_chat_ovl
PUBLIC _gui_log_remove_last_move_ovl
PUBLIC _spectrum_gui_notify
PUBLIC _spectrum_gui_notify_persistent
PUBLIC _spectrum_gui_notify_success

EXTERN _gui_log_notify_msg_ovl_entry

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    xor a
    ld (test_id), a
    ld (test_context + 1), a
    ld hl, expected_messages
    ld (expected_ptr), hl

test_message_loop:
    ld a, (test_id)
    cp 46
    jr z, test_kinds
    ld (test_context), a
    xor a
    ld (notify_calls), a
    ld hl, test_context
    call _gui_log_notify_msg_ovl_entry
    ld a, l
    cp 1
    jp nz, test_bad_return
    ld a, (notify_calls)
    cp 1
    jp nz, test_bad_call
    ld a, (notify_kind)
    or a
    jp nz, test_bad_kind
    ld hl, (notify_ptr)
    ld de, (expected_ptr)
test_compare_message:
    ld a, (de)
    cp (hl)
    jp nz, test_bad_text
    inc de
    inc hl
    or a
    jr nz, test_compare_message
    ld (expected_ptr), de
    ld hl, test_id
    inc (hl)
    jr test_message_loop

test_kinds:
    ld a, 1
    ld (test_kind), a
test_kind_loop:
    ld (test_context + 1), a
    xor a
    ld (notify_calls), a
    ld hl, test_context
    call _gui_log_notify_msg_ovl_entry
    ld a, l
    cp 1
    jp nz, test_bad_return
    ld a, (notify_calls)
    cp 1
    jp nz, test_bad_call
    ld a, (test_kind)
    ld b, a
    ld a, (notify_kind)
    cp b
    jp nz, test_bad_kind
    ld a, (test_kind)
    inc a
    ld (test_kind), a
    cp 4
    jr c, test_kind_loop

    ld a, 46
    ld (test_context), a
    xor a
    ld (test_context + 1), a
    ld (notify_calls), a
    ld hl, test_context
    call _gui_log_notify_msg_ovl_entry
    ld a, l
    or a
    jp nz, test_bad_invalid
    ld a, (notify_calls)
    or a
    jp nz, test_bad_invalid_call
    xor a
    jr test_store_result

test_bad_return:
    ld a, 1
    jr test_store_result
test_bad_call:
    ld a, 2
    jr test_store_result
test_bad_kind:
    ld a, 3
    jr test_store_result
test_bad_text:
    ld a, 4
    jr test_store_result
test_bad_invalid:
    ld a, 5
    jr test_store_result
test_bad_invalid_call:
    ld a, 6
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

_spectrum_gui_notify:
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld a, (hl)
    ex de, hl
    jr notify_record

_spectrum_gui_notify_persistent:
    ld a, 2
    jr notify_record

_spectrum_gui_notify_success:
    ld a, 3
notify_record:
    ld (notify_ptr), hl
    ld (notify_kind), a
    ld hl, notify_calls
    inc (hl)
    ret

_gui_log_add_move_ovl:
_gui_log_add_chat_ovl:
_gui_log_remove_last_move_ovl:
    ret

test_context: DEFB 0, 0
test_id: DEFB 0
test_kind: DEFB 0
expected_ptr: DEFW 0
notify_ptr: DEFW 0
notify_kind: DEFB 0
notify_calls: DEFB 0
test_result: DEFB 0xff

expected_messages:
    DEFB "Connecting", 0
    DEFB "Connected", 0
    DEFB "ENTER/SPC=SELECT ARROWS=MOVE", 0
    DEFB "Waiting opponent", 0
    DEFB "READY - PRESS START", 0
    DEFB "Opponent ready - wait", 0
    DEFB "Opponent turn", 0
    DEFB "Game not started", 0
    DEFB "Disconnect? Y/N", 0
    DEFB "REQUEST RESET? y/n", 0
    DEFB "Opponent: Reset? Y/N", 0
    DEFB "Opponent restart? Y/N", 0
    DEFB "RESET rejected", 0
    DEFB "Waiting ACK", 0
    DEFB "GAME STARTED", 0
    DEFB "DRAW rejected", 0
    DEFB "Draw? Y/N", 0
    DEFB "Opponent: Draw? Y/N", 0
    DEFB "Resign? Y/N", 0
    DEFB "Restart game? Y/N", 0
    DEFB "Room conflict", 0
    DEFB "Takeback? Y/N", 0
    DEFB "Opponent: Takeback? Y/N", 0
    DEFB "Takeback requested", 0
    DEFB "Takeback accepted", 0
    DEFB "Takeback rejected", 0
    DEFB "No move to take back", 0
    DEFB "Game saved", 0
    DEFB "Game loaded", 0
    DEFB "Save failed", 0
    DEFB "Load failed", 0
    DEFB "Load unavailable", 0
    DEFB "OPPONENT WANTS LOAD Y/N", 0
    DEFB "Waiting opponent approval", 0
    DEFB "Loading", 0
    DEFB "Load declined", 0
    DEFB "Bad move", 0
    DEFB "Opponent starts", 0
    DEFB "Starting", 0
    DEFB "BUSY", 0
    DEFB "RTC or UTC -11..+13", 0
    DEFB "Bad IP", 0
    DEFB "Bad port", 0
    DEFB "Invalid MQTT room", 0
    DEFB "CONFIG INVALID", 0
    DEFB "Erase? Y/N", 0
