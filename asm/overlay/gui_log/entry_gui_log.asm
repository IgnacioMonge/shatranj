SECTION code_user

PUBLIC _gui_log_add_move_ovl_entry
PUBLIC _gui_log_add_chat_ovl_entry
PUBLIC _gui_log_notify_msg_ovl_entry
PUBLIC _gui_log_remove_last_move_ovl_entry
PUBLIC _gui_log_animate_board_ovl_entry
PUBLIC _gui_log_morph_board_ovl_entry
PUBLIC _gui_log_restore_side_panels_ovl_entry
PUBLIC _gui_log_apply_move_ovl_entry
PUBLIC _chat_clean_char
PUBLIC _chat_word_len
PUBLIC _chat_copy_clock_line
PUBLIC _gui_log_parse_ply
PUBLIC _clear_move_line
PUBLIC _clear_log_line
PUBLIC _scroll_move_lines
PUBLIC _scroll_chat_lines
PUBLIC _move_line_at
PUBLIC _log_line_at

EXTERN _gui_log_add_move_ovl
EXTERN _gui_log_add_chat_ovl
EXTERN _gui_log_remove_last_move_ovl
EXTERN _spectrum_gui_notify
EXTERN _spectrum_gui_notify_persistent
EXTERN _spectrum_gui_notify_success
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _gui_log_animate_board_ovl
EXTERN _gui_log_morph_board_ovl
EXTERN _gui_log_restore_side_panels_ovl
EXTERN _gui_log_apply_move_ovl
ENDIF

chat_clock_line_src EQU 0x5e92
IFDEF NETCHESSZX_NEXT_BANKING
gui_log_msg_scratch EQU 0x3c2b
ELSE
gui_log_msg_scratch EQU 0x672b
ENDIF
gui_log_msg_count EQU 46

    DEFB 8
    DW _gui_log_add_move_ovl_entry
    DW _gui_log_add_chat_ovl_entry
    DW _gui_log_notify_msg_ovl_entry
    DW _gui_log_remove_last_move_ovl_entry
    DW _gui_log_animate_board_ovl_entry
    DW _gui_log_morph_board_ovl_entry
    DW _gui_log_restore_side_panels_ovl_entry
    DW _gui_log_apply_move_ovl_entry

DEFC _gui_log_add_move_ovl_entry = _gui_log_add_move_ovl

DEFC _gui_log_add_chat_ovl_entry = _gui_log_add_chat_ovl

DEFC _gui_log_notify_msg_ovl_entry = _gui_log_notify_msg_ovl

DEFC _gui_log_remove_last_move_ovl_entry = _gui_log_remove_last_move_ovl

_gui_log_animate_board_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _gui_log_animate_board_ovl
ELSE
    ld hl, 1
    ret
ENDIF

_gui_log_morph_board_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _gui_log_morph_board_ovl
ELSE
    ld hl, 1
    ret
ENDIF

_gui_log_restore_side_panels_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _gui_log_restore_side_panels_ovl
ELSE
    ld hl, 1
    ret
ENDIF

_gui_log_apply_move_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _gui_log_apply_move_ovl
ELSE
    ld hl, 1
    ret
ENDIF

_gui_log_notify_msg_ovl:
    ld a, (hl)
    cp gui_log_msg_count
    jr nc, gui_log_notify_bad_id
    ld b, a
    inc hl
    ld c, (hl)
    push bc
    ld hl, gui_log_msg_blob
    ld a, b
    or a
    jr z, gui_log_msg_decode
gui_log_msg_find:
    ld a, (hl)
    inc hl
    or a
    jr nz, gui_log_msg_find
    djnz gui_log_msg_find
gui_log_msg_decode:
    ld de, gui_log_msg_scratch
gui_log_msg_decode_next:
    ld a, (hl)
    inc hl
    or a
    jr z, gui_log_msg_decode_done
    bit 7, a
    jr z, gui_log_msg_literal
    push hl
    and 0x7f
    ld b, a
    ld hl, gui_log_msg_dictionary
    jr z, gui_log_msg_token
gui_log_msg_seek_token:
    ld c, (hl)
    inc hl
gui_log_msg_skip_token:
    inc hl
    dec c
    jr nz, gui_log_msg_skip_token
    djnz gui_log_msg_seek_token
gui_log_msg_token:
    ld c, (hl)
    inc hl
    ld b, 0
    ldir
    pop hl
    jr gui_log_msg_decode_next
gui_log_msg_literal:
    ld (de), a
    inc de
    jr gui_log_msg_decode_next
gui_log_msg_decode_done:
    ld (de), a
    pop bc
    ld hl, gui_log_msg_scratch
    ld a, c
    cp 2
    jr z, gui_log_notify_wait
    cp 3
    jr z, gui_log_notify_success
    dec a
    jr nz, gui_log_notify_info
    inc a
    jr gui_log_notify_ready
gui_log_notify_info:
    xor a
gui_log_notify_ready:
    push af
    inc sp
    push hl
    call _spectrum_gui_notify
    pop af
    inc sp
    jr gui_log_notify_done
gui_log_notify_wait:
    call _spectrum_gui_notify_persistent
    jr gui_log_notify_done
gui_log_notify_success:
    call _spectrum_gui_notify_success
