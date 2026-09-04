SECTION code_user

PUBLIC _menu_config_run_ovl_entry
PUBLIC _menu_config_paint_attrs_ovl_entry
PUBLIC _menu_config_validate_ip_ovl_entry
PUBLIC _menu_config_edit_line_ovl_entry
PUBLIC _menu_config_render_ovl_entry
PUBLIC _menu_config_nav_ovl_entry
PUBLIC _menu_config_piece_set_options_asm

EXTERN _spectrum_info_line
EXTERN _spectrum_info_show_game_setup
EXTERN _spectrum_overlay_context
EXTERN _spectrum_info_show_setup
EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_game_focus
EXTERN _setup_visible_mask
EXTERN _setup_defined_mask
EXTERN _setup_cursor
EXTERN _setup_config_dirty
EXTERN _setup_action_focus
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _netchesszx_direct_host
EXTERN _last_ip

ROW_GAME        EQU 0
ROW_LINK1       EQU 1
ROW_LINK2       EQU 2
ROW_TIME        EQU 4
ROW_COLOR       EQU 5
ROW_NOTATION    EQU 6
ROW_BOARD       EQU 7
ROW_SET         EQU 8
ROW_HINTS       EQU 9
ROW_ACTION      EQU 10
SETUP_CLEAR_ENDPOINT EQU 0xfe
CTX_KEY         EQU 0x5fe0
CTX_FORCE_LO    EQU 0x5fe1
CTX_CLEAR_FROM  EQU 0x5fe3
CTX_FLAGS       EQU 0x5fe6
PAINT_MASK_ALL EQU 0x03ef
KEY_UP          EQU 0x81
KEY_DOWN        EQU 0x82
FLAG_PAINT      EQU 0x02
FLAG_TIME_UI    EQU 0x10
FLAG_ACTION_UI  EQU 0x20

    DEFB 6
    DW _menu_config_run_ovl_entry
    DW _menu_config_paint_attrs_ovl_entry
    DW _menu_config_validate_ip_ovl_entry
    DW _menu_config_edit_line_ovl_entry
    DW _menu_config_render_ovl_entry
    DW _menu_config_nav_ovl_entry

_menu_config_run_ovl_entry:
    inc de
    inc de
    ld a, (de)
    ld c, a
    inc de
    ld a, (de)
    ld b, a
menu_config_run_dirty:
    bit 0, c
    ld hl, menu_config_line_game
    call nz, menu_config_info_line
    bit 1, c
    ld hl, menu_config_line_link
    call nz, menu_config_info_line
    ld a, c
    and 0x60
    call nz, menu_config_show_game_setup
    bit 5, c
    ld hl, menu_config_line_color
    call nz, menu_config_info_line
    bit 6, c
    ld hl, menu_config_line_notation
    call nz, menu_config_info_line
    bit 7, c
    ld hl, menu_config_line_board
    call nz, menu_config_info_line
    bit 0, b
    ld hl, menu_config_line_set
    call nz, menu_config_info_line
    bit 1, b
    ld hl, menu_config_line_hints
    call nz, menu_config_info_line
    ld l, 1
    ret

_menu_config_render_ovl_entry:
    push de
    ld hl, (_setup_visible_mask)
    ld a, (_setup_cursor)
    ld b, a
    inc b
    ld de, 1
menu_config_render_cursor_shift:
    dec b
    jr z, menu_config_render_cursor_mask
    sla e
    rl d
    jr menu_config_render_cursor_shift
menu_config_render_cursor_mask:
    ld a, l
    and e
    ld c, a
    ld a, h
    and d
    or c
    jr nz, menu_config_render_cursor_ready
    xor a
    ld (_setup_cursor), a
menu_config_render_cursor_ready:
    pop de
    inc de
    inc de
    ld a, (de)
    or a
    jr z, menu_config_render_incremental
    ld hl, (_setup_visible_mask)
    ld (menu_config_render_dirty), hl
    call _spectrum_info_show_setup
    jr menu_config_render_force
