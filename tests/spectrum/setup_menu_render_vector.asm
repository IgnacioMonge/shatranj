SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _spectrum_info_line
PUBLIC _spectrum_info_show_setup
PUBLIC _spectrum_info_show_game_setup
PUBLIC _setup_focus_choice
PUBLIC _setup_focus_board_theme
PUBLIC _setup_visible_mask
PUBLIC _setup_cursor
PUBLIC _setup_room_editing
PUBLIC _setup_edit_row
PUBLIC _setup_port_text
PUBLIC _setup_timezone_text
PUBLIC _setup_config_dirty
PUBLIC _setup_game_focus
PUBLIC _setup_time_focus
PUBLIC _setup_action_focus
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host
PUBLIC _netchesszx_board_theme_index
PUBLIC _last_ip
PUBLIC _spectrum_overlay_context
PUBLIC _setup_choice
PUBLIC _setup_defined_mask
PUBLIC _edit_max

EXTERN _menu_config_paint_attrs_ovl_entry
EXTERN _menu_config_render_ovl_entry
EXTERN _menu_config_nav_ovl_entry
EXTERN _input_edit_setup_line_ovl
EXTERN _spectrum_gui_edit_show

test_start:
    jp test_current_contract

test_current_contract:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld (_spectrum_overlay_context), hl
    ld (_setup_defined_mask), hl
    xor a
    ld (_setup_cursor), a
    ld (_setup_config_dirty), a
    ld a, 1
    ld (render_context + 2), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    cp 1
    jp nz, test_bad_current_full
    ld a, (game_setup_calls)
    cp 1
    jp nz, test_bad_current_full
    xor a
    ld (render_context + 2), a
    ld a, 0xff
    ld (render_context + 3), a
    ld (line_calls), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    cp 1
    jp nz, test_bad_current_incremental
    ld a, (line_calls)
    or a
    jp z, test_bad_current_incremental

    ; JOIN+MQTT renders NC#### while the editor cursor starts after NC.
    ld hl, test_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld a, 4
    ld (render_context), a
    ld de, render_context
    call test_render_setup
    ld hl, (last_line_ptr)
    ld de, 7
    add hl, de
    ld de, test_room
    ld b, 6
test_current_room_text:
    ld a, (de)
    cp (hl)
    jp nz, test_bad_current_room
    inc de
    inc hl
    djnz test_current_room_text
    ld a, (last_edit_col)
    cp 45
    jp nz, test_bad_current_room

    ; A masked attribute repaint touches BOARD but leaves GAME unchanged.
    ld a, 0xa5
    ld (0x58f5), a
    ld (0x5a15), a
    ld a, 0x80
    ld (0x5fe1), a
    xor a
    ld (0x5fe2), a
    ld a, 7
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x58f5)
    cp 0xa5
    jp nz, test_bad_current_mask
    ld a, (0x5a15)
    cp 0xa5
    jp z, test_bad_current_mask

    ; GAME gets the 4x8 chevron marker. Horizontal focus clears the old
    ; marker; vertical focus clears both markers on the old row.
    xor a
    ld (_setup_cursor), a
    ld (_setup_focus_choice), a
    ld a, 1
    ld (0x5fe1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x41f5
    call test_marker_left_on
    ld hl, 0x41fa
    call test_marker_left_off

    ld a, 1
    ld (_setup_focus_choice), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x41f5
    call test_marker_left_off
    ld hl, 0x41fa
    call test_marker_left_on

    ld a, 1
    ld (_setup_cursor), a
    ld (_setup_focus_choice + 1), a
    ld a, 3
    ld (0x5fe1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x41f5
    call test_marker_left_off
    ld hl, 0x41fa
    call test_marker_left_off
    ld hl, 0x4915
    call test_marker_left_on
    ld hl, 0x491a
    call test_marker_left_off

    ; GAME SETUP options start on an even half-column. Their marker occupies
    ; the right nibble: bit 1 is the line and bit 0 is the one-pixel gap.
    ld a, 5
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_choice + 2), a
    ld a, 0x20
    ld (0x5fe1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x49d5
    call test_marker_right_on

    xor a
    ld (0x5fe1), a

    ; BOARD keeps each chip's theme colour. Focus is BRIGHT plus the shared
    ; chevron marker and the 4-pixel bar on scanline 7.
    ld hl, 0x6312
IFDEF NETCHESSZX_NEXT
    ld (hl), 0x78
    inc hl
    ld (hl), 0x6f
    inc hl
    ld (hl), 0x66
    inc hl
    ld (hl), 0x77
    inc hl
    ld (hl), 0x37
ELSE
    ld (hl), 0x38
    inc hl
    ld (hl), 0x31
    inc hl
    ld (hl), 0x3a
    inc hl
    ld (hl), 0x29
    inc hl
    ld (hl), 0x62
ENDIF
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld (_setup_defined_mask), hl
    ld a, 7
    ld (_setup_cursor), a
    ld a, 0x80
    ld (0x5fe1), a
    xor a
    ld (0x5fe2), a
    ld (_setup_focus_board_theme), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5a16)
IFDEF NETCHESSZX_NEXT
    cp 0x78
ELSE
    cp 0x78
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x5716)
    cp 0xf0
    jp nz, test_bad_board_focus
    ld a, (0x5a18)
IFDEF NETCHESSZX_NEXT
    cp 0x6f
ELSE
    cp 0x31
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x5718)
    or a
    jp nz, test_bad_board_focus
    ld hl, 0x5115
    call test_marker_right_on
    ld hl, 0x5117
    call test_marker_right_off
    ld a, 1
    ld (_setup_focus_board_theme), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5a18)
IFDEF NETCHESSZX_NEXT
    cp 0x6f
ELSE
    cp 0x71
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x5716)
    or a
    jp nz, test_bad_board_focus
    ld a, (0x5718)
    cp 0xf0
    jp nz, test_bad_board_focus
    ld hl, 0x5115
    call test_marker_right_off
    ld hl, 0x5117
    call test_marker_right_on
    ld a, 2
    ld (_setup_focus_board_theme), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5a18)
IFDEF NETCHESSZX_NEXT
    cp 0x6f
ELSE
    cp 0x31
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x5a1a)
IFDEF NETCHESSZX_NEXT
    cp 0x66
ELSE
    cp 0x7a
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x571a)
    cp 0xf0
    jp nz, test_bad_board_focus
    ld a, (0x5718)
    or a
    jp nz, test_bad_board_focus
    ld hl, 0x5117
    call test_marker_right_off
    ld hl, 0x5119
    call test_marker_right_on
    ld a, 4
    ld (_setup_focus_board_theme), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5a16)
IFDEF NETCHESSZX_NEXT
    cp 0x78
ELSE
    cp 0x38
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x5a1e)
IFDEF NETCHESSZX_NEXT
    cp 0xc1
ELSE
    cp 0x62
ENDIF
    jp nz, test_bad_board_focus
    ld a, (0x571e)
    cp 0xf0
    jp nz, test_bad_board_focus
    ld a, (0x5a1f)
    cp 0x07
    jp nz, test_bad_board_focus
    ld a, (0x571f)
    or a
    jp nz, test_bad_board_focus
    ld hl, 0x5119
    call test_marker_right_off
    ld hl, 0x511d
    call test_marker_right_on
    xor a
    ld (0x5fe1), a
    ld (0x5fe2), a

    ; Worst-case DIRECT edit line. The host is deliberately longer than
    ; the 15-character render clamp, so the clamp is what truncates it;
    ; with a five-digit port that fills setup_edit_line_buf to its
    ; final byte and leaves zero margin. The
    ; Python runner reads both addresses from the map and compares the
    ; whole rendered line.
    xor a
    ld (line_calls), a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld (edit_line_row), a
    ld hl, test_long_ip
    ld de, _last_ip
    ld bc, 18
    ldir
    ld hl, test_long_port
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ld de, edit_line_row
    call _input_edit_setup_line_ovl
    ld a, (line_calls)
    cp 1
    jp nz, test_bad_edit_line_worst

    ; DIRECT: 15-char IP uses attrs 21-28. ':' shares PORT[0]'s cell at 29
    ; and inherits PORT colour. Five port digits occupy 29-31 on-screen.
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld (_setup_defined_mask), hl
    xor a
    ld (_last_ip + 15), a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_cursor), a
    ld hl, 0x493d
    ld de, test_colon_plain_pixels
    call test_copy_six_scanlines
    ld hl, 0x5935
    ld b, 11
    ld a, 0xa5
