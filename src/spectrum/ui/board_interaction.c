#include "spectrum/ui/interaction.h"
#include "spectrum/app/ux_ports.h"
#include "spectrum/ui/gui.h"
#include "spectrum/ui/cell_cursor.h"

/* Capture the ID once: z88dk -SO3 miscompiles repeated global byte casts.
   This also keeps the Shatranj coordinate conversion in one small adapter. */
static void redraw_cell(space_cell_t cell) NETCHESSZX_FASTCALL
{
    spectrum_gui_redraw_square((uint8_t)cell, (uint8_t)(cell >> 8));
}
/* Arguments below are side-effect-free IDs. */
#define MARK(cell, selected) spectrum_gui_mark_cursor((uint8_t)(cell), (uint8_t)((cell) >> 8), (selected))
#define SELECTABLE(cell) spectrum_ux_selectable((uint8_t)(cell), (uint8_t)((cell) >> 8))

static space_cell_t selected_position;

void cursor_home(uint16_t square) NETCHESSZX_FASTCALL
{
    spectrum_cell_cursor_home(square);
    selected_position = SPACE_CELL_NONE;
}

void cursor_refresh(void)
{
    space_cell_t cell = spectrum_cell_cursor_current();
    redraw_cell(cell);
}

void cursor_request_hints(void)
{
    if (selected_position != SPACE_CELL_NONE) {
        movement_hints_show(selected_position);
    }
}

static void cursor_redraw_square(space_cell_t cell) NETCHESSZX_FASTCALL
{
    redraw_cell(cell);
    if (spectrum_ux_local_turn() && selected_position == cell) {
        MARK(cell, 1u);
    }
}

void cursor_hide(void)
{
    space_cell_t cell = spectrum_cell_cursor_current();
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    if (selected_position != SPACE_CELL_NONE) {
        redraw_cell(selected_position);
        if (selected_position != cell) {
            redraw_cell(cell);
        }
    } else {
        redraw_cell(cell);
    }
    selected_position = SPACE_CELL_NONE;
}

void cursor_show(void)
{
    space_cell_t cell;
    if (!spectrum_ux_can_navigate()) {
        return;
    }
    cell = spectrum_cell_cursor_current();
    if (selected_position == cell) {
        MARK(cell, 1u);
        return;
    }
    if (selected_position != SPACE_CELL_NONE) {
        MARK(selected_position, 1u);
    }
    MARK(cell, 0u);
}

void cursor_move(uint8_t key) NETCHESSZX_FASTCALL
{
    space_cell_t old;
    space_cell_t next;
    if (!spectrum_ux_can_navigate()) {
        return;
    }
    old = spectrum_cell_cursor_current();
    next = spectrum_cell_cursor_move(key);
    if (old != next) {
        cursor_redraw_square(old);
        cursor_request_hints();
        if (spectrum_ux_move_pending() == 0u) {
            MARK(next, (uint8_t)(selected_position == next));
        }
    }
}

uint8_t spectrum_ux_board_activate(void)
{
    space_cell_t cell = spectrum_cell_cursor_current();
    uint8_t move_rc;
    if (selected_position == SPACE_CELL_NONE) {
        if (!SELECTABLE(cell)) {
            spectrum_ux_selection_notice(cell);
            return 1u;
        }
    } else {
        if (selected_position == cell) {
            movement_hints_clear();
            selected_position = SPACE_CELL_NONE;
            redraw_cell(cell);
            cursor_show();
            return 1u;
        }
        if (!SELECTABLE(cell)) {
            goto send_move;
        }
        movement_hints_clear();
        redraw_cell(selected_position);
    }
    selected_position = cell;
    cursor_request_hints();
    cursor_show();
    spectrum_ux_selection_notice(cell);
    return 1u;

send_move:
    move_rc = spectrum_ux_move_intent(selected_position, cell);
    if (move_rc == UX_MOVE_FAILED) {
        return 0u;
    }
    if (move_rc == UX_MOVE_REJECTED) {
        cursor_redraw_square(selected_position);
        cursor_request_hints();
        cursor_show();
        return 1u;
    }
    redraw_cell(selected_position);
    selected_position = SPACE_CELL_NONE;
    cursor_show();
    return 1u;
}

#ifdef NETCHESSZX_HOST_TEST
uint16_t spectrum_ux_cursor_position(void)
{
    space_cell_t cell = spectrum_cell_cursor_current();
    return (uint16_t)((uint8_t)cell * 8u + (cell >> 8));
}

uint16_t spectrum_ux_selected_position(void)
{
    return selected_position == SPACE_CELL_NONE ? 0xffffu :
        (uint16_t)((uint8_t)selected_position * 8u + (selected_position >> 8));
}
#endif
