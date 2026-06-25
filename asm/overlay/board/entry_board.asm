SECTION code_user

PUBLIC _board_apply_ovl_entry
EXTERN _board_apply_trusted_ovl

    DW 1
    DW _board_apply_ovl_entry

_board_apply_ovl_entry:
    ex de, hl
    jp _board_apply_trusted_ovl