test_poison_direct_attrs:
    ld (hl), a
    inc hl
    djnz test_poison_direct_attrs
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x5935
    ld b, 8
test_direct_ip_focus_attrs:
    ld a, (hl)
    cp 0x46
    jp nz, test_bad_direct_ip_focus_attr
    inc hl
    djnz test_direct_ip_focus_attrs
    ld a, (0x593d)
    cp 0x05
    jp nz, test_bad_direct_colon_attr
    ld hl, 0x493d
    call test_colon_plain
    ld a, 3
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x593c)
    cp 0x05
    jp nz, test_bad_direct_ip_idle_attr
    ld hl, 0x593d
    ld b, 3
test_direct_port_focus_attrs:
    ld a, (hl)
    cp 0x46
    jp nz, test_bad_direct_port_focus_attr
    inc hl
    djnz test_direct_port_focus_attrs
    ld hl, 0x493d
    call test_colon_marked
    ld a, 2
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x493d
    call test_colon_plain

    ld a, 1
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld (edit_line_row), a
    ld a, 15
    ld (_edit_max), a
    ld de, edit_line_row
    call _input_edit_setup_line_ovl
    ld a, (last_edit_col)
    cp 43
    jp nz, test_bad_direct_edit_col
    ld a, 3
    ld (_setup_edit_row), a
    ld (edit_line_row), a
    ld a, 5
    ld (_edit_max), a
    ld de, edit_line_row
    call _input_edit_setup_line_ovl
    ld a, (last_edit_col)
    cp 59
    jp nz, test_bad_direct_edit_col
    xor a
    ld (_setup_room_editing), a

    ; Completed CREATE: CONNECTION changes must not touch GAME SETUP.
    xor a
    ld (line_calls), a
    ld (game_setup_calls), a
    ld a, 0xa5
    ld (0x48d2), a
    ld (0x48d3), a
    ld (0x48d4), a
    ld (0x48f2), a
    ld (0x5a15), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld hl, 0x0004
    ld (_spectrum_overlay_context), hl
    xor a
    ld (render_context + 2), a
    ld a, 0xfe
    ld (render_context + 3), a
    ld de, render_context
    call test_render_setup
    ld a, (game_setup_calls)
    or a
    jp nz, test_bad_endpoint_idle
    ld a, (line_calls)
    cp 4
    jp nz, test_bad_endpoint_idle
    ld a, (0x48d2)
    cp 0xa5
    jp nz, test_bad_endpoint_idle
    ld a, (0x48f2)
    cp 0xa5
    jp nz, test_bad_endpoint_idle
    ld a, (0x5a15)
    cp 0xa5
    jp nz, test_bad_endpoint_idle

    ; CREATE -> JOIN removes COLOR. Do not clear or redraw NOTAT and below.
    xor a
    ld (line_calls), a
    ld (game_setup_calls), a
    ld hl, 0x07df
    ld (_setup_visible_mask), hl
    ld de, render_context
    call test_render_setup
    ld a, (game_setup_calls)
    or a
    jp nz, test_bad_hidden_color
    ld a, (0x48d2)
    or a
    jp nz, test_bad_hidden_color
    ld a, (0x48f2)
    cp 0xa5
    jp nz, test_bad_hidden_color
    ld a, (0x5a15)
    cp 0xa5
    jp nz, test_bad_hidden_color

    ; JOIN -> CREATE restores COLOR without redrawing the GAME SETUP header.
    xor a
    ld (line_calls), a
    ld (game_setup_calls), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld de, render_context
    call test_render_setup
    ld a, (game_setup_calls)
    or a
    jp nz, test_bad_endpoint_color
    ld a, (line_calls)
    cp 5
    jp nz, test_bad_endpoint_color
    ld a, (0x48f2)
    cp 0xa5
    jp nz, test_bad_endpoint_color
    ld a, (0x5a15)
    cp 0xa5
    jp nz, test_bad_endpoint_color

    xor a
    jp test_store_result

test_bad_current_full:       ld a, 23
    jp test_store_result
test_bad_current_incremental: ld a, 24
    jp test_store_result
test_bad_current_room:       ld a, 25
    jp test_store_result
test_bad_current_mask:       ld a, 26
    jp test_store_result
test_bad_edit_line_worst:    ld a, 27
    jp test_store_result
test_bad_hidden_color:       ld a, 28
    jp test_store_result
