#include "spectrum/board/board.h"
#include "common/chess/move_coords.h"
#include "spectrum/lowram_map.h"
#ifndef NETCHESSZX_HOST_TEST
#include "spectrum/overlay/overlay.h"
#else
#include "spectrum/board/rules_compact.h"
#endif

#define NETCHESSZX_RULE_EMPTY 0
#define NETCHESSZX_RULE_WHITE 0u
#define NETCHESSZX_RULE_BLACK 1u
#define NETCHESSZX_RULE_WP 1
#define NETCHESSZX_RULE_WN 2
#define NETCHESSZX_RULE_WB 3
#define NETCHESSZX_RULE_WR 4
#define NETCHESSZX_RULE_WQ 5
#define NETCHESSZX_RULE_WK 6
#define NETCHESSZX_RULE_BP -1
#define NETCHESSZX_RULE_BN -2
#define NETCHESSZX_RULE_BB -3
#define NETCHESSZX_RULE_BR -4
#define NETCHESSZX_RULE_BQ -5
#define NETCHESSZX_RULE_BK -6
#define NETCHESSZX_RULE_CASTLE_WK 1u
#define NETCHESSZX_RULE_CASTLE_WQ 2u
#define NETCHESSZX_RULE_CASTLE_BK 4u
#define NETCHESSZX_RULE_CASTLE_BQ 8u
#define NETCHESSZX_RULE_NO_SQUARE (-1)

#define NO_EP NETCHESSZX_RULE_NO_SQUARE

#define SPECTRUM_BOARD_CELL_COUNT 64u

#ifndef NETCHESSZX_HOST_TEST
#if SPECTRUM_BOARD_CELL_COUNT != NETCHESSZX_LOWRAM_BOARD_CELL_COUNT
#error "board cell count must match low-RAM map"
#endif
#if NETCHESSZX_LOWRAM_CHESS_BOARD_END != NETCHESSZX_LOWRAM_RULES_BOARD_ADDR
#error "chess_board export must stay 64 contiguous bytes"
#endif
#define chess_board ((char *)NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR)
#define rules_board ((int8_t *)NETCHESSZX_LOWRAM_RULES_BOARD_ADDR)
#else
static char chess_board[SPECTRUM_BOARD_CELL_COUNT];
static int8_t rules_board[SPECTRUM_BOARD_CELL_COUNT];
typedef char chess_board_layout_guard[
    sizeof(chess_board) == SPECTRUM_BOARD_CELL_COUNT ? 1 : -1];
#endif
uint8_t side_to_move;
uint8_t castle_rights;
int8_t ep_square;

