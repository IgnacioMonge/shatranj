#include <stdint.h>

#include "common/chess/rules_compact.h"
#include "spectrum/board/board.h"
#include "spectrum/lowram_map.h"

#define chess_board ((char *)NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR)
#define rules_board ((int8_t *)NETCHESSZX_LOWRAM_RULES_BOARD_ADDR)

extern uint8_t side_to_move;
extern uint8_t castle_rights;
extern int8_t ep_square;

uint8_t abs_delta(uint8_t a, uint8_t b);
uint8_t piece_side(char piece);
int8_t rules_piece_from_char(char piece);
char promotion_piece(char pawn, char promo);

#include "spectrum/board/board_apply_impl.h"

static spectrum_board_snapshot_t *snapshot_from_ctx(uint8_t *ctx)
{
    return (spectrum_board_snapshot_t *)((uint16_t)ctx[0] |
                                        ((uint16_t)ctx[1] << 8));
}

uint8_t board_snapshot_restore_ovl(uint8_t *ctx) __z88dk_fastcall
{
    spectrum_board_snapshot_t *snapshot = snapshot_from_ctx(ctx);
    uint8_t i;

    for (i = 0u; i < 64u; ++i) {
        board_apply_set_cell(i, snapshot->cells[i]);
    }
    side_to_move = snapshot->side;
    castle_rights = snapshot->castle;
    ep_square = snapshot->ep;
    return 1u;
}

uint8_t board_apply_trusted_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *move = (const char *)((uint16_t)ctx[0] | ((uint16_t)ctx[1] << 8));
    spectrum_board_undo_t *undo =
        (spectrum_board_undo_t *)((uint16_t)ctx[6] |
                                  ((uint16_t)ctx[7] << 8));

    return board_apply_parsed_move(move, ctx[2], ctx[3], ctx[4], ctx[5], undo);
}

uint8_t board_undo_restore_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const spectrum_board_undo_t *undo =
        (const spectrum_board_undo_t *)((uint16_t)ctx[0] |
                                        ((uint16_t)ctx[1] << 8));
    board_apply_undo_restore(undo);
    return 1u;
}
