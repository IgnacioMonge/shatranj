#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NO_SQUARE 0xffu
#define KEY_UP 0x81u
#define KEY_DOWN 0x82u
#define KEY_LEFT 0x83u
#define KEY_RIGHT 0x84u
#include "spectrum/ui/interaction.h"
#include "spectrum/app/ux_ports.h"
#include "common/space/rect.h"
#include "spectrum/ui/space_navigation.h"

static char board[64];
static uint8_t ready = 1, turn = 1, pending, flipped;
static uint8_t result = UX_MOVE_REJECTED, hints, sends;
static uint16_t intention_from, intention_to, notice_square;
uint8_t spectrum_ux_can_navigate(void) { return ready && turn && !pending; }
uint8_t spectrum_ux_local_turn(void) { return turn; }
uint16_t spectrum_ux_move_pending(void) { return pending; }
/* Deliberately not chess characters: interaction sees only selectability. */
uint8_t spectrum_ux_selectable(uint8_t r, uint8_t c) { return board[r * 8 + c] == 37; }
void movement_hints_clear(void) { hints = 0; }
void movement_hints_show(uint16_t square) { assert(square == 0x0406u); hints = 1; }
void spectrum_ux_selection_notice(uint16_t square) { notice_square = square; }
uint8_t spectrum_ux_move_intent(uint16_t from, uint16_t to)
{
    intention_from = from;
    intention_to = to;
    ++sends;
    return result;
}
void spectrum_gui_redraw_square(uint8_t r, uint8_t c) {(void)r; (void)c;}
void spectrum_gui_mark_cursor(uint8_t r, uint8_t c, uint8_t on) {(void)r; (void)c; (void)on;}
void spectrum_gui_clear_cursor_coords(void) {}
uint8_t spectrum_gui_is_board_flipped(void) {return flipped;}


static void check_rectangle_navigation_parity(void)
{
    const space_rect_t rect = {8, 8};
    const uint8_t keys[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, 'q', 'a', 'o', 'p'};
    uint8_t cell, key;

    for (flipped = 0; flipped < 2u; ++flipped) {
        for (cell = 0; cell < 64u; ++cell) {
            for (key = 0; key < sizeof(keys); ++key) {
                space_cell_t next = space_rect_neighbor(&rect, cell,
                    (uint8_t)((key % 4u) ^ flipped));
                cursor_home((uint16_t)((cell / 8u) | ((uint16_t)(cell % 8u) << 8)));
                cursor_move(keys[key]);
                assert(spectrum_ux_cursor_position() ==
                       (next == SPACE_CELL_NONE ? cell : next));
                assert(spectrum_ux_selected_position() == 0xffffu);
            }
        }
    }
    flipped = 0;
    assert(spectrum_space_neighbor(SPACE_CELL_NONE, KEY_UP) == SPACE_CELL_NONE);
    assert(spectrum_space_neighbor(0x0800u, KEY_UP) == SPACE_CELL_NONE);
    assert(spectrum_space_neighbor(0x0008u, KEY_UP) == SPACE_CELL_NONE);
    assert(spectrum_space_neighbor(0x0302u, 0u) == 0x0302u);
}

int main(void)
{
    char original[64];
    memset(board, 0, sizeof(board));
    board[52] = 37;
    memcpy(original, board, sizeof(board));
    check_rectangle_navigation_parity();
    cursor_home(0x0406u);
    assert(spectrum_ux_cursor_position() == 52u && spectrum_ux_selected_position() == 0xffffu);
    cursor_move(KEY_UP);
    assert(spectrum_ux_cursor_position() == 44u);
    flipped = 1;
    cursor_move(KEY_UP);
    assert(spectrum_ux_cursor_position() == 52u);
    flipped = 0;
    assert(spectrum_ux_board_activate() && spectrum_ux_selected_position() == 52u && hints);
    assert(notice_square == 0x0406u);
    assert(spectrum_ux_board_activate() && spectrum_ux_selected_position() == 0xffffu && !hints);
    spectrum_ux_board_activate();
    cursor_move(KEY_UP);
    assert(spectrum_ux_board_activate() && spectrum_ux_selected_position() == 52u);
    assert(sends == 1 && intention_from == 0x0406u && intention_to == 0x0405u);
    assert(memcmp(board, original, sizeof(board)) == 0);
    result = UX_MOVE_FAILED;
    assert(!spectrum_ux_board_activate());
    assert(spectrum_ux_selected_position() == 52u);
    assert(memcmp(board, original, sizeof(board)) == 0);
    result = UX_MOVE_ACCEPTED;
    assert(spectrum_ux_board_activate() && spectrum_ux_selected_position() == 0xffffu);
    assert(memcmp(board, original, sizeof(board)) == 0);
    pending = 1;
    cursor_move(KEY_LEFT);
    assert(spectrum_ux_cursor_position() % 8u == 4u);
    pending = 0;
    cursor_hide();
    assert(spectrum_ux_selected_position() == 0xffffu && !hints);
    cursor_home(0x0406u);
    assert(spectrum_ux_cursor_position() == 52u);
    cursor_home(0x0203u);
    assert(spectrum_ux_cursor_position() == 26u);
    assert(spectrum_ux_board_activate() && spectrum_ux_selected_position() == 0xffffu);
    assert(notice_square == 0x0203u && sends == 3u);
    turn = 0;
    cursor_move(KEY_UP);
    assert(spectrum_ux_cursor_position() == 26u);
    puts("UX navigation/selection/intention/restoration contracts passed");
    return 0;
}
