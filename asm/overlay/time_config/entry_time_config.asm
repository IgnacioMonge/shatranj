SECTION code_user

IFDEF NETCHESSZX_NEXT_BANKING
INCLUDE "asm/next/extension_bank_layout.asm"
ENDIF

PUBLIC _time_config_ui_ovl_entry
PUBLIC _time_config_step_ovl_entry
PUBLIC _time_config_init_ovl_entry
PUBLIC _time_config_commit_ovl_entry

EXTERN _spectrum_info_line
EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key
EXTERN _spectrum_gui_edit_hide
EXTERN _spectrum_gui_edit_show
EXTERN _spectrum_append_u16
EXTERN _setup_choice
EXTERN _setup_focus_choice
EXTERN _setup_focus_board_theme
EXTERN _setup_game_focus
EXTERN _setup_defined_mask
EXTERN _setup_visible_mask
EXTERN _setup_cursor
EXTERN _setup_room_editing
EXTERN _setup_edit_row
EXTERN _setup_timezone_text
EXTERN _setup_timezone_value
EXTERN _setup_config_dirty
EXTERN _setup_time_focus
EXTERN _setup_action_focus
EXTERN _setup_port_text
EXTERN _netchesszx_session_role
EXTERN _netchesszx_transport
EXTERN _netchesszx_local_color
EXTERN _netchesszx_host_color
EXTERN _netchesszx_host_color_ready
EXTERN _netchesszx_notation
EXTERN _netchesszx_movement_hints
EXTERN _netchesszx_board_theme_index
EXTERN _netchesszx_piece_set_index
EXTERN _netchesszx_timezone
EXTERN _netchesszx_timezone_last
EXTERN _netchesszx_rtc_available
EXTERN _netchesszx_direct_port
EXTERN _edit_buf
EXTERN _edit_max

CTX             EQU 0x5FE0
CTX_KEY         EQU CTX + 0
CTX_FORCE_LO    EQU CTX + 1
CTX_FORCE_HI    EQU CTX + 2
CTX_CLEAR_FROM  EQU CTX + 3
CTX_ACTION      EQU CTX + 4
CTX_NOTICE      EQU CTX + 5
CTX_FLAGS       EQU CTX + 6

KEY_LEFT        EQU 0x83
KEY_RIGHT       EQU 0x84
KEY_END         EQU 0x89
KEY_CANCEL      EQU 0x8a
FLAG_RENDER     EQU 0x01
FLAG_PAINT      EQU 0x02
FLAG_EDIT       EQU 0x04
FLAG_SUPPRESS   EQU 0x08
FLAG_TIME_UI    EQU 0x10
FLAG_ACTION_UI  EQU 0x20
ACTION_START    EQU 3
ACTION_SAVE     EQU 4
ACTION_TIMEZONE EQU 5
NOTICE_SELECT   EQU 1
NOTICE_BAD_TZ   EQU 6
ROW_TIME        EQU 4
ROW_COLOR       EQU 5
ROW_NOTATION    EQU 6
ROW_ACTION      EQU 10
MASK_ALL        EQU 0x07ff
INIT_CLEAN      EQU 0x01
INIT_COMPLETE   EQU 0x02
TIME_RTC        EQU 127
TC_PROBE_KEY       EQU 0xff
ZXUNOADDR       EQU 0xfc3b
M_GETDATE       EQU 0x8e
M_DRVAPI        EQU 0x92
I2CREG          EQU 0xf8
I2CADDR_W       EQU 0xa2
I2CADDR_R       EQU 0xa3
SCL0SDA0        EQU 0
SCL0SDA1        EQU 1
SCL1SDA0        EQU 2
SCL1SDA1        EQU 3

    DEFB 4
    DW _time_config_ui_ovl_entry
    DW _time_config_step_ovl_entry
    DW _time_config_init_ovl_entry
    DW _time_config_commit_ovl_entry

