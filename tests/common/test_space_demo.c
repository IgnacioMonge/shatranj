#include <assert.h>
#include <stdio.h>
#include "demo.h"
#include "spectrum/ui/cell_cursor.h"
#include "spectrum/ui/space_navigation.h"

static void test_actions(uint8_t mode)
{
    uint8_t direct[120][2];
    space_cell_t cell;
    uint8_t player;
    demo_action_t action;

    demo_reset(mode);
    action.cell = demo_first();
    player = (uint8_t)((action.cell & 1u) ^ 1u);
    assert(demo_apply_action(&action, player));
    for (cell = 0; cell < 120; ++cell) {
        direct[cell][0] = demo_view(cell, 0);
        direct[cell][1] = demo_view(cell, 1);
    }
    demo_reset(mode);
    /* Both actual presentations enter through this existing adapter. */
    assert(demo_activate(action.cell, player));
    action.cell = SPACE_CELL_NONE;
    assert(!demo_apply_action(&action, player));
    action.cell = mode ? 24u : 120u;
    assert(!demo_apply_action(&action, player));
    action.cell = demo_first();
    assert(!demo_apply_action(&action, 2));
    assert(!demo_apply_action(0, player));
    for (cell = 0; cell < 120; ++cell) {
        assert(demo_view(cell, 0) == direct[cell][0]);
        assert(demo_view(cell, 1) == direct[cell][1]);
    }
}

int main(void)
{
    const uint8_t keys[] = {'q', 'a', 'o', 'p', 'e', 'z'};
    uint8_t seen[120];
    uint8_t mode, key, count;
    space_cell_t cell, next;
    unsigned pass;

    for (mode = 0u; mode < 2u; ++mode) {
        test_actions(mode);
        demo_reset(mode);
        for (cell = 0; cell < 120; ++cell) {
            seen[cell] = 0;
        }
        seen[demo_first()] = 1;
        /* Explore all paths through the productive cursor; the center hole
           cannot be reached, while every real cell must be reachable. */
        for (pass = 0; pass < demo_extent(); ++pass) {
            for (cell = 0; cell < demo_extent(); ++cell) {
                if (!seen[cell]) {
                    continue;
                }
                for (key = 0; key < sizeof(keys); ++key) {
                    spectrum_cell_cursor_home(cell);
                    next = spectrum_cell_cursor_move(keys[key]);
                    assert(demo_valid(next));
                    assert(spectrum_cell_cursor_current() == next);
                    seen[next] = 1;
                    if (next != cell) {
                        assert(spectrum_cell_cursor_move(keys[key ^ 1u]) == cell);
                    }
                }
            }
        }
        count = 0;
        for (cell = 0; cell < demo_extent(); ++cell) {
            assert(seen[cell] == demo_valid(cell));
            if (!demo_valid(cell)) {
                assert(!demo_activate(cell, 0));
                assert(!demo_view(cell, 0));
                continue;
            }
            ++count;
            assert(demo_x(cell) + 16u <= 256u && demo_y(cell) + 16u <= 192u);
            assert(demo_view(cell, (uint8_t)(cell & 1u)) != 0u);
            assert(demo_view(cell, (uint8_t)((cell & 1u) ^ 1u)) == 0u);
            assert(demo_activate(cell, (uint8_t)((cell & 1u) ^ 1u)));
            assert(demo_view(cell, 0) == demo_view(cell, 1));
            assert(!demo_activate(cell, 2));
            assert(!demo_view(cell, 2));
        }
        assert(count == (mode ? 36u : 120u));
        assert(!demo_activate(SPACE_CELL_NONE, 0));
        spectrum_cell_cursor_home(demo_first());
        assert(spectrum_cell_cursor_move(0xffu) == demo_first());
        demo_reset(mode);
        cell = (space_cell_t)(demo_first() + 1u);
        assert(demo_view(cell, (uint8_t)((cell & 1u) ^ 1u)) == 0u);
    }
    puts("Product cursor: rectangle/hexagon paths, hole, single activation and private views passed");
    return 0;
}
