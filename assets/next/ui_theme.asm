; Next-only replacements in the existing 889-byte runtime UI DAT.
; Consumed by tools/gen_assets.py; not linked as an additional payload.
banner_info_top_msg:
    DEFB "ONLINE CHESS FOR SPECTRUM NEXT", 0
board_theme_mark_inks:
    DEFB 2, 2, 1, 0, 0
board_theme_light_attrs:
    DEFB 0x78, 0x6f, 0x66, 0x77, 0x37
board_theme_dark_attrs:
    DEFB 0x07, 0x4d, 0x20, 0x56, 0x52
ui_theme_end:
