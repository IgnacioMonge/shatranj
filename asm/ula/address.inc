compute_screen_base:
    ld l, a
    and 0x18
    or 0x40
    ld h, a
    ld a, l
    and 0x07
    rrca
    rrca
    rrca
    ld l, a
    ret

compute_screen_base_de8:
    call compute_screen_base
    ld d, h
    ld e, l
    ld b, 8
    ret

compute_pixel_base:
    ld l, a
    and 0xc0
    rrca
    rrca
    rrca
    ld h, a
    ld a, l
    and 0x07
    or h
    or 0x40
    ld h, a
    ld a, l
    and 0x38
    rlca
    rlca
    ld l, a
    ret

compute_attr_base:
    rrca
    rrca
    rrca
    ld l, a
    and 0x03
    or 0x58
    ld h, a
    ld a, l
    and 0xe0
    ld l, a
    ret

ula_screen_board_top_m1 EQU 0x4000 + ((NETCHESSZX_GAME_BOARD_TOP_ROW - 1) / 8) * 0x800 + ((NETCHESSZX_GAME_BOARD_TOP_ROW - 1) - ((NETCHESSZX_GAME_BOARD_TOP_ROW - 1) / 8) * 8) * 32
ula_screen_board_bottom EQU 0x4000 + ((NETCHESSZX_GAME_BOARD_TOP_ROW + 16) / 8) * 0x800 + ((NETCHESSZX_GAME_BOARD_TOP_ROW + 16) - ((NETCHESSZX_GAME_BOARD_TOP_ROW + 16) / 8) * 8) * 32
ula_screen_menu EQU 0x4000 + (NETCHESSZX_MENU_ROW / 8) * 0x800 + (NETCHESSZX_MENU_ROW - (NETCHESSZX_MENU_ROW / 8) * 8) * 32
ula_screen_info_header EQU 0x4000 + (NETCHESSZX_INFO_HEADER_ROW / 8) * 0x800 + (NETCHESSZX_INFO_HEADER_ROW - (NETCHESSZX_INFO_HEADER_ROW / 8) * 8) * 32
ula_screen_top_turn EQU 0x4000 + (NETCHESSZX_TOP_TURN_ROW / 8) * 0x800 + (NETCHESSZX_TOP_TURN_ROW - (NETCHESSZX_TOP_TURN_ROW / 8) * 8) * 32
ula_screen_fileui_separator EQU 0x4000 + (NETCHESSZX_FILEUI_SEPARATOR_ROW / 8) * 0x800 + (NETCHESSZX_FILEUI_SEPARATOR_ROW - (NETCHESSZX_FILEUI_SEPARATOR_ROW / 8) * 8) * 32
ula_screen_status EQU 0x4000 + (NETCHESSZX_STATUS_ROW / 8) * 0x800 + (NETCHESSZX_STATUS_ROW - (NETCHESSZX_STATUS_ROW / 8) * 8) * 32

ula_attr_board_top_m1 EQU 0x5800 + (NETCHESSZX_GAME_BOARD_TOP_ROW - 1) * 32
ula_attr_board_top EQU 0x5800 + NETCHESSZX_GAME_BOARD_TOP_ROW * 32
ula_attr_menu EQU 0x5800 + NETCHESSZX_MENU_ROW * 32
ula_attr_info_header EQU 0x5800 + NETCHESSZX_INFO_HEADER_ROW * 32
ula_attr_status EQU 0x5800 + NETCHESSZX_STATUS_ROW * 32

