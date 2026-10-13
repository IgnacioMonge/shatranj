SECTION code_user

PUBLIC _gui_log_add_move_ovl_entry
PUBLIC _gui_log_add_chat_ovl_entry
PUBLIC _gui_log_notify_msg_ovl_entry
PUBLIC _gui_log_remove_last_move_ovl_entry
PUBLIC _gui_log_animate_board_ovl_entry
PUBLIC _gui_log_morph_board_ovl_entry
PUBLIC _gui_log_restore_side_panels_ovl_entry
PUBLIC _gui_log_apply_move_ovl_entry
PUBLIC _gui_log_add_chat_ovl
PUBLIC _gui_log_copy_move
PUBLIC _gui_log_parse_ply
PUBLIC _clear_move_line
PUBLIC _scroll_move_lines
PUBLIC _move_line_at

EXTERN _gui_log_add_move_ovl
EXTERN _gui_log_remove_last_move_ovl
EXTERN _chat_line_count
EXTERN _spectrum_render_chat_at
EXTERN _spectrum_gui_notify
EXTERN _spectrum_gui_notify_persistent
EXTERN _spectrum_gui_notify_success
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _gui_log_animate_board_ovl
EXTERN _gui_log_morph_board_ovl
EXTERN _gui_log_restore_side_panels_ovl
EXTERN _gui_log_apply_move_ovl
ENDIF
IFDEF NETCHESSZX_NEXT_BANKING
EXTERN _spectrum_render_chat_scroll
ENDIF

chat_clock_line_src EQU 0x5e92
chat_rows EQU 9              ; NETCHESSZX_CHAT_ROWS
chat_slot EQU 28             ; NETCHESSZX_CHAT_SLOT_SIZE
; lowram_map.h asserts the chat log ends where the clock text starts.
chat_lines EQU chat_clock_line_src - chat_rows * chat_slot
IFDEF NETCHESSZX_NEXT_BANKING
gui_log_msg_scratch EQU 0x3c78
ELSE
gui_log_msg_scratch EQU 0x672b
ENDIF
gui_log_msg_count EQU 51     ; SPECTRUM_GUI_MSG_COUNT

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
gui_log_msg_seek_token:         ; each dictionary word ends with bit 7 set
    bit 7, (hl)
    inc hl
    jr z, gui_log_msg_seek_token
    djnz gui_log_msg_seek_token
gui_log_msg_token:
    ld a, (hl)
    and 0x7f
    ld (de), a
    inc de
    bit 7, (hl)
    inc hl
    jr z, gui_log_msg_token
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

; Word-wraps ctx text into chat rows: row 0 holds who, the clock and text
; from column 7, later rows text from column 1; text stops before column 25
; and rows are zeroed first, so each stays NUL-terminated. A word that does
; not fit after a space moves to the next row; a first word too long for its
; row is split there. Callers pass who 'W' or 'B' (maybe | 0x80), never 0, so
; a zero who marks a continuation row. Byte-for-byte the C it replaces
; (tests/spectrum/gui_log_chat_vector.asm).
_gui_log_add_chat_ovl:
IFDEF NETCHESSZX_SDCC_IY
IFNDEF NETCHESSZX_SPECTRANEXT
    ; The sdcc_iy build preserves the shared live-interrupt IY contract while
    ; chat render/scroll runs; SpectraNext retains its separate policy.
    ei
ENDIF
ENDIF
    ld c, (hl)              ; who
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)              ; text
    inc hl
    inc hl
    ld b, (hl)              ; render
    ex de, hl
chat_new_row:               ; HL = text, B = render, C = who (0 after row 0)
    push hl
    push bc
    ld hl, _chat_line_count
    ld a, (hl)
    cp chat_rows
    jr c, chat_row_free
    ld de, chat_lines
    ld hl, chat_lines + chat_slot
    ld bc, (chat_rows - 1) * chat_slot
    ldir
    pop bc
    push bc
    inc b
    dec b
    call nz, _spectrum_render_chat_scroll
    ld a, chat_rows - 1
    jr chat_row_at
chat_row_free:
    inc (hl)
chat_row_at:
    ld hl, chat_lines - chat_slot
    ld de, chat_slot
    inc a
chat_row_seek:
    add hl, de
    dec a
    jr nz, chat_row_seek
    ld d, h
    ld e, l
    ld b, chat_slot
