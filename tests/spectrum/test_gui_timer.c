#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../src/spectrum/ui/gui.c"

static void check_time(uint8_t hour,
                       uint8_t minute,
                       uint8_t second,
                       uint8_t expected_hour,
                       uint8_t expected_minute,
                       uint8_t expected_second,
                       const char *message)
{
    if (hour != expected_hour || minute != expected_minute ||
        second != expected_second) {
        fprintf(stderr,
                "FAIL: %s (got %u:%02u:%02u, expected %u:%02u:%02u)\n",
                message,
                (unsigned)hour,
                (unsigned)minute,
                (unsigned)second,
                (unsigned)expected_hour,
                (unsigned)expected_minute,
                (unsigned)expected_second);
        exit(1);
    }
}

int main(void)
{
    uint8_t hour = 99u;
    uint8_t minute = 59u;
    uint8_t second = 0u;
    uint8_t timers[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    uint8_t saved[6] = { 0u };

    spectrum_gui_game_timer_restore(timers);
    spectrum_gui_game_timer_save(saved);
    if (memcmp(timers, saved, sizeof(timers)) != 0) {
        fputs("FAIL: save/restore timer state\n", stderr);
        return 1;
    }

    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 1u,
               "99:59 advances before the saturation second");

    second = 58u;
    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 59u,
               "timer reaches its 99:59:59 maximum");

    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 59u,
               "timer remains saturated at 99:59:59");

    hour = 98u;
    minute = 59u;
    second = 59u;
    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 0u, 0u,
               "timer carries into hour 99");

    check_time(shifted_clock_hour(23u, 2), 0u, 0u, 1u, 0u, 0u,
               "timezone shift wraps after midnight");
    check_time(shifted_clock_hour(1u, -3), 0u, 0u, 22u, 0u, 0u,
               "timezone shift wraps before midnight");
    check_time(shifted_clock_hour(9u, 24), 0u, 0u, 9u, 0u, 0u,
               "full positive timezone range preserves the hour");
    check_time(shifted_clock_hour(9u, -24), 0u, 0u, 9u, 0u, 0u,
               "full negative timezone range preserves the hour");

    puts("gui timer tests passed");
    return 0;
}
