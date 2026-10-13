#include "common/chess/game_stats.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_totals(game_stats_t *stats, uint16_t white, uint16_t black)
{
    stats->bytes[0] = (uint8_t)white;
    stats->bytes[1] = (uint8_t)(white >> 8);
    stats->bytes[GAME_STATS_SIDE_SIZE] = (uint8_t)black;
    stats->bytes[GAME_STATS_SIDE_SIZE + 1u] = (uint8_t)(black >> 8);
}

int main(void)
{
    game_stats_t stats;
    game_stats_t before;

    assert(sizeof(stats) == 33u);
    assert(GAME_STATS_CAPTURED + 5u == GAME_STATS_SIZE);
    assert(GAME_STATS_UNDO + GAME_STATS_SIDE_SIZE + 1u == GAME_STATS_CAPTURED);
    game_stats_reset(&stats, 0u, 0u);
    assert(game_stats_moves(&stats, 0u, 0u) == 0u);
    assert(game_stats_total(&stats, 0u) == 0u);
    assert(game_stats_white_share(&stats) == 255u);
    game_stats_record(&stats, 1u, 0u, 0u);
    assert(game_stats_at(&stats, 0u) == 1u);
    assert(game_stats_fastest(&stats, 0u) == 0u);
    assert(game_stats_fastest_at(&stats, 0u) == 1u);
    game_stats_record(&stats, 2u, 18u, 0u);
    game_stats_record(&stats, 3u, 24u, 1u);
    game_stats_record(&stats, 4u, 18u, 2u);
    game_stats_record(&stats, 5u, 24u, 5u);
    assert(game_stats_total(&stats, 0u) == 48u);
    assert(game_stats_total(&stats, 1u) == 36u);
    assert(game_stats_moves(&stats, 5u, 0u) == 3u);
    assert(game_stats_moves(&stats, 5u, 1u) == 2u);
    assert(game_stats_longest(&stats, 0u) == 24u);
    assert(game_stats_at(&stats, 0u) == 2u);
    assert(game_stats_at(&stats, 1u) == 1u);
    /* Ties keep the first occurrence for both extremes. */
    assert(game_stats_fastest(&stats, 0u) == 0u);
    assert(game_stats_fastest_at(&stats, 0u) == 1u);
    assert(game_stats_fastest(&stats, 1u) == 18u);
    assert(game_stats_fastest_at(&stats, 1u) == 1u);
    assert(game_stats_material(&stats, 0u) == 10u);
    assert(game_stats_material(&stats, 1u) == 3u);
    assert(game_stats_balance(&stats) == 7);
    assert(game_stats_white_share(&stats) == 57u);
    game_stats_takeback(&stats, 4u, 5u);
    assert(game_stats_total(&stats, 0u) == 24u);
    assert(game_stats_captured(&stats, 0u, 5u) == 0u);
    assert(game_stats_captured(&stats, 0u, 1u) == 1u);
    assert(game_stats_balance(&stats) == -2);
    assert((stats.bytes[GAME_STATS_FLAGS] & GAME_STATS_PARTIAL) == 0u);

    /* Undo restores the previous maximum and its number, not just total. */
    game_stats_record(&stats, 5u, 190u, 4u);
    assert(game_stats_at(&stats, 0u) == 3u);
    game_stats_takeback(&stats, 4u, 4u);
    assert(game_stats_longest(&stats, 0u) == 24u);
    assert(game_stats_at(&stats, 0u) == 2u);

    /* Undo restores the previous minimum and its number as well. */
    game_stats_reset(&stats, 0u, 0u);
    game_stats_record(&stats, 1u, 30u, 0u);
    game_stats_record(&stats, 2u, 9u, 0u);
    game_stats_record(&stats, 3u, 12u, 0u);
    assert(game_stats_fastest(&stats, 0u) == 12u);
    assert(game_stats_fastest_at(&stats, 0u) == 2u);
    assert(game_stats_longest(&stats, 0u) == 30u);
    assert(game_stats_at(&stats, 0u) == 1u);
    game_stats_takeback(&stats, 2u, 0u);
    assert(game_stats_fastest(&stats, 0u) == 30u);
    assert(game_stats_fastest_at(&stats, 0u) == 1u);

    /* Undo also recovers an exact total after a saturating addition. */
    game_stats_reset(&stats, 0u, 0u);
    game_stats_record(&stats, 1u, 65000u, 0u);
    game_stats_record(&stats, 2u, 1u, 0u);
    game_stats_record(&stats, 3u, 600u, 0u);
    assert(game_stats_total(&stats, 0u) == 65535u);
    assert(stats.bytes[GAME_STATS_FLAGS] & GAME_STATS_TOTAL_LIMIT_WHITE);
    assert(game_stats_white_share(&stats) == 100u);
    game_stats_takeback(&stats, 2u, 0u);
    assert(game_stats_total(&stats, 0u) == 65000u);
    assert((stats.bytes[GAME_STATS_FLAGS] & GAME_STATS_TOTAL_LIMIT_WHITE) == 0u);
    assert(game_stats_at(&stats, 0u) == 1u);

    /* Loaded games with odd base ply start with black; no phantom samples. */
    game_stats_reset(&stats, 43u, 1u);
    game_stats_record(&stats, 44u, 108u, 1u);
    assert(game_stats_moves(&stats, 44u, 0u) == 0u);
    assert(game_stats_moves(&stats, 44u, 1u) == 1u);
    assert(game_stats_at(&stats, 1u) == 22u);
    assert(game_stats_fastest_at(&stats, 1u) == 22u);
    assert(game_stats_white_share(&stats) == 0u);
    assert(stats.bytes[GAME_STATS_FLAGS] & GAME_STATS_PARTIAL);

    /* Large values saturate explicitly, including the final wire ply. */
    game_stats_reset(&stats, 65533u, 1u);
    game_stats_record(&stats, 65534u, 65536u, 5u);
    assert(game_stats_total(&stats, 1u) == 65535u);
    assert(game_stats_longest(&stats, 1u) == 65535u);
    assert(game_stats_at(&stats, 1u) == GAME_STATS_AT_SATURATED);
    assert(game_stats_fastest_at(&stats, 1u) == GAME_STATS_AT_SATURATED);
    assert(stats.bytes[GAME_STATS_FLAGS] & (GAME_STATS_TOTAL_LIMIT_WHITE << 1));
    assert(game_stats_moves(&stats, 65535u, 0u) == 1u);
    before = stats;
    game_stats_record(&stats, 65535u, 0xffffffffu, 0u);
    game_stats_takeback(&stats, 65534u, 0u);
    assert(memcmp(stats.bytes, before.bytes, GAME_STATS_FLAGS) == 0);
    assert((stats.bytes[GAME_STATS_FLAGS] & ~GAME_STATS_UNDO_VALID) ==
           (before.bytes[GAME_STATS_FLAGS] & ~GAME_STATS_UNDO_VALID));

    /* Without a corresponding move snapshot, do not invent undo history. */
    game_stats_takeback(&stats, 65533u, 0u);
    assert(stats.bytes[GAME_STATS_FLAGS] == GAME_STATS_PARTIAL);
    assert(game_stats_total(&stats, 1u) == 0u);

    /* Share: rounded, scaled into 16 bits, black is the complement. */
    game_stats_reset(&stats, 0u, 0u);
    set_totals(&stats, 1u, 2u);
    assert(game_stats_white_share(&stats) == 33u);
    set_totals(&stats, 2u, 1u);
    assert(game_stats_white_share(&stats) == 67u);
    set_totals(&stats, 751u, 545u);
    assert(game_stats_white_share(&stats) == 58u);
    set_totals(&stats, 65535u, 65535u);
    assert(game_stats_white_share(&stats) == 50u);
    set_totals(&stats, 0u, 65535u);
    assert(game_stats_white_share(&stats) == 0u);

    assert(game_stats_capture_type('p') == 1u);
    assert(game_stats_capture_type('Q') == 5u);
    assert(game_stats_capture_type('K') == 0u);
    assert(game_stats_capture_type('.') == 0u);
    /* King against king: all 15 enemy pieces, independently for each side. */
    {
        static const uint8_t captures[15] = {
            5u, 4u, 4u, 3u, 3u, 2u, 2u,
            1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u
        };
        uint16_t ply;

        game_stats_reset(&stats, 0u, 0u);
        for (ply = 1u; ply <= 30u; ++ply) {
            game_stats_record(&stats, ply, 1u, captures[(ply - 1u) / 2u]);
        }
        assert(game_stats_captured(&stats, 0u, 1u) == 8u);
        assert(game_stats_captured(&stats, 1u, 1u) == 8u);
        assert(game_stats_captured(&stats, 0u, 2u) == 2u);
        assert(game_stats_captured(&stats, 1u, 4u) == 2u);
        assert(game_stats_captured(&stats, 0u, 5u) == 1u);
        assert(game_stats_captured(&stats, 1u, 5u) == 1u);
        assert(game_stats_material(&stats, 0u) == 39u);
        assert(game_stats_balance(&stats) == 0);
        game_stats_takeback(&stats, 29u, 1u);
        assert(game_stats_captured(&stats, 1u, 1u) == 7u);
        assert(game_stats_balance(&stats) == 1);
    }
    game_stats_reset(&stats, 0u, 0u);
    assert(game_stats_balance(&stats) == 0);
    assert(game_stats_material(&stats, 1u) == 0u);
    assert(stats.bytes[GAME_STATS_FLAGS] == 0u);
    puts("game stats: OK");
    return 0;
}
