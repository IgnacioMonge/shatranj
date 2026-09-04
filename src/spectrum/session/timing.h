#ifndef NETCHESSZX_SPECTRUM_SESSION_TIMING_H
#define NETCHESSZX_SPECTRUM_SESSION_TIMING_H

#include <stdint.h>

/* The application counters advance once per empty transport read.  DIRECT
   on SpectraNext yields one frame; every other session read yields two. */
#ifdef NETCHESSZX_SPECTRANEXT
#define NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES 1u
#else
#define NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES 2u
#endif
#define NETCHESSZX_SESSION_MQTT_EMPTY_POLL_FRAMES 2u

#ifndef NETCHESSZX_SESSION_FRAME_HZ
#define NETCHESSZX_SESSION_FRAME_HZ 50u
#endif

/* ceil(seconds * hz / empty-poll frames), with a rational duration. */
#define NETCHESSZX_SESSION_POLLS_CEIL(numerator, denominator, hz, frames) \
    ((uint16_t)((((uint16_t)(numerator) * (uint16_t)(hz)) + \
                 ((uint16_t)(denominator) * (uint16_t)(frames)) - 1u) / \
                ((uint16_t)(denominator) * (uint16_t)(frames))))

#define NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(3u, 1u, (hz), \
                                  NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES)
#define NETCHESSZX_SESSION_DIRECT_REPLY_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(5u, 2u, (hz), \
                                  NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES)
#define NETCHESSZX_SESSION_MQTT_REPLY_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(5u, 2u, (hz), \
                                  NETCHESSZX_SESSION_MQTT_EMPTY_POLL_FRAMES)
#define NETCHESSZX_SESSION_DIRECT_GRACE_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(300u, 1u, (hz), \
                                  NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES)
#define NETCHESSZX_SESSION_MQTT_4_8S_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(24u, 5u, (hz), \
                                  NETCHESSZX_SESSION_MQTT_EMPTY_POLL_FRAMES)
#define NETCHESSZX_SESSION_MQTT_GRACE_POLLS_AT(hz) \
    NETCHESSZX_SESSION_POLLS_CEIL(300u, 1u, (hz), \
                                  NETCHESSZX_SESSION_MQTT_EMPTY_POLL_FRAMES)

/* Next's UART initializer and the Spectranext preflight store the 3-second
   DIRECT poll count so all protocol deadlines follow the live 50/60 Hz mode. */
#if defined(NETCHESSZX_NEXT) || defined(NETCHESSZX_SPECTRANEXT)
#if defined(NETCHESSZX_SPECTRANEXT) && defined(NETCHESSZX_FIXED_LOW_RAM)
#include "spectrum/lowram_map.h"
extern __at(NETCHESSZX_LOWRAM_SPXN_DIRECT_3S_POLLS_ADDR)
uint8_t net_uart_direct_idle_ticks;
#else
extern uint8_t net_uart_direct_idle_ticks;
#endif
#define NETCHESSZX_SESSION_DIRECT_3S_POLLS net_uart_direct_idle_ticks
#else
#define NETCHESSZX_SESSION_DIRECT_3S_POLLS \
    NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(NETCHESSZX_SESSION_FRAME_HZ)
#endif

#define NETCHESSZX_SESSION_DIRECT_3S_POLLS_FROM_NEXTREG(peripheral_1) \
    ((uint8_t)(((peripheral_1) & 0x04u) != 0u \
        ? NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(60u) \
        : NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(50u)))

#define NETCHESSZX_SESSION_IS_60HZ \
    (NETCHESSZX_SESSION_DIRECT_3S_POLLS == \
     NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(60u))
#define NETCHESSZX_SESSION_DIRECT_REPLY_POLLS \
    (NETCHESSZX_SESSION_IS_60HZ \
         ? NETCHESSZX_SESSION_DIRECT_REPLY_POLLS_AT(60u) \
         : NETCHESSZX_SESSION_DIRECT_REPLY_POLLS_AT(50u))
#define NETCHESSZX_SESSION_MQTT_REPLY_POLLS \
    (NETCHESSZX_SESSION_IS_60HZ \
         ? NETCHESSZX_SESSION_MQTT_REPLY_POLLS_AT(60u) \
         : NETCHESSZX_SESSION_MQTT_REPLY_POLLS_AT(50u))
#define NETCHESSZX_SESSION_DIRECT_GRACE_POLLS \
    (NETCHESSZX_SESSION_IS_60HZ \
         ? NETCHESSZX_SESSION_DIRECT_GRACE_POLLS_AT(60u) \
         : NETCHESSZX_SESSION_DIRECT_GRACE_POLLS_AT(50u))
#define NETCHESSZX_SESSION_MQTT_4_8S_POLLS \
    (NETCHESSZX_SESSION_IS_60HZ \
         ? NETCHESSZX_SESSION_MQTT_4_8S_POLLS_AT(60u) \
         : NETCHESSZX_SESSION_MQTT_4_8S_POLLS_AT(50u))
#define NETCHESSZX_SESSION_MQTT_GRACE_POLLS \
    (NETCHESSZX_SESSION_IS_60HZ \
         ? NETCHESSZX_SESSION_MQTT_GRACE_POLLS_AT(60u) \
         : NETCHESSZX_SESSION_MQTT_GRACE_POLLS_AT(50u))

#endif
