#include "common/savegame/journal.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint8_t failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

int main(void)
{
    uint8_t bytes[8];
    uint16_t out[4];
    uint16_t count;
    uint16_t e2e4;
    uint16_t rec;

    e2e4 = NETCHESSZX_JOURNAL_PACK_MOVE(52u, 36u, 0u);
    bytes[0] = (uint8_t)e2e4;
    bytes[1] = (uint8_t)(e2e4 >> 8);
    bytes[2] = (uint8_t)NETCHESSZX_JOURNAL_UNDO_ONE;
    bytes[3] = (uint8_t)(NETCHESSZX_JOURNAL_UNDO_ONE >> 8);
    CHECK(netchesszx_journal_decode(bytes, 4u, out, 4u, &count) ==
          NETCHESSZX_JOURNAL_OK);
    CHECK(count == 2u);
    CHECK(out[0] == e2e4);
    CHECK(NETCHESSZX_JOURNAL_FROM(out[0]) == 52u);
    CHECK(NETCHESSZX_JOURNAL_TO(out[0]) == 36u);
    CHECK(out[1] == NETCHESSZX_JOURNAL_UNDO_ONE);

    CHECK(netchesszx_journal_decode(bytes, 3u, out, 4u, &count) ==
          NETCHESSZX_JOURNAL_ERR_SIZE);
    rec = 0x8001u;
    bytes[0] = (uint8_t)rec;
    bytes[1] = (uint8_t)(rec >> 8);
    CHECK(netchesszx_journal_decode(bytes, 2u, out, 4u, &count) ==
          NETCHESSZX_JOURNAL_ERR_RECORD);
    CHECK(netchesszx_journal_decode(bytes, 2u, out, 0u, &count) ==
          NETCHESSZX_JOURNAL_ERR_ROOM);

    CHECK(netchesszx_crc16_ccitt(NETCHESSZX_STJ_CRC_INIT, 0, 0u) ==
          NETCHESSZX_STJ_CRC_INIT);

    if (failures != 0u) {
        return 1;
    }
    puts("journal decode ok");
    return 0;
}
