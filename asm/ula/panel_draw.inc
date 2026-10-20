INCLUDE "asm/app/player_name_layout.inc"

clear_screen:
    xor a
    out (0xfe), a

    ld hl, SCREEN_BASE
    ld de, SCREEN_BASE + 1
    ld bc, 6143
    ld (hl), a
    ldir

    ld h, d
    ld l, e
    inc de
    ld a, ATTR_TEXT
    ld (hl), a
    ld bc, 767
    ldir
    ret

draw_banner:
    xor a
    ld c, ATTR_BANNER_TOP
    call fill_attr_line
    ld a, 1
    ld c, ATTR_BANNER_BOT
    call fill_attr_line

    ld hl, title_msg
    call draw_scaled_text

    call draw_banner_info
    jp draw_badge

draw_banner_info:
    ld a, ATTR_TEXT
    ld (current_attr), a
    ld hl, version_banner_msg
    ld b, 4
    ld c, NETCHESSZX_BANNER_INFO_COL
    call draw_ikkle_text_abs_y
    ld hl, banner_info_top_msg
    ld b, 10
    ld c, NETCHESSZX_BANNER_INFO_COL
    jp draw_ikkle_text_abs_y

draw_menu:
    ld (mark_mode), a
    ld a, NETCHESSZX_MENU_ROW
    ld c, ATTR_STATUS
    call fill_attr_line
    ld a, ATTR_STATUS
    ld (current_attr), a
    call clear_menu_pixels
    ld hl, tab_label_file
    ld c, NETCHESSZX_MENU_FILE_TEXT_COL
dm_tab_loop:
    call draw_tab_text_at
    ld a, (tmp_col)
    cp NETCHESSZX_MENU_TEXT_END_COL
    jr z, dm_tabs_done
    inc hl
    inc a
    ld c, a
    jr dm_tab_loop
dm_tabs_done:
    ld a, (mark_mode)
    dec a
    cp NETCHESSZX_TABOPTION_COUNT
    jr c, dm_focus_ok
    xor a
dm_focus_ok:
    jr draw_menu_cursor

draw_menu_partial:
    ld (mark_mode), a
    and 0x38
    srl a
    srl a
    srl a
    call draw_menu_cursor
    ld a, (mark_mode)
    and 0x07
    jr draw_menu_cursor

clear_menu_pixels:
    ld hl, ula_screen_menu
    xor a
    ld b, 32
cmp_sep_loop:
    ld (hl), a
    inc l
    djnz cmp_sep_loop
    ld a, l
    sub 32
    ld l, a
    inc h
    ld c, 7
    xor a
cmp_scan_loop:
    ld b, 20
    push hl
cmp_px_loop:
    ld (hl), a
    inc l
    djnz cmp_px_loop
    pop hl
    inc h
    dec c
    jr nz, cmp_scan_loop
    ret

draw_menu_cursor:
    cp NETCHESSZX_TABOPTION_COUNT
    ret nc
    add a, a
    add a, a
    ld e, a
    ld d, 0
    ld hl, menu_cursor_masks
    add hl, de
    ld a, (hl)
    ld (tmp_col), a
    inc hl
    ld a, (hl)
    ld (piece_scan), a
    inc hl
    ld a, (hl)
    ld (tmp_char), a
    inc hl
    ld a, (hl)
    ld (tmp_scan), a
    ld hl, ula_screen_menu
    inc h
    ld c, l
    ld b, 6
dmc_scan_loop:
    push bc
    ld l, c
    ld a, (tmp_col)
    add a, l
    ld l, a
    ld a, (piece_scan)
    xor (hl)
    ld (hl), a
    inc l
    ld a, (tmp_char)
    ld b, a
dmc_mid_loop:
    ld a, (hl)
    cpl
    ld (hl), a
    inc l
    djnz dmc_mid_loop
    ld a, (tmp_scan)
    xor (hl)
    ld (hl), a
    pop bc
    inc h
    djnz dmc_scan_loop
    ret

draw_tab_text_at:
    ld a, NETCHESSZX_MENU_ROW
    ld (tmp_row), a
    ld a, 2
    ld (tmp_scan), a
    ld a, c
    ld (tmp_col), a
    jp draw_ikkle_text_at

hide_menu:
    call clear_menu_pixels
    ld hl, ula_attr_menu
    ld b, 20
    ld a, ATTR_TEXT
hm_left_attr:
    ld (hl), a
    inc hl
    djnz hm_left_attr
    ld b, 12
    ld a, ATTR_TIMER
hm_right_attr:
    ld (hl), a
    inc hl
    djnz hm_right_attr
    jp draw_banner_separator

draw_static_panels:
    call draw_moves_header

    call draw_ikkle_inline
    DEFB NETCHESSZX_INFO_CHAT_TITLE_ROW, NETCHESSZX_INFO_TEXT_COL, ATTR_CHAT_TITLE
    DEFW chat_msg
    ld a, NETCHESSZX_INFO_CHAT_HLINE_ROW
    jp draw_right_hline_full_left

draw_moves_header:
    ld de, ula_screen_info_header + NETCHESSZX_INFO_PANEL_COL
    ld c, 13
    call clear_pixels_8

    ld hl, ula_attr_info_header + NETCHESSZX_INFO_PANEL_COL
    ld b, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
dmh_attr_loop:
    ld (hl), ATTR_STATUS
    inc hl
    djnz dmh_attr_loop
    ; Discs, divider and player labels: STATUS overlay (moves header entry),
    ; painted by spectrum_gui_moves_header() once the panels are visible.
    ; The row is blank now, so both labels must be drawn again.
    xor a
    ld (_spectrum_moves_header_shown), a
    ld (_spectrum_moves_header_shown + PLAYER_NAME_MAX + 1), a
    ret
