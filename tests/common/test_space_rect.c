#include <assert.h>
#include <stdio.h>
#include "common/space/rect.h"

static void check_rectangle(uint16_t columns, uint16_t rows)
{
    const space_rect_t rect = {columns, rows};
    const int8_t dr[] = {-1, 1, 0, 0};
    const int8_t dc[] = {0, 0, -1, 1};
    uint16_t row, col;
    uint8_t direction;

    assert(space_rect_count(&rect) == (uint32_t)columns * rows);
    for (row = 0; row < rows; ++row) {
        for (col = 0; col < columns; ++col) {
            space_cell_t cell = (space_cell_t)((uint32_t)row * columns + col);
            for (direction = 0; direction < 4u; ++direction) {
                int32_t next_row = (int32_t)row + dr[direction];
                int32_t next_col = (int32_t)col + dc[direction];
                space_cell_t next = space_rect_neighbor(&rect, cell, direction);
                if (next_row < 0 || next_row >= rows ||
                    next_col < 0 || next_col >= columns) {
                    assert(next == SPACE_CELL_NONE);
                } else {
                    assert(next == (space_cell_t)(next_row * columns + next_col));
                    assert(space_rect_neighbor(&rect, next, direction ^ 1u) == cell);
                }
            }
            assert(space_rect_neighbor(&rect, cell, 4u) == SPACE_CELL_NONE);
            assert(space_rect_neighbor(&rect, cell, UINT8_MAX) == SPACE_CELL_NONE);
        }
    }
    assert(space_rect_neighbor(&rect, space_rect_count(&rect), 0u) == SPACE_CELL_NONE);
    assert(space_rect_neighbor(&rect, SPACE_CELL_NONE, 0u) == SPACE_CELL_NONE);
    assert(rect.columns == columns && rect.rows == rows);
}

int main(void)
{
    const space_rect_t invalid[] = {{0, 8}, {8, 0}, {256, 256},
                                   {UINT16_MAX, UINT16_MAX}};
    unsigned i;

    check_rectangle(8, 8);
    check_rectangle(12, 10);
    check_rectangle(17, 17);
    check_rectangle(1, 1);
    check_rectangle(1, 19);
    check_rectangle(19, 1);
    check_rectangle(257, 255);
    check_rectangle(UINT16_MAX, 1);
    check_rectangle(1, UINT16_MAX);
    assert(space_rect_count(0) == 0u);
    assert(space_rect_neighbor(0, 0u, 0u) == SPACE_CELL_NONE);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(space_rect_count(&invalid[i]) == 0u);
        assert(space_rect_neighbor(&invalid[i], 0u, 0u) == SPACE_CELL_NONE);
    }
    puts("Rectangle identity, adjacency, edges and capacity contracts passed");
    return 0;
}