test_bad_direct_colon_attr:  ld a, 29
    jp test_store_result
test_bad_direct_ip_focus_attr: ld a, 30
    jp test_store_result
test_bad_direct_port_focus_attr: ld a, 31
    jp test_store_result
test_bad_direct_edit_col:    ld a, 32
    jp test_store_result
test_bad_direct_ip_idle_attr: ld a, 33
    jp test_store_result
test_bad_endpoint_idle:      ld a, 34
    jp test_store_result
test_bad_endpoint_color:     ld a, 35
    jp test_store_result
test_bad_board_focus:        ld a, 36
    jp test_store_result
test_bad_focus_marker:       ld a, 37
    jp test_store_result
test_bad_port_colon_pixels:  ld a, 38
    jp test_store_result

    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld (0x5fe3), a

    ; Production DAT board-light attribute table.
    ld hl, 0x6312
IFDEF NETCHESSZX_NEXT
    ld (hl), 0x78
    inc hl
    ld (hl), 0x6f
    inc hl
    ld (hl), 0x66
    inc hl
    ld (hl), 0x77
    inc hl
    ld (hl), 0x37
ELSE
    ld (hl), 0x38
    inc hl
    ld (hl), 0x31
    inc hl
    ld (hl), 0x3a
    inc hl
    ld (hl), 0x29
    inc hl
    ld (hl), 0x62
ENDIF
    xor a
    ld (_setup_cursor), a
    ld (_setup_game_focus), a
    ld (_setup_focus_board_theme), a
    ld (_setup_focus_choice + 2), a
    ld (_setup_focus_choice + 3), a
    ld (_setup_focus_choice + 4), a
    ld (_setup_focus_choice + 5), a
    ld a, 1
    ld (_setup_focus_choice + 1), a
    ld (_setup_config_dirty), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld a, 1
    ld (render_context + 2), a
    ld de, render_context
    call test_render_setup
    xor a
    ld (render_context + 2), a

    ld a, (0x58f2)
    cp 0x03
    jp nz, test_bad_label
    ld a, (0x58f5)
    cp 0x39
    jp nz, test_bad_game
    ld a, (0x58f9)
    cp 0x06
    jp nz, test_bad_game
    ld a, (0x5a35)
    cp 0x06
    jp nz, test_bad_set
    ld a, (0x5a99)
    cp 0x45
    jp nz, test_bad_action
    ld a, (0x5915)
    cp 0x38
    jp nz, test_bad_input_box

    ; JOIN + DIRECT paints its editable IP box before focus enters IP.
    ld a, 1
    ld (_setup_focus_choice), a
    xor a
    ld (_setup_focus_choice + 1), a
    ld a, 0x07
    ld (render_context), a
    ld de, render_context
    call test_render_setup
    xor a
    ld (render_context), a
    ld hl, 0x5915
    ld b, 8
test_ip_input_span:
    ld a, (hl)
    cp 0x38
    jp nz, test_bad_input_box
    inc hl
    djnz test_ip_input_span
    ld a, (0x5a9d)
    cp 0x44
    jp nz, test_bad_action

    ; One full attribute cell per swatch; selected theme gets a yellow tick.
IFDEF NETCHESSZX_NEXT
    ld a, (0x5a15)
    cp 0x78
ELSE
    ld a, (0x5a15)
    cp 0x38
ENDIF
    jp nz, test_bad_swatch_attr
    ld a, (0x5a16)
    cp 0x06
    jp nz, test_bad_swatch_spacer
    ld a, (0x5115)
    cp 0x0f
    jp nz, test_bad_swatch_regular
    ld a, (0x5516)
    cp 0x3c
    jp nz, test_bad_swatch_marker
    ld a, (0x5616)
    cp 0x3c
    jp nz, test_bad_swatch_marker

    ; Focus swaps INK/PAPER and theme five never spills into col 31.
    ld a, 6
    ld (_setup_cursor), a
    ld a, 4
    ld (_netchesszx_board_theme_index), a
    ld a, 0xa5
    ld (0x511e), a
    ld a, 0xa5
    ld (0x58f2), a
    call _menu_config_paint_attrs_ovl_entry
IFDEF NETCHESSZX_NEXT
    ld a, (0x5a1e)
    cp 0xc8
ELSE
    ld a, (0x5a1e)
    cp 0x54
