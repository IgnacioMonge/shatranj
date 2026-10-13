SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _gui_log_add_move_ovl
PUBLIC _gui_log_remove_last_move_ovl
PUBLIC _spectrum_gui_notify
PUBLIC _spectrum_gui_notify_persistent
PUBLIC _spectrum_gui_notify_success
PUBLIC _spectrum_render_chat_at
PUBLIC _chat_line_count
PUBLIC compute_pixel_base
PUBLIC pixel_down_hl
PUBLIC tmp_scan
PUBLIC piece_scan

EXTERN _gui_log_add_chat_ovl_entry
EXTERN _gui_log_copy_move

INCLUDE "ui_layout.inc"

; Low-RAM chat rows and clock text (NETCHESSZX_LOWRAM_CHAT_LOG_ADDR and
; NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR); the event log sits above the vector.
test_chat_lines EQU 0x5d96
test_chat_clock EQU 0x5e92
test_events EQU 0xa000
test_pixels EQU 0xf000

; Each chat case: line count to preset (0xfe keeps it, 0xff ends), who,
; render, six clock bytes, NUL-terminated text. Per case the log holds 'C',
; each render call ('S' for the overlay's own chat scroll, or 'L' + row
; address + 28 row bytes), the return value, then the 252 chat bytes and the
; line count after the call. Each move cell case: 0 (NULL move) or 1 (text
; follows), six clock bytes, the text; the cell is written into 16 log bytes
; preset to 0xaa. The log ends with 'E'.
test_start:
    ld sp, 0xff00
    ld hl, test_events
    ld (test_event_ptr), hl
    ld hl, test_chat_cases
test_case:
    ld a, (hl)
    inc hl
    cp 0xff
    jr z, test_moves
    cp 0xfe
    jr z, test_keep_count
    ld (_chat_line_count), a
test_keep_count:
    ld a, (hl)
    ld (test_context), a
    inc hl
    ld a, (hl)
    ld (test_context + 4), a
    inc hl
    ld de, test_chat_clock
    ld bc, 6
    ldir
    ld (test_context + 1), hl
    xor a
    ld b, a
    ld c, a
    cpir
    push hl
    ld a, 'C'
    call test_log_byte
    ld hl, test_context
    call _gui_log_add_chat_ovl_entry
    ld a, l
    call test_log_byte
    ld hl, test_chat_lines
    ld de, (test_event_ptr)
    ld bc, 252
    ldir
    ld a, (_chat_line_count)
    ld (de), a
    inc de
    ld (test_event_ptr), de
    pop hl
    jr test_case
test_moves:
    ld hl, test_move_cases
test_move:
    ld a, (hl)
    inc hl
    cp 0xff
    jr z, test_cases_done
    ld de, test_chat_clock
    ld bc, 6
    ldir
    ld de, 0
    or a
    jr z, test_move_null
    ld d, h
    ld e, l
    xor a
    cpir
test_move_null:
    push hl
    ld hl, (test_event_ptr)
    push hl
    ld b, 16
test_move_fill:
    ld (hl), 0xaa
    inc hl
    djnz test_move_fill
    ld (test_event_ptr), hl
    pop hl
    push de
    push hl
    call _gui_log_copy_move
    pop hl
    jr test_move
test_cases_done:
    ld a, 'E'
    call test_log_byte
    xor a
    ld (test_result), a
test_done:
    jp test_done

test_log_byte:
    push hl
    ld hl, (test_event_ptr)
    ld (hl), a
    inc hl
    ld (test_event_ptr), hl
    pop hl
    ret

_spectrum_render_chat_at:
    ld a, 'L'
    call test_log_byte
    ld a, l
    call test_log_byte
    ld a, h
    call test_log_byte
    ld de, (test_event_ptr)
    ld bc, 28
    ldir
    ld (test_event_ptr), de
; Resident renderers do not preserve the caller's registers.
    ld hl, 0xdead
    ld de, 0xbeef
    ld bc, 0x5aa5
    ld a, 0x3c
    or a
    ret

; The ULA chat scroll in GUI_LOG asks for its first band row once per scroll;
; its pixel copies land in a scratch area.
compute_pixel_base:
    cp NETCHESSZX_INFO_CHAT_FIRST_Y
    jr nz, test_pixel_row
    ld a, 'S'
    call test_log_byte
test_pixel_row:
    ld hl, test_pixels
    ret

_gui_log_add_move_ovl:
_gui_log_remove_last_move_ovl:
_spectrum_gui_notify:
_spectrum_gui_notify_persistent:
_spectrum_gui_notify_success:
pixel_down_hl:
    ret

tmp_scan: DEFB 0
piece_scan: DEFB 0
_chat_line_count: DEFB 0
test_context: DEFB 0, 0, 0, 0x77, 0
test_event_ptr: DEFW 0
test_result: DEFB 0xff

test_chat_cases:
