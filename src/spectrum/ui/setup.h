#ifndef SPECTRUM_UI_SETUP_H
#define SPECTRUM_UI_SETUP_H

#include <stdint.h>
#include <stddef.h>
#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define SETUP_ROW_GAME 0u
#define SETUP_ROW_LINK_FIRST 1u
#define SETUP_ROW_LINK_SECOND 2u
#define SETUP_ROW_TIME 4u
#define SETUP_ROW_COLOR 5u
#define SETUP_ROW_NOTATION 6u
#define SETUP_ROW_BOARD 7u
#define SETUP_ROW_SET 8u
#define SETUP_ROW_HINTS 9u
#define SETUP_ROW_ACTION 10u
/* Logical row 11, drawn and navigated between TIME and COLOR. */
#define SETUP_ROW_NAME 11u
#define SETUP_MASK_ALL 0x0fffu
#define SETUP_MASK_TIME 0x0010u
#define SETUP_MASK_COLOR 0x0020u
#define SETUP_MASK_NOTATION 0x0040u
#define SETUP_MASK_BOARD 0x0080u
#define SETUP_MASK_SET 0x0100u
#define SETUP_MASK_HINTS 0x0200u
#define SETUP_MASK_ACTION 0x0400u
#define SETUP_MASK_NAME 0x0800u
#define SETUP_CHOICE_ROLE 0u
#define SETUP_CHOICE_TRANSPORT 1u
#define SETUP_CHOICE_COLOR 2u
#define SETUP_CHOICE_NOTATION 3u
#define SETUP_CHOICE_HINTS 4u
#define SETUP_CHOICE_SET 5u
#define SETUP_CHOICE_TIME 6u
#define SETUP_CLEAR_NONE 0xffu
#define SETUP_PORT_INPUT_MAX 5u

#define SPECTRUM_SETUP_WORKSPACE_SIZE 55u
typedef struct {
    uint8_t choice[7];
    uint8_t focus_choice[7];
    uint8_t focus_board_theme;
    uint16_t defined_mask;
    uint16_t visible_mask;
    uint8_t cursor;
    uint8_t room_editing;
    uint8_t edit_row;
    char port_text[SETUP_PORT_INPUT_MAX + 1u];
    char timezone_text[4];
    int8_t timezone_value;
    uint8_t config_dirty;
    uint8_t selected_board_theme;
    uint8_t time_focus;
    uint8_t action_focus;
    uint8_t edit_was_dirty;
    char edit_backup[17u];
} spectrum_setup_view_t;


#ifndef NETCHESSZX_HOST_SESSION_TEST
typedef char spectrum_setup_view_layout_check[
    sizeof(spectrum_setup_view_t) == 55u &&
    offsetof(spectrum_setup_view_t, focus_choice) == 7u &&
    offsetof(spectrum_setup_view_t, focus_board_theme) == 14u &&
    offsetof(spectrum_setup_view_t, defined_mask) == 15u &&
    offsetof(spectrum_setup_view_t, visible_mask) == 17u &&
    offsetof(spectrum_setup_view_t, cursor) == 19u &&
    offsetof(spectrum_setup_view_t, room_editing) == 20u &&
    offsetof(spectrum_setup_view_t, edit_row) == 21u &&
    offsetof(spectrum_setup_view_t, port_text) == 22u &&
    offsetof(spectrum_setup_view_t, timezone_text) == 28u &&
    offsetof(spectrum_setup_view_t, timezone_value) == 32u &&
    offsetof(spectrum_setup_view_t, config_dirty) == 33u &&
    offsetof(spectrum_setup_view_t, selected_board_theme) == 34u &&
    offsetof(spectrum_setup_view_t, edit_backup) == 38u ? 1 : -1];
#endif

/* Borrowed during Setup only. CONFIG/RESTORE invalidate this view; Setup
   reconstructs it after SAVE. No additional storage or writable public view. */
#ifdef NETCHESSZX_HOST_SESSION_TEST
const volatile spectrum_setup_view_t *netchesszx_host_setup_view(void);
#define spectrum_setup_view (*netchesszx_host_setup_view())
#else
extern const volatile spectrum_setup_view_t spectrum_setup_view;
#endif
void spectrum_setup_open(uint8_t flags) NETCHESSZX_FASTCALL;
uint8_t spectrum_setup_step(uint8_t key) NETCHESSZX_FASTCALL;
#endif