menu_config_render_incremental:
    inc de
    ld a, (de)
    cp SETUP_CLEAR_ENDPOINT
    jr nz, menu_config_render_incremental_normal
    ; CONNECTION rows only. Skip the GAME SETUP header and NOTAT..HINTS.
    ; Do not merge CTX force: it used to carry COLOR and retriggered the
    ; header via run_dirty's 0x60 test. Wipe COLOR/ACTION when hidden;
    ; recreate COLOR only when its info-panel pixels are blank.
    ld hl, (_setup_visible_mask)
    ld a, l
    and 0x0f
    ld l, a
    ld h, 0
    ld (menu_config_render_dirty), hl
    ld a, (_setup_visible_mask)
    bit 5, a
    jr nz, menu_config_endpoint_color
    ld a, 14
    call menu_config_clear_row
    jr menu_config_endpoint_action
menu_config_endpoint_color:
    ld hl, 0x48d2
    ld a, (hl)
    inc hl
    or (hl)
    inc hl
    or (hl)
    jr nz, menu_config_endpoint_action
    ld hl, menu_config_line_color
    call menu_config_info_line
menu_config_endpoint_action:
    ld a, (_setup_visible_mask + 1)
    bit 2, a
    jr nz, menu_config_endpoint_apply
    ld a, 20
    call menu_config_clear_row
menu_config_endpoint_apply:
    ld hl, (menu_config_render_dirty)
    jr menu_config_render_apply
menu_config_render_incremental_normal:
    ld hl, (_setup_visible_mask)
    ld (menu_config_render_dirty), hl
    cp 0xff
    jr z, menu_config_render_force
    cp ROW_ACTION
    jr nz, menu_config_render_clear_normal
    ld a, 13
    jr menu_config_render_clear
menu_config_render_clear_normal:
    cp ROW_TIME
    jr c, menu_config_render_clear
    jr nz, menu_config_render_clear_game
    ld a, 3
    jr menu_config_render_clear
menu_config_render_clear_game:
    add a, 2
menu_config_render_clear:
    ld l, a
    call menu_config_clear_tail
menu_config_render_force:
    ld hl, (_spectrum_overlay_context)
    ld de, (_setup_visible_mask)
    ld a, l
    and e
    ld l, a
    ld a, h
    and d
    ld h, a
    ld de, (menu_config_render_dirty)
    ld a, l
    or e
    ld l, a
    ld a, h
    or d
    ld h, a
    ld (menu_config_render_dirty), hl
menu_config_render_apply:
    ld a, l
    and 0x0c
    cpl
    and l
    ld l, a
    ld a, h
    or l
    jr z, menu_config_render_overlay_done
    ld b, h
    ld c, l
    call menu_config_run_dirty
menu_config_render_overlay_done:
    ld hl, (menu_config_render_dirty)
    ld a, h
    or a
    jr nz, menu_config_paint_attrs_all
    ld a, l
    and 0xc0
    jr nz, menu_config_paint_attrs_all
    ; CONNECTION-only dirty (ENDPOINT): keep GAME SETUP attrs, including
    ; BOARD swatches, on screen. COLOR attrs follow visibility.
    ld a, (_setup_visible_mask)
    and 0x20
    or l
    ld l, a
    jp menu_config_paint_attrs_masked

menu_config_clear_tail:
    ld a, l
    add a, 7
menu_config_clear_tail_loop:
    cp 21
    ret nc
    push af
    call menu_config_clear_row
    pop af
    inc a
    jr menu_config_clear_tail_loop

menu_config_clear_row:
    ld c, a
    and 7
    rrca
    rrca
    rrca
    add a, 18
    ld e, a
    ld a, c
    and 0x18
    add a, 0x40
    ld d, a
    ld b, 8
menu_config_clear_pixels:
    push bc
    push de
    ld h, d
    ld l, e
    ld (hl), 0
    inc de
    ld bc, 13
    ldir
    pop de
    pop bc
    inc d
    djnz menu_config_clear_pixels
    ld l, e
    ld a, c
    srl a
    srl a
    srl a
    add a, 0x58
    ld h, a
    ld (hl), 0x07
    ld d, h
    ld e, l
    inc de
    ld bc, 13
    ldir
    ret

menu_config_info_line:
    push bc
    call _spectrum_info_line
    pop bc
    ret

menu_config_show_game_setup:
    push bc
    call _spectrum_info_show_game_setup
    pop bc
    ret

_menu_config_paint_attrs_ovl_entry:
    ld hl, (CTX_FORCE_LO)
    ld a, h
    or l
    jr nz, menu_config_paint_attrs_masked
menu_config_paint_attrs_all:
    ld hl, PAINT_MASK_ALL
