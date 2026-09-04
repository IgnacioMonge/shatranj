#ifndef NETCHESSZX_COMMON_PLATFORM_PROTOCOL_H
#define NETCHESSZX_COMMON_PLATFORM_PROTOCOL_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define NETCHESS_PROTO_MACH_PREFIX "MACH "

/* Optional peer machine identity after link ready. Informational only. */
#define NETCHESS_PLAT_UNKNOWN 0u
#define NETCHESS_PLAT_ZX 1u
#define NETCHESS_PLAT_NXT 2u
#define NETCHESS_PLAT_MAC 3u
#define NETCHESS_PLAT_LNX 4u
#define NETCHESS_PLAT_PC 5u
#define NETCHESS_PLAT_SPCX 6u

#ifdef __cplusplus
extern "C" {
#endif

/* "MACH ZX|NXT|MAC|LNX|PC|SPCX" -> platform id; 0 on failure. */
uint8_t netchess_proto_parse_mach(const char *rx, uint8_t *plat);
/* Format MACH line for a known platform id. */
uint8_t netchess_proto_format_mach(char *out, uint8_t out_cap, uint8_t plat);
/* Fixed wire token for plat, or 0 if unknown. */
const char *netchess_proto_mach_code(uint8_t plat) NETCHESSZX_FASTCALL;

#ifdef __cplusplus
}
#endif

#endif
