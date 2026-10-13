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

/* Optional player name after link ready ("MNAME <name>"). Informational. */
#define NETCHESS_PROTO_NAME_PREFIX "MNAME "
#define NETCHESS_PROTO_NAME_PREFIX_LEN 6u
#define NETCHESS_PLAYER_NAME_MAX 10u
#define NETCHESS_PLAYER_NAME_SIZE (NETCHESS_PLAYER_NAME_MAX + 1u)
#define NETCHESS_PROTO_NAME_LINE_SIZE \
    (NETCHESS_PROTO_NAME_PREFIX_LEN + NETCHESS_PLAYER_NAME_SIZE)
#define NETCHESS_PLAYER_NAME_CHAR_OK(c) \
    (((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') || \
     (c) == '-' || (c) == '_')

#ifdef __cplusplus
extern "C" {
#endif

/* "MACH ZX|NXT|MAC|LNX|PC|SPCX" -> platform id; 0 on failure. */
uint8_t netchess_proto_parse_mach(const char *rx, uint8_t *plat);
/* Format MACH line for a known platform id. */
uint8_t netchess_proto_format_mach(char *out, uint8_t out_cap, uint8_t plat);
/* Fixed wire token for plat, or 0 if unknown. */
const char *netchess_proto_mach_code(uint8_t plat) NETCHESSZX_FASTCALL;
/* 1 when name is 1..NETCHESS_PLAYER_NAME_MAX valid characters. */
uint8_t netchess_player_name_valid(const char *name) NETCHESSZX_FASTCALL;
/* "MNAME <valid name>" -> the name inside rx; 0 on failure. */
const char *netchess_proto_name_tail(const char *rx) NETCHESSZX_FASTCALL;
/* Format "MNAME <name>" for a valid name. */
uint8_t netchess_proto_format_name(char *out, uint8_t out_cap,
                                   const char *name);

#ifdef __cplusplus
}
#endif

#endif