menu_config_paint_attrs_masked:
    ld (menu_config_paint_mask), hl
    ld hl, _setup_choice + 4
    call menu_config_pack_choices
    ld (menu_config_values), a
    ld hl, (_setup_visible_mask)
    ld a, l
    ld (menu_config_visible_l), a
    ld a, h
    ld (menu_config_visible_h), a
    ld hl, (_setup_defined_mask)
    ld a, l
    ld (menu_config_defined_l), a
    ld a, h
    ld (menu_config_defined_h), a
    ld a, (_setup_cursor)
    ld (menu_config_cursor), a
    ld hl, _setup_focus_choice + 4
    call menu_config_pack_choices
    ld (menu_config_focus_bits), a
    ld a, (_setup_focus_board_theme)
    ld (menu_config_board_theme), a
    ld a, (_setup_focus_choice + 5)
    ld (menu_config_piece_set), a
    ld a, (_setup_choice + 5)
    ld (menu_config_piece_selected), a
    call menu_config_paint_binary_attrs
    ld a, (menu_config_paint_mask)
    bit 2, a
    call nz, menu_config_paint_room_attrs
    ld a, (menu_config_paint_mask)
    bit 3, a
    call nz, menu_config_paint_mqtt_attrs
    ld a, (menu_config_paint_mask)
    bit 7, a
    call nz, menu_config_paint_board_attrs
    ld a, (menu_config_paint_mask + 1)
    bit 0, a
    call nz, menu_config_paint_set_attrs
    ld l, 1
    ret

menu_config_pack_choices:
    ld b, 5
    xor a
menu_config_pack_choices_loop:
    add a, a
    or (hl)
    dec hl
    djnz menu_config_pack_choices_loop
    ret

_menu_config_nav_ovl_entry:
    call menu_config_nav_clear_ctx
    ld a, (CTX_KEY)
    cp KEY_UP
    jr z, menu_config_nav_prev
    cp KEY_DOWN
    jr z, menu_config_nav_next
    ld l, 1
    ret

menu_config_nav_clear_ctx:
    xor a
    ld hl, CTX_FORCE_LO
    ld b, 6
menu_config_nav_clear_loop:
    ld (hl), a
    inc hl
    djnz menu_config_nav_clear_loop
    dec a
    ld (CTX_CLEAR_FROM), a
    ret

menu_config_nav_prev:
    xor a
    jr menu_config_nav_go
menu_config_nav_next:
    ld a, 1
menu_config_nav_go:
    ld b, a
    ld a, (_setup_cursor)
    ld (menu_config_nav_scratch), a
    ld a, b
    call menu_config_nav_step_row
    ld (_setup_cursor), a
    cp ROW_ACTION
    jr nz, menu_config_nav_paint
    ld a, (_setup_config_dirty)
    xor 1
    ld (_setup_action_focus), a
menu_config_nav_paint:
    ld a, (menu_config_nav_scratch)
    call menu_config_nav_mark_row
    ld a, (menu_config_nav_scratch)
    cp ROW_BOARD
    jr nz, menu_config_nav_mark_next
    ld a, (_setup_game_focus)
    ld (_setup_focus_board_theme), a
menu_config_nav_mark_next:
    ld a, (_setup_cursor)
    call menu_config_nav_mark_row
    ld l, 1
    ret

menu_config_nav_mark_row:
    push af
    call menu_config_nav_row_bit
    ld hl, (CTX_FORCE_LO)
    ld a, l
    or c
    ld l, a
    ld a, h
    or b
    ld h, a
    ld (CTX_FORCE_LO), hl
    pop af
    ld d, FLAG_TIME_UI
    cp ROW_TIME
    jr z, menu_config_nav_mark_flag
    ld d, FLAG_ACTION_UI
    cp ROW_ACTION
    jr z, menu_config_nav_mark_flag
    ld d, FLAG_PAINT
menu_config_nav_mark_flag:
    ld a, (CTX_FLAGS)
    or d
    ld (CTX_FLAGS), a
    ret

menu_config_nav_step_row:
    or a
    jr z, menu_config_nav_step_prev
    ld a, (_setup_cursor)
    inc a
menu_config_nav_step_next_loop:
    cp 11
    jr nc, menu_config_nav_step_restore
    call menu_config_nav_visible
    ld a, e
    ret nz
    inc a
    jr menu_config_nav_step_next_loop