#ifdef NETCHESSZX_HOST_TEST
static uint8_t abs_delta(uint8_t a, uint8_t b)
{
    return a > b ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static uint8_t piece_side(char piece)
{
    return (uint8_t)(piece >= 'a' && piece <= 'z');
}
#endif

static int8_t rules_piece_from_char(char piece)
{
    static const char pieces[] = "PNBRQKpnbrqk";
    static const int8_t values[] = {
        NETCHESSZX_RULE_WP,
        NETCHESSZX_RULE_WN,
        NETCHESSZX_RULE_WB,
        NETCHESSZX_RULE_WR,
        NETCHESSZX_RULE_WQ,
        NETCHESSZX_RULE_WK,
        NETCHESSZX_RULE_BP,
        NETCHESSZX_RULE_BN,
        NETCHESSZX_RULE_BB,
        NETCHESSZX_RULE_BR,
        NETCHESSZX_RULE_BQ,
        NETCHESSZX_RULE_BK,
    };
    uint8_t i;

    for (i = 0u; i < sizeof(pieces) - 1u; ++i) {
        if (piece == pieces[i]) {
            return values[i];
        }
    }
    return NETCHESSZX_RULE_EMPTY;
}

#ifdef NETCHESSZX_HOST_TEST
static char promotion_piece(char pawn, char promo)
{
    if (promo == '\0') {
        promo = 'q';
    }

    if (pawn >= 'A' && pawn <= 'Z') {
        if (promo >= 'a' && promo <= 'z') {
            promo = (char)(promo - 'a' + 'A');
        }
    } else if (promo >= 'A' && promo <= 'Z') {
        promo = (char)(promo - 'A' + 'a');
    }

    return promo;
}
#endif

void spectrum_board_reset(void)
{
    static const char initial_board[] =
        "rnbqkbnr"
        "pppppppp"
        "........"
        "........"
        "........"
        "........"
        "PPPPPPPP"
        "RNBQKBNR";
    uint8_t i;

    for (i = 0u; i < 64u; ++i) {
        chess_board[i] = initial_board[i];
        rules_board[i] = rules_piece_from_char(initial_board[i]);
    }
    side_to_move = NETCHESSZX_RULE_WHITE;
    castle_rights = NETCHESSZX_RULE_CASTLE_WK |
                    NETCHESSZX_RULE_CASTLE_WQ |
                    NETCHESSZX_RULE_CASTLE_BK |
                    NETCHESSZX_RULE_CASTLE_BQ;
    ep_square = NO_EP;
}

void spectrum_board_clear(void)
{
    uint8_t i;

    for (i = 0u; i < 64u; ++i) {
        chess_board[i] = '.';
        rules_board[i] = NETCHESSZX_RULE_EMPTY;
    }
    side_to_move = NETCHESSZX_RULE_WHITE;
    castle_rights = 0u;
    ep_square = NO_EP;
}

#ifdef NETCHESSZX_HOST_TEST
void spectrum_board_test_set(const char cells[64],
                             uint8_t side,
                             uint8_t castle,
                             int8_t ep)
{
    uint8_t i;

    for (i = 0u; i < 64u; ++i) {
        chess_board[i] = cells[i];
        rules_board[i] = rules_piece_from_char(cells[i]);
    }
    side_to_move = side;
    castle_rights = castle;
    ep_square = ep;
}
#endif

const char *spectrum_board_cells(void)
{
    return chess_board;
}

char spectrum_board_cell(uint8_t row, uint8_t col)
{
    if (row >= 8u || col >= 8u) {
        return '.';
    }
    return chess_board[(uint8_t)((row << 3) + col)];
}

uint8_t spectrum_board_is_legal_move_coords(uint8_t from_idx, uint8_t to_idx)
{
    if (from_idx >= 64u || to_idx >= 64u) {
        return 0u;
    }

#ifdef NETCHESSZX_HOST_TEST
    return netchesszx_compact_is_legal_move(rules_board,
                                            side_to_move,
                                            from_idx,
                                            to_idx,
                                            castle_rights,
                                            ep_square);
#else
    {
        uint16_t board_addr = (uint16_t)rules_board;

        spectrum_overlay_context[0] = (uint8_t)board_addr;
        spectrum_overlay_context[1] = (uint8_t)(board_addr >> 8);
        spectrum_overlay_context[2] = side_to_move;
        spectrum_overlay_context[3] = from_idx;
        spectrum_overlay_context[4] = to_idx;
        spectrum_overlay_context[5] = castle_rights;
        spectrum_overlay_context[6] = (uint8_t)ep_square;
    }
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_RULES, SPECTRUM_OVL_RULES_PLAY);
#endif
}

uint8_t spectrum_board_is_legal_move(const char *move) NETCHESSZX_FASTCALL
{
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    uint8_t from_idx;
    uint8_t to_idx;
    char promo;
    char piece;

    if (!netchesszx_move_parse_coords(move, &from_row, &from_col, &to_row, &to_col)) {
        return 0u;
    }

    promo = move[4];
    if (promo >= 'A' && promo <= 'Z') {
        promo = (char)(promo - ('A' - 'a'));
    }
    if (promo != '\0') {
        if (move[5] != '\0' ||
            (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n')) {
            return 0u;
        }
    }

    from_idx = (uint8_t)((from_row << 3) + from_col);
    to_idx = (uint8_t)((to_row << 3) + to_col);
    piece = chess_board[from_idx];
    if (promo != '\0' && piece != 'P' && piece != 'p') {
        return 0u;
    }
    if (((piece == 'P' && to_row == 0u) ||
         (piece == 'p' && to_row == 7u)) &&
        promo == '\0') {
        return 0u;
    }
    if ((piece == 'P' && to_row != 0u) ||
        (piece == 'p' && to_row != 7u)) {
        if (promo != '\0') {
            return 0u;
        }
    }

    return spectrum_board_is_legal_move_coords(from_idx, to_idx);
}

uint8_t spectrum_board_check_state(void)
{
#ifdef NETCHESSZX_HOST_TEST
    return netchesszx_compact_check_state(rules_board,
                                          side_to_move,
                                          castle_rights,
                                          ep_square);
#else
    {
        uint16_t board_addr = (uint16_t)rules_board;

        spectrum_overlay_context[0] = (uint8_t)board_addr;
        spectrum_overlay_context[1] = (uint8_t)(board_addr >> 8);
        spectrum_overlay_context[2] = side_to_move;
        spectrum_overlay_context[3] = 0u;
        spectrum_overlay_context[4] = 0u;
        spectrum_overlay_context[5] = castle_rights;
        spectrum_overlay_context[6] = (uint8_t)ep_square;
    }
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_RULES,
                                        SPECTRUM_OVL_RULES_CHECK);
#endif
}

