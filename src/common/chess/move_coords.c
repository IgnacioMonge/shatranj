#include "common/chess/move_coords.h"

uint8_t netchesszx_move_parse_coords(const char *move,
                                     uint8_t *from_row,
                                     uint8_t *from_col,
                                     uint8_t *to_row,
                                     uint8_t *to_col)
{
    uint8_t fc = (uint8_t)(move[0] - 'a');
    uint8_t fr = (uint8_t)('8' - move[1]);
    uint8_t tc = (uint8_t)(move[2] - 'a');
    uint8_t tr = (uint8_t)('8' - move[3]);

    if (fc >= 8u || fr >= 8u || tc >= 8u || tr >= 8u) {
        return 0u;
    }

    *from_col = fc;
    *from_row = fr;
    *to_col = tc;
    *to_row = tr;
    return (uint8_t)(fr != tr || fc != tc);
}
