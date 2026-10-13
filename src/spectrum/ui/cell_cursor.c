#include "spectrum/ui/cell_cursor.h"
#include "spectrum/ui/space_navigation.h"

static space_cell_t cell_cursor;

void spectrum_cell_cursor_home(space_cell_t cell) NETCHESSZX_FASTCALL
{
    cell_cursor = cell;
}

space_cell_t spectrum_cell_cursor_current(void)
{
    return cell_cursor;
}

space_cell_t spectrum_cell_cursor_move(uint8_t key) NETCHESSZX_FASTCALL
{
    space_cell_t next = spectrum_space_neighbor(cell_cursor, key);
    if (next != SPACE_CELL_NONE) {
        cell_cursor = next;
    }
    return cell_cursor;
}