menu_config_nav_step_prev:
    ld a, (_setup_cursor)
    or a
    jr z, menu_config_nav_step_restore
    dec a
menu_config_nav_step_prev_loop:
    call menu_config_nav_visible
    ld a, e
    ret nz
    or a
    jr z, menu_config_nav_step_restore
    dec a
    jr menu_config_nav_step_prev_loop
menu_config_nav_step_restore:
    ld a, (_setup_cursor)
    ret

menu_config_nav_visible:
    ld e, a
    cp ROW_LINK2
    jr nz, menu_config_nav_check_port
    ld a, (_setup_choice + 1)
    or a
    jr nz, menu_config_nav_bit_restore
    ld a, (_setup_choice)
    or a
    jr z, menu_config_nav_false
menu_config_nav_bit_restore:
    ld a, e
menu_config_nav_check_port:
    cp 3
    jr nz, menu_config_nav_bit
    ld a, (_setup_choice + 1)
    or a
    jr nz, menu_config_nav_false
    ld a, e
menu_config_nav_bit:
    call menu_config_nav_row_bit
    ld hl, (_setup_visible_mask)
    ld a, l
    and c
    ret nz
    ld a, h
    and b
    ret
menu_config_nav_false:
    xor a
    ret

menu_config_nav_row_bit:
    ld bc, 1
    or a
    ret z
menu_config_nav_row_bit_loop:
    sla c
    rl b
    dec a
    jr nz, menu_config_nav_row_bit_loop
    ret

_menu_config_edit_line_ovl_entry:
    ld l, 0
    ret

menu_config_setup_screen_row:
    cp ROW_ACTION
    jr z, menu_config_setup_screen_row_action
    cp ROW_TIME
    jr z, menu_config_setup_screen_row_time
    cp 3
    jr nz, menu_config_setup_screen_row_normal
    dec a
menu_config_setup_screen_row_normal:
    ld e, a
menu_config_setup_screen_row_norm:
    ld a, e
    cp ROW_COLOR
    ld a, e
    jr c, menu_config_setup_screen_row_base
    add a, 2
menu_config_setup_screen_row_base:
    add a, 7
    ret
menu_config_setup_screen_row_time:
    ld a, 10
    ret
menu_config_setup_screen_row_action:
    ld a, 20
    ret

menu_config_paint_binary_attrs:
    ld hl, menu_config_binary_attrs
    ld a, 1
menu_config_paint_binary_loop:
    ld (menu_config_binary_bit), a
    ld a, (hl)
    or a
    ret z
    ld (menu_config_binary_mask), a
    inc hl
    ld a, (hl)
    ld c, a
    and 0x3f
    ld (menu_config_binary_row), a
    ld a, c
    ld (menu_config_binary_invert), a
    inc hl
    push hl
    bit 7, c
    jr nz, menu_config_paint_binary_high
    ld a, (menu_config_paint_mask)
    ld b, a
    ld a, (menu_config_visible_l)
    jr menu_config_paint_binary_active
menu_config_paint_binary_high:
    ld a, (menu_config_paint_mask + 1)
    ld b, a
    ld a, (menu_config_visible_h)
menu_config_paint_binary_active:
    and b
    ld b, a
    ld a, (menu_config_binary_mask)
    and b
    jr z, menu_config_paint_binary_skip
    call menu_config_prepare_binary
    ld a, (menu_config_binary_row)
    call menu_config_setup_screen_row
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5812
    add hl, de
    ex (sp), hl
    ld b, (hl)
    inc hl
    ex (sp), hl
    call menu_config_header_span
    ; CONNECTION SETUP values start on an odd 4-pixel column after the
    ; 4-pixel shift, so the first option cell is immediately after a
    ; 3-cell label. GAME SETUP still uses a 4-cell label and lands here
    ; without a skip.
menu_config_paint_binary_left:
    ex (sp), hl
    ld b, (hl)
    inc hl
    ld c, b
    ex (sp), hl
    call menu_config_paint_left_binary
    ld a, 5
    sub c
menu_config_paint_binary_gap:
    inc hl
    dec a
    jr nz, menu_config_paint_binary_gap
    ex (sp), hl
    ld b, (hl)
    inc hl
    ex (sp), hl
    call menu_config_paint_right_binary
    pop hl
    jr menu_config_paint_binary_next