chat_row_clear:
    ld (hl), a
    inc hl
    djnz chat_row_clear
    pop bc
    pop hl
    ld a, c
    ld c, 0
    push bc                 ; render, who 0 for the next rows
    push de                 ; row
    ld (de), a
    inc de
    ld b, 24                ; room: columns 1..24
    or a
    jr z, chat_skip
    push hl
    ld hl, chat_clock_line_src
    ld bc, 6
    ldir                    ; columns 1..6 = clock, C = 0
    pop hl
    ld b, 18                ; room: columns 7..24
chat_skip:                  ; DE = column, B = room, C = 1 after a word
    ld a, (hl)
    cp ' '
    jr nz, chat_word
    inc hl
    jr chat_skip
chat_word:
    or a
    jr z, chat_row_done
    inc b
    dec b
    jr z, chat_row_done
    dec c
    jr nz, chat_copy        ; first word: copy what fits, the rest wraps
    push hl
    push bc
chat_fit:                   ; does ' ' + word fit in B columns?
    ld a, (hl)
    or a
    jr z, chat_fit_done
    cp ' '
    jr z, chat_fit_done
    inc hl
    djnz chat_fit
chat_fit_done:              ; Z: it fits
    pop bc
    pop hl
    jr nz, chat_row_done
    ld a, ' '
    ld (de), a
    inc de
    dec b
chat_copy:
    ld a, (hl)
    or a
    jr z, chat_copied
    sub ' '
    jr z, chat_copied
    cp '~' - ' ' + 1
    jr c, chat_char
    xor a                   ; controls, DEL and high bytes print as spaces
chat_char:
    add a, ' '
    ld (de), a
    inc de
    inc hl
    djnz chat_copy
chat_copied:
    ld c, 1
    jr chat_skip
chat_row_done:
    pop de                  ; row
    pop bc                  ; render
    push hl
    push bc
    ex de, hl
    inc b
    dec b
    call nz, _spectrum_render_chat_at
    pop bc
    pop hl
    ld a, (hl)
    or a
    jp nz, chat_new_row
    ld l, 1
    ret

; void gui_log_copy_move(char *out, const char *move) __z88dk_callee
; Writes the clock ("--:-- " before the first tick), the move cut or padded
; to 7 columns and a NUL; a NULL move writes the RESTORED marker instead.
_gui_log_copy_move:
    pop bc
    pop de                  ; out
    pop hl                  ; move
    push bc
    ld a, h
    or l
    jr z, gcm_restored
    push hl
    ld hl, chat_clock_line_src
    ld a, (hl)
    or a
    jr nz, gcm_clock
    ld hl, gcm_no_clock
gcm_clock:
    ld bc, 6
    ldir
    pop hl
    ld b, 7
gcm_move:
    ld a, (hl)
    or a
    jr nz, gcm_char
    ld a, ' '               ; past the move's NUL: pad, staying on the NUL
    dec hl
gcm_char:
    inc hl
    ld (de), a
    inc de
    djnz gcm_move
    xor a
    ld (de), a
    ret
gcm_restored:
    ld hl, gcm_restored_text
gcm_text:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, gcm_text
    ret

gcm_no_clock:
    DEFB "--:-- "
gcm_restored_text:
    DEFB "RESTORED", 0      ; NETCHESSZX_UI_EVENT_RESTORED

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

_move_line_at:
    ld a, 32
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

IFNDEF NETCHESSZX_NEXT_BANKING
INCLUDE "ui_layout.inc"
EXTERN compute_pixel_base
EXTERN pixel_down_hl
EXTERN tmp_scan
EXTERN piece_scan
INCLUDE "asm/ula/log_scroll.inc"
ENDIF