ENDIF
    jp nz, test_bad_swatch_five
    ld a, (0x5a1f)
    cp 0x07
    jp nz, test_bad_swatch_spacer
    ld a, (0x511e)
    cp 0xa5
    jp nz, test_bad_swatch_focus
    ld a, (0x521e)
    cp 0x0f
    jp nz, test_bad_swatch_focus
    ld a, (0x511f)
    or a
    jp nz, test_bad_swatch_spill
    ld a, (0x5516)
    cp 0x3c
    jp nz, test_bad_swatch_marker
    ld a, (0x551f)
    or a
    jp nz, test_bad_swatch_marker
    ld a, (0x58f2)
    cp 0xa5
    jp nz, test_bad_masked_paint

    ; Confirming a preview moves only the persistent marker and BOARD attrs.
    ld a, 4
    ld (_setup_focus_board_theme), a
    ld a, 0x40
    ld (render_context), a
    ld de, render_context
    call test_render_setup
    xor a
    ld (render_context), a
    ld a, (0x5516)
    or a
    jp nz, test_bad_swatch_marker
    ld a, (0x551f)
    cp 0x3c
    jp nz, test_bad_swatch_marker
    ld a, (0x561f)
    cp 0x3c
    jp nz, test_bad_swatch_marker
    ld a, (0x5a1f)
    cp 0x06
    jp nz, test_bad_swatch_marker

    ; DIRECT needs four cells at its odd half-column; MQTT needs three.
    xor a
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_game_focus), a
    xor a
    ld (_setup_focus_choice + 1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x58f9
    ld b, 4
test_direct_span:
    ld a, (hl)
    cp 0x39
    jp nz, test_bad_game_width
    inc hl
    djnz test_direct_span
    ld a, (hl)
    cp 0x07
    jp nz, test_bad_game_width
    ld a, 1
    ld (_setup_focus_choice + 1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x58f9
    ld b, 3
test_mqtt_span:
    ld a, (hl)
    cp 0x39
    jp nz, test_bad_game_width
    inc hl
    djnz test_mqtt_span
    ld a, (hl)
    cp 0x07
    jp nz, test_bad_game_width

    ; PORT input is exactly five characters: three attribute cells.
    ld a, 2
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_choice + 1), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x5935
    ld b, 3
test_port_span:
    ld a, (hl)
    cp 0x39
    jp nz, test_bad_port_width
    inc hl
    djnz test_port_span
    ld a, (hl)
    cp 0x07
    jp nz, test_bad_port_width

    ; An ACTION-only dirty render emits one line and never clears the panel.
    xor a
    ld (line_calls), a
    ld (setup_calls), a
    ld (game_setup_calls), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    or a
    jp nz, test_bad_incremental
    ld a, (game_setup_calls)
    or a
    jp nz, test_bad_incremental
    ld a, (line_calls)
    cp 1
    jp nz, test_bad_incremental
    ld a, (last_line_row)
    cp 20
    jp nz, test_bad_incremental

    ; Full render still owns the one required panel clear and all ten rows.
    xor a
    ld (line_calls), a
    ld (setup_calls), a
    ld (game_setup_calls), a
    ld a, 1
    ld (render_context + 2), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    cp 1
    jp nz, test_bad_full
    ld a, (game_setup_calls)
    cp 1
    jp nz, test_bad_full
    ld a, (line_calls)
    cp 10
    jp nz, test_bad_full

    ; First-run rendering hides GAME SETUP until CONNECTION SETUP completes.
    xor a
    ld (line_calls), a
    ld (setup_calls), a
    ld (game_setup_calls), a
    ld hl, 0x000f
    ld (_setup_visible_mask), hl
    ld a, 1
    ld (render_context + 2), a
    ld a, 0xff
    ld (render_context + 3), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    cp 1
    jp nz, test_bad_hidden_setup
    ld a, (game_setup_calls)
    or a
    jp nz, test_bad_hidden_setup
    ld a, (line_calls)
    cp 4
    jp nz, test_bad_hidden_setup
    ld a, (0x5a15)
    cp 0x07
    jp nz, test_bad_hidden_attrs
    ld a, (0x5a99)
    cp 0x07
    jp nz, test_bad_hidden_attrs

    ; The one-time reveal draws only GAME SETUP and its six rows.
    ld a, 3
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, 4
    ld (_setup_cursor), a
    xor a
    ld (line_calls), a
    ld (setup_calls), a
    ld (game_setup_calls), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    xor a
    ld (render_context + 2), a
    ld a, 0xfe
    ld (render_context + 3), a
    ld de, render_context
    call test_render_setup
    ld a, (setup_calls)
    or a
    jp nz, test_bad_hidden_setup
    ld a, (game_setup_calls)
    cp 1
    jp nz, test_bad_hidden_setup
    ld a, (line_calls)
    cp 6
    jp nz, test_bad_hidden_setup
    ld a, (0x5955)
    cp 0x06
    jp nz, test_bad_reveal_focus
    ld a, 0xff
    ld (render_context + 3), a

    xor a
    ld (_setup_config_dirty), a
    ld a, 9
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_action_focus), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5a9d)
    cp 0x60
    jp nz, test_bad_start
    ld a, (0x5a9c)
    cp 0x07
    jp nz, test_bad_start

    ; SAVE/EDIT uses its cyan ink as paper with high-contrast black ink.
    ld a, 1
    ld (_setup_config_dirty), a
    xor a
    ld (_setup_action_focus), a
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x5a99
    ld b, 3
