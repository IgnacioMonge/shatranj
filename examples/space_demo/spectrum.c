#include <arch/zx.h>
#include <input.h>
#include "demo.h"
#include "spectrum/ui/cell_cursor.h"

static uint8_t player;

static void label(uint8_t row, const char *text)
{
    uint8_t col = 0, scan;
    while (*text && col < 32u) {
        const uint8_t *glyph = (const uint8_t *)(0x3c00u + (uint16_t)(uint8_t)*text++ * 8u);
        for (scan = 0; scan < 8u; ++scan) {
            *zx_pxy2saddr((uint8_t)(col * 8u), (uint8_t)(row * 8u + scan)) = glyph[scan];
        }
        ++col;
    }
}

static void redraw(void)
{
    /* ponytail: full redraw of at most 120 cells; repaint changed cells if
       this demonstrator becomes a game with frequent updates. */
    static const uint16_t hex_edge[16] = {
        0x0180, 0x0660, 0x1818, 0x6006, 0x8001, 0x8001, 0x8001, 0x8001,
        0x8001, 0x8001, 0x8001, 0x8001, 0x6006, 0x1818, 0x0660, 0x0180
    };
    space_cell_t cell;
    uint8_t row;
    uint8_t hex = demo_hexagonal();
    space_cell_t cursor = spectrum_cell_cursor_current();
    zx_cls(56u);
    label(0, player ? "SPACE DEMO  -  VIEW B" : "SPACE DEMO  -  VIEW A");
    for (cell = 0; cell < demo_extent(); ++cell) {
        const uint8_t *glyph;
        uint8_t value;
        uint8_t x, y;
        if (!demo_valid(cell)) {
            continue;
        }
        value = demo_view(cell, player);
        x = demo_x(cell);
        y = demo_y(cell);
        glyph = (const uint8_t *)(0x3c00u + (uint16_t)(value ? '0' + value : '?') * 8u);
        for (row = 0; row < 16u; ++row) {
            uint16_t bits = hex ? hex_edge[row] :
                ((row == 0u || row == 15u) ? 0xffffu : 0x8001u);
            uint8_t *dest = zx_pxy2saddr(x, (uint8_t)(y + row));
            if (row >= 4u && row < 12u) {
                uint8_t ink = glyph[row - 4u];
                if (cell == cursor) {
                    ink ^= 0xffu;
                }
                bits |= (uint16_t)ink << 4;
            }
            dest[0] |= (uint8_t)(bits >> 8);
            dest[1] |= (uint8_t)bits;
        }
    }
    label(22, "QAOP move E/Z diagonal V view");
    label(23, "1/2 map  SPACE reveal");
}

int main(void)
{
    uint8_t key;
    demo_reset(0);
    spectrum_cell_cursor_home(demo_first());
    redraw();
    for (;;) {
        in_wait_nokey();
        do {
            key = (uint8_t)in_inkey();
        } while (!key);
        if (key == '1' || key == '2') {
            demo_reset((uint8_t)(key == '2'));
            spectrum_cell_cursor_home(demo_first());
        } else if (key == 'v') {
            player ^= 1u;
        } else if (key == ' ') {
            demo_activate(spectrum_cell_cursor_current(), player);
        } else {
            spectrum_cell_cursor_move(key);
        }
        redraw();
    }
}
