#ifndef NETCHESSZX_SPECTRUM_APP_TIME_H
#define NETCHESSZX_SPECTRUM_APP_TIME_H

#include "spectrum/session/timing.h"
#include "spectrum/app/time_view.h"
#ifdef NETCHESSZX_FIXED_LOW_RAM
#include "spectrum/lowram_map.h"
#endif

#include <stdint.h>
#include <string.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define SPECTRUM_TIME_DIRTY_CLOCK 0x01u
#define SPECTRUM_TIME_DIRTY_GAME 0x02u
#define SPECTRUM_TIME_FRAMES_PER_SECOND \
    ((uint8_t)(NETCHESSZX_SESSION_IS_60HZ ? 60u : 50u))

/* Application-owner operations. UI may include only time_view.h. */
extern spectrum_time_view_t spectrum_time_state;
extern uint8_t spectrum_time_frames;
#ifdef NETCHESSZX_FIXED_LOW_RAM
extern __at(NETCHESSZX_LOWRAM_TIME_MOVE_PHASE_ADDR) uint8_t spectrum_time_move_frame_phase;
#else
extern uint8_t spectrum_time_move_frame_phase;
#endif
/* Civil time keeps its fraction across all stopwatch operations. */
extern uint8_t spectrum_time_clock_frames;
extern int16_t spectrum_time_day_delta;
extern uint16_t spectrum_time_runtime_fat_date;

#define spectrum_time_view() \
    ((const spectrum_time_view_t *)&spectrum_time_state)

#ifdef NETCHESSZX_SDCC_IY
void netchesszx_asm_timer_tick_one_second(uint8_t *hour,
                                          uint8_t *minute,
                                          uint8_t *second);
#define SPECTRUM_TIME_TICK_ONE_SECOND(hour, minute, second) \
    netchesszx_asm_timer_tick_one_second((hour), (minute), (second))
#else
void spectrum_time_tick_one_second(uint8_t *hour,
                                   uint8_t *minute,
                                   uint8_t *second);
#define SPECTRUM_TIME_TICK_ONE_SECOND(hour, minute, second) \
    spectrum_time_tick_one_second((hour), (minute), (second))
#endif

#define SPECTRUM_TIME_TICK(dirty) do { \
    (dirty) = 0u; \
    if (spectrum_time_state.game_timer_active) { \
        if (++spectrum_time_frames >= SPECTRUM_TIME_FRAMES_PER_SECOND) { \
            spectrum_time_frames = 0u; \
            SPECTRUM_TIME_TICK_ONE_SECOND( \
                &spectrum_time_state.game_timers[0], \
                &spectrum_time_state.game_timers[1], \
                &spectrum_time_state.game_timers[2]); \
            (dirty) = SPECTRUM_TIME_DIRTY_GAME; \
        } \
        if (spectrum_time_frames == spectrum_time_move_frame_phase) { \
            SPECTRUM_TIME_TICK_ONE_SECOND( \
                &spectrum_time_state.game_timers[3], \
                &spectrum_time_state.game_timers[4], \
                &spectrum_time_state.game_timers[5]); \
            (dirty) = SPECTRUM_TIME_DIRTY_GAME; \
        } \
    } \
    if (spectrum_time_state.clock_valid && \
        ++spectrum_time_clock_frames >= SPECTRUM_TIME_FRAMES_PER_SECOND) { \
        spectrum_time_clock_frames = 0u; \
        ++spectrum_time_state.clock_second; \
        if (spectrum_time_state.clock_second >= 60u) { \
            spectrum_time_state.clock_second = 0u; \
            ++spectrum_time_state.clock_minute; \
            if (spectrum_time_state.clock_minute >= 60u) { \
                spectrum_time_state.clock_minute = 0u; \
                ++spectrum_time_state.clock_hour; \
                if (spectrum_time_state.clock_hour >= 24u) { \
                    spectrum_time_state.clock_hour = 0u; \
                    ++spectrum_time_day_delta; \
                } \
            } \
            (dirty) |= SPECTRUM_TIME_DIRTY_CLOCK; \
        } \
    } \
} while (0)

#define spectrum_time_set_clock(hour, minute, second) do { \
    spectrum_time_state.clock_hour = (hour); \
    spectrum_time_state.clock_minute = (minute); \
    spectrum_time_state.clock_second = (second); \
    spectrum_time_state.clock_valid = 1u; \
    spectrum_time_clock_frames = 0u; \
    spectrum_time_day_delta = 0; \
} while (0)

#define SPECTRUM_TIME_SHIFT_CLOCK(hour_delta, changed) do { \
    int8_t time_shift_delta_ = (hour_delta); \
    int8_t time_shifted_; \
    (changed) = 0u; \
    if (spectrum_time_state.clock_valid && time_shift_delta_ != 0) { \
        time_shifted_ = (int8_t)spectrum_time_state.clock_hour + \
                        time_shift_delta_; \
        if (time_shifted_ < 0) { \
            --spectrum_time_day_delta; \
            time_shifted_ += 24; \
        } else if (time_shifted_ >= 24) { \
            ++spectrum_time_day_delta; \
            time_shifted_ -= 24; \
        } \
        spectrum_time_state.clock_hour = (uint8_t)time_shifted_; \
        (changed) = 1u; \
    } \
} while (0)

#define spectrum_time_clock_ready() (spectrum_time_view()->clock_valid)
#define spectrum_time_set_fat_date(date) \
    (spectrum_time_runtime_fat_date = (date))
#define spectrum_time_fat_date() spectrum_time_runtime_fat_date
#define spectrum_time_fat_time() \
    ((uint16_t)(((uint16_t)spectrum_time_state.clock_hour << 11) | \
                ((uint16_t)spectrum_time_state.clock_minute << 5) | \
                (spectrum_time_state.clock_second >> 1)))
#define spectrum_time_fat_day_delta() spectrum_time_day_delta

#define spectrum_time_move_timer_reset() do { \
    spectrum_time_state.game_timers[3] = 0u; \
    spectrum_time_state.game_timers[4] = 0u; \
    spectrum_time_state.game_timers[5] = 0u; \
    spectrum_time_move_frame_phase = spectrum_time_frames; \
} while (0)

#define spectrum_time_game_timer_start() do { \
    spectrum_time_state.game_timer_active = 1u; \
    spectrum_time_state.game_timers[0] = 0u; \
    spectrum_time_state.game_timers[1] = 0u; \
    spectrum_time_state.game_timers[2] = 0u; \
    spectrum_time_frames = 0u; \
    spectrum_time_move_timer_reset(); \
} while (0)

#define spectrum_time_game_timer_stop() do { \
    spectrum_time_state.game_timer_active = 0u; \
    spectrum_time_frames = 0u; \
    spectrum_time_move_frame_phase = 0u; \
} while (0)

#define spectrum_time_game_timer_save(timers) \
    memcpy((timers), spectrum_time_state.game_timers, \
           sizeof(spectrum_time_state.game_timers))
#define spectrum_time_game_timer_restore(timers) do { \
    memcpy(spectrum_time_state.game_timers, (timers), \
           sizeof(spectrum_time_state.game_timers)); \
    spectrum_time_frames = 0u; \
    spectrum_time_move_frame_phase = 0u; \
} while (0)

#endif
