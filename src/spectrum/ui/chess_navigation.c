#include "spectrum/ui/space_navigation.h"
#include "spectrum/ui/gui.h"

space_cell_t spectrum_space_neighbor(space_cell_t position, uint8_t key)
{
    uint8_t row = (uint8_t)position;
    uint8_t col = (uint8_t)(position >> 8);
    uint8_t direction;
    uint8_t coord;

    if (row > 7u || col > 7u) {
        return SPACE_CELL_NONE;
    }
    if (key >= 0x81u && key <= 0x84u) {
        direction = (uint8_t)(key - 0x81u);
    } else if (key == 'q') {
        direction = 0u;
    } else if (key == 'a') {
        direction = 1u;
    } else if (key == 'o') {
        direction = 2u;
    } else if (key == 'p') {
        direction = 3u;
    } else {
        return position;
    }
    direction ^= (uint8_t)(spectrum_gui_is_board_flipped() != 0u);
    coord = (direction & 2u) ? col : row;
    if (direction & 1u) {
        if (coord < 7u) {
            ++coord;
        }
    } else if (coord > 0u) {
        --coord;
    }
    return (direction & 2u) ? (space_cell_t)(row | ((uint16_t)coord << 8)) :
                             (space_cell_t)(coord | ((uint16_t)col << 8));
}