_time_config_init_ovl_entry:
    ld a, (CTX_KEY)
    cp TC_PROBE_KEY
    jp z, tc_probe
    ld (tc_init_flags), a
    ld hl, _setup_choice
    ld a, (_netchesszx_session_role)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_transport)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_host_color)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_notation)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_movement_hints)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_piece_set_index)
    ld (hl), a
    inc hl
    ld a, (_netchesszx_timezone)
    cp TIME_RTC
    jr nz, tc_init_numeric_time
    ld a, (_netchesszx_timezone_last)
    ld (_setup_timezone_value), a
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_init_utc
    ld a, 1
    jr tc_init_source
tc_init_numeric_time:
    ld (_setup_timezone_value), a
tc_init_utc:
    xor a
tc_init_source:
    ld (hl), a
    ld hl, _setup_choice
    ld de, _setup_focus_choice
    ld bc, 7
    ldir
    ld a, (_netchesszx_board_theme_index)
    ld (_setup_focus_board_theme), a
    ld (_setup_game_focus), a
    ld hl, 0
    ld a, (tc_init_flags)
    and INIT_COMPLETE
    jr z, tc_init_defined
    ld hl, MASK_ALL
tc_init_defined:
    ld (_setup_defined_mask), hl
    ld hl, 0
    ld (_setup_visible_mask), hl
    ld a, (tc_init_flags)
    and INIT_COMPLETE
    jr z, tc_init_cursor
    ld a, ROW_ACTION
tc_init_cursor:
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld a, (tc_init_flags)
    and INIT_CLEAN
    xor INIT_CLEAN
    ld (_setup_config_dirty), a
    ld a, (_setup_choice + 6)
    xor 1
    ld (_setup_time_focus), a
    ld a, (tc_init_flags)
    and INIT_CLEAN
    ld (_setup_action_focus), a
    ld hl, (_netchesszx_direct_port)
    push hl
    ld hl, _setup_port_text
    push hl
    call _spectrum_append_u16
    call tc_write_timezone_text_core
    ld a, FLAG_TIME_UI | FLAG_ACTION_UI
    ld (CTX_FLAGS), a
    jp tc_ret1

_time_config_commit_ovl_entry:
    ld a, (_setup_choice)
    ld (_netchesszx_session_role), a
    ld b, a
    inc a
    and 1
    ld (_netchesszx_host_color_ready), a
    ld a, (_setup_choice + 1)
    ld (_netchesszx_transport), a
    ld a, (_setup_choice + 2)
    ld (_netchesszx_host_color), a
    ld c, a
    ld a, b
    or a
    ld a, c
    jr z, tc_commit_local_color
    xor 1
tc_commit_local_color:
    ld (_netchesszx_local_color), a
    ld a, (_setup_choice + 3)
    ld (_netchesszx_notation), a
    ld a, (_setup_choice + 4)
    ld (_netchesszx_movement_hints), a
    ld a, (_setup_timezone_value)
    ld (_netchesszx_timezone_last), a
    ld a, (_setup_choice + 6)
    or a
    jr z, tc_commit_utc
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_commit_utc
    ld a, TIME_RTC
    jr tc_commit_timezone
tc_commit_utc:
    ld a, (_setup_timezone_value)
tc_commit_timezone:
    ld (_netchesszx_timezone), a
    jp tc_ret1

_time_config_step_ovl_entry:
    call tc_clear_ctx
    ld a, (CTX_KEY)
    ld (tc_key), a
    ld a, (_setup_room_editing)
    or a
    jp nz, tc_edit_key
    ld a, (_setup_cursor)
    cp ROW_ACTION
    jp z, tc_action_key
    cp ROW_TIME
    jp nz, tc_ret1
    ld a, (tc_key)
    cp KEY_LEFT
    jr z, tc_time_left
    cp KEY_RIGHT
    jr z, tc_time_right
    cp 13
    jp z, tc_time_select
    cp 32
    jp z, tc_time_select
    jp tc_ret1

tc_time_left:
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_time_right
    xor a
    jr tc_time_set_focus
tc_time_right:
    ld a, 1
tc_time_set_focus:
    ld (_setup_time_focus), a
tc_flag_paint:
    ld a, FLAG_TIME_UI
    jp tc_or_flags

