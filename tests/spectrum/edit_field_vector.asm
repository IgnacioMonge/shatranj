SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC draw_char64_pixels_at_tmp
PUBLIC tmp_row
PUBLIC tmp_col
PUBLIC tmp_char
PUBLIC tmp_scan

EXTERN _spectrum_gui_edit_show
EXTERN _spectrum_gui_edit_hide
EXTERN _spectrum_gui_edit_tick
EXTERN _edit_buf
EXTERN _edit_len
EXTERN _edit_pos

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, edit_text
    ld (_edit_buf), hl
    ld a, 3
    ld (_edit_len), a
    ld a, 1
    ld (_edit_pos), a
    ld hl, 0x0a07
    call _spectrum_gui_edit_show
    ld a, (draw_count)
    cp 1
    jr nz, test_bad_show
    ld a, (draw_row)
    cp 7
    jr nz, test_bad_show
    ld a, (draw_col)
    cp 11
    jr nz, test_bad_show
    ld a, (draw_char)
    cp 'b'
    jr nz, test_bad_show

    call tick_sixteen
    ld a, (draw_count)
    cp 2
    jr nz, test_bad_blank
    ld a, (draw_char)
    cp ' '
    jr nz, test_bad_blank

    call tick_sixteen
    ld a, (draw_count)
    cp 3
    jr nz, test_bad_restore
    ld a, (draw_char)
    cp 'b'
    jr nz, test_bad_restore
    call _spectrum_gui_edit_hide
    call tick_sixteen
    ld a, (draw_count)
    cp 3
    jr nz, test_bad_hide

    ; Hiding during the blank phase must restore the snapped character.
    ld hl, 0x0a07
    call _spectrum_gui_edit_show
    call tick_sixteen
    call _spectrum_gui_edit_hide
    ld a, (draw_count)
    cp 6
    jr nz, test_bad_hide_restore
    ld a, (draw_char)
    cp 'b'
    jr nz, test_bad_hide_restore

    xor a
    ld (_edit_len), a
    ld (_edit_pos), a
    ld hl, 0x1209
    call _spectrum_gui_edit_show
    ld a, (draw_col)
    cp 18
    jr nz, test_bad_empty
    ld a, (draw_char)
    cp '_'
    jr nz, test_bad_empty
    xor a
    jr test_store

test_bad_show:
    ld a, 1
    jr test_store
test_bad_blank:
    ld a, 2
    jr test_store
test_bad_restore:
    ld a, 3
    jr test_store
test_bad_hide:
    ld a, 4
    jr test_store
test_bad_hide_restore:
    ld a, 5
    jr test_store
test_bad_empty:
    ld a, 6
test_store:
    ld (test_result), a
test_done:
    jp test_done

tick_sixteen:
    ld a, 16
tick_loop:
    push af
    call _spectrum_gui_edit_tick
    pop af
    dec a
    jr nz, tick_loop
    ret

draw_char64_pixels_at_tmp:
    ld (draw_char), a
    ld a, (tmp_row)
    ld (draw_row), a
    ld a, (tmp_col)
    ld (draw_col), a
    ld hl, draw_count
    inc (hl)
    ret

test_result: defb 0xff
draw_count: defb 0
draw_row: defb 0
draw_col: defb 0
draw_char: defb 0
edit_text: defb "abc",0

SECTION bss_user
tmp_row: defb 0
tmp_col: defb 0
tmp_char: defb 0
tmp_scan: defb 0
