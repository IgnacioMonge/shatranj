#ifndef NETCHESSZX_SPECTRUM_APP_TIME_VIEW_H
#define NETCHESSZX_SPECTRUM_APP_TIME_VIEW_H

#include <stdint.h>

typedef struct spectrum_time_view {
    uint8_t clock_hour;
    uint8_t clock_minute;
    uint8_t clock_second;
    uint8_t clock_valid;
    uint8_t game_timer_active;
    uint8_t game_timers[6];
} spectrum_time_view_t;

/* Borrowed read-only resident view; presentation has no mutating time API. */
const spectrum_time_view_t *spectrum_time_snapshot(void);

#endif