menu_config_paint_binary_skip:
    pop hl
    inc hl
    inc hl
    inc hl
menu_config_paint_binary_next:
    ld a, (menu_config_binary_bit)
    add a, a
    jr menu_config_paint_binary_loop

menu_config_prepare_binary:
    ld a, (menu_config_binary_bit)
    ld b, a
    ld a, (menu_config_values)
    and b
    ld c, a
    ld a, (menu_config_focus_bits)
    and b
    ld e, a
    ld a, (menu_config_binary_invert)
    bit 6, a
    jr z, menu_config_prepare_binary_no_invert
    ld a, c
    xor b
    ld c, a
    ld a, e
    xor b
    ld e, a
menu_config_prepare_binary_no_invert:
    ld a, c
    ld (menu_config_value_flag), a
    ld a, e
    ld (menu_config_focus_value), a
    ld a, (menu_config_binary_invert)
    bit 7, a
    ld a, (menu_config_defined_l)
    jr z, menu_config_prepare_binary_defined
    ld a, (menu_config_defined_h)
menu_config_prepare_binary_defined:
    ld c, a
    ld a, (menu_config_binary_mask)
    and c
    ld (menu_config_defined_flag), a
    ld a, (menu_config_cursor)
    ld c, a
    ld a, (menu_config_binary_row)
    cp c
    ld a, 0
    jr nz, menu_config_prepare_binary_row_done
    inc a
menu_config_prepare_binary_row_done:
    ld (menu_config_row_focus), a
    ret

menu_config_header_span:
    ld a, 0x03
    jp menu_config_attr_span

menu_config_paint_left_binary:
    ld a, b
    ld (menu_config_option_width), a
    ld d, 0
    ld a, (menu_config_defined_flag)
    or a
    jr z, menu_config_left_selected_done
    ld a, (menu_config_value_flag)
    or a
    jr nz, menu_config_left_selected_done
    inc d
menu_config_left_selected_done:
    ld e, 0
    ld a, (menu_config_row_focus)
    or a
    jr z, menu_config_left_focused_done
    ld a, (menu_config_focus_value)
    or a
    jr nz, menu_config_left_focused_done
    inc e
menu_config_left_focused_done:
    ld a, (menu_config_option_width)
    ld b, a
    jp menu_config_option_span

menu_config_paint_right_binary:
    ld a, b
    ld (menu_config_option_width), a
    ld d, 0
    ld a, (menu_config_defined_flag)
    or a
    jr z, menu_config_right_selected_done
    ld a, (menu_config_value_flag)
    or a
    jr z, menu_config_right_selected_done
    inc d
menu_config_right_selected_done:
    ld e, 0
    ld a, (menu_config_row_focus)
    or a
    jr z, menu_config_right_focused_done
    ld a, (menu_config_focus_value)
    or a
    jr z, menu_config_right_focused_done
    inc e
menu_config_right_focused_done:
    ld a, (menu_config_option_width)
    ld b, a
    jp menu_config_option_span

; A = defined bit, C = setup row. D=1 if defined and not being edited.
; B (span width) is preserved; E is scratch.
menu_config_text_selected:
    ld e, a
    ld d, 0
    ld a, (_setup_room_editing)
    or a
    jr z, menu_config_text_selected_def
    ld a, (_setup_edit_row)
    cp c
    ret z
menu_config_text_selected_def:
    ld a, (menu_config_defined_l)
    and e
    ret z
    inc d
    ret

menu_config_paint_room_attrs:
    ld a, (menu_config_visible_l)
    bit 2, a
    ret z
    ld hl, 0x5932
    ld de, 0x5935
    push de
    ld b, 3
    call menu_config_header_span
    pop hl
    ld a, (menu_config_values)
    bit 1, a
    jr nz, menu_config_room_mqtt
    ; DIRECT keeps IP and PORT as independent controls. The 4-pixel shift
    ; puts ':' and PORT[0] in one ULA cell, so PORT's span owns that cell
    ; and ':' inherits PORT's colour. IP never paints into 0x593d.
    ; Repaint the whole IP field unfocused first: swapping seats changes
    ; the source between _last_ip and the peer host, and a shorter value
    ; would leave the previous one's attributes standing to its right.
    ld a, 4
    ld c, 2
    call menu_config_text_selected
    push hl
    ld e, 0
    ld b, 8
    call menu_config_option_span
    pop hl
    call menu_config_direct_ip_width
    ld b, a
    jr menu_config_room_width_done
