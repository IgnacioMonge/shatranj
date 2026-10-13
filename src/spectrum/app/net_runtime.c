#include "spectrum/platform/net_runtime.h"

#include "spectrum/app/time.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/platform.h"
#include "spectrum/transport/link.h"
#include "spectrum/ui/gui.h"

typedef char time_dirty_abi_check[
    SPECTRUM_TIME_DIRTY_CLOCK == SPECTRUM_GUI_TIME_CLOCK &&
    SPECTRUM_TIME_DIRTY_GAME == SPECTRUM_GUI_TIME_GAME ? 1 : -1];

#ifdef NETCHESSZX_FIXED_LOW_RAM
static __at(NETCHESSZX_LOWRAM_TIME_LAST_FRAME_ADDR) uint16_t last_frame_count;
#else
static uint16_t last_frame_count;
#endif

static uint8_t spectrum_app_tick_frame(void)
{
    uint8_t dirty;

    SPECTRUM_TIME_TICK(dirty);
    return dirty;
}

#ifdef NETCHESSZX_SDCC_IY
#ifndef NETCHESSZX_CLASSIC_BOOT
void spectrum_app_tick_init(void) __naked
{
#asm
    ld hl, (0x5c78)
    ld (_last_frame_count), hl
    ret
#endasm
}
#endif

void spectrum_app_tick(void) __naked
{
#asm
    ld hl, (0x5c78)
    ld de, (_last_frame_count)
    ld (_last_frame_count), hl
    or a
    sbc hl, de
    ld b, h
    ld c, l
    ld a, b
    or c
    ret z
    ld e, 0
app_tick_frame_loop:
    push bc
    push de
    call _spectrum_app_tick_frame
    pop de
    pop bc
    ld a, e
    or l
    ld e, a
    dec bc
    ld a, b
    or c
    jr nz, app_tick_frame_loop
    ld l, e
    jp _spectrum_gui_present_tick
#endasm
}
#else
void spectrum_app_tick_init(void)
{
    last_frame_count = spectrum_frame_count();
}

void spectrum_app_tick(void)
{
    uint16_t frame_count = spectrum_frame_count();
    uint16_t elapsed;
    uint8_t dirty = 0u;
    uint8_t step;

    elapsed = (uint16_t)(frame_count - last_frame_count);
    last_frame_count = frame_count;
    /* The active loop may sample several times inside one video frame. */
    if (elapsed == 0u) {
        return;
    }
    do {
        step = spectrum_app_tick_frame();
        dirty |= step;
    } while (--elapsed != 0u);
    spectrum_gui_present_tick(dirty);
}
#endif

void spectrum_app_shift_clock(int8_t hour_delta) NETCHESSZX_FASTCALL
{
    uint8_t changed;
    SPECTRUM_TIME_SHIFT_CLOCK(hour_delta, changed);
    if (changed) {
        spectrum_gui_time_changed(SPECTRUM_GUI_TIME_CLOCK);
    }
}

void spectrum_app_game_timer_start(void)
{
    spectrum_time_game_timer_start();
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_CLOCK |
        SPECTRUM_GUI_TIME_GAME | SPECTRUM_GUI_TIME_FORCE);
}

void spectrum_app_game_timer_stop(void)
{
    spectrum_time_game_timer_stop();
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_CLOCK | SPECTRUM_GUI_TIME_GAME |
        SPECTRUM_GUI_TIME_FORCE | SPECTRUM_GUI_TIME_CLEAR_TURN);
}

void spectrum_app_move_timer_reset(void)
{
    spectrum_time_move_timer_reset();
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_GAME);
}

void spectrum_app_game_timer_restore(const uint8_t *timers) NETCHESSZX_FASTCALL
{
    spectrum_time_game_timer_restore(timers);
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_GAME | SPECTRUM_GUI_TIME_FORCE);
}

void spectrum_net_runtime_wait_frame(void)
{
    spectrum_frame_wait();
    spectrum_link_background_drain();
    spectrum_app_tick();
}

void spectrum_net_runtime_wait_frame_plain(void)
{
    spectrum_frame_wait();
    spectrum_app_tick();
}

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    if (hour >= 24u || minute >= 60u || second >= 60u) {
        return;
    }
    spectrum_time_set_clock(hour, minute, second);
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_CLOCK);
}

uint8_t spectrum_net_runtime_clock_ready(void)
{
    return spectrum_time_clock_ready();
}

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    (void)time;
    spectrum_time_set_fat_date(date);
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return spectrum_time_fat_date();
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return spectrum_time_fat_time();
}

int16_t spectrum_net_runtime_fat_day_delta(void)
{
    return spectrum_time_fat_day_delta();
}