tc_time_select:
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_begin_edit
    ld a, (_setup_time_focus)
    or a
    jr nz, tc_begin_edit
    ld a, 1
    ld (_setup_choice + 6), a
    jp tc_define_time

tc_begin_edit:
    call tc_begin_edit_bind
    ld a, FLAG_EDIT | FLAG_TIME_UI | FLAG_SUPPRESS
    jp tc_or_flags
tc_begin_edit_bind:
    ld a, ROW_TIME
    ld (_setup_edit_row), a
    ld a, 1
    ld (_setup_room_editing), a
    ld a, 2
    ld (_edit_max), a
    ld hl, _setup_timezone_text + 1
    jp _spectrum_gui_edit_bind

tc_edit_key:
    ld a, (tc_key)
    cp '+'
    jr z, tc_edit_sign
    cp '-'
    jr z, tc_edit_sign
    cp KEY_CANCEL
    jp z, tc_edit_cancel
    cp 8
    jr z, tc_edit_apply
    cp 13
    jp z, tc_edit_confirm
    cp 32
    jp z, tc_edit_confirm
    cp KEY_LEFT
    jr c, tc_edit_char
    cp KEY_END + 1
    jr c, tc_edit_apply
tc_edit_char:
    call tc_time_char
    or a
    jp z, tc_ret1
tc_edit_apply:
    ld l, a
    call _spectrum_gui_edit_key
    ld a, l
    or a
    jp z, tc_ret1
    ld a, FLAG_EDIT | FLAG_TIME_UI
    jp tc_or_flags

tc_edit_sign:
    ld (_setup_timezone_text), a
    ld a, FLAG_EDIT | FLAG_TIME_UI
    jp tc_or_flags

tc_time_char:
    cp '0'
    jr c, tc_time_invalid
    cp '9' + 1
    ret c
tc_time_invalid:
    xor a
    ret

tc_edit_cancel:
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    call tc_write_timezone_text_core
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, FLAG_EDIT | FLAG_TIME_UI | FLAG_SUPPRESS
    jp tc_or_flags

tc_edit_confirm:
    call tc_parse_timezone
    jr nz, tc_edit_ok
    ld a, NOTICE_BAD_TZ
    ld (CTX_NOTICE), a
    jp tc_ret1
tc_edit_ok:
    call _spectrum_gui_edit_hide
    call tc_write_timezone_text_core
    xor a
    ld (_setup_room_editing), a
    ld (_setup_choice + 6), a
tc_define_time:
    ld a, ACTION_TIMEZONE
    ld (CTX_ACTION), a
    ld hl, _setup_defined_mask
    set 4, (hl)
    call tc_mark_dirty
    ld a, (_setup_choice)
    or a
    ld a, ROW_COLOR
    jr z, tc_time_next
    ld a, ROW_NOTATION
tc_time_next:
    ld (_setup_cursor), a
    ld a, NOTICE_SELECT
    ld (CTX_NOTICE), a
    ld a, FLAG_RENDER | FLAG_TIME_UI | FLAG_ACTION_UI | FLAG_SUPPRESS
    jp tc_or_flags

tc_action_key:
    ld a, (tc_key)
    cp KEY_LEFT
    jr z, tc_action_left
    cp KEY_RIGHT
    jr z, tc_action_right
    cp 13
    jr z, tc_action_select
    cp 32
    jp nz, tc_ret1
tc_action_select:
    ld a, (_setup_action_focus)
    or a
    jr nz, tc_action_start
    ld a, (_setup_config_dirty)
    or a
    jr z, tc_action_edit
    ld a, ACTION_SAVE
    ld (CTX_ACTION), a
    jr tc_flag_suppress
tc_action_edit:
    xor a
    ld (_setup_cursor), a
    inc a
    ld (CTX_FORCE_LO), a
    ld a, FLAG_PAINT | FLAG_ACTION_UI | FLAG_SUPPRESS
    jp tc_or_flags
tc_action_start:
    ld a, ACTION_START
    ld (CTX_ACTION), a
tc_flag_suppress:
    ld a, FLAG_SUPPRESS
    jp tc_or_flags