menu_config_room_mqtt:
    ld b, 4
menu_config_room_width_done:
    ld a, 4
    ld c, 2
    call menu_config_text_selected
    call menu_config_focus_box
    jp menu_config_option_span

menu_config_direct_ip_width:
    ld de, _last_ip
    ld a, (_setup_choice)
    or a
    jr z, menu_config_direct_ip_count
    ld de, _netchesszx_direct_host
menu_config_direct_ip_count:
    ld b, 0
menu_config_direct_ip_count_loop:
    ld a, (de)
    or a
    jr z, menu_config_direct_ip_width_ready
    inc de
    inc b
    jr menu_config_direct_ip_count_loop
menu_config_direct_ip_width_ready:
    ld a, b
    or a
    jr nz, menu_config_direct_ip_nonempty
    inc a
menu_config_direct_ip_nonempty:
    add a, 2
    srl a
    cp 9
    ret c
    ld a, 8
    ret

menu_config_paint_mqtt_attrs:
    ld a, (menu_config_visible_l)
    bit 2, a
    ret z
    ld a, (_setup_choice + 1)
    or a
    jr z, menu_config_port_attrs
    ; HIVEMQ starts with JOIN at attr 26 after the 4-pixel value shift.
    ld hl, 0x593a
    ld b, 4
    ld a, 0x05
    jp menu_config_attr_span
