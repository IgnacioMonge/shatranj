#include "spectrum/app/time.h"

spectrum_time_view_t spectrum_time_state;
uint8_t spectrum_time_frames;
#ifdef NETCHESSZX_FIXED_LOW_RAM
__at(NETCHESSZX_LOWRAM_TIME_MOVE_PHASE_ADDR)
#endif
uint8_t spectrum_time_move_frame_phase;
uint8_t spectrum_time_clock_frames;
int16_t spectrum_time_day_delta;
uint16_t spectrum_time_runtime_fat_date;

const spectrum_time_view_t *spectrum_time_snapshot(void)
{
    return &spectrum_time_state;
}

#ifndef NETCHESSZX_SDCC_IY
void spectrum_time_tick_one_second(uint8_t *hour,
                                   uint8_t *minute,
                                   uint8_t *second)
{
    if (*hour == 99u && *minute == 59u && *second == 59u) {
        return;
    }
    ++*second;
    if (*second < 60u) {
        return;
    }
    *second = 0u;
    ++*minute;
    if (*minute < 60u) {
        return;
    }
    *minute = 0u;
    ++*hour;
}
#endif
