SECTION code_user

PUBLIC _gui_log_add_move_ovl_entry
PUBLIC _gui_log_add_chat_ovl_entry
PUBLIC _app_input_parse_move_ovl_entry
PUBLIC _connection_panel_ovl_entry
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
EXTERN _connection_panel_ovl

chat_clock_line_src EQU 0x5e92

    DW 4
    DW _gui_log_add_move_ovl_entry
    DW _gui_log_add_chat_ovl_entry
    DW _app_input_parse_move_ovl_entry
    DW _connection_panel_ovl_entry

_gui_log_add_move_ovl_entry:
    ex de, hl
    jp _gui_log_add_move_ovl

_gui_log_add_chat_ovl_entry:
    ex de, hl
    jp _gui_log_add_chat_ovl

_connection_panel_ovl_entry:
    ex de, hl
    jp _connection_panel_ovl

_app_input_parse_move_ovl_entry:
    ld l, (de)
    inc de
    ld h, (de)
    inc de
    ld a, (de)
    inc de
    ld c, a
    ld b, (de)

app_input_skip_leading:
    ld a, (hl)
    cp ' '
    jr nz, app_input_first_square
    inc hl
    jr app_input_skip_leading

app_input_first_square:
    call app_input_parse_square
    jr c, app_input_fail
    ld a, (hl)
    cp '-'
    jr z, app_input_skip_separator
    cp ' '
    jr nz, app_input_second_square

app_input_skip_separator:
    inc hl

app_input_second_square:
    call app_input_parse_square
    jr c, app_input_fail
    xor a
    ld (bc), a
    ld a, (hl)
    call app_input_lower_a
    cp 'q'
    jr z, app_input_store_promo
    cp 'r'
    jr z, app_input_store_promo
    cp 'b'
    jr z, app_input_store_promo
    cp 'n'
    jr nz, app_input_skip_trailing

app_input_store_promo:
    ld (bc), a
    inc bc
    xor a
    ld (bc), a
    inc hl

app_input_skip_trailing:
    ld a, (hl)
    cp ' '
    jr nz, app_input_end_check
    inc hl
    jr app_input_skip_trailing

app_input_end_check:
    or a
    jr nz, app_input_fail
    ld l, 1
    ret

app_input_fail:
    ld l, 0
    ret

app_input_parse_square:
    ld a, (hl)
    call app_input_lower_a
    cp 'a'
    jr c, app_input_square_fail
    cp 'i'
    jr nc, app_input_square_fail

app_input_file_ok:
    ld (bc), a
    inc hl
    inc bc
    ld a, (hl)
    cp '1'
    jr c, app_input_square_fail
    cp '9'
    jr nc, app_input_square_fail
    ld (bc), a
    inc hl
    inc bc
    or a
    ret

app_input_square_fail:
    scf
    ret

app_input_lower_a:
    cp 'A'
    ret c
    cp 'Z' + 1
    ret nc
    add a, 'a' - 'A'
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
    ld b, 32
    jr cll_start

_clear_log_line:
    ld b, 28
cll_start:
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
    ld hl, 4
    add hl, sp
    ld b, (hl)
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld de, 32
mla_loop:
    ld a, b
    or a
    ret z
    add hl, de
    djnz mla_loop
    ret

_log_line_at:
    ld hl, 4
    add hl, sp
    ld b, (hl)
    ld hl, 2
    add hl, sp
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld de, 28
lla_loop:
    ld a, b
    or a
    ret z
    add hl, de
    djnz lla_loop
    ret
