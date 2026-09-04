#ifndef NETCHESSZX_SPECTRUM_RENDER_STATUS_H
#define NETCHESSZX_SPECTRUM_RENDER_STATUS_H

#include <stdint.h>

#define SPECTRUM_STATUS_PHASE_MASK 0x07u
#define SPECTRUM_STATUS_PLATFORM_SHIFT 3u
#define SPECTRUM_STATUS_PLATFORM_MASK 0x38u
#define SPECTRUM_STATUS_PLATFORM_BITS(platform) \
    ((uint8_t)(((platform) & 0x07u) << SPECTRUM_STATUS_PLATFORM_SHIFT))
#define SPECTRUM_STATUS_WITH_PHASE(status, phase) \
    ((uint8_t)(((status) & SPECTRUM_STATUS_PLATFORM_MASK) | \
               ((phase) & SPECTRUM_STATUS_PHASE_MASK)))
#define SPECTRUM_STATUS_PACK(phase, platform) \
    ((uint8_t)(((phase) & SPECTRUM_STATUS_PHASE_MASK) | \
               SPECTRUM_STATUS_PLATFORM_BITS(platform)))
#define SPECTRUM_STATUS_UNPACK_PHASE(value) \
    ((uint8_t)((value) & SPECTRUM_STATUS_PHASE_MASK))
#define SPECTRUM_STATUS_UNPACK_PLATFORM(value) \
    ((uint8_t)(((value) >> SPECTRUM_STATUS_PLATFORM_SHIFT) & 0x07u))

void spectrum_render_status_error(const char *text) __z88dk_fastcall;

#endif
