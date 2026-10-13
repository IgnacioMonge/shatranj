/* Chess-specific repaint/animation consequences. Composed in gui.c to retain
   private borrowed views, function placement and the existing render ABI. */
void spectrum_gui_prepare_move(const char *move) NETCHESSZX_FASTCALL
{
    uint16_t coords;
    uint8_t from_idx;

    if (about_visible) {
        return;
    }
    coords = netchesszx_move_parse_coords(move);
    if (coords == NETCHESSZX_MOVE_COORDS_INVALID) {
        return;
    }
    from_idx = NETCHESSZX_MOVE_FROM_INDEX(coords);
    if (gui_board_cell((uint8_t)(from_idx >> 3),
                       (uint8_t)(from_idx & 7u)) != '.') {
        flash_square((uint8_t)(from_idx >> 3),
                     (uint8_t)(from_idx & 7u));
    }
}

#ifndef NETCHESSZX_SPECTRANEXT
void spectrum_gui_apply_move(const char *move) NETCHESSZX_FASTCALL
{
    uint16_t coords;
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    char piece;

    if (about_visible) {
        return;
    }
    coords = netchesszx_move_parse_coords(move);
    if (coords == NETCHESSZX_MOVE_COORDS_INVALID) {
        return;
    }
    from_col = NETCHESSZX_MOVE_FROM_INDEX(coords);
    to_col = NETCHESSZX_MOVE_TO_INDEX(coords);
    from_row = (uint8_t)(from_col >> 3);
    to_row = (uint8_t)(to_col >> 3);
    from_col &= 7u;
    to_col &= 7u;

    render_square_from_board(from_row, from_col);
    render_square_from_board(to_row, to_col);
    flash_square(to_row, to_col);

    piece = gui_board_cell(to_row, to_col);
    if ((piece == 'P' || piece == 'p') && from_col != to_col) {
        render_square_from_board(from_row, to_col);
    }
    if ((piece == 'K' || piece == 'k') &&
        from_row == to_row && from_col == 4u) {
        if (to_col == 6u) {
            render_square_from_board(from_row, 7u);
            render_square_from_board(from_row, 5u);
        } else if (to_col == 2u) {
            render_square_from_board(from_row, 0u);
            render_square_from_board(from_row, 3u);
        }
    }
}
#endif
