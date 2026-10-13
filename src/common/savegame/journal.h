#ifndef NETCHESSZX_SAVEGAME_JOURNAL_H
#define NETCHESSZX_SAVEGAME_JOURNAL_H

#include <stdint.h>

/* 2-byte little-endian records, headerless tmp. Spec packing, not parse_coords. */
#define NETCHESSZX_JOURNAL_UNDO_ONE 0x8000u
#define NETCHESSZX_JOURNAL_PROMO_NONE 0u
#define NETCHESSZX_JOURNAL_PROMO_MAX 4u

#define NETCHESSZX_JOURNAL_PACK_MOVE(from, to, promo) \
    ((uint16_t)(((from) & 63u) | \
                (((uint16_t)(to) & 63u) << 6) | \
                (((uint16_t)(promo) & 7u) << 12)))

#define NETCHESSZX_JOURNAL_FROM(rec) ((uint8_t)((rec) & 63u))
#define NETCHESSZX_JOURNAL_TO(rec) ((uint8_t)(((rec) >> 6) & 63u))
#define NETCHESSZX_JOURNAL_PROMO(rec) ((uint8_t)(((rec) >> 12) & 7u))
#define NETCHESSZX_JOURNAL_IS_CONTROL(rec) (((rec) & 0x8000u) != 0u)

#define NETCHESSZX_JOURNAL_OK 0
#define NETCHESSZX_JOURNAL_ERR_SIZE -1
#define NETCHESSZX_JOURNAL_ERR_RECORD -2
#define NETCHESSZX_JOURNAL_ERR_ROOM -3

#define NETCHESSZX_STJ_HEADER_SIZE 76u
#define NETCHESSZX_STJ_FLAG_COMPLETE 0x01u
#define NETCHESSZX_STJ_CRC_INIT 0xffffu

uint16_t netchesszx_crc16_ccitt(uint16_t crc, const uint8_t *data, uint16_t len);
int netchesszx_journal_decode(const uint8_t *data, uint16_t len,
                              uint16_t *out, uint16_t max, uint16_t *count);

#endif