tc_action_left:
    xor a
    jr tc_action_set_focus
tc_action_right:
    ld a, 1
tc_action_set_focus:
    ld (_setup_action_focus), a
    ld a, FLAG_ACTION_UI
    jp tc_or_flags

tc_mark_dirty:
    ld a, 1
    ld (_setup_config_dirty), a
    xor a
    ld (_setup_action_focus), a
    ret

tc_parse_timezone:
    ld hl, _setup_timezone_text
    ld c, 0
    ld a, (hl)
    cp '-'
    jr nz, tc_pt_plus
    inc c
    inc hl
    jr tc_pt_digits
tc_pt_plus:
    cp '+'
    jr nz, tc_pt_digits
    inc hl
tc_pt_digits:
    ld b, 0
    ld d, 0
tc_pt_loop:
    ld a, (hl)
    or a
    jr z, tc_pt_done
    cp '0'
    jr c, tc_pt_fail
    cp '9' + 1
    jr nc, tc_pt_fail
    sub '0'
    ld e, a
    ld a, d
    add a, a
    ld d, a
    add a, a
    add a, a
    add a, d
    add a, e
    ld d, a
    inc b
    inc hl
    jr tc_pt_loop
tc_pt_done:
    ld a, b
    or a
    jr z, tc_pt_fail
    cp 3
    jr nc, tc_pt_fail
    ld a, c
    or a
    ld a, d
    jr z, tc_pt_positive
    cp 12
    jr nc, tc_pt_fail
    neg
    ld (_setup_timezone_value), a
    or 1
    ret
tc_pt_positive:
    cp 14
    jr nc, tc_pt_fail
    ld (_setup_timezone_value), a
    or 1
    ret
tc_pt_fail:
    xor a
    ret

tc_write_timezone_text:
    call tc_write_timezone_text_core
    jp tc_ret1
tc_write_timezone_text_core:
    ld de, _setup_timezone_text
    ld a, (_setup_timezone_value)
    or a
    jp m, tc_wtz_negative
    ld a, '+'
    ld (de), a
    ld a, (_setup_timezone_value)
    jr tc_wtz_value
tc_wtz_negative:
    ld a, '-'
    ld (de), a
    ld a, (_setup_timezone_value)
    neg
tc_wtz_value:
    inc de
    cp 10
    jr c, tc_wtz_units
    push af
    ld a, '1'
    ld (de), a
    pop af
    inc de
    sub 10
tc_wtz_units:
    add a, '0'
    ld (de), a
    inc de
    xor a
    ld (de), a
    ret

tc_clear_ctx:
    xor a
    ld hl, CTX_FORCE_LO
    ld b, 6
tc_clear_loop:
    ld (hl), a
    inc hl
    djnz tc_clear_loop
    dec a
    ld (CTX_CLEAR_FROM), a
    ret
tc_or_flags:
    ld hl, CTX_FLAGS
    or (hl)
    ld (hl), a
tc_ret1:
    ld l, 1
    ret

_time_config_ui_ovl_entry:
    ld a, (CTX_FLAGS)
    bit 4, a
    jr z, tc_ui_check_action
    ld a, (_setup_visible_mask)
    bit 4, a
    call nz, tc_ui_time
tc_ui_check_action:
    ld a, (CTX_FLAGS)
    bit 5, a
    jr z, tc_ui_done
    ld a, (_setup_visible_mask + 1)
    bit 2, a
    call nz, tc_ui_action
tc_ui_done:
    ld l, 1
    ret

tc_ui_time:
    call tc_ui_draw_time
    ld hl, 0x5952
    ld b, 3
    ld a, 0x03
    call tc_ui_attr_span
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_ui_time_utc
    ld d, 0
    ld a, (_setup_choice + 6)
    or a
    jr z, tc_ui_time_rtc_selected
    ld a, (_setup_defined_mask)
    bit 4, a
    jr z, tc_ui_time_rtc_selected
    inc d
