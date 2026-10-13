#include "common/savegame/journal.h"

uint16_t netchesszx_crc16_ccitt(uint16_t crc, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t bit;

    for (i = 0u; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0u; bit < 8u; ++bit) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

int netchesszx_journal_decode(const uint8_t *data, uint16_t len,
                              uint16_t *out, uint16_t max, uint16_t *count)
{
    uint16_t n;
    uint16_t i;
    uint16_t rec;

    if (count == 0) {
        return NETCHESSZX_JOURNAL_ERR_SIZE;
    }
    *count = 0u;
    if (data == 0 || (len & 1u) != 0u) {
        return NETCHESSZX_JOURNAL_ERR_SIZE;
    }
    n = (uint16_t)(len >> 1);
    if (out == 0 || n > max) {
        return NETCHESSZX_JOURNAL_ERR_ROOM;
    }
    for (i = 0u; i < n; ++i) {
        rec = (uint16_t)data[i * 2u] | ((uint16_t)data[i * 2u + 1u] << 8);
        if (NETCHESSZX_JOURNAL_IS_CONTROL(rec)) {
            if (rec != NETCHESSZX_JOURNAL_UNDO_ONE) {
                return NETCHESSZX_JOURNAL_ERR_RECORD;
            }
        } else if (NETCHESSZX_JOURNAL_FROM(rec) > 63u ||
                   NETCHESSZX_JOURNAL_TO(rec) > 63u ||
                   NETCHESSZX_JOURNAL_PROMO(rec) > NETCHESSZX_JOURNAL_PROMO_MAX) {
            return NETCHESSZX_JOURNAL_ERR_RECORD;
        }
        out[i] = rec;
    }
    *count = n;
    return NETCHESSZX_JOURNAL_OK;
}
