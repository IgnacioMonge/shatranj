_netchesszx_board_theme_apply:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    cp 5
    jr c, nbta_index_ok
    xor a
nbta_index_ok:
    ld (_netchesszx_board_theme_index), a
    ld e, a
    ld d, 0
    ld hl, board_theme_light_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_light_attr), a
    ld hl, board_theme_dark_attrs
    add hl, de
    ld a, (hl)
    ld (_netchesszx_board_dark_attr), a
    ; Reuse the canonical parity/geometry path without touching bitmap VRAM.
    ld a, RENDER_ATTRS_ONLY
    ld (render_skip_clear), a
    call draw_board
    xor a
    ld (render_skip_clear), a
    jp restore_board_frame_attrs


