#include "common/space/rect.h"

uint16_t space_rect_count(const space_rect_t *rect)
{
    uint32_t count;

    if (rect == 0) {
        return 0u;
    }
    count = (uint32_t)rect->columns * rect->rows;
    return count <= UINT16_MAX ? (uint16_t)count : 0u;
}

space_cell_t space_rect_neighbor(const space_rect_t *rect, space_cell_t cell,
                                 uint8_t direction)
{
    uint16_t count = space_rect_count(rect);

    if (cell >= count) {
        return SPACE_CELL_NONE;
    }
    switch (direction) {
    case SPACE_RECT_NORTH:
        if (cell >= rect->columns) {
            return (space_cell_t)(cell - rect->columns);
        }
        break;
    case SPACE_RECT_SOUTH:
        if (cell < count - rect->columns) {
            return (space_cell_t)(cell + rect->columns);
        }
        break;
    case SPACE_RECT_WEST:
        if (cell % rect->columns != 0u) {
            return (space_cell_t)(cell - 1u);
        }
        break;
    case SPACE_RECT_EAST:
        if (cell % rect->columns != rect->columns - 1u) {
            return (space_cell_t)(cell + 1u);
        }
        break;
    }
    return SPACE_CELL_NONE;
}
