#include "common/chess/legal.h"

#include "mcu-max.h"

#include <stddef.h>
#include <stdint.h>

#define NETCHESSZX_MAX_LEGAL_MOVES 256u

static int parse_move(const char *text, mcumax_move *move)
{
    char promotion;

    if (text == NULL || move == NULL) {
        return NETCHESSZX_ERR_NULL;
    }

    if (text[0] < 'a' || text[0] > 'h' ||
        text[1] < '1' || text[1] > '8' ||
        text[2] < 'a' || text[2] > 'h' ||
        text[3] < '1' || text[3] > '8') {
        return NETCHESSZX_ERR_MOVE;
    }

    promotion = text[4];
    if (promotion != '\0') {
        if (text[5] != '\0') {
            return NETCHESSZX_ERR_MOVE;
        }
        if (promotion != 'q') {
            return NETCHESSZX_ERR_UNSUPPORTED;
        }
    }

    move->from = (mcumax_square)((uint8_t)(text[0] - 'a') +
                                 (uint8_t)(16u * (uint8_t)('8' - text[1])));
    move->to = (mcumax_square)((uint8_t)(text[2] - 'a') +
                               (uint8_t)(16u * (uint8_t)('8' - text[3])));

    return NETCHESSZX_OK;
}

static int parse_square(const char *text, mcumax_square *square)
{
    if (text == NULL || square == NULL) {
        return NETCHESSZX_ERR_NULL;
    }

    if (text[0] < 'a' || text[0] > 'h' ||
        text[1] < '1' || text[1] > '8' ||
        text[2] != '\0') {
        return NETCHESSZX_ERR_MOVE;
    }

    *square = (mcumax_square)((uint8_t)(text[0] - 'a') +
                              (uint8_t)(16u * (uint8_t)('8' - text[1])));
    return NETCHESSZX_OK;
}

static int move_is_listed(mcumax_move move)
{
    mcumax_move moves[NETCHESSZX_MAX_LEGAL_MOVES];
    uint32_t count;
    uint32_t i;

    count = mcumax_search_valid_moves(moves, NETCHESSZX_MAX_LEGAL_MOVES);
    if (count > NETCHESSZX_MAX_LEGAL_MOVES) {
        count = NETCHESSZX_MAX_LEGAL_MOVES;
    }

    for (i = 0; i < count; ++i) {
        if (moves[i].from == move.from && moves[i].to == move.to) {
            return NETCHESSZX_OK;
        }
    }

    return NETCHESSZX_ERR_ILLEGAL;
}

int netchesszx_rules_reset(void)
{
    mcumax_init();
    return NETCHESSZX_OK;
}

int netchesszx_rules_can_play(const char *move_text)
{
    mcumax_move move;
    int rc;

    rc = parse_move(move_text, &move);
    if (rc != NETCHESSZX_OK) {
        return rc;
    }

    return move_is_listed(move);
}

int netchesszx_rules_play(const char *move_text)
{
    mcumax_move move;
    int rc;

    rc = parse_move(move_text, &move);
    if (rc != NETCHESSZX_OK) {
        return rc;
    }

    if (move_is_listed(move) != NETCHESSZX_OK) {
        return NETCHESSZX_ERR_ILLEGAL;
    }

    if (!mcumax_play_move(move)) {
        return NETCHESSZX_ERR_ILLEGAL;
    }

    return NETCHESSZX_OK;
}

int netchesszx_rules_has_legal_moves(void)
{
    mcumax_move move;

    return mcumax_search_valid_moves(&move, 1u) != 0u;
}

size_t netchesszx_rules_state_size(void)
{
    return mcumax_state_size();
}

#ifndef NETCHESSZX_FIXED_LOW_RAM
int netchesszx_rules_save(void *state, size_t cap)
{
    if (state == NULL) {
        return NETCHESSZX_ERR_NULL;
    }
    if (cap < mcumax_state_size()) {
        return NETCHESSZX_ERR_MOVE;
    }
    mcumax_get_state(state);
    return NETCHESSZX_OK;
}

int netchesszx_rules_restore(const void *state, size_t cap)
{
    if (state == NULL) {
        return NETCHESSZX_ERR_NULL;
    }
    if (cap < mcumax_state_size()) {
        return NETCHESSZX_ERR_MOVE;
    }
    mcumax_set_state(state);
    return NETCHESSZX_OK;
}
#endif

int netchesszx_rules_legal_targets(const char *from_text, char *out, size_t cap)
{
    mcumax_move moves[NETCHESSZX_MAX_LEGAL_MOVES];
    mcumax_square from;
    uint32_t count;
    uint32_t i;
    size_t used;
    int rc;

    if (out == NULL || cap == 0u) {
        return NETCHESSZX_ERR_NULL;
    }
    out[0] = '\0';

    rc = parse_square(from_text, &from);
    if (rc != NETCHESSZX_OK) {
        return rc;
    }

    count = mcumax_search_valid_moves(moves, NETCHESSZX_MAX_LEGAL_MOVES);
    if (count > NETCHESSZX_MAX_LEGAL_MOVES) {
        count = NETCHESSZX_MAX_LEGAL_MOVES;
    }

    used = 0;
    for (i = 0; i < count; ++i) {
        if (moves[i].from == from) {
            char file = (char)('a' + (moves[i].to & 0x07u));
            char rank = (char)('8' - ((moves[i].to >> 4) & 0x07u));

            if (used != 0u) {
                if (used + 1u >= cap) {
                    return NETCHESSZX_ERR_BUFFER;
                }
                out[used++] = ' ';
            }

            if (used + 2u >= cap) {
                return NETCHESSZX_ERR_BUFFER;
            }
            out[used++] = file;
            out[used++] = rank;
            out[used] = '\0';
        }
    }

    return NETCHESSZX_OK;
}
