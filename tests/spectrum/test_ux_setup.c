#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "spectrum/ui/setup.h"
#include "spectrum/config/setup_menu.h"
#include "spectrum/config/session.h"
#include "spectrum/ui/gui.h"
#include "common/ui_messages.h"

/* Simulate CONFIG overwriting the same arena, not a second UI allocation. */
union {
    uint8_t bytes[128];
    spectrum_setup_view_t setup;
} app_workspace;
#define SPECTRUM_SETUP_CUSTOM_OVERLAYS
#include "setup_fixture.h"
volatile uint8_t netchesszx_host_overlay_context[8];
uint8_t netchesszx_piece_set_index, netchesszx_board_theme_index;
int8_t netchesszx_timezone;
static uint8_t dispatch_ok = 1, saved = 1, time_valid = 1, load_ok = 1;
static uint8_t seen_key, init_flags, suppressed, starts, paint, time_ui, edited;
static uint16_t next_visible, dirty, mode;
static uint16_t notice_message;
const char spectrum_app_overlay_load_error[] = NETCHESSZX_UI_ERROR_OVERLAY_LOAD;

uint8_t netchesszx_setup_dispatch_overlay(uint8_t key)
{
    seen_key = key;
    /* Endpoint navigation may change the mask before composition. */
    setup_test_state.visible_mask = next_visible;
    return dispatch_ok;
}
uint16_t netchesszx_setup_compute_visible(uint16_t mask)
{ (void)mask; return next_visible; }
void netchesszx_setup_render_overlay(uint16_t mask, uint16_t flags)
{ dirty = mask; mode = flags; }
void netchesszx_setup_render_edit_line(uint8_t row) { edited = row; }
void netchesszx_setup_paint_attrs(void) { ++paint; }
void netchesszx_setup_time_ui(void) { ++time_ui; }
uint8_t netchesszx_setup_time_commit(void) { return time_valid; }
uint8_t netchesszx_setup_time_init(uint8_t flags)
{
    memset(&app_workspace, 0, sizeof app_workspace);
    init_flags = flags;
    setup_test_state.cursor = SETUP_ROW_ACTION;
    setup_test_state.config_dirty = !(flags & 1);
    return 1u;
}
uint8_t spectrum_setup_save_config(void)
{ memset(&app_workspace, 0xa5, sizeof app_workspace); return saved; }
uint8_t spectrum_setup_start(uint8_t key) { seen_key = key; ++starts; return time_valid; }
void spectrum_setup_preview_piece_set(uint8_t set)
{ if (load_ok) netchesszx_piece_set_index = set; }
void spectrum_setup_apply_timezone(int8_t previous) { assert(previous == netchesszx_timezone); }
void netchesszx_board_theme_apply(uint8_t theme) { netchesszx_board_theme_index = theme; }
void spectrum_gui_set_board_view(uint8_t side) { (void)side; }
void spectrum_gui_notify(const char *text, uint8_t error) { (void)text; (void)error; }
void spectrum_gui_notify_msg(uint16_t message) { notice_message = message; }
void spectrum_gui_restore_board_area(void) {}
void spectrum_input_suppress_until_release(uint8_t key) { suppressed = key; }