gui_log_msg_blob:
    DEFB "C", 0x88, 0x92, 0
    DEFB "C", 0x88, 0x8a, 0
    DEFB "ENTER/SPC=SELEC", 0x95, "A", 0x9c, "WS=MOVE", 0
    DEFB 0x85, "o", 0x80, 0
    DEFB 0x8e, "ADY", 0x97, " P", 0x8e, "SS ", 0x86, 0
    DEFB "O", 0x80, 0x99, 0x9d, "y", 0x97, " wait", 0
    DEFB "O", 0x80, " turn", 0
    DEFB 0x89, "not s", 0x8f, 0x8a, 0
    DEFB "Disc", 0x88, 0x81, 0
    DEFB 0x8e, "QUES", 0x95, 0x9b, "? y/n", 0
    DEFB "O", 0x80, 0x9a, 0x90, "et", 0x81, 0
    DEFB 0x84, "ACCEP", 0x95, 0x8e, 0x86, 0x81, 0
    DEFB 0x9b, 0x83, 0
    DEFB 0x85, "ACK", 0
    DEFB "GAME ", 0x86, "ED", 0
    DEFB "DRAW", 0x83, 0
    DEFB 0x93, 0x81, 0
    DEFB "O", 0x80, 0x9a, 0x93, 0x81, 0
    DEFB 0x90, "ign", 0x81, 0
    DEFB 0x84, " ", 0x8e, 0x86, 0x81, 0
    DEFB "R", 0xa1, " conf", 0xa0, "ct", 0
    DEFB 0x82, 0x81, 0
    DEFB "O", 0x80, 0x9a, 0x82, 0x81, 0
    DEFB 0x82, 0x99, "quest", 0x8a, 0
    DEFB 0x82, " accept", 0x8a, 0
    DEFB 0x82, 0x83, 0
    DEFB "No", 0x9e, 0x96, "to tak", 0x96, "back", 0
    DEFB 0x89, "s", 0x9f, 0x8a, 0
    DEFB 0x89, "lo", 0x9d, 0x8a, 0
    DEFB "S", 0x9f, 0x96, "f", 0x91, 0x8a, 0
    DEFB 0x94, 0x87, "f", 0x91, 0x8a, 0
    DEFB 0x94, 0x87, "un", 0x9f, 0x91, "able", 0
    DEFB 0x8c, 0x98, "NTS LOAD Y/N", 0
    DEFB 0x85, "o", 0x80, " approval", 0
    DEFB 0x94, 0x9d, 0x92, 0
    DEFB 0x94, 0x87, "dec", 0xa0, "n", 0x8a, 0
    DEFB "B", 0x9d, 0x9e, "e", 0
    DEFB "O", 0x80, " s", 0x8f, "s", 0
    DEFB "S", 0x8f, 0x92, 0
    DEFB "BUSY", 0
    DEFB "RTC or UTC", 0x97, "11..+13", 0
    DEFB "B", 0x87, "IP", 0
    DEFB "B", 0x87, "port", 0
    DEFB "Inva", 0xa0, "d MQT", 0x95, "r", 0xa1, 0
    DEFB 0x8b, "NVALID", 0
    DEFB "Erase", 0x81, 0
    DEFB 0x8b, "/O E", 0x9c, "R", 0
    DEFB 0x90, "um", 0x96, "last game", 0x81, 0
    DEFB 0x8d, "FF", 0
    DEFB 0x8d, "N", 0
    DEFB 0x84, 0x98, "ITING ", 0x8c, 0

gui_log_msg_dictionary:
    DEFB "pponen", 't' | 0x80
    DEFB "? Y/", 'N' | 0x80
    DEFB "Takebac", 'k' | 0x80
    DEFB " rejecte", 'd' | 0x80
    DEFB "S:STATS", ' ' | 0x80
    DEFB "Waiting", ' ' | 0x80
    DEFB "STAR", 'T' | 0x80
    DEFB "ad", ' ' | 0x80
    DEFB "onnec", 't' | 0x80
    DEFB "Game", ' ' | 0x80
    DEFB "e", 'd' | 0x80
    DEFB "CONFIG ", 'I' | 0x80
    DEFB "OPPONEN", 'T' | 0x80
    DEFB "HINTS ", 'O' | 0x80
    DEFB "R", 'E' | 0x80
    DEFB "tar", 't' | 0x80
    DEFB "Re", 's' | 0x80
    DEFB "ai", 'l' | 0x80
    DEFB "in", 'g' | 0x80
    DEFB "Dra", 'w' | 0x80
    DEFB "L", 'o' | 0x80
    DEFB "T", ' ' | 0x80
    DEFB "e", ' ' | 0x80
    DEFB " ", '-' | 0x80
    DEFB " W", 'A' | 0x80
    DEFB " r", 'e' | 0x80
    DEFB ":", ' ' | 0x80
    DEFB "RESE", 'T' | 0x80
    DEFB "RR", 'O' | 0x80
    DEFB "a", 'd' | 0x80
    DEFB " mo", 'v' | 0x80
    DEFB "a", 'v' | 0x80
    DEFB "l", 'i' | 0x80
    DEFB "oo", 'm' | 0x80
