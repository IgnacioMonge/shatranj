#ifndef NETCHESSZX_COMMON_CHESS_MOVE_COORDS_H
#define NETCHESSZX_COMMON_CHESS_MOVE_COORDS_H

#include <stdint.h>

uint8_t netchesszx_move_parse_coords(const char *move,
                                     uint8_t *from_row,
                                     uint8_t *from_col,
                                     uint8_t *to_row,
                                     uint8_t *to_col);

#endif
