SECTION code_user
PUBLIC _match_picker_frame, _match_picker_clear, _match_picker_row, _match_picker_cursor, _match_picker_finish
EXTERN clear_right_panel_rows, clear_right_pixel_band_abs
EXTERN draw_text64_line_attr_fast, draw_right_hline, compute_pixel_base, pixel_down_hl
EXTERN draw_ikkle_inline, _spectrum_render_ikkle_abs_at
EXTERN current_attr
EXTERN _spectrum_gui_set_status, _spectrum_gui_notify_persistent

_match_picker_frame:
    ld hl, match_status
    call _spectrum_gui_set_status
    ld hl, match_empty
    push hl
    call _spectrum_gui_notify_persistent
    call clear_right_panel_rows
    pop hl
    ld b, 6
    call match_line
    call draw_ikkle_inline
    DEFB 4, 37, 0x46
    DEFW match_header
    ; Rows start at half-column 37; colour starts after 22 characters.
    call draw_ikkle_inline
    DEFB 4, 59, 0x46
    DEFW match_plays
    ld a, 5
    call draw_right_hline
    ; Keep the separator in a different attribute row from the footer.
    ld hl, 0x5452                ; Y148, byte column 18 (panel already clear)
    ld (hl), 0x0f
    ld b, 13
match_footer_line:
    inc l
    ld (hl), 0xff
    djnz match_footer_line
    ld hl, match_select_hint
    call _spectrum_render_ikkle_abs_at
    ld hl, match_join_hint
    jp _spectrum_render_ikkle_abs_at

_match_picker_row:
    ld a, (hl)
    add a, 7
    ld b, a
    inc hl
match_line:
    ld a, 0x47
    ld (current_attr), a
    ld c, 37
    ld d, 14
    jp draw_text64_line_attr_fast

; Rows are painted together before the cursor; shift the existing 4x6 font.
_match_picker_finish:
    ld hl, 0x40f2                ; Y56, byte column 18
    ld de, 0x44d2                ; Y52, byte column 18
    ld c, 9 * 8
match_shift_scan:
    push bc
    push hl
    push de
    ld bc, 14
    ldir
    pop de
    pop hl
    pop bc
    call pixel_down_hl
    ex de, hl
    call pixel_down_hl
    ex de, hl
    dec c
    jr nz, match_shift_scan
    ld a, 124
    ld b, 4
    jp clear_right_pixel_band_abs

_match_picker_clear:
    ld a, 7 * 8 - 4
    ld b, 9 * 8 + 4
    jp clear_right_pixel_band_abs

; Same bitmap XOR strategy as TABOPTION; leave all attributes untouched.
_match_picker_cursor:
    ld a, l
    cp 9
    ret nc
    add a, a
    add a, a
    add a, a
    add a, 52
    call compute_pixel_base
    ld a, l
    add a, 18
    ld l, a
    ld c, 8
match_cursor_scan:
    push hl
    ld b, 14
    ld a, 0x0f
match_cursor_byte:
    xor (hl)
    ld (hl), a
    inc l
    ld a, 0xff
    djnz match_cursor_byte
    pop hl
    call pixel_down_hl
    dec c
    jr nz, match_cursor_scan
    ret

match_plays: DEFM "PLAYS", 0
match_select_hint: DEFB 152, 37, 0x46
    DEFM "SELECT HOST: UP/DOWN", 0
match_join_hint: DEFB 160, 37, 0x46
    DEFM "ENTER/SPACE:JOIN BREAK:BACK", 0
match_status: DEFM "MATCH - SELECT HOST", 0
; Reuse the status suffix and its terminator for frame labels.
DEFC match_header = match_status + 15
DEFC match_empty = match_status + 19

; char *line, snapshot *: SDCC callee, IX preserved, L = accepted host.
; Snapshot offsets are asserted against the SDK in match_picker.inc.
PUBLIC _match_picker_label
_match_picker_label:
    pop hl                      ; return address
    pop de                      ; output row (byte 0 is owned by caller)
    ex (sp), hl                 ; SDK snapshot; return address replaces arguments
    push de
    ld bc, 4
    add hl, bc
    ld a, (hl)                  ; device must be present
    or a
    jr z, match_label_reject
    ld c, 89                    ; B remains zero from the device offset
    add hl, bc
    ld a, (hl)                  ; requires_code: public hosts only
    or a
    jr nz, match_label_reject
    ld bc, -24
    add hl, bc                  ; name[24], terminated by SDK
    ld d, h
    ld e, l
    ld c, 0
match_label_length:
    ld a, (hl)
    or a
    jr z, match_label_suffix
    inc hl
    inc c
    jr match_label_length
match_label_suffix:
    ld a, c
    sub 4
    jr c, match_label_reject
    jr z, match_label_reject
    ld c, a                     ; visible name length, at most 19
    dec hl
    ld a, (hl)
    cp ')'
    jr nz, match_label_reject
    dec hl
    ld a, (hl)
    ld b, a
    cp 'W'
    jr z, match_label_colour
    cp 'B'
    jr nz, match_label_reject
match_label_colour:
    dec hl
    ld a, (hl)
    cp '('
    jr nz, match_label_reject
    dec hl
    ld a, (hl)
    cp ' '
    jr nz, match_label_reject
    pop hl
    inc hl
    ex de, hl                   ; HL name, DE output
    push bc                     ; save colour in B
    ld b, c
    ld a, 22
    sub c
    ld c, a                     ; padding to the PLAYS column
match_label_copy:
    ld a, (hl)
    cp 32
    jr c, match_label_replace
    cp 127
    jr c, match_label_print
match_label_replace:
    ld a, '?'
match_label_print:
    ld (de), a
    inc hl
    inc de
    djnz match_label_copy
    ld a, ' '
match_label_pad:
    ld (de), a
    inc de
    dec c
    jr nz, match_label_pad
    pop af                      ; A = saved B (W/B)
    ld hl, match_white
    cp 'W'
    jr z, match_label_tail
    ld hl, match_black
match_label_tail:
    ld bc, 6
    ldir
    ld l, a                     ; W/B is the nonzero acceptance flag
    ret
match_label_reject:
    pop de
    ld l, 0
    ret
match_white: DEFM "WHITE", 0
match_black: DEFM "BLACK", 0
