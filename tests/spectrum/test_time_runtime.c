#include "spectrum/app/time.h"
#include "spectrum/platform/net_runtime.h"
#include <assert.h>
#include <stdio.h>

static uint16_t host_frame_count;
static unsigned present_ticks;

/* No GUI implementation is linked: presentation cannot drive this clock. */
void spectrum_gui_present_tick(uint8_t dirty)
{
    (void)dirty;
    ++present_ticks;
}
void spectrum_gui_time_changed(uint8_t dirty) { (void)dirty; }
uint16_t spectrum_frame_count(void) { return host_frame_count; }
void spectrum_frame_wait(void) { ++host_frame_count; }
void spectrum_net_background_drain_clock(void) {}

int main(void)
{
    uint8_t frame;
    unsigned presented;
    const uint8_t saved[6] = {1u, 2u, 3u, 4u, 5u, 6u};
    spectrum_app_tick_init();
    spectrum_net_runtime_set_clock(23u, 59u, 59u);
    spectrum_app_game_timer_start();
    spectrum_app_game_timer_restore(saved);
    for (frame = 0u; frame < SPECTRUM_TIME_FRAMES_PER_SECOND; ++frame) {
        spectrum_net_runtime_wait_frame();
    }
    assert(spectrum_time_snapshot()->clock_hour == 0u);
    assert(spectrum_net_runtime_fat_day_delta() == 1);
    assert(spectrum_time_snapshot()->game_timers[2] == 4u);
    assert(spectrum_time_snapshot()->game_timers[5] == 7u);

    presented = present_ticks;
    host_frame_count = (uint16_t)(host_frame_count +
        5u * SPECTRUM_TIME_FRAMES_PER_SECOND + 7u);
    spectrum_app_tick();
    assert(spectrum_time_snapshot()->clock_second == 5u);
    assert(spectrum_time_snapshot()->game_timers[2] == 9u);
    assert(spectrum_time_snapshot()->game_timers[5] == 12u);
    assert(spectrum_time_frames == 7u && spectrum_time_move_frame_phase == 0u);
    assert(present_ticks == presented + 1u);
    spectrum_app_tick();
    assert(present_ticks == presented + 1u);

    spectrum_app_game_timer_stop();
    for (frame = 0u; frame < SPECTRUM_TIME_FRAMES_PER_SECOND; ++frame) {
        spectrum_net_runtime_wait_frame_plain();
    }
    assert(spectrum_time_snapshot()->clock_second == 6u);
    assert(spectrum_time_snapshot()->game_timers[2] == 9u);
    puts("application time follows elapsed hardware frames without GUI");
    return 0;
}