gui_log_notify_done:
    ld l, 1
    ret
gui_log_notify_bad_id:
    ld l, 0
    ret

_chat_clean_char:
    ld a, l
    cp ' '
    jr c, ccc_space
    cp '~' + 1
    jr c, ccc_done
ccc_space:
    ld a, ' '
ccc_done:
    ld l, a
    ret

_chat_word_len:
    ld b, 24
    ld c, 0
cwl_loop:
    ld a, (hl)
    or a
    jr z, cwl_done
    cp ' '
    jr z, cwl_done
    inc hl
    inc c
    djnz cwl_loop
cwl_done:
    ld l, c
    ret

_chat_copy_clock_line:
    inc hl
    ex de, hl
    ld hl, chat_clock_line_src
    ld bc, 6
    ldir
    ret

_gui_log_parse_ply:
    ld b, h
    ld c, l
    ld hl, 0
glpp_loop:
    ld a, (bc)
    sub '0'
    jr c, glpp_done
    cp 10
    jr nc, glpp_done
    inc bc
    add hl, hl
    ld d, h
    ld e, l
    add hl, hl
    add hl, hl
    add hl, de
    ld e, a
    ld d, 0
    add hl, de
    jr glpp_loop
glpp_done:
    ret

_clear_move_line:
    ld a, ' '
    ld b, 32
    jr cll_loop

_clear_log_line:
    ld b, 28
    xor a
cll_loop:
    ld (hl), a
    inc hl
    djnz cll_loop
    ret

_scroll_move_lines:
    push hl
    pop de
    ld bc, 32
    add hl, bc
    ld bc, 192
    ldir
    ret

_scroll_chat_lines:
    ld bc, 224

scroll_log_lines_count:
    push bc
    ld d, h
    ld e, l
    ld bc, 28
    add hl, bc
    pop bc
    ldir
    ret

_move_line_at:
    ld a, 32
    jr line_at

_log_line_at:
    ld a, 28
line_at:
    ld hl, 4
    add hl, sp
    ld b, (hl)
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld e, a
    ld d, 0
line_at_loop:
    ld a, b
    or a
    ret z
    add hl, de
    djnz line_at_loop
    ret

gui_log_msg_blob:
    DEFB "C", 0x86, "ing", 0
    DEFB "C", 0x86, "ed", 0
    DEFB "ENTER/SPC=SELECT ARROWS=MOVE", 0
    DEFB 0x84, "o", 0x80, 0
    DEFB "READY - PRESS", 0x8b, 0
    DEFB "O", 0x80, " ready - wait", 0
    DEFB "O", 0x80, " turn", 0
    DEFB 0x88, "not ", 0x85, "ed", 0
    DEFB "Disc", 0x86, 0x81, 0
    DEFB "REQUEST ", 0x8c, "? y/n", 0
    DEFB "O", 0x80, ": Reset", 0x81, 0
    DEFB "O", 0x80, " re", 0x85, 0x81, 0
    DEFB 0x8c, 0x83, 0
    DEFB 0x84, "ACK", 0
    DEFB "GAME", 0x8b, "ED", 0
    DEFB "DRAW", 0x83, 0
    DEFB 0x8d, 0x81, 0
    DEFB "O", 0x80, ": ", 0x8d, 0x81, 0
    DEFB "Resign", 0x81, 0
    DEFB "Re", 0x85, " game", 0x81, 0
    DEFB "Room conflict", 0
    DEFB 0x82, 0x81, 0
    DEFB "O", 0x80, ": ", 0x82, 0x81, 0
    DEFB 0x82, " requested", 0
    DEFB 0x82, " accepted", 0
    DEFB 0x82, 0x83, 0
    DEFB "No ", 0x8e, " to take back", 0
    DEFB 0x88, "saved", 0
    DEFB 0x88, "loaded", 0
    DEFB "Save", 0x89, 0
    DEFB 0x87, 0x89, 0
    DEFB 0x87, " unavailable", 0
    DEFB "OPPONENT WANTS LOAD Y/N", 0
    DEFB 0x84, "o", 0x80, " approval", 0
    DEFB 0x87, "ing", 0
    DEFB 0x87, " declined", 0
    DEFB 0x8a, 0x8e, 0
    DEFB "O", 0x80, " ", 0x85, "s", 0
    DEFB "Starting", 0
    DEFB "BUSY", 0
    DEFB "RTC or UTC -11..+13", 0
    DEFB 0x8a, "IP", 0
    DEFB 0x8a, "port", 0
    DEFB "Invalid MQTT room", 0
    DEFB "CONFIG INVALID", 0
    DEFB "Erase", 0x81, 0

gui_log_msg_dictionary:
    DEFB 7, "pponent"
    DEFB 5, "? Y/N"
    DEFB 8, "Takeback"
    DEFB 9, " rejected"
    DEFB 8, "Waiting "
    DEFB 5, "start"
    DEFB 6, "onnect"
    DEFB 4, "Load"
    DEFB 5, "Game "
    DEFB 7, " failed"
    DEFB 4, "Bad "
    DEFB 6, " START"
    DEFB 5, "RESET"
    DEFB 4, "Draw"
    DEFB 4, "move"
