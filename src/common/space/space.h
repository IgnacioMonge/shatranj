#ifndef NETCHESSZX_SPACE_H
#define NETCHESSZX_SPACE_H

#include <stdint.h>

/* Logical identity only: no row/column, screen position or game state.
   This representation supports 65535 cells; 0xffff is never a valid cell.
   It is internal, not a persistence or network encoding. */
typedef uint16_t space_cell_t;
#define SPACE_CELL_NONE UINT16_MAX

#endif
