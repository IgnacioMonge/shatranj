#include "spectrum/ui/setup.h"
#include "spectrum/app/setup_ports.h"
#include "spectrum/config/setup_menu.h"
#include "spectrum/config/session.h"
#include "spectrum/ui/gui.h"
#include "spectrum/ui/presentation.h"
#include "spectrum/platform/input.h"
#include "common/ui_messages.h"

#define KEY_LEFT 0x83u
#define KEY_RIGHT 0x84u
#define KEY_UP 0x81u
#define KEY_DOWN 0x82u

#define setup_focus_choice spectrum_setup_view.focus_choice
#define setup_focus_board_theme spectrum_setup_view.focus_board_theme
#define setup_defined_mask spectrum_setup_view.defined_mask
#define setup_visible_mask spectrum_setup_view.visible_mask
#define setup_cursor spectrum_setup_view.cursor
#define setup_room_editing spectrum_setup_view.room_editing
#define setup_edit_row spectrum_setup_view.edit_row
#define setup_focus_color setup_focus_choice[SETUP_CHOICE_COLOR]
#define setup_focus_piece_set setup_focus_choice[SETUP_CHOICE_SET]

/* Resident ASM operations own writes to the phase-shared storage. */
void spectrum_setup_set_visible(uint16_t mask) NETCHESSZX_FASTCALL;
void spectrum_setup_accept_set(void);
uint8_t netchesszx_setup_nav_key_alias(uint8_t key) NETCHESSZX_FASTCALL;

#ifdef NETCHESSZX_HOST_SESSION_TEST
uint8_t netchesszx_setup_nav_key_alias(uint8_t key)
{
    if (key == '5' || key == 'o') {
        return KEY_LEFT;
    }
    if (key == '6' || key == 'a') {
        return KEY_DOWN;
    }
    if (key == '7' || key == 'q') {
        return KEY_UP;
    }
    if (key == '8' || key == 'p') {
        return KEY_RIGHT;
    }
    return key;
}
#endif

#define notify_error(text) spectrum_gui_notify((text), 1u)
#define notify_error_msg(id) spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_ERROR))

static const uint16_t setup_notice_messages[] = {
    0u,
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_SELECT_OPTIONS, SPECTRUM_GUI_MSG_KIND_INFO),
    0u, /* Pending is handled by the initiating action, not a new notice. */
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_IP, SPECTRUM_GUI_MSG_KIND_ERROR),
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_INVALID_MQTT_ROOM, SPECTRUM_GUI_MSG_KIND_ERROR),
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_PORT, SPECTRUM_GUI_MSG_KIND_ERROR),
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_BAD_TIMEZONE, SPECTRUM_GUI_MSG_KIND_ERROR)
};

static void session_setup_notice(uint8_t notice) NETCHESSZX_FASTCALL
{
    if (notice < sizeof(setup_notice_messages) / sizeof(setup_notice_messages[0])) {
        uint16_t message = setup_notice_messages[notice];
        if (message != 0u) {
            spectrum_gui_notify_msg(message);
        }
    }
}

static void session_setup_render(uint8_t full,
                                 uint16_t force_dirty,
                                 uint8_t clear_from)
{
    uint16_t visible = netchesszx_setup_compute_visible(setup_defined_mask);

    /* Changing the seat invalidates endpoints, not the unchanged LINK row. */
    if (clear_from == SETUP_ROW_LINK_FIRST && (setup_visible_mask & 2u)) {
        clear_from = SETUP_ROW_LINK_SECOND;
        force_dirty &= (uint16_t)~2u;
    }
    force_dirty |= visible & (uint16_t)~setup_visible_mask;
    spectrum_setup_set_visible(visible);
    netchesszx_setup_render_overlay(force_dirty,
        (uint16_t)full | ((uint16_t)clear_from << 8));
    netchesszx_setup_render_edit_line(0xffu);
    netchesszx_setup_time_ui();
}

static void session_setup_sync_board_view(void)
{
    spectrum_gui_set_board_view(setup_focus_color);
}

