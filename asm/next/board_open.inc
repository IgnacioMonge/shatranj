_spectrum_render_board:
    ld (board_ptr), hl
    call next_board_coord_palette_sync
    call clear_screen
    call draw_banner
    call draw_banner_separator
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board
    jp draw_board_frame

_spectrum_render_board_area:
    ld (board_ptr), hl
    call clear_board_coords
    call restore_board_frame_attrs
    call draw_board_coords
    call draw_board
    jp draw_board_frame


_spectrum_render_board_coords:
    call clear_board_coords
    call restore_board_frame_attrs
    call draw_board_coords
    jp draw_board_frame

_spectrum_render_board_coord_mark:
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld a, (hl)
    or a
    jr z, srcm_text
    ld a, (_netchesszx_board_theme_index)
    or a
    jr z, srcm_selected_classic
    ld a, NEXT_BOARD_COORD_SELECTED_ATTR
    jr srcm_attr_ready
srcm_selected_classic:
    ld a, (_netchesszx_board_light_attr)
    jr srcm_attr_ready
srcm_text:
    call board_light_line_attr
srcm_attr_ready:
    ld (tmp_attr), a

    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    ld c, a
    ld hl, ula_attr_board_top_m1
    ld a, c
    add a, l
    ld l, a
    ld a, (tmp_attr)
    ld (hl), a
    inc hl
    ld (hl), a

    ld a, (board_row)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW
    call compute_attr_base
    ld a, NETCHESSZX_GAME_BOARD_LEFT_COL - 1
    add a, l
    ld l, a
    ld a, (tmp_attr)
    ld (hl), a
    ld de, 32
    add hl, de
    ld (hl), a
    ret

