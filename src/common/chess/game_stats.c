#include "common/chess/game_stats.h"

#include <string.h>

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void write16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

void game_stats_reset(game_stats_t *stats, uint16_t base_ply, uint8_t partial)
{
    memset(stats, 0, sizeof(*stats));
    write16(stats->bytes + GAME_STATS_BASE, base_ply);
    stats->bytes[GAME_STATS_FLAGS] = partial ? GAME_STATS_PARTIAL : 0u;
}

uint16_t game_stats_total(const game_stats_t *stats, uint8_t side)
{
    return read16(stats->bytes + side * GAME_STATS_SIDE_SIZE);
}

uint16_t game_stats_longest(const game_stats_t *stats, uint8_t side)
{
    return read16(stats->bytes + side * GAME_STATS_SIDE_SIZE + GAME_STATS_LONGEST);
}

uint8_t game_stats_at(const game_stats_t *stats, uint8_t side)
{
    return stats->bytes[side * GAME_STATS_SIDE_SIZE + GAME_STATS_LONGEST + 2u];
}

uint16_t game_stats_fastest(const game_stats_t *stats, uint8_t side)
{
    return read16(stats->bytes + side * GAME_STATS_SIDE_SIZE + GAME_STATS_FASTEST);
}

uint8_t game_stats_fastest_at(const game_stats_t *stats, uint8_t side)
{
    return stats->bytes[side * GAME_STATS_SIDE_SIZE + GAME_STATS_FASTEST + 2u];
}

uint16_t game_stats_moves(const game_stats_t *stats, uint16_t ply, uint8_t side)
{
    uint16_t base = read16(stats->bytes + GAME_STATS_BASE);
    uint32_t round = side == 0u ? 1u : 0u;

    if (ply < base) {
        return 0u;
    }
    return (uint16_t)(((uint32_t)ply + round) / 2u -
                      ((uint32_t)base + round) / 2u);
}

uint8_t game_stats_captured(const game_stats_t *stats, uint8_t side, uint8_t type)
{
    return (uint8_t)((stats->bytes[GAME_STATS_CAPTURED + type - 1u] >>
                     (side * 4u)) & 15u);
}

static void capture_change(game_stats_t *stats, uint8_t side, uint8_t type,
                            int8_t delta)
{
    uint8_t count;
    uint8_t *packed;
    uint8_t shift = (uint8_t)(side * 4u);

    if (type == 0u) {
        return;
    }
    count = game_stats_captured(stats, side, type);
    if ((delta > 0 && count == 15u) || (delta < 0 && count == 0u)) {
        stats->bytes[GAME_STATS_FLAGS] |= GAME_STATS_PARTIAL;
        return;
    }
    packed = stats->bytes + GAME_STATS_CAPTURED + type - 1u;
    *packed = (uint8_t)((*packed & (uint8_t)~(15u << shift)) |
                        ((uint8_t)(count + delta) << shift));
}

/* Extreme (value, AT) pair: an AT of 0 marks the side's first move, which
   always replaces; ties keep the first occurrence. */
static void update_extreme(uint8_t *extreme, uint16_t sample, uint16_t ply,
                           uint8_t fastest)
{
    uint16_t current = read16(extreme);
    uint16_t at = (uint16_t)(((uint32_t)ply + 1u) / 2u);

    if (extreme[2] == 0u ||
        (fastest ? sample < current : sample > current)) {
        write16(extreme, sample);
        extreme[2] = at > GAME_STATS_AT_SATURATED ?
            (uint8_t)GAME_STATS_AT_SATURATED : (uint8_t)at;
    }
}

void game_stats_record(game_stats_t *stats, uint16_t ply,
                       uint32_t seconds, uint8_t captured_type)
{
    uint8_t side = (uint8_t)((ply - 1u) & 1u);
    uint8_t *entry = stats->bytes + side * GAME_STATS_SIDE_SIZE;
    uint8_t *flags = stats->bytes + GAME_STATS_FLAGS;
    uint16_t total = read16(entry);
    uint16_t sample = seconds > 65535u ? 65535u : (uint16_t)seconds;

    memcpy(stats->bytes + GAME_STATS_UNDO, entry, GAME_STATS_SIDE_SIZE);
    stats->bytes[GAME_STATS_UNDO + GAME_STATS_SIDE_SIZE] = *flags;
    *flags |= GAME_STATS_UNDO_VALID;
    if (seconds > (uint32_t)(65535u - total)) {
        write16(entry, 65535u);
        *flags |= (uint8_t)(GAME_STATS_TOTAL_LIMIT_WHITE << side);
    } else {
        write16(entry, (uint16_t)(total + seconds));
    }
    update_extreme(entry + GAME_STATS_LONGEST, sample, ply, 0u);
    update_extreme(entry + GAME_STATS_FASTEST, sample, ply, 1u);
    capture_change(stats, side, captured_type, 1);
}

void game_stats_takeback(game_stats_t *stats, uint16_t ply_after,
                         uint8_t captured_type)
{
    uint8_t side = (uint8_t)(ply_after & 1u);

    if ((stats->bytes[GAME_STATS_FLAGS] & GAME_STATS_UNDO_VALID) == 0u) {
        game_stats_reset(stats, ply_after, 1u);
        return;
    }
    memcpy(stats->bytes + side * GAME_STATS_SIDE_SIZE,
           stats->bytes + GAME_STATS_UNDO, GAME_STATS_SIDE_SIZE);
    stats->bytes[GAME_STATS_FLAGS] = (uint8_t)(
        stats->bytes[GAME_STATS_UNDO + GAME_STATS_SIDE_SIZE] &
        (uint8_t)~GAME_STATS_UNDO_VALID);
    capture_change(stats, side, captured_type, -1);
}

uint8_t game_stats_capture_type(char piece)
{
    switch (piece) {
    case 'P': case 'p': return 1u;
    case 'N': case 'n': return 2u;
    case 'B': case 'b': return 3u;
    case 'R': case 'r': return 4u;
    case 'Q': case 'q': return 5u;
    default: return 0u;
    }
}

uint16_t game_stats_material(const game_stats_t *stats, uint8_t side)
{
    static const uint8_t value[5] = {1u, 3u, 3u, 5u, 9u};
    uint16_t material = 0u;
    uint8_t type;

    for (type = 1u; type <= 5u; ++type) {
        material = (uint16_t)(material + value[type - 1u] *
                              game_stats_captured(stats, side, type));
    }
    return material;
}

int16_t game_stats_balance(const game_stats_t *stats)
{
    return (int16_t)((int16_t)game_stats_material(stats, 0u) -
                     (int16_t)game_stats_material(stats, 1u));
}

uint8_t game_stats_white_share(const game_stats_t *stats)
{
    uint32_t sum = (uint32_t)game_stats_total(stats, 0u) +
                   game_stats_total(stats, 1u);
    uint16_t white = game_stats_total(stats, 0u);

    if (sum == 0u) {
        return 255u;
    }
    /* Keep white * 100 + sum / 2 inside 16 bits, exactly as the Z80 page. */
    while (sum >= 640u) {
        sum >>= 1;
        white >>= 1;
    }
    return (uint8_t)(((uint16_t)(white * 100u) + (uint16_t)(sum / 2u)) /
                     (uint16_t)sum);
}
