#ifndef NETCHESSZX_SPACE_RECT_H
#define NETCHESSZX_SPACE_RECT_H

#include "common/space/space.h"

/* Immutable, caller-owned dimensions. IDs are row * columns + column.
   These cardinal connections describe this geometry, not cursor orientation
   or legal game actions. Other geometries need not use these directions. */
typedef struct {
    uint16_t columns;
    uint16_t rows;
} space_rect_t;

enum {
    SPACE_RECT_NORTH,
    SPACE_RECT_SOUTH,
    SPACE_RECT_WEST,
    SPACE_RECT_EAST
};

/* Zero for null, empty or unrepresentable rectangles. */
uint16_t space_rect_count(const space_rect_t *rect);
/* NONE for invalid input, unknown direction or an edge; never wraps. */
space_cell_t space_rect_neighbor(const space_rect_t *rect, space_cell_t cell,
                                 uint8_t direction);

#endif
