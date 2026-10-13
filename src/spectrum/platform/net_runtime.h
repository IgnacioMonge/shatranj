#ifndef NETCHESSZX_SPECTRUM_NET_RUNTIME_H
#define NETCHESSZX_SPECTRUM_NET_RUNTIME_H

#include <stdint.h>
#include "spectrum/platform/platform.h"

/* Keep the existing resident overlay ABI name; ownership is application-side. */
void spectrum_gui_tick(void);
#define spectrum_app_tick spectrum_gui_tick
void spectrum_app_tick_init(void);
void spectrum_app_shift_clock(int8_t hour_delta) NETCHESSZX_FASTCALL;
void spectrum_app_game_timer_start(void);
void spectrum_app_game_timer_stop(void);
void spectrum_app_move_timer_reset(void);
void spectrum_app_game_timer_restore(const uint8_t *timers) NETCHESSZX_FASTCALL;

/* Narrow transport -> app bridge. It lives under platform/ so transport can call
   timing/UI composition hooks without including app/ or ui/ directly. */
void spectrum_net_runtime_wait_frame(void);
void spectrum_net_runtime_wait_frame_plain(void);
void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second);
uint8_t spectrum_net_runtime_clock_ready(void);
void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time);
uint16_t spectrum_net_runtime_fat_date(void);
uint16_t spectrum_net_runtime_fat_time(void);
int16_t spectrum_net_runtime_fat_day_delta(void);

#endif