test_save_focus_span:
    ld a, (hl)
    cp 0x68
    jp nz, test_bad_save_focus
    inc hl
    djnz test_save_focus_span
    ld a, (hl)
    cp 0x07
    jp nz, test_bad_save_focus

    ; Vertical movement stays inside MENU_CONFIG and preserves SETUP rules.
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld a, 1
    ld (_setup_cursor), a
    ld a, 0x82
    ld (_spectrum_overlay_context), a
    ld de, _spectrum_overlay_context
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_bad_nav
    ld hl, (_spectrum_overlay_context + 1)
    ld de, 0x000a
    or a
    sbc hl, de
    jp nz, test_bad_nav

    ld a, 4
    ld (_setup_game_focus), a
    ld a, 1
    ld (_setup_focus_board_theme), a
    ld a, 7
    ld (_setup_cursor), a
    ld a, 0x82
    ld (_spectrum_overlay_context), a
    ld de, _spectrum_overlay_context
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 8
    jp nz, test_bad_nav
    ld a, (_setup_focus_board_theme)
    cp 4
    jp nz, test_bad_nav
    ld hl, (_spectrum_overlay_context + 1)
    ld de, 0x0180
    or a
    sbc hl, de
    jp nz, test_bad_nav

    ld a, 1
    ld (_setup_config_dirty), a
    ld a, 9
    ld (_setup_cursor), a
    ld a, 0x82
    ld (_spectrum_overlay_context), a
    ld de, _spectrum_overlay_context
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 10
    jp nz, test_bad_nav
    ld a, (_setup_action_focus)
    or a
    jp nz, test_bad_nav
    ld a, (_spectrum_overlay_context + 6)
    cp 0x22
    jp nz, test_bad_nav
    xor a
    jp test_store_result

test_bad_label:          ld a, 1
    jp test_store_result
test_bad_game:           ld a, 2
    jp test_store_result
test_bad_set:            ld a, 3
    jp test_store_result
test_bad_action:         ld a, 4
    jp test_store_result
test_bad_swatch_attr:    ld a, 5
    jp test_store_result
test_bad_swatch_spacer:  ld a, 6
    jp test_store_result
test_bad_swatch_regular: ld a, 7
    jp test_store_result
test_bad_swatch_five:    ld a, 8
    jp test_store_result
test_bad_swatch_focus:   ld a, 9
    jp test_store_result
test_bad_swatch_spill:   ld a, 10
    jp test_store_result
test_bad_game_width:     ld a, 11
    jp test_store_result
test_bad_port_width:     ld a, 12
    jp test_store_result
test_bad_incremental:    ld a, 13
    jp test_store_result
test_bad_full:           ld a, 14
    jp test_store_result
test_bad_start:          ld a, 15
    jp test_store_result
test_bad_save_focus:     ld a, 16
    jp test_store_result
test_bad_swatch_marker:  ld a, 17
    jp test_store_result
test_bad_masked_paint:   ld a, 18
    jp test_store_result
test_bad_input_box:      ld a, 19
    jp test_store_result
test_bad_hidden_setup:   ld a, 20
    jp test_store_result