tc_ui_time_rtc_selected:
    ld e, 0
    ld a, (_setup_cursor)
    cp ROW_TIME
    jr nz, tc_ui_time_rtc_focus_done
    ld a, (_setup_time_focus)
    or a
    jr nz, tc_ui_time_rtc_focus_done
    inc e
tc_ui_time_rtc_focus_done:
    ld hl, 0x5955
    ld b, 2
    call tc_ui_option_span
tc_ui_time_utc:
    ld d, 0
    ld a, (_setup_choice + 6)
    or a
    jr nz, tc_ui_time_utc_selected
    ld a, (_setup_defined_mask)
    bit 4, a
    jr z, tc_ui_time_utc_selected
    inc d
tc_ui_time_utc_selected:
    ld e, 0
    ld a, (_setup_cursor)
    cp ROW_TIME
    jr nz, tc_ui_time_utc_focus_done
    ld a, (_setup_time_focus)
    or a
    jr z, tc_ui_time_utc_focus_done
    inc e
tc_ui_time_utc_focus_done:
    ld a, (_setup_room_editing)
    or a
    jr z, tc_ui_time_utc_paint
    ld a, (_setup_edit_row)
    cp ROW_TIME
    jr nz, tc_ui_time_utc_paint
    ld d, 0
    ld e, 1
tc_ui_time_utc_paint:
    ld hl, 0x5955
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_ui_time_utc_positioned
    ld l, 0x5a
tc_ui_time_utc_positioned:
    ld b, 4
    call tc_ui_option_span
    ld a, (_setup_room_editing)
    or a
    ret z
    ld a, (_setup_edit_row)
    cp ROW_TIME
    ret nz
    ld l, 10
    ld h, 48
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_ui_time_edit_positioned
    ld h, 58
tc_ui_time_edit_positioned:
    jp _spectrum_gui_edit_show

tc_ui_draw_time:
    ld a, (_netchesszx_rtc_available)
    or a
    ld hl, tc_ui_prefix_utc
    ld bc, 11
    jr z, tc_ui_time_prefix
    ld hl, tc_ui_prefix_rtc
    ld c, 21
tc_ui_time_prefix:
    ld de, tc_ui_line_buf
    ldir
    ld hl, _setup_timezone_text
tc_ui_time_value:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, tc_ui_time_value
    ld hl, tc_ui_line_buf + 6
    ld (hl), ' '
    ld hl, tc_ui_line_buf + 16
    ld (hl), ' '
    ld a, (_setup_cursor)
    cp ROW_TIME
    jr nz, tc_ui_time_draw
    ld hl, tc_ui_line_buf + 6
    ld a, (_netchesszx_rtc_available)
    or a
    jr z, tc_ui_time_mark
    ld a, (_setup_time_focus)
    or a
    jr z, tc_ui_time_mark
    ld hl, tc_ui_line_buf + 16
tc_ui_time_mark:
    ; Font slot 92 is the setup cursor glyph.
    ld (hl), 92
tc_ui_time_draw:
    ld hl, tc_ui_line_buf
    jp _spectrum_info_line

tc_ui_action:
    ld a, (_setup_config_dirty)
    or a
    ld hl, tc_ui_line_edit
    jr z, tc_ui_action_draw
    ld hl, tc_ui_line_save
tc_ui_action_draw:
    call _spectrum_info_line
    ld hl, 0x5a99
    ld b, 3
    ld a, 0x45
    call tc_ui_attr_span
    ld hl, 0x5a9d
    ld b, 3
    ld a, 0x44
    call tc_ui_attr_span
    ld a, (_setup_cursor)
    cp ROW_ACTION
    ret nz
    ld a, (_setup_action_focus)
    or a
    ld hl, 0x5a99
    ld b, 3
    ld a, 0x38
    jr z, tc_ui_attr_span
    ld hl, 0x5a9d
    jr tc_ui_attr_span

tc_ui_option_span:
    ld a, e
    or a
    jr z, tc_ui_option_plain
    ld a, d
    or a
    ld a, 0x47
    jr z, tc_ui_attr_span
    dec a
    jr tc_ui_attr_span
