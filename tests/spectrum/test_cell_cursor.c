#include <assert.h>
#include "spectrum/ui/cell_cursor.h"
#include "spectrum/ui/space_navigation.h"

static space_cell_t requested;
space_cell_t spectrum_space_neighbor(space_cell_t cell, uint8_t key)
{
    assert(cell == spectrum_cell_cursor_current());
    (void)key;
    return requested;
}

int main(void)
{
    spectrum_cell_cursor_home(65534u);
    assert(spectrum_cell_cursor_current() == 65534u);
    requested = 511u;
    assert(spectrum_cell_cursor_move(0u) == 511u);
    requested = SPACE_CELL_NONE;
    assert(spectrum_cell_cursor_move(0u) == 511u);
    requested = 255u;
    assert(spectrum_cell_cursor_move(0u) == 255u);
    requested = 0u;
    assert(spectrum_cell_cursor_move(0u) == 0u);
    return 0;
}
