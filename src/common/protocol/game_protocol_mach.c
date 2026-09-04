#include "common/protocol/game_protocol.h"
#include "common/protocol/game_protocol_internal.h"

uint8_t netchess_proto_parse_mach(const char *rx, uint8_t *plat)
{
    const char *p = netchess_after_prefix(rx, NETCHESS_PROTO_MACH_PREFIX);
    uint8_t value = NETCHESS_PLAT_UNKNOWN;

    if (p == 0 || plat == 0) {
        return 0u;
    }
    if (p[0] == 'Z' && p[1] == 'X' && p[2] == '\0') {
        value = NETCHESS_PLAT_ZX;
    } else if (p[0] == 'N' && p[1] == 'X' && p[2] == 'T' && p[3] == '\0') {
        value = NETCHESS_PLAT_NXT;
    } else if (p[0] == 'M' && p[1] == 'A' && p[2] == 'C' && p[3] == '\0') {
        value = NETCHESS_PLAT_MAC;
    } else if (p[0] == 'L' && p[1] == 'N' && p[2] == 'X' && p[3] == '\0') {
        value = NETCHESS_PLAT_LNX;
    } else if (p[0] == 'P' && p[1] == 'C' && p[2] == '\0') {
        value = NETCHESS_PLAT_PC;
    } else if (p[0] == 'S' && p[1] == 'P' && p[2] == 'C' && p[3] == 'X' &&
               p[4] == '\0') {
        value = NETCHESS_PLAT_SPCX;
    } else {
        return 0u;
    }
    *plat = value;
    return 1u;
}
