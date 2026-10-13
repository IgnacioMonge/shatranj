; Log panel scroll: resident on Next, GUI_LOG overlay code on ULA builds.
PUBLIC _spectrum_render_moves_scroll
PUBLIC _spectrum_render_chat_scroll

_spectrum_render_moves_scroll:
    ld a, NETCHESSZX_INFO_MOVES_FIRST_Y
    ld b, (NETCHESSZX_MOVE_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP
    jr scroll_right_tight_band_up

_spectrum_render_chat_scroll:
    ld a, NETCHESSZX_INFO_CHAT_FIRST_Y
    ld b, (NETCHESSZX_CHAT_ROWS - 1) * NETCHESSZX_INFO_TIGHT_LINE_STEP

scroll_right_tight_band_up:
    ld (tmp_scan), a
    ld a, b
    ld (piece_scan), a
    ld a, (tmp_scan)
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
    ex de, hl
    ld a, (tmp_scan)
    add a, NETCHESSZX_INFO_TIGHT_LINE_STEP
    call compute_pixel_base
    ld a, NETCHESSZX_INFO_PANEL_COL
    add a, l
    ld l, a
srub_loop:
    ld a, (piece_scan)
    or a
    ret z
    push hl
    push de
    ld bc, NETCHESSZX_INFO_PANEL_WIDTH_BYTES
    ldir
    pop de
    pop hl
    call pixel_down_hl
    ex de, hl
    call pixel_down_hl
    ex de, hl
    ld a, (piece_scan)
    dec a
    ld (piece_scan), a
    jr srub_loop