static void session_setup_apply_set(uint8_t key)
{
    spectrum_setup_preview_piece_set(setup_focus_piece_set);
    if (setup_focus_piece_set != netchesszx_piece_set_index) {
        return;
    }
    spectrum_setup_accept_set();
    session_setup_render(0u, SETUP_MASK_SET, SETUP_CLEAR_NONE);
    spectrum_input_suppress_until_release(key);
}

static void session_setup_save(uint8_t key)
{
    uint16_t visible_mask = setup_visible_mask;
    uint8_t saved;

    if (!netchesszx_setup_time_commit()) {
        return;
    }
    saved = spectrum_setup_save_config();
    netchesszx_setup_time_init((uint8_t)(saved | 2u));
    spectrum_setup_set_visible(visible_mask);
    netchesszx_setup_time_ui();
    if (!saved) {
        notify_error_msg(SPECTRUM_GUI_MSG_SAVE_FAIL);
    }
    spectrum_input_suppress_until_release(key);
}

uint8_t spectrum_setup_step(uint8_t key) NETCHESSZX_FASTCALL
{
    uint8_t flags;
    uint8_t action;
    uint8_t notice;
    uint16_t force_dirty;
    uint8_t clear_from;
    uint16_t previous_visible = setup_visible_mask;
    int8_t previous_timezone = netchesszx_timezone;

    if (!setup_room_editing) {
        key = netchesszx_setup_nav_key_alias(key);
    }
    if (!netchesszx_setup_dispatch_overlay(key)) {
        notify_error(spectrum_app_overlay_load_error);
        return 0u;
    }
    flags = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FLAGS];
    action = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_ACTION];
    notice = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_NOTICE];
    force_dirty = (uint16_t)netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_LO] |
                  ((uint16_t)netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_HI] << 8);
    clear_from = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_CLEAR_FROM];

    if (action == NETCHESSZX_SETUP_ACTION_START) {
        return spectrum_setup_start(key);
    }
    if (action == NETCHESSZX_SETUP_ACTION_SAVE) {
        session_setup_save(key);
        return 0u;
    }
    if (action == NETCHESSZX_SETUP_ACTION_TIMEZONE) {
        spectrum_setup_apply_timezone(previous_timezone);
    }
    if (force_dirty & SETUP_MASK_BOARD) {
        volatile uint8_t board_theme = setup_focus_board_theme;

        if (netchesszx_board_theme_index != board_theme) {
            spectrum_presentation_apply_theme(board_theme);
        }
    }
    if (action == NETCHESSZX_SETUP_ACTION_SET) {
        session_setup_apply_set(key);
        return 0u;
    }
    if (flags & NETCHESSZX_SETUP_FLAG_RENDER) {
        /* Endpoint editing can update visibility while choosing the next row. */
        spectrum_setup_set_visible(previous_visible);
        session_setup_render(0u, force_dirty, clear_from);
    }
    if (flags & NETCHESSZX_SETUP_FLAG_EDIT) {
        if (setup_edit_row != SETUP_ROW_TIME) {
            netchesszx_setup_render_edit_line(setup_edit_row);
        }
    }
    if (flags & NETCHESSZX_SETUP_FLAG_PAINT) {
        netchesszx_setup_paint_attrs();
    }
    if (flags & (NETCHESSZX_SETUP_FLAG_TIME_UI |
                 NETCHESSZX_SETUP_FLAG_ACTION_UI)) {
        netchesszx_setup_time_ui();
    }
    session_setup_sync_board_view();
    if (setup_cursor == SETUP_ROW_SET) {
        spectrum_setup_preview_piece_set(setup_focus_piece_set);
    }
    session_setup_notice(notice);
    if (flags & NETCHESSZX_SETUP_FLAG_SUPPRESS) {
        spectrum_input_suppress_until_release(key);
    }
    return 0u;
}


void spectrum_setup_open(uint8_t flags) NETCHESSZX_FASTCALL
{
    netchesszx_setup_time_init(flags);
    netchesszx_board_theme_apply(setup_focus_board_theme);
    session_setup_render(1u, 0u, SETUP_CLEAR_NONE);
    session_setup_sync_board_view();
}