#ifdef NETCHESSZX_HOST_TEST
static void clear_castle_rights(uint8_t from_idx, uint8_t to_idx,
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

static uint8_t apply_parsed_move(const char *move,
                                 uint8_t from_row, uint8_t from_col,
                                 uint8_t to_row, uint8_t to_col)
{
    uint8_t from_idx = (uint8_t)((from_row << 3) + from_col);
    uint8_t to_idx = (uint8_t)((to_row << 3) + to_col);
    char piece = chess_board[from_idx];
    char target = chess_board[to_idx];

    if (piece == '.' || piece_side(piece) != side_to_move ||
        (target != '.' && piece_side(target) == side_to_move)) {
        return 0u;
    }

    clear_castle_rights(from_idx, to_idx, piece, target);

    ep_square = NO_EP;

    if (((piece == 'K' && from_row == 7u) ||
         (piece == 'k' && from_row == 0u)) &&
        from_col == 4u && (to_col == 6u || to_col == 2u)) {
        if (to_col == 6u) {
            chess_board[(uint8_t)((from_row << 3) + 5u)] =
                chess_board[(uint8_t)((from_row << 3) + 7u)];
            chess_board[(uint8_t)((from_row << 3) + 7u)] = '.';
        } else {
            chess_board[(uint8_t)((from_row << 3) + 3u)] =
                chess_board[(uint8_t)(from_row << 3)];
            chess_board[(uint8_t)(from_row << 3)] = '.';
        }
    }

    if ((piece == 'P' || piece == 'p') &&
        from_col != to_col &&
        target == '.') {
        chess_board[(uint8_t)((from_row << 3) + to_col)] = '.';
    }

    if ((piece == 'P' || piece == 'p') &&
        abs_delta(from_row, to_row) == 2u) {
        ep_square = (int8_t)(((from_row + to_row) >> 1) << 3);
        ep_square = (int8_t)(ep_square + (int8_t)from_col);
    }

    if ((piece == 'P' && to_row == 0u) ||
        (piece == 'p' && to_row == 7u)) {
        if (move[4] == '\0') {
            return 0u;
        }
        piece = promotion_piece(piece, move[4]);
    }

    chess_board[to_idx] = piece;
    chess_board[from_idx] = '.';
    rules_board[to_idx] = rules_piece_from_char(piece);
    rules_board[from_idx] = NETCHESSZX_RULE_EMPTY;
    if ((piece == 'P' || piece == 'p') &&
        from_col != to_col &&
        target == '.') {
        rules_board[(uint8_t)((from_row << 3) + to_col)] =
            NETCHESSZX_RULE_EMPTY;
    }
    if (((piece == 'K' && from_row == 7u) ||
         (piece == 'k' && from_row == 0u)) &&
        from_col == 4u && (to_col == 6u || to_col == 2u)) {
        uint8_t rook_from = (uint8_t)((from_row << 3) +
                                      (to_col == 6u ? 7u : 0u));
        uint8_t rook_to = (uint8_t)((from_row << 3) +
                                    (to_col == 6u ? 5u : 3u));
        rules_board[rook_to] = rules_board[rook_from];
        rules_board[rook_from] = NETCHESSZX_RULE_EMPTY;
    }

    side_to_move ^= 1u;
    return 1u;
}
#endif

uint8_t spectrum_board_apply_trusted_move(const char *move) NETCHESSZX_FASTCALL
{
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;

    if (!netchesszx_move_parse_coords(move, &from_row, &from_col, &to_row, &to_col)) {
        return 0u;
    }
#ifndef NETCHESSZX_HOST_TEST
    {
        uint16_t move_addr = (uint16_t)move;

        spectrum_overlay_context[0] = (uint8_t)move_addr;
        spectrum_overlay_context[1] = (uint8_t)(move_addr >> 8);
        spectrum_overlay_context[2] = from_row;
        spectrum_overlay_context[3] = from_col;
        spectrum_overlay_context[4] = to_row;
        spectrum_overlay_context[5] = to_col;
    }
    return spectrum_overlay_exec(SPECTRUM_OVL_BOARD, SPECTRUM_OVL_BOARD_APPLY);
#else
    return apply_parsed_move(move, from_row, from_col, to_row, to_col);
#endif
}

#ifdef NETCHESSZX_HOST_TEST
uint8_t spectrum_board_apply_move(const char *move) NETCHESSZX_FASTCALL
{
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;

    if (!netchesszx_move_parse_coords(move, &from_row, &from_col, &to_row, &to_col) ||
        !spectrum_board_is_legal_move(move)) {
        return 0u;
    }
    return apply_parsed_move(move, from_row, from_col, to_row, to_col);
}
#endif
