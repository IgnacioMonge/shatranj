#ifndef NETCHESSZX_SPECTRUM_GAME_STATS_H
#define NETCHESSZX_SPECTRUM_GAME_STATS_H

#include "common/chess/game_stats.h"
#include "spectrum/lowram_map.h"

#ifdef NETCHESSZX_FIXED_LOW_RAM
extern __at(NETCHESSZX_LOWRAM_GAME_STATS_ADDR)
#else
extern
#endif
uint8_t spectrum_game_stats[GAME_STATS_SIZE];
/* After OVER there is no live takeback. Its six-byte statistics backup can
   retain the result text pointer without another resident byte. */
#ifndef NETCHESSZX_HOST_SESSION_TEST
#define spectrum_game_stats_result \
    (*(const char **)(spectrum_game_stats + GAME_STATS_UNDO))
#endif

#endif
