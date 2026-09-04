#ifndef NETCHESSZX_SPECTRUM_BOARD_APPLY_IMPL_H
#define NETCHESSZX_SPECTRUM_BOARD_APPLY_IMPL_H

/* Shared source for the production BOARD overlay and host board tests.
   Resident builds of board.c deliberately do not include this file. */
static void board_apply_set_cell(uint8_t index, char piece)
{
    chess_board[index] = piece;
    rules_board[index] = rules_piece_from_char(piece);
}

static void board_apply_clear_castle_rights(uint8_t from_idx, uint8_t to_idx,
                                            char piece, char target)
{
    if (piece == 'K') {
        castle_rights &= (uint8_t)~(NETCHESSZX_RULE_CASTLE_WK |
                                    NETCHESSZX_RULE_CASTLE_WQ);
    } else if (piece == 'k') {
        castle_rights &= (uint8_t)~(NETCHESSZX_RULE_CASTLE_BK |
                                    NETCHESSZX_RULE_CASTLE_BQ);
    } else if (piece == 'R') {
        if (from_idx == 56u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_WQ;
        } else if (from_idx == 63u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_WK;
        }
    } else if (piece == 'r') {
        if (from_idx == 0u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_BQ;
        } else if (from_idx == 7u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_BK;
        }
    }

    if (target == 'R') {
        if (to_idx == 56u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_WQ;
        } else if (to_idx == 63u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_WK;
        }
    } else if (target == 'r') {
        if (to_idx == 0u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_BQ;
        } else if (to_idx == 7u) {
            castle_rights &= (uint8_t)~NETCHESSZX_RULE_CASTLE_BK;
        }
    }
}

static uint8_t board_apply_parsed_move(const char *move,
                                       uint8_t from_row, uint8_t from_col,
                                       uint8_t to_row, uint8_t to_col,
                                       spectrum_board_undo_t *undo)
{
    uint8_t from_idx = (uint8_t)((from_row << 3) + from_col);
    uint8_t to_idx = (uint8_t)((to_row << 3) + to_col);
    char piece = chess_board[from_idx];
    char target = chess_board[to_idx];
    uint8_t promotion = (uint8_t)((piece == 'P' && to_row == 0u) ||
                                  (piece == 'p' && to_row == 7u));
    char promo = move[4];

    if (piece == '.' || piece_side(piece) != side_to_move ||
        (target != '.' && piece_side(target) == side_to_move)) {
        return 0u;
    }
    if (promo >= 'A' && promo <= 'Z') {
        promo = (char)(promo - ('A' - 'a'));
    }
    if ((promotion &&
         (move[4] == '\0' || move[5] != '\0' ||
          (promo != 'q' && promo != 'r' &&
           promo != 'b' && promo != 'n'))) ||
        (!promotion && move[4] != '\0')) {
        return 0u;
    }

    if (undo != 0) {
        undo->from = (uint8_t)(from_idx |
            (promotion ? SPECTRUM_BOARD_UNDO_PROMOTION : 0u));
        undo->to = to_idx;
        undo->captured = target;
        undo->castle = castle_rights;
        undo->ep = ep_square;
    }

    board_apply_clear_castle_rights(from_idx, to_idx, piece, target);
    if (((piece == 'K' && from_row == 7u) ||
         (piece == 'k' && from_row == 0u)) &&
        to_row == from_row && from_col == 4u &&
        (to_col == 6u || to_col == 2u)) {
        uint8_t rook_from = (uint8_t)((from_row << 3) +
                                      (to_col == 6u ? 7u : 0u));
        uint8_t rook_to = (uint8_t)((from_row << 3) +
                                    (to_col == 6u ? 5u : 3u));

        board_apply_set_cell(rook_to, chess_board[rook_from]);
        board_apply_set_cell(rook_from, '.');
    }

    if ((piece == 'P' || piece == 'p') &&
        from_col != to_col && target == '.' &&
        ep_square == (int8_t)to_idx) {
        board_apply_set_cell((uint8_t)((from_row << 3) + to_col), '.');
    }

    ep_square = NETCHESSZX_RULE_NO_SQUARE;

    if ((piece == 'P' || piece == 'p') &&
        abs_delta(from_row, to_row) == 2u) {
        ep_square = (int8_t)(((from_row + to_row) >> 1) << 3);
        ep_square = (int8_t)(ep_square + (int8_t)from_col);
    }

    if (promotion) {
        piece = promotion_piece(piece, move[4]);
    }

    board_apply_set_cell(to_idx, piece);
    board_apply_set_cell(from_idx, '.');
    side_to_move ^= 1u;
    return 1u;
}

static void board_apply_undo_restore(const spectrum_board_undo_t *undo)
{
    char moved = chess_board[undo->to];
    char original = moved;
    uint8_t side = piece_side(moved);
    uint8_t from_idx = (uint8_t)(undo->from &
                                  SPECTRUM_BOARD_UNDO_INDEX_MASK);
    uint8_t from_row = (uint8_t)(from_idx >> 3);
    uint8_t from_col = (uint8_t)(from_idx & 7u);
    uint8_t to_row = (uint8_t)(undo->to >> 3);
    uint8_t to_col = (uint8_t)(undo->to & 7u);

    if (undo->from & SPECTRUM_BOARD_UNDO_PROMOTION) {
        original = side == NETCHESSZX_RULE_WHITE ? 'P' : 'p';
    }
    board_apply_set_cell(from_idx, original);
    board_apply_set_cell(undo->to, undo->captured);

    if ((moved == 'P' || moved == 'p') && from_col != to_col &&
        undo->captured == '.' && undo->ep == (int8_t)undo->to) {
        board_apply_set_cell((uint8_t)((from_row << 3) + to_col),
                             side == NETCHESSZX_RULE_WHITE ? 'p' : 'P');
    } else if (((moved == 'K' && from_row == 7u) ||
                (moved == 'k' && from_row == 0u)) && from_col == 4u &&
               to_row == from_row && (to_col == 6u || to_col == 2u)) {
        uint8_t rook_from = (uint8_t)((from_row << 3) +
                                      (to_col == 6u ? 7u : 0u));
        uint8_t rook_to = (uint8_t)((from_row << 3) +
                                    (to_col == 6u ? 5u : 3u));

        board_apply_set_cell(rook_from,
                             side == NETCHESSZX_RULE_WHITE ? 'R' : 'r');
        board_apply_set_cell(rook_to, '.');
    }
    side_to_move = side;
    castle_rights = undo->castle;
    ep_square = undo->ep;
}

#endif
