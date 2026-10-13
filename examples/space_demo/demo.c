#include "demo.h"
#include "common/space/rect.h"
#include "spectrum/ui/space_navigation.h"

static uint8_t hexagonal;
static uint8_t revealed[120];

void demo_reset(uint8_t hex)
{
    uint8_t i;
    hexagonal = (uint8_t)(hex != 0u);
    for (i = 0u; i < sizeof(revealed); ++i) {
        revealed[i] = 0u;
    }
}

uint8_t demo_hexagonal(void) { return hexagonal; }
uint16_t demo_extent(void) { return hexagonal ? 49u : 120u; }

uint8_t demo_valid(space_cell_t cell)
{
    int16_t diagonal;
    if (cell >= demo_extent()) {
        return 0u;
    }
    if (!hexagonal) {
        return 1u;
    }
    diagonal = (int16_t)(cell / 7u + cell % 7u);
    return (uint8_t)(cell != 24u && diagonal >= 3 && diagonal <= 9);
}

space_cell_t demo_first(void) { return hexagonal ? 3u : 0u; }

uint8_t demo_view(space_cell_t cell, uint8_t player)
{
    if (!demo_valid(cell) || player > 1u ||
        ((cell & 1u) != player && !(revealed[cell] & (1u << player)))) {
        return 0u;
    }
    return (uint8_t)(1u + cell % 3u);
}

uint8_t demo_activate(space_cell_t cell, uint8_t player)
{
    const demo_action_t action = {cell};
    return demo_apply_action(&action, player);
}

uint8_t demo_apply_action(const demo_action_t *action, uint8_t player)
{
    if (action == 0 || !demo_valid(action->cell) || player > 1u) {
        return 0u;
    }
    revealed[action->cell] |= (uint8_t)(1u << player);
    return 1u;
}

space_cell_t spectrum_space_neighbor(space_cell_t cell, uint8_t key)
{
    const space_rect_t rectangle = {12, 10};
    int16_t row, col;
    uint8_t direction;
    space_cell_t next;
    if (!demo_valid(cell)) {
        return SPACE_CELL_NONE;
    }
    if (key >= 0x81u && key <= 0x84u) {
        direction = (uint8_t)(key - 0x81u);
    } else if (key == 'q') {
        direction = SPACE_RECT_NORTH;
    } else if (key == 'a') {
        direction = SPACE_RECT_SOUTH;
    } else if (key == 'o') {
        direction = SPACE_RECT_WEST;
    } else if (key == 'p') {
        direction = SPACE_RECT_EAST;
    } else if (hexagonal && (key == 'e' || key == 'z')) {
        direction = key == 'e' ? 4u : 5u;
    } else {
        return cell;
    }
    if (!hexagonal) {
        return space_rect_neighbor(&rectangle, cell, direction);
    }
    row = (int16_t)(cell / 7u);
    col = (int16_t)(cell % 7u);
    switch (direction) {
    case SPACE_RECT_NORTH: --row; break;
    case SPACE_RECT_SOUTH: ++row; break;
    case SPACE_RECT_WEST: --col; break;
    case SPACE_RECT_EAST: ++col; break;
    case 4u: --row; ++col; break;
    case 5u: ++row; --col; break;
    }
    if (row < 0 || row >= 7 || col < 0 || col >= 7) {
        return SPACE_CELL_NONE;
    }
    next = (space_cell_t)(row * 7 + col);
    return demo_valid(next) ? next : SPACE_CELL_NONE;
}

/* Presentation coordinates in a 256x192 canvas, shared by both demo views. */
uint8_t demo_x(space_cell_t cell)
{
    return (uint8_t)(8u + (cell % (hexagonal ? 7u : 12u)) * 16u +
                     (hexagonal ? (cell / 7u) * 8u : 0u));
}

uint8_t demo_y(space_cell_t cell)
{
    return (uint8_t)(16u + (cell / (hexagonal ? 7u : 12u)) *
                     (hexagonal ? 12u : 16u));
}