tc_ui_option_plain:
    ld a, d
    or a
    ld a, 0x05
    jr nz, tc_ui_attr_span
    ld a, 0x07
tc_ui_attr_span:
    ld (hl), a
    inc hl
    djnz tc_ui_attr_span
    ret

tc_ui_prefix_rtc:
    DEFB 10, "TIME  RTC       UTC "
tc_ui_prefix_utc:
    DEFB 10, "TIME  UTC "
tc_ui_line_save:
    DEFB 20, "              SAVE    START", 0
tc_ui_line_edit:
    DEFB 20, "              EDIT    START", 0
tc_ui_line_buf:
    DEFS tc_ui_prefix_utc - tc_ui_prefix_rtc + 4
tc_key:
    DEFB 0
tc_init_flags:
    DEFB 0

IFDEF NETCHESSZX_SPECTRANEXT
; Spectranext has no esxDOS/NextZXOS RTC. The resident dispatcher normally
; answers this probe without entering the overlay; keep the entry safe too.
tc_probe:
    ld l, 0
    ret
ELSE
; Cold RTC detection adapted from the proven SpecTalkZX chain: documented
; driver API, M_GETDATE, then direct DivTIESUS PCF8563 I2C. On success the
; shared overlay context receives second/minute/hour at offsets 0/1/2.
tc_probe:
    call tc_rtc_try_drvapi
    jr nc, tc_rtc_ok
    call tc_rtc_try_getdate
    jr nc, tc_rtc_ok
    call tc_rtc_try_pcf8563
    jr c, tc_rtc_no
tc_rtc_ok:
    jp tc_ret1
tc_rtc_no:
    ld l, 0
    ret

tc_rtc_try_drvapi:
    push iy
    push ix
    ld iy, 0x5c3a
    xor a
    ld b, a
    ld c, a
    ld d, a
    ld e, a
    ld h, a
    ld l, a
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    DEFB M_DRVAPI
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr tc_rtc_esx_epilogue

tc_rtc_try_getdate:
    push iy
    push ix
    ld iy, 0x5c3a
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    DEFB M_GETDATE
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF

tc_rtc_esx_epilogue:
    pop ix
    pop iy
    ret c

tc_rtc_decode_msdos:
    ld a, b
    cp 88
    jr c, tc_rtc_fail
    cp 112
    jr nc, tc_rtc_fail
    ld a, c
    and 31
    jr z, tc_rtc_fail
    ld a, b
    and 1
    add a, a
    add a, a
    add a, a
    ld b, a
    ld a, c
    and 0xe0
    rlca
    rlca
    rlca
    or b
    jr z, tc_rtc_fail
    cp 13
    jr nc, tc_rtc_fail

    ld a, d
    and 0xf8
    rrca
    rrca
    rrca
    cp 24
    jr nc, tc_rtc_fail
    ld (CTX + 2), a

    ld a, d
    and 7
    add a, a
    add a, a
    add a, a
    ld b, a
    ld a, e
    and 0xe0
    rlca
    rlca
    rlca
    or b
    cp 60
    jr nc, tc_rtc_fail
    ld (CTX + 1), a

    ld a, e
    and 31
    cp 30
    jr nc, tc_rtc_fail
    add a, a
    ld (CTX), a
    ret

tc_rtc_fail:
    scf
    ret

tc_rtc_try_pcf8563:
    ld a, 1
tc_rtc_pcf_mask_loop:
    ld (CTX_FLAGS), a
    push af
    call tc_rtc_try_pcf8563_mask
    jr nc, tc_rtc_pcf_mask_ok
    pop af
    add a, a
    jr nc, tc_rtc_pcf_mask_loop
    scf
    ret
tc_rtc_pcf_mask_ok:
    pop af
    or a
    ret

tc_rtc_try_pcf8563_mask:
    ld bc, ZXUNOADDR
    ld a, I2CREG
    out (c), a
    inc b
    di

    call tc_rtc_i2c_start
    ld a, I2CADDR_W
    call tc_rtc_i2c_send_byte
    ld a, 2
    call tc_rtc_i2c_send_byte

    call tc_rtc_i2c_start
    ld a, I2CADDR_R
    call tc_rtc_i2c_send_byte

    ld hl, CTX
    ld e, 6