test_bad_hidden_attrs:   ld a, 21
    jp test_store_result
test_bad_reveal_focus:   ld a, 22
    jp test_store_result
test_bad_nav:            ld a, 23
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_render_setup:
    call _menu_config_render_ovl_entry
    ld de, render_edit_all
    jp _input_edit_setup_line_ovl

_spectrum_info_line:
    ld (last_line_ptr), hl
    ld a, (hl)
    ld (last_line_row), a
    ld hl, line_calls
    inc (hl)
    ret

_spectrum_info_show_setup:
    ld hl, setup_calls
    inc (hl)
    ret

_spectrum_info_show_game_setup:
    ld hl, game_setup_calls
    inc (hl)
    ret

test_marker_left_on:
    ld de, test_marker_left_pixels
    ld c, 0xf0
    jr test_marker_on_scan
test_marker_left_off:
    ld b, 6
test_marker_left_scan_loop:
    ld a, (hl)
    and 0xf0
    jp nz, test_bad_focus_marker
    inc h
    djnz test_marker_left_scan_loop
    ret

test_marker_right_off:
    ld b, 6
test_marker_right_scan_loop:
    ld a, (hl)
    and 0x0f
    jp nz, test_bad_focus_marker
    inc h
    djnz test_marker_right_scan_loop
    ret

test_marker_right_on:
    ld de, test_marker_right_pixels
    ld c, 0x0f
test_marker_on_scan:
    ex de, hl
    ld b, 6
test_marker_on_scan_loop:
    ld a, (de)
    and c
    cp (hl)
    jp nz, test_bad_focus_marker
    inc d
    inc hl
    djnz test_marker_on_scan_loop
    ret

test_marker_left_pixels:
    DEFB 0x00, 0x40, 0x20, 0x20, 0x40, 0x00
test_marker_right_pixels:
    DEFB 0x00, 0x04, 0x02, 0x02, 0x04, 0x00

test_copy_six_scanlines:
    ld b, 6
test_copy_six_scanlines_loop:
    ld a, (de)
    ld (hl), a
    inc de
    inc h
    djnz test_copy_six_scanlines_loop
    ret

test_colon_plain:
    ld de, test_colon_plain_pixels
    jr test_colon_scan
test_colon_marked:
    ld de, test_colon_marked_pixels
test_colon_scan:
    ld b, 6
test_colon_scan_loop:
    ld a, (de)
    cp (hl)
    jp nz, test_bad_port_colon_pixels
    inc de
    inc h
    djnz test_colon_scan_loop
    ret

test_colon_plain_pixels:
    DEFB 0x0b, 0x0b, 0x8b, 0x0b, 0x0b, 0x8b
test_colon_marked_pixels:
    DEFB 0x0b, 0x4b, 0x2b, 0x2b, 0x4b, 0x0b

render_context:
    DEFB 0x00, 0x02, 0x00, 0xff
render_edit_all:
    DEFB 0xff
line_calls: DEFB 0
setup_calls: DEFB 0
game_setup_calls: DEFB 0
last_line_row: DEFB 0
last_line_ptr: DEFW 0
last_edit_col: DEFB 0
test_room: DEFB "NC1234",0
edit_line_row: DEFB 0
test_long_ip: DEFB "192.168.100.20055", 0
test_long_port: DEFB "65535", 0
test_result: DEFB 0xff
_setup_focus_choice: DEFS 7, 0
_setup_focus_board_theme: DEFB 0
_setup_visible_mask: DEFW 0x03ff
_setup_cursor: DEFB 0
_setup_room_editing: DEFB 0
_setup_edit_row: DEFB 0
_setup_port_text: DEFS 6, 0
_setup_timezone_text: DEFS 4, 0
_setup_config_dirty: DEFB 0
_setup_game_focus: DEFB 0
_setup_time_focus: DEFB 0
_setup_action_focus: DEFB 0
_netchesszx_mqtt_code: DEFS 17, 0
_netchesszx_direct_host: DEFS 16, 0
_netchesszx_board_theme_index: DEFB 0
_last_ip: DEFS 18, 0
_spectrum_overlay_context: DEFW 0
_setup_choice: DEFS 7, 0
_setup_defined_mask: DEFW 0
_edit_max: DEFB 0
_spectrum_gui_edit_show:
    ld a, h
    ld (last_edit_col), a
    ret
