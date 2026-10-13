#ifndef NETCHESSZX_GAME_STATS_H
#define NETCHESSZX_GAME_STATS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte layout is also the Spectrum SAVELOAD oracle: no padding or host endian
   dependency. Colours are 0 = white, 1 = black; piece types P/N/B/R/Q = 1..5.
   Per side: total(2) longest(2) longest-at(1) fastest(2) fastest-at(1).
   An AT of 0 means "no move yet"; 255 means move 255 or later. */
#define GAME_STATS_SIZE 33u
#define GAME_STATS_SIDE_SIZE 8u
#define GAME_STATS_LONGEST 2u
#define GAME_STATS_FASTEST 5u
#define GAME_STATS_BASE 16u
#define GAME_STATS_FLAGS 18u
#define GAME_STATS_UNDO 19u
#define GAME_STATS_CAPTURED 28u
#define GAME_STATS_AT_SATURATED 255u
#define GAME_STATS_PARTIAL 0x01u
#define GAME_STATS_TOTAL_LIMIT_WHITE 0x02u
#define GAME_STATS_UNDO_VALID 0x20u

typedef struct {
    uint8_t bytes[GAME_STATS_SIZE];
} game_stats_t;

void game_stats_reset(game_stats_t *stats, uint16_t base_ply, uint8_t partial);
/* Call exactly once per committed move, after the position/ply changes and
   before TURN resets. Rejected moves and duplicate protocol traffic never call. */
void game_stats_record(game_stats_t *stats, uint16_t ply,
                       uint32_t seconds, uint8_t captured_type);
/* The existing one-move undo supplies the same captured type as record. */
void game_stats_takeback(game_stats_t *stats, uint16_t ply_after,
                         uint8_t captured_type);
uint16_t game_stats_total(const game_stats_t *stats, uint8_t side);
uint16_t game_stats_longest(const game_stats_t *stats, uint8_t side);
uint8_t game_stats_at(const game_stats_t *stats, uint8_t side);
uint16_t game_stats_fastest(const game_stats_t *stats, uint8_t side);
uint8_t game_stats_fastest_at(const game_stats_t *stats, uint8_t side);
uint16_t game_stats_moves(const game_stats_t *stats, uint16_t ply, uint8_t side);
uint8_t game_stats_captured(const game_stats_t *stats, uint8_t side, uint8_t type);
uint8_t game_stats_capture_type(char piece);
/* Material taken by one side: pawn 1, knight/bishop 3, rook 5, queen 9. */
uint16_t game_stats_material(const game_stats_t *stats, uint8_t side);
int16_t game_stats_balance(const game_stats_t *stats);
/* White's share of both totals, rounded; black's is 100 minus it. Returns
   255 when no time was recorded. Mirrors the Spectrum page's scaling. */
uint8_t game_stats_white_share(const game_stats_t *stats);

#ifdef __cplusplus
}
#endif
#endif
