_spectrum_info_show_setup:
    call clear_right_panel_rows
    jr draw_connection_setup_header

_spectrum_info_show_game_setup:
    call draw_ikkle_inline
    DEFB NETCHESSZX_INFO_SETUP_GAME_HEADER_ROW, NETCHESSZX_INFO_TEXT_COL, ATTR_MOVES_TITLE
    DEFW game_setup_msg
    ld a, NETCHESSZX_INFO_SETUP_GAME_HLINE_ROW
    jp draw_right_hline

draw_connection_setup_header:
    call draw_ikkle_inline
    DEFB NETCHESSZX_INFO_SETUP_HEADER_ROW, NETCHESSZX_INFO_TEXT_COL, ATTR_MOVES_TITLE
    DEFW session_setup_msg
    ld a, NETCHESSZX_INFO_SETUP_HLINE_ROW
    jp draw_right_hline