tc_rtc_pcf_read_ack:
    call tc_rtc_i2c_read_byte
    ld (hl), a
    inc hl
    call tc_rtc_i2c_ack
    dec e
    jr nz, tc_rtc_pcf_read_ack

    call tc_rtc_i2c_read_byte
    ld (hl), a
    call tc_rtc_i2c_nack
    call tc_rtc_i2c_stop
    jp tc_rtc_pcf_validate

tc_rtc_i2c_start:
    ld a, SCL1SDA1
    out (c), a
    ld a, SCL1SDA0
    out (c), a
    ret

tc_rtc_i2c_stop:
    ld a, SCL0SDA0
    out (c), a
    ld a, SCL1SDA0
    out (c), a
    ld a, SCL1SDA1
    out (c), a
    ret

tc_rtc_i2c_send_byte:
    scf
tc_rtc_i2c_tx_bit:
    adc a, a
    jr z, tc_rtc_i2c_nack
    call c, tc_rtc_i2c_nack
    call nc, tc_rtc_i2c_ack
    and a
    jr tc_rtc_i2c_tx_bit

tc_rtc_i2c_nack:
    ld d, SCL0SDA1
    jr tc_rtc_i2c_clock_pulse
tc_rtc_i2c_ack:
    ld d, SCL0SDA0
tc_rtc_i2c_clock_pulse:
    out (c), d
    set 1, d
    out (c), d
    res 1, d
    out (c), d
    ret

tc_rtc_i2c_read_byte:
    push hl
    push de
    ld e, 8
    ld h, 0
tc_rtc_i2c_rx_bit:
    ld d, SCL0SDA1
    out (c), d
    ld d, SCL1SDA1
    out (c), d
    in d, (c)
    ld a, (CTX_FLAGS)
    and d
    add a, 0xff
    rl h
    ld d, SCL0SDA1
    out (c), d
    dec e
    jr nz, tc_rtc_i2c_rx_bit
    ld a, h
    pop de
    pop hl
    ret

tc_rtc_pcf_validate:
    ld hl, CTX
    ld a, (hl)
    bit 7, a
    jr nz, tc_rtc_pcf_fail
    and 0x7f
    call tc_rtc_bcd_to_bin
    ret c
    cp 60
    jr nc, tc_rtc_pcf_fail
    ld (hl), a

    inc hl
    ld a, (hl)
    and 0x7f
    call tc_rtc_bcd_to_bin
    ret c
    cp 60
    jr nc, tc_rtc_pcf_fail
    ld (hl), a

    inc hl
    ld a, (hl)
    and 0x3f
    call tc_rtc_bcd_to_bin
    ret c
    cp 24
    jr nc, tc_rtc_pcf_fail
    ld (hl), a

    inc hl
    ld a, (hl)
    and 0x3f
    call tc_rtc_bcd_to_bin
    ret c
    or a
    jr z, tc_rtc_pcf_fail
    cp 32
    jr nc, tc_rtc_pcf_fail

    inc hl
    inc hl
    ld a, (hl)
    and 0x1f
    call tc_rtc_bcd_to_bin
    ret c
    or a
    jr z, tc_rtc_pcf_fail
    cp 13
    jr nc, tc_rtc_pcf_fail

    inc hl
    ld a, (hl)
    call tc_rtc_bcd_to_bin
    ret c
    cp 24
    jr c, tc_rtc_pcf_fail
    cp 36
    jr nc, tc_rtc_pcf_fail

    or a
    ret

tc_rtc_pcf_fail:
tc_rtc_bcd_fail:
    scf
    ret

tc_rtc_bcd_to_bin:
    ld b, a
    and 0x0f
    cp 10
    jr nc, tc_rtc_bcd_fail
    ld c, a
    ld a, b
    and 0xf0
    rrca
    rrca
    rrca
    rrca
    ld b, a
    add a, a
    add a, a
    add a, b
    add a, a
    add a, c
    or a
    ret
ENDIF