menu_config_port_attrs:
    ld a, (menu_config_visible_l)
    bit 3, a
    ret z
    ; Colon plus five port digits occupy attrs 29-31 (':' shares PORT[0]'s
    ; cell and therefore PORT's colour). All five digits stay on-screen.
    ld hl, 0x593d
    ld b, 3
    ld a, 8
    ld c, 3
    call menu_config_text_selected
    call menu_config_focus_box
    ld a, e
    or a
    jr z, menu_config_port_unfocused
    ; PORT's marker replaces ':' while focused.
    push hl
    ld hl, 0x4b3d
    res 7, (hl)
    ld h, 0x4e
    res 7, (hl)
    pop hl
    jp menu_config_option_span
menu_config_port_unfocused:
    call menu_config_option_span
    ; Restore ':' after removing the PORT marker.
    ld hl, 0x4b3d
    set 7, (hl)
    ld h, 0x4e
    set 7, (hl)
    ret

; C = setup row. Sets E if the cursor is on that row and it is not being edited.
menu_config_focus_box:
    ld e, 0
    ld a, (_setup_room_editing)
    or a
    jr z, menu_config_focus_box_cursor
    ld a, (_setup_edit_row)
    cp c
    ret z
menu_config_focus_box_cursor:
    ld a, (menu_config_cursor)
    cp c
    ret nz
    inc e
    ret

menu_config_paint_board_attrs:
    ld a, (menu_config_visible_l)
    bit 7, a
    ret z
    ld hl, 0x5a12
    ld b, 4
    call menu_config_header_span
    ld a, (menu_config_cursor)
    cp ROW_BOARD
    jr nz, menu_config_board_not_cursor
    ld a, (menu_config_board_theme)
    ld l, a
    jp menu_config_board_swatches
menu_config_board_not_cursor:
    ld a, (menu_config_defined_l)
    bit 7, a
    jr z, menu_config_board_not_defined
    ld a, (menu_config_board_theme)
    add a, 5
    ld l, a
    jp menu_config_board_swatches
menu_config_board_not_defined:
    ld l, 0xff
    jp menu_config_board_swatches

menu_config_paint_set_attrs:
    ld a, (menu_config_visible_h)
    bit 0, a
    ret z
    ld hl, 0x5a32
    ld b, 3
    call menu_config_header_span
    ld a, (menu_config_defined_h)
    bit 0, a
    jr nz, menu_config_set_selected_ready
    ld a, 0xff
    jr menu_config_set_selected_store
menu_config_set_selected_ready:
    ld a, (_setup_choice + 5)
menu_config_set_selected_store:
    ld (menu_config_piece_selected), a
    ld a, (menu_config_cursor)
    cp ROW_SET
    jr nz, menu_config_set_not_cursor
    ld a, (menu_config_piece_set)
    ld l, a
    jp _menu_config_piece_set_options_asm
menu_config_set_not_cursor:
    ld l, 0xff
    jp _menu_config_piece_set_options_asm

menu_config_option_span:
    call menu_config_option_marker
    ld a, e
    or a
    jr z, menu_config_option_not_focused
    ; Ink-only focus: ULA cells are 8 pixels and the font is 4, so a paper
    ; box always spills into the neighbouring glyph. Bright ink on black
    ; paper leaves padding spaces invisible.
    ld a, d
    or a
    ld a, 0x47
    jr z, menu_config_attr_span
    dec a
    jr menu_config_attr_span
menu_config_option_not_focused:
    ld a, d
    or a
    ld a, 0x05
    jr nz, menu_config_attr_span
    ld a, 0x07
menu_config_attr_span:
    ld (hl), a
    inc hl
    djnz menu_config_attr_span
    ret

; HL = first option attribute cell. E != 0 only for the focused option.
menu_config_option_marker:
    push bc
    push de
    push hl
    ld b, 0
    ld a, e
    or a
    jr z, menu_config_marker_focus_ready
    inc b
menu_config_marker_focus_ready:
    ld a, h
    sub 0x58
    add a, a
    add a, a
    add a, a
    ld d, a
    ld a, l
    and 0xe0
    rlca
    rlca
    rlca
    or d
    ld d, a
    ld a, l
    and 0x1f
    add a, a
    ld e, a
    ld a, d
    cp 14
    jr c, menu_config_marker_col
    cp 19
    jr nc, menu_config_marker_col
    dec e
menu_config_marker_col:
    ld a, b
    or a
    ld c, 0x03
    jr z, menu_config_marker_attr_ready
    ld c, 0x47
menu_config_marker_attr_ready:
    ld a, d
    cp 14
    jr c, menu_config_marker_attr_store
    cp 19
    jr nc, menu_config_marker_attr_store
    dec hl
menu_config_marker_attr_store:
    ld (hl), c

    ld a, e
    srl a
    ld c, a
    ld a, d
    and 0x18
    add a, 0x40
    ld h, a
    ld a, d
    and 0x07
    rrca
    rrca
    rrca
    add a, c
    ld l, a
    bit 0, e
    ld de, menu_config_marker_left
    jr z, menu_config_marker_pixels
    ld de, menu_config_marker_right
menu_config_marker_pixels:
    inc h
    ld c, 6
menu_config_marker_scan:
    ld a, (de)
    cpl
    and (hl)
    ld (hl), a
    ld a, b
    or a
    jr z, menu_config_marker_next
    ld a, (de)
    or (hl)
    ld (hl), a
menu_config_marker_next:
    inc de
    inc h
    dec c
    jr nz, menu_config_marker_scan
    pop hl
    pop de
    pop bc
    ret

; 4x8 glyph; the cell supplies blank top and bottom scanlines.
menu_config_marker_left:
    DEFB 0x00, 0x40, 0x20, 0x20, 0x40, 0x00
menu_config_marker_right:
    DEFB 0x00, 0x04, 0x02, 0x02, 0x04, 0x00

_menu_config_validate_ip_ovl_entry:
    ld l, 0
    ret

_menu_config_piece_set_options_asm:
    ld c, l
    ld b, 0
    ld hl, 0x5a36
    ld d, 2
    call menu_config_piece_set_option_span
    ld b, 1
    ld hl, 0x5a39
    ld d, 2
    call menu_config_piece_set_option_span
    ld b, 2
    ld hl, 0x5a3c
    ld d, 2
menu_config_piece_set_option_span:
    ld e, 0
    ld a, c
    cp b
    jr nz, menu_config_piece_set_marker
    inc e
menu_config_piece_set_marker:
    call menu_config_option_marker
    call menu_config_piece_set_option_attr
menu_config_piece_set_option_store:
    ld (hl), a
    inc hl
    dec d
    jr nz, menu_config_piece_set_option_store
    ret

menu_config_piece_set_option_attr:
    ld a, c
    cp b
    jr z, menu_config_piece_set_option_focus
    ld a, (menu_config_piece_selected)
    cp b
    jr z, menu_config_piece_set_option_selected
    ld a, 0x07
    ret
menu_config_piece_set_option_focus:
    ld a, (menu_config_piece_selected)
    cp b
    ld a, 0x47
    ret nz
    dec a
    ret
menu_config_piece_set_option_selected:
    ld a, 0x06
    ret

; Swatches read the per-platform theme attrs straight from the resident DAT.
; Next replaces only wood with its dedicated ULA+ group-3 colour pair.
IFDEF NETCHESSZX_NEXT_BANKING
menu_config_dat_light_attrs EQU 0x3500 + 786
ELSE
menu_config_dat_light_attrs EQU 0x6000 + 786
ENDIF

menu_config_board_swatches:
    ld c, l
    ld hl, 0x5a16
    ld b, 0
    ld de, menu_config_dat_light_attrs
menu_config_board_swatch_loop:
    ld a, (de)
    push de
    call menu_config_board_swatch
    pop de
    inc de
    inc b
    ld a, b
    cp 5
    jr nz, menu_config_board_swatch_loop
    ret
menu_config_board_swatch:
IFDEF NETCHESSZX_NEXT
    cp 0x37
    jr nz, menu_config_board_swatch_attr_ready
    ld a, 0xc1
menu_config_board_swatch_attr_ready:
ENDIF
    push af
    ld e, 0
    ld a, c
    cp b
    jr nz, menu_config_board_swatch_marker
    inc e
menu_config_board_swatch_marker:
    call menu_config_option_marker
    pop af
    ld e, a
    ld a, c
    cp b
    ld a, e
    jr z, menu_config_board_swatch_focus
    ld a, b
    add a, 5
    cp c
    ld a, e
    jr nz, menu_config_board_swatch_store
menu_config_board_swatch_focus:
IFDEF NETCHESSZX_NEXT
    ; Keep the ULA+ palette group selected by the theme attribute.
ELSE
    or 0x40
ENDIF
menu_config_board_swatch_store:
    ld (hl), a
    inc hl
    ld (hl), 0x07
    inc hl
    ; Keep the original 4-pixel bar under the chip as a second focus cue.
    ld a, b
    add a, a
    add a, 0x16
    ld e, a
    ld d, 0x57
    ld a, c
    cp b
    ld a, 0xf0
    jr z, menu_config_board_swatch_bar
    xor a
menu_config_board_swatch_bar:
    ld (de), a
    ret

menu_config_line_game:
    DEFB 7, "GAME  CREATE    JOIN", 0
menu_config_line_link:
    DEFB 8, "LINK  MQTT      DIRECT", 0
menu_config_line_color:
    DEFB 14, "COLOR  WHITE     BLACK", 0
menu_config_line_notation:
    DEFB 15, "NOTAT  COORD     SAN", 0
menu_config_line_board:
    DEFB 16, "BOARD  ", 127, "   ", 127, "   ", 127, "   ", 127, "   ", 127, 0
menu_config_line_set:
IFDEF NETCHESSZX_NEXT
    DEFB 17, "SET    CALI  MPCH  TOTY", 0
ELSE
    DEFB 17, "SET    BRRY  SPCY  PIXL", 0
ENDIF
menu_config_line_hints:
    DEFB 18, "HINTS  OFF       ON", 0
menu_config_binary_attrs:
    DEFB 1, 0x00, 3, 4, 3
    DEFB 2, 0x41, 3, 3, 4
    DEFB 32, 0x05, 4, 3, 3
    DEFB 64, 0x06, 4, 3, 2
    DEFB 2, 0x89, 4, 2, 1
    DEFB 0
menu_config_values:
    DEFB 0
menu_config_visible_l:
    DEFB 0
menu_config_visible_h:
    DEFB 0
menu_config_defined_l:
    DEFB 0
menu_config_defined_h:
    DEFB 0
menu_config_cursor:
    DEFB 0
menu_config_focus_bits:
    DEFB 0
menu_config_board_theme:
    DEFB 0
menu_config_piece_set:
    DEFB 0
menu_config_piece_selected:
    DEFB 0
menu_config_binary_mask:
    DEFB 0
menu_config_binary_row:
    DEFB 0
menu_config_binary_bit:
    DEFB 0
menu_config_binary_invert:
    DEFB 0
menu_config_defined_flag:
    DEFB 0
menu_config_value_flag:
    DEFB 0
menu_config_focus_value:
    DEFB 0
menu_config_row_focus:
    DEFB 0
menu_config_option_width:
    DEFB 0
menu_config_render_dirty:
    DEFW 0
menu_config_nav_scratch:
    DEFB 0
menu_config_paint_mask:
    DEFW 0