static void event(uint8_t action, uint8_t flags, uint16_t mask, uint8_t clear)
{
    memset((void *)netchesszx_host_overlay_context, 0, 8);
    netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_ACTION] = action;
    netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_FLAGS] = flags;
    netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_LO] = (uint8_t)mask;
    netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_HI] = (uint8_t)(mask >> 8);
    netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_CLEAR_FROM] = clear;
}
static void render_case(uint16_t before, uint16_t after, uint16_t requested,
                        uint8_t clear, uint16_t expected, uint8_t expected_clear)
{
    setup_test_state.visible_mask = before;
    next_visible = after;
    event(0, NETCHESSZX_SETUP_FLAG_RENDER, requested, clear);
    assert(!spectrum_setup_step('x'));
    assert(spectrum_setup_view.visible_mask == after && dirty == expected);
    assert(mode == (uint16_t)expected_clear << 8);
}
int main(void)
{
    uint16_t notice;
    render_case(1, 3, 0, 255, 2, 255);
    render_case(1, 3, 0xfffe, 1, 0xfffe, 1);
    render_case(3, 3, 0xfffe, 1, 0xfffc, 2);
    render_case(0x7ff, 3, 0xfffe, 1, 0xfffc, 2);
    render_case(0x7ff, 0x7ff, 0x80, 255, 0x80, 255);
    render_case(0x1ff, 0x3ff, 0x100, 255, 0x300, 255);
    render_case(3, 0x1f, 4, 255, 0x1c, 255);
    render_case(0x7ff, 0x7ff, 0, 255, 0, 255);

    event(0, 0, 0, 255);
    spectrum_setup_step('q'); assert(seen_key == 0x81);
    setup_test_state.room_editing = 1;
    spectrum_setup_step('q'); assert(seen_key == 'q');
    setup_test_state.room_editing = 0;
    event(0, NETCHESSZX_SETUP_FLAG_EDIT | NETCHESSZX_SETUP_FLAG_PAINT |
             NETCHESSZX_SETUP_FLAG_TIME_UI | NETCHESSZX_SETUP_FLAG_ACTION_UI, 0, 255);
    setup_test_state.edit_row = SETUP_ROW_TIME;
    edited = 255; paint = time_ui = 0;
    spectrum_setup_step('x');
    assert(edited == 255 && paint == 1 && time_ui == 1);

    event(NETCHESSZX_SETUP_ACTION_SET, 0, 0, 255);
    setup_test_state.focus_choice[SETUP_CHOICE_SET] = 2;
    setup_test_state.defined_mask = 0; setup_test_state.cursor = SETUP_ROW_SET;
    netchesszx_piece_set_index = 0; load_ok = 0;
    spectrum_setup_step(13);
    assert(setup_test_state.cursor == SETUP_ROW_SET && !setup_test_state.defined_mask);
    load_ok = 1; spectrum_setup_step(13);
    assert(setup_test_state.cursor == SETUP_ROW_HINTS);
    assert(setup_test_state.choice[SETUP_CHOICE_SET] == 2);
    assert(setup_test_state.defined_mask & SETUP_MASK_SET);

    for (saved = 0; saved <= 1; ++saved) {
        event(NETCHESSZX_SETUP_ACTION_SAVE, 0, 0, 255);
        setup_test_state.visible_mask = 0x7ff;
        spectrum_setup_step(13);
        assert(init_flags == (saved | 2) && spectrum_setup_view.visible_mask == 0x7ff);
        assert(spectrum_setup_view.config_dirty == !saved && suppressed == 13);
    }
    event(NETCHESSZX_SETUP_ACTION_START, 0, 0, 255);
    time_valid = 0; assert(!spectrum_setup_step(13));
    time_valid = 1; assert(spectrum_setup_step(13)); assert(starts == 2);
    for (notice = 0; notice < 256; ++notice) {
        uint16_t expected = 0;
        switch (notice) {
        case NETCHESSZX_SETUP_NOTICE_SELECT: expected = SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_SELECT_OPTIONS, SPECTRUM_GUI_MSG_KIND_INFO); break;
        case NETCHESSZX_SETUP_NOTICE_BAD_IP: expected = SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_IP, SPECTRUM_GUI_MSG_KIND_ERROR); break;
        case NETCHESSZX_SETUP_NOTICE_BAD_ROOM: expected = SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_INVALID_MQTT_ROOM, SPECTRUM_GUI_MSG_KIND_ERROR); break;
        case NETCHESSZX_SETUP_NOTICE_BAD_PORT: expected = SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_PORT, SPECTRUM_GUI_MSG_KIND_ERROR); break;
        case NETCHESSZX_SETUP_NOTICE_BAD_TIMEZONE: expected = SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_TIMEZONE, SPECTRUM_GUI_MSG_KIND_ERROR); break;
        }
        event(0, 0, 0, 255);
        notice_message = 0;
        netchesszx_host_overlay_context[NETCHESSZX_SETUP_CTX_NOTICE] = (uint8_t)notice;
        assert(!spectrum_setup_step('x') && notice_message == expected);
    }
    dispatch_ok = 0; assert(!spectrum_setup_step(13)); assert(starts == 2);
    puts("Setup composition/navigation/save-reconstruction contracts passed");
    return 0;
}
