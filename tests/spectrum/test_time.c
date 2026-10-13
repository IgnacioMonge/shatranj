#include "spectrum/app/time.h"
#include "spectrum/session/timing.h"

#include <stdio.h>

#define FRAMES_PER_SECOND \
    (NETCHESSZX_SESSION_FRAME_HZ == 60u ? 60u : 50u)

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static uint8_t tick_second(void)
{
    uint8_t dirty = 0u;
    uint8_t i;

    for (i = 0u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
        if (i + 1u < FRAMES_PER_SECOND) {
            check(dirty == 0u, "time advances only on the final frame");
        }
    }
    return dirty;
}

int main(void)
{
    const spectrum_time_view_t *time;
    uint8_t timers[6] = { 99u, 59u, 58u, 98u, 59u, 59u };
    uint8_t saved[6];
    uint8_t shifted;
    uint8_t i;
    uint8_t dirty;

    check(spectrum_time_clock_ready() == 0u, "clock starts unready");
    spectrum_time_game_timer_start();
    (void)tick_second();
    spectrum_time_game_timer_stop();
    check(spectrum_time_clock_ready() == 0u,
          "game timers do not mark the wall clock ready");
    spectrum_time_set_clock(23u, 59u, 59u);
    check(spectrum_time_clock_ready() == 1u, "setting clock marks it ready");
    check(tick_second() == SPECTRUM_TIME_DIRTY_CLOCK,
          "midnight dirties the clock display");
    time = spectrum_time_view();
    check(time->clock_hour == 0u && time->clock_minute == 0u &&
              time->clock_second == 0u,
          "wall clock advances across midnight");
    check(spectrum_time_fat_day_delta() == 1 &&
              spectrum_time_fat_time() == 0u,
          "FAT time tracks midnight");

    SPECTRUM_TIME_SHIFT_CLOCK(-1, shifted);
    check(shifted == 1u, "timezone shift reports a change");
    check(time->clock_hour == 23u && spectrum_time_fat_day_delta() == 0,
          "timezone shift adjusts the FAT date");
    SPECTRUM_TIME_SHIFT_CLOCK(24, shifted);
    check(time->clock_hour == 23u && spectrum_time_fat_day_delta() == 1,
          "full positive timezone range preserves the hour");
    SPECTRUM_TIME_SHIFT_CLOCK(-24, shifted);
    check(time->clock_hour == 23u && spectrum_time_fat_day_delta() == 0,
          "full negative timezone range preserves the hour");

    spectrum_time_set_fat_date(0x4a21u);
    check(spectrum_time_fat_date() == 0x4a21u, "FAT date round trip");

    spectrum_time_set_clock(12u, 0u, 0u);
    spectrum_time_game_timer_start();
    spectrum_time_game_timer_restore(timers);
    check(tick_second() == SPECTRUM_TIME_DIRTY_GAME,
          "active timers dirty only their presentation each second");
    spectrum_time_game_timer_save(saved);
    check(saved[0] == 99u && saved[1] == 59u && saved[2] == 59u,
          "game timer reaches its maximum");
    check(saved[3] == 99u && saved[4] == 0u && saved[5] == 0u,
          "move timer carries into hour 99");
    (void)tick_second();
    spectrum_time_game_timer_save(saved);
    check(saved[0] == 99u && saved[1] == 59u && saved[2] == 59u,
          "game timer remains saturated");
    spectrum_time_game_timer_stop();

    /* Stopwatch mutations must not discard a civil-clock fraction. */
    spectrum_time_set_clock(23u, 59u, 59u);
    spectrum_time_game_timer_start();
    for (i = 1u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
    }
    spectrum_time_move_timer_reset();
    spectrum_time_game_timer_stop();
    spectrum_time_game_timer_restore(timers);
    spectrum_time_game_timer_start();
    SPECTRUM_TIME_TICK(dirty);
    check(dirty == SPECTRUM_TIME_DIRTY_CLOCK,
          "civil-clock rollover dirties its display independently");
    check(time->clock_hour == 0u && time->clock_minute == 0u &&
              time->clock_second == 0u && spectrum_time_fat_day_delta() == 1,
          "stopwatch mutations preserve wall-clock midnight fraction");
    check(time->game_timers[2] == 0u && time->game_timers[5] == 0u,
          "a newly started stopwatch still waits a full second");
    for (i = 1u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
    }
    check(time->game_timers[2] == 1u && time->game_timers[5] == 1u,
          "stopwatch progresses independently of civil-clock boundaries");
    check(dirty == SPECTRUM_TIME_DIRTY_GAME,
          "stopwatch rollover dirties its display independently");

    for (i = 1u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
    }
    spectrum_time_move_timer_reset();
    SPECTRUM_TIME_TICK(dirty);
    check(time->game_timers[2] == 2u && time->game_timers[5] == 0u,
          "resetting TURN preserves the GAME fraction");
    for (i = 1u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
    }
    check(time->game_timers[2] == 2u && time->game_timers[5] == 1u,
          "reset TURN waits a complete second before advancing");

    spectrum_time_game_timer_start();
    for (i = 1u; i < FRAMES_PER_SECOND; ++i) {
        SPECTRUM_TIME_TICK(dirty);
    }
    spectrum_time_set_clock(12u, 0u, 0u);
    SPECTRUM_TIME_TICK(dirty);
    check(time->game_timers[2] == 1u && time->game_timers[5] == 1u &&
              time->clock_second == 0u,
          "clock synchronization preserves the stopwatch fraction");

    if (failures != 0) {
        return 1;
    }
    puts("time tests passed");
    return 0;
}
