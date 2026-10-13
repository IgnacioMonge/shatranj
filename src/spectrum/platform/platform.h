#ifndef NETCHESSZX_SPECTRUM_PLATFORM_H
#define NETCHESSZX_SPECTRUM_PLATFORM_H

#include <stdint.h>
#include "spectrum/platform/uart.h"

void spectrum_frame_wait(void);
void spectrum_frame_clock_init(void);
#ifdef NETCHESSZX_HOST_TEST
uint16_t spectrum_frame_count(void);
#else
/* Low 16 bits of the hardware-frame counter: one atomic Z80 load and a
   21-minute wrap. Classic's resident IM2 ISR maintains the ROM FRAMES slot
   even while DivMMC owns the ROM window. */
#define spectrum_frame_count() (*(const volatile uint16_t *)23672u)
#endif
uint8_t spectrum_assets_load(void);
void spectrum_assets_fatal(void);

#endif
