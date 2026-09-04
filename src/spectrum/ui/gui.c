#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/ui/render.h"

#include "common/chess/move_coords.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/text.h"
#include "spectrum/platform/uart.h"
#include "spectrum/session/timing.h"

#include <string.h>

#define ATTR_FLASH 0x46u
#define ATTR_TEXT 0x07u

#define GUI_KEY_LEFT 0x83u
#define GUI_KEY_RIGHT 0x84u
#define MENU_OPTION_COUNT 6u
#define GUI_CLOCK_FRAMES \
    ((uint8_t)(NETCHESSZX_SESSION_IS_60HZ ? 60u : 50u))

static void render_clock_only(void);

#ifdef NETCHESSZX_SDCC_IY
void netchesszx_asm_put_timer_digit(char *dst, uint8_t value);
void netchesszx_asm_timer_tick_one_second(uint8_t *hour,
                                          uint8_t *minute,
                                          uint8_t *second);
#define put_timer_digit netchesszx_asm_put_timer_digit
#define timer_tick_one_second netchesszx_asm_timer_tick_one_second
#endif

#define CLOCK_TEXT_SAVE_SIZE 8u
#define GAME_TIMER_SAVE_SIZE 24u

#if NETCHESSZX_MOVE_ROWS * NETCHESSZX_MOVE_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_MOVE_LOG_SIZE
#error "move log size must match low-RAM map"
#endif
#if NETCHESSZX_CHAT_ROWS * NETCHESSZX_CHAT_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_CHAT_LOG_SIZE
#error "chat log size must match low-RAM map"
#endif
#if CLOCK_TEXT_SAVE_SIZE != NETCHESSZX_LOWRAM_CLOCK_SAVE_SIZE
#error "clock save size must match low-RAM map"
#endif
#if GAME_TIMER_SAVE_SIZE != NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_SIZE
#error "game timer save size must match low-RAM map"
#endif
#if NETCHESSZX_LOWRAM_INPUT_HISTORY_END + NETCHESSZX_NOTICE_TEXT_SIZE > \
    NETCHESSZX_LOWRAM_STATUS_ADDR
#error "notice text must fit low-RAM gap before status"
#endif
#define move_lines ((char *)NETCHESSZX_LOWRAM_MOVE_LOG_ADDR)
#define chat_lines ((char *)NETCHESSZX_LOWRAM_CHAT_LOG_ADDR)
#define last_clock_line ((char *)NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR)
#define last_game_timer_line ((char *)NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_ADDR)
#define notice_text ((char *)NETCHESSZX_LOWRAM_INPUT_HISTORY_END)
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static uint8_t clock_valid;
static uint8_t clock_frames;
static uint8_t game_timer_active;
static uint8_t game_timers[6];
#define game_timer_hour game_timers[0]
#define game_timer_minute game_timers[1]
#define game_timer_second game_timers[2]
#define move_timer_hour game_timers[3]
#define move_timer_minute game_timers[4]
#define move_timer_second game_timers[5]
static uint8_t clock_force_redraw;
static uint8_t timer_force_redraw;
static uint8_t menu_visible;
static uint8_t menu_focus;
#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spectrum_gui_about_visible_state;
uint8_t spectrum_gui_side_panels_visible_state;
#define about_visible spectrum_gui_about_visible_state
#define side_panels_visible spectrum_gui_side_panels_visible_state
#else
static uint8_t about_visible;
static uint8_t side_panels_visible;
#endif
static uint16_t notice_ticks;
static uint8_t notice_error;
static uint8_t notice_success;
static uint16_t last_ply_seen;
static uint8_t move_line_count;
static uint8_t chat_line_count;
uint8_t spectrum_gui_board_flipped;
uint8_t spectrum_gui_board_pieces_visible;
static uint8_t board_coords_dirty;
static uint8_t connected_state;
static uint8_t active_coord_valid;
static uint8_t active_coord_row;
static uint8_t active_coord_col;
static uint8_t move_marker_mode = SPECTRUM_GUI_TURN_CLEAR;
static uint8_t move_marker_frames;
static uint8_t move_marker_visible;
static uint8_t move_marker_y;
static uint8_t move_marker_col;
/* Sole UI-side, read-only view of the board-owned low-RAM cells. */
#define gui_live_board ((const char *)NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR)

static void build_status_line(char *status_line, const char *text)
{
    uint8_t i = 0u;

    while (text[i] != '\0' && i < NETCHESSZX_STATUS_LEFT_TEXT_SIZE) {
        status_line[i] = text[i];
        ++i;
    }
    /* Pad with spaces: ikkle rendering self-clears each cell, so a fixed
       width draw replaces the old text without a destructive pre-clear. */
    while (i < NETCHESSZX_STATUS_LEFT_TEXT_SIZE) {
        status_line[i] = ' ';
        ++i;
    }
    status_line[i] = '\0';
}

void spectrum_gui_set_status(const char *text) NETCHESSZX_FASTCALL
{
    char status_line[NETCHESSZX_STATUS_LEFT_TEXT_SIZE + 1u];

    build_status_line(status_line, text);
    spectrum_render_status(status_line);
    render_clock_only();
}

#ifndef NETCHESSZX_SDCC_IY
static void put_timer_digit(char *dst, uint8_t value)
{
    uint8_t tens = 0u;

    while (value >= 10u) {
        value = (uint8_t)(value - 10u);
        ++tens;
    }
    dst[0] = (char)('0' + tens);
    dst[1] = (char)('0' + value);
}
#endif

static void put_hhmm(char *dst)
{
    put_timer_digit(dst, game_timer_hour);
    dst[2] = 'h';
    put_timer_digit(dst + 3u, game_timer_minute);
    dst[5] = 'm';
}

static void put_move_timer(char *dst)
{
    if (move_timer_hour != 0u) {
        put_timer_digit(dst, move_timer_hour);
        dst[2] = 'h';
        put_timer_digit(dst + 3u, move_timer_minute);
        dst[5] = 'm';
        return;
    }

    put_timer_digit(dst, move_timer_minute);
    dst[2] = 'm';
    put_timer_digit(dst + 3u, move_timer_second);
    dst[5] = 's';
}

static void reset_move_timer(void)
{
    move_timer_hour = 0u;
    move_timer_minute = 0u;
    move_timer_second = 0u;
    clock_frames = 0u;
}

#ifndef NETCHESSZX_SDCC_IY
static void timer_tick_one_second(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (*hour == 99u && *minute == 59u && *second == 59u) {
        return;
    }
    ++*second;
    if (*second < 60u) {
        return;
    }
    *second = 0u;
    ++*minute;
    if (*minute < 60u) {
        return;
    }
    *minute = 0u;
    ++*hour;
}
#endif

static void build_game_timer_line(char *game_timer_line)
{
    memset(game_timer_line, ' ', NETCHESSZX_GAME_TIMER_TEXT_SIZE);
    game_timer_line[NETCHESSZX_GAME_TIMER_TEXT_SIZE] = '\0';

    if (game_timer_active) {
        memcpy(game_timer_line, "GAME:", 5u);
        put_hhmm(game_timer_line + 5u);
        memcpy(game_timer_line + 11u, " TURN:", 6u);
        put_move_timer(game_timer_line + 17u);
    }
}

static uint8_t render_game_timer_delta(const char *game_timer_line, uint8_t menu_mode)
{
    char game_timer_char_spec[3];
    uint8_t i;
    uint8_t changed = 0u;

    for (i = 0u; i < NETCHESSZX_GAME_TIMER_TEXT_SIZE; ++i) {
        if (game_timer_line[i] == last_game_timer_line[i]) {
            continue;
        }
        changed = 1u;
        game_timer_char_spec[0] = (char)i;
        game_timer_char_spec[1] = game_timer_line[i];
        game_timer_char_spec[2] = (char)(game_timer_line[i] == ' ');
        if (menu_mode) {
            spectrum_render_menu_timer_char(game_timer_char_spec);
        } else {
            spectrum_render_game_timer_char(game_timer_char_spec);
        }
    }
    return changed;
}

static void render_game_timer_only(void)
{
    char game_timer_line[24];
    uint8_t force;

    if (menu_visible) {
        /* Timer pixels are shared between menu/closed states; the taboption
           open/close paths only retint attrs, so no forced redraw needed. */
        build_game_timer_line(game_timer_line);
        if (render_game_timer_delta(game_timer_line, 1u)) {
            memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
        }
        return;
    }

    build_game_timer_line(game_timer_line);

    force = timer_force_redraw;
    timer_force_redraw = 0u;
    if (force) {
        memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
        spectrum_render_game_timer_clear(game_timer_line);
    } else if (render_game_timer_delta(game_timer_line, 0u)) {
        memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
    }
}

static void render_clock_only(void)
{
    char clock_time[7];
    char clock_line[8];

    if (clock_valid) {
        put_timer_digit(clock_time, clock_hour);
        clock_time[2] = ':';
        put_timer_digit(clock_time + 3u, clock_minute);
    } else {
        memcpy(clock_time, "--:--", 5u);
    }
    clock_time[5] = ' ';
    clock_time[6] = '\0';

    if (!clock_force_redraw && spectrum_streq(clock_time, last_clock_line)) {
        return;
    }
    memcpy(last_clock_line, clock_time, 7u);
    clock_line[0] = '[';
    memcpy(clock_line + 1u, clock_time, 5u);
    clock_line[6] = ']';
    clock_line[7] = '\0';
    clock_force_redraw = 0u;
    spectrum_render_clock(clock_line);
}

static void render_menu_focus(uint8_t old_focus) NETCHESSZX_FASTCALL
{
    spectrum_render_menu((uint8_t)(0x80u | (uint8_t)(old_focus << 3) | menu_focus));
}

static uint8_t menu_action_key(uint8_t key) NETCHESSZX_FASTCALL
{
    menu_visible = 0u;
    spectrum_render_menu(0u);
    return key;
}

static void toggle_menu_bar(void)
{
    if (menu_visible) {
        menu_visible = 0u;
        spectrum_render_menu(0u);
    } else {
        menu_visible = 1u;
        spectrum_render_menu((uint8_t)(menu_focus + 1u));
    }
}

void spectrum_gui_hide_menu(void)
{
    if (menu_visible) {
        (void)menu_action_key(0u);
    }
}

void spectrum_gui_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    clock_hour = hour;
    clock_minute = minute;
    clock_second = second;
    clock_valid = 1u;
    clock_frames = 0u;
    render_clock_only();
}

static uint8_t shifted_clock_hour(uint8_t hour, int8_t delta)
{
    int8_t shifted = (int8_t)hour + delta;

    if (shifted < 0) {
        shifted += 24;
    } else if (shifted >= 24) {
        shifted -= 24;
    }
    return (uint8_t)shifted;
}

void spectrum_gui_shift_clock(int8_t hour_delta) NETCHESSZX_FASTCALL
{
    if (!clock_valid || hour_delta == 0) {
        return;
    }
    clock_hour = shifted_clock_hour(clock_hour, hour_delta);
    render_clock_only();
}

void spectrum_gui_set_status_error(const char *text) NETCHESSZX_FASTCALL
{
    char status_line[NETCHESSZX_STATUS_LEFT_TEXT_SIZE + 1u];

    build_status_line(status_line, text);
    spectrum_render_status_error(status_line);
    render_clock_only();
}

void spectrum_gui_game_timer_start(void)
{
    game_timer_active = 1u;
    game_timer_hour = 0u;
    game_timer_minute = 0u;
    game_timer_second = 0u;
    reset_move_timer();
    timer_force_redraw = 1u;
    render_game_timer_only();
    render_clock_only();
}

void spectrum_gui_game_timer_stop(void)
{
    game_timer_active = 0u;
    clock_frames = 0u;
    timer_force_redraw = 1u;
    render_game_timer_only();
    spectrum_gui_set_turn_label(SPECTRUM_GUI_TURN_CLEAR);
    render_clock_only();
}

void spectrum_gui_move_timer_reset(void)
{
    reset_move_timer();
    render_game_timer_only();
}

void spectrum_gui_game_timer_save(uint8_t *timers) NETCHESSZX_FASTCALL
{
    memcpy(timers, game_timers, sizeof(game_timers));
}

void spectrum_gui_game_timer_restore(const uint8_t *timers) NETCHESSZX_FASTCALL
{
    memcpy(game_timers, timers, sizeof(game_timers));
    clock_frames = 0u;
    timer_force_redraw = 1u;
#ifndef NETCHESSZX_HOST_TEST
    render_game_timer_only();
#endif
}

static void move_marker_render(uint8_t visible)
{
    char spec[5];

#ifndef NETCHESSZX_NEXT_BANKING
    if (about_visible != 0u) {
        return;
    }
#endif
    spec[0] = (char)move_marker_y;
    spec[1] = (char)move_marker_col;
    spec[2] = (char)ATTR_TEXT;
    spec[3] = visible ? '_' : ' ';
    spec[4] = '\0';
    spectrum_render_ikkle_abs_at(spec);
}

static void move_marker_clear(void)
{
    if (move_marker_mode != SPECTRUM_GUI_TURN_CLEAR) {
        move_marker_render(0u);
        move_marker_mode = SPECTRUM_GUI_TURN_CLEAR;
        move_marker_visible = 0u;
    }
}

static void move_marker_place(uint8_t mode)
{
    uint8_t row = move_line_count;
    uint8_t col;

    if ((mode & 1u) != 0u) {
        row = move_line_count == 0u
            ? 0u
            : (uint8_t)(move_line_count - 1u);
        col = NETCHESSZX_MOVE_BLACK_COL;
    } else {
        col = NETCHESSZX_INFO_TEXT_COL;
    }
    if (row >= NETCHESSZX_MOVE_ROWS) {
        row = NETCHESSZX_MOVE_ROWS - 1u;
    }
    move_marker_y = (uint8_t)(NETCHESSZX_INFO_MOVES_FIRST_Y +
                              (row * NETCHESSZX_INFO_TIGHT_LINE_STEP));
    move_marker_col = col;
}

void spectrum_gui_set_turn_label(uint8_t mode) NETCHESSZX_FASTCALL
{
    if (mode == SPECTRUM_GUI_TURN_MARKER_CLEAR) {
        move_marker_clear();
        return;
    }
    move_marker_clear();
    move_marker_mode = mode;
    move_marker_frames = 0u;
    move_marker_visible = 1u;
    spectrum_render_turn_label(mode);
    if (mode < SPECTRUM_GUI_TURN_CLEAR) {
        move_marker_place(mode);
        move_marker_render(1u);
    }
}

void spectrum_gui_set_connected(uint8_t connected) NETCHESSZX_FASTCALL
{
    if (connected > 2u) {
        connected = 2u;
    }
    if (connected == 0u) {
        if (menu_visible) {
            (void)menu_action_key(0u);
        }
    }
    connected_state = connected;
    spectrum_render_connection(connected);
}

static void notify_internal(const char *text,
                            uint8_t is_error,
                            uint8_t is_success,
                            uint8_t ticks)
{
    strncpy(notice_text, text, NETCHESSZX_NOTICE_TEXT_SIZE - 1u);
    notice_text[NETCHESSZX_NOTICE_TEXT_SIZE - 1u] = '\0';
    notice_error = is_error;
    notice_success = is_success;
    notice_ticks = ticks;
#ifndef NETCHESSZX_NEXT_BANKING
    if (about_visible != 0u) {
        return;
    }
#endif
    if (is_error) {
        spectrum_render_notice_error(notice_text);
    } else if (is_success) {
        spectrum_render_notice_success(notice_text);
    } else {
        spectrum_render_notice(notice_text);
    }
}

void spectrum_gui_notify(const char *text, uint8_t is_error)
{
    notify_internal(text, is_error, 0u, is_error ? 0u : 250u);
}

void spectrum_gui_notify_persistent(const char *text) NETCHESSZX_FASTCALL
{
    notify_internal(text, 0u, 0u, 0u);
}

void spectrum_gui_notify_success(const char *text) NETCHESSZX_FASTCALL
{
    notify_internal(text, 0u, 1u, 250u);
}

void spectrum_gui_tick(void)
{
    if (move_marker_mode != SPECTRUM_GUI_TURN_CLEAR) {
        ++move_marker_frames;
        if (move_marker_frames >= 25u) {
            move_marker_frames = 0u;
            move_marker_visible ^= 1u;
            move_marker_render(move_marker_visible);
        }
    }

    if (!notice_error && notice_ticks != 0u) {
        --notice_ticks;
        if (notice_ticks == 0u) {
            notice_text[0] = '\0';
            notice_success = 0u;
#ifndef NETCHESSZX_NEXT_BANKING
            if (about_visible == 0u) {
#endif
                spectrum_render_notice(notice_text);
#ifndef NETCHESSZX_NEXT_BANKING
            }
#endif
        }
    }

    if (!game_timer_active && !clock_valid) {
        return;
    }
    ++clock_frames;
    if (clock_frames < GUI_CLOCK_FRAMES) {
        return;
    }
    clock_frames = 0u;
    if (game_timer_active) {
        timer_tick_one_second(&game_timer_hour, &game_timer_minute,
                              &game_timer_second);
        timer_tick_one_second(&move_timer_hour, &move_timer_minute,
                              &move_timer_second);
        render_game_timer_only();
    }

    if (clock_valid) {
        ++clock_second;
        if (clock_second < 60u) {
            return;
        }
        clock_second = 0u;
        ++clock_minute;
        if (clock_minute >= 60u) {
            clock_minute = 0u;
            ++clock_hour;
            if (clock_hour >= 24u) {
                clock_hour = 0u;
            }
        }
        render_clock_only();
    }
}

void spectrum_gui_reset_moves(void)
{
    memset(move_lines, 0, NETCHESSZX_MOVE_ROWS * NETCHESSZX_MOVE_SLOT_SIZE);
    move_line_count = 0u;
    last_ply_seen = 0u;
    if (side_panels_visible) {
        spectrum_render_moves(move_lines);
    }
}

void spectrum_gui_reset_logs(void)
{
    spectrum_gui_reset_moves();
    memset(chat_lines, 0, NETCHESSZX_CHAT_ROWS * NETCHESSZX_CHAT_SLOT_SIZE);
    chat_line_count = 0u;
    if (side_panels_visible) {
        spectrum_render_chat(chat_lines);
    }
}

void spectrum_gui_set_input(const char *text) NETCHESSZX_FASTCALL
{
    if (about_visible) {
        return;
    }
    spectrum_render_input(text);
}

void spectrum_gui_set_input_edit(const char *text, uint8_t len, uint8_t cursor)
{
    if (about_visible) {
        return;
    }
    if (cursor > len) {
        cursor = len;
    }
    spectrum_render_input(text);
    spectrum_gui_input_cell(cursor, cursor < len ? text[cursor] : ' ', 1u);
}

void spectrum_gui_input_cell(uint8_t pos, char c, uint8_t cursor)
{
    char spec[3];

    if (about_visible) {
        return;
    }
    spec[0] = (char)pos;
    spec[1] = c;
    spec[2] = (char)cursor;
    spectrum_render_input_cell(spec);
}

static uint8_t display_coord(uint8_t coord) NETCHESSZX_FASTCALL
{
    return spectrum_gui_board_flipped ? (uint8_t)(7u - coord) : coord;
}

static char gui_board_cell(uint8_t row, uint8_t col)
{
    if (row >= 8u || col >= 8u) {
        return '.';
    }
    return gui_live_board[(uint8_t)((row << 3) + col)];
}

void spectrum_gui_set_board_view(uint8_t local_black) NETCHESSZX_FASTCALL
{
    uint8_t flipped = (uint8_t)(local_black != 0u);

    if (flipped == spectrum_gui_board_flipped) {
        return;
    }
    spectrum_gui_clear_cursor_coords();
    spectrum_gui_board_flipped = flipped;
    board_coords_dirty = 1u;
    if (spectrum_gui_board_pieces_visible) {
        spectrum_gui_redraw_board_view();
    }
}

uint8_t spectrum_gui_is_board_flipped(void)
{
    return spectrum_gui_board_flipped;
}

void spectrum_gui_toggle_board_view(void)
{
    spectrum_gui_clear_cursor_coords();
    spectrum_gui_board_flipped = (uint8_t)!spectrum_gui_board_flipped;
    spectrum_gui_redraw_board_view();
}

void spectrum_gui_set_board_pieces_visible(uint8_t visible) NETCHESSZX_FASTCALL
{
    spectrum_gui_board_pieces_visible = (uint8_t)(visible != 0u);
}

void spectrum_gui_clear_cursor_coords(void)
{
    char coord_mark_spec[3];

    if (!active_coord_valid) {
        return;
    }
    coord_mark_spec[0] = (char)active_coord_row;
    coord_mark_spec[1] = (char)active_coord_col;
    coord_mark_spec[2] = 0;
    spectrum_render_board_coord_mark(coord_mark_spec);
    active_coord_valid = 0u;
}

uint8_t spectrum_gui_show_about(void)
{
    uint8_t was_menu_visible = menu_visible;

    menu_visible = 0u;
    active_coord_valid = 0u;
    if (was_menu_visible) {
        spectrum_render_menu(0u);
    }
#ifndef NETCHESSZX_NEXT_BANKING
    side_panels_visible = 0u;
#endif
    about_visible = 1u;
    if (!spectrum_render_about()) {
        about_visible = 0u;
        return 0u;
    }
    return 1u;
}

uint8_t spectrum_gui_about_visible(void)
{
    return about_visible;
}

/* The FILE browser shares the about_visible gate (value 2) so every
   board-area suppression path keeps working unchanged. */
uint8_t spectrum_gui_show_fileui(void)
{
    uint8_t was_menu_visible = menu_visible;

    menu_visible = 0u;
    active_coord_valid = 0u;
    if (was_menu_visible) {
        spectrum_render_menu(0u);
    }
    about_visible = 2u;
    return 1u;
}

uint8_t spectrum_gui_fileui_visible(void)
{
    return (uint8_t)(about_visible == 2u);
}

static void spectrum_gui_mark_cursor_coords(uint8_t row, uint8_t col)
{
    char coord_mark_spec[3];

    row = display_coord(row);
    col = display_coord(col);
    if (active_coord_valid &&
        active_coord_row == row &&
        active_coord_col == col) {
        return;
    }
    spectrum_gui_clear_cursor_coords();
    coord_mark_spec[0] = (char)row;
    coord_mark_spec[1] = (char)col;
    coord_mark_spec[2] = 1;
    spectrum_render_board_coord_mark(coord_mark_spec);
    active_coord_row = row;
    active_coord_col = col;
    active_coord_valid = 1u;
}

void spectrum_gui_hide_board_pieces(void)
{
    if (!spectrum_gui_board_pieces_visible) {
        return;
    }
    spectrum_gui_board_pieces_visible = 0u;
    if (about_visible) {
        return;
    }
    spectrum_gui_redraw_board_squares();
}

static void render_square_from_board(uint8_t row, uint8_t col)
{
    char piece;
    char square_spec[6];

    piece = spectrum_gui_board_pieces_visible ? gui_board_cell(row, col) : '.';
    square_spec[0] = (char)display_coord(row);
    square_spec[1] = (char)display_coord(col);
    square_spec[2] = piece;
    if (spectrum_gui_board_pieces_visible) {
        square_spec[3] = (char)row;
        square_spec[4] = (char)col;
        spectrum_render_square_with_hint(square_spec);
    } else {
        spectrum_render_square(square_spec);
    }
}

void spectrum_gui_redraw_square(uint8_t row, uint8_t col)
{
    if (about_visible) {
        return;
    }
    render_square_from_board(row, col);
}

void spectrum_gui_redraw_board_squares(void)
{
    uint8_t row;
    uint8_t col;

    if (about_visible) {
        return;
    }
    for (row = 0u; row < 8u; ++row) {
        for (col = 0u; col < 8u; ++col) {
            render_square_from_board(row, col);
        }
    }
}

static void spectrum_gui_redraw_board_flip_squares(void)
{
    uint8_t pass;
    uint8_t row;
    uint8_t col;

    for (pass = 0u; pass < 2u; ++pass) {
        for (row = 0u; row < 8u; ++row) {
            for (col = 0u; col < 8u; ++col) {
                if ((uint8_t)(gui_live_board[(uint8_t)((row << 3) + col)] != '.') == pass) {
                    render_square_from_board(row, col);
                }
            }
        }
    }
}

void spectrum_gui_mark_cursor(uint8_t row, uint8_t col, uint8_t selected)
{
    char square_spec[6];

    if (about_visible) {
        return;
    }
    spectrum_gui_mark_cursor_coords(row, col);
    square_spec[0] = (char)display_coord(row);
    square_spec[1] = (char)display_coord(col);
    square_spec[2] = (char)(selected ? 1u : 0u);
    if (spectrum_gui_board_pieces_visible) {
        square_spec[3] = (char)row;
        square_spec[4] = (char)col;
        square_spec[5] = gui_board_cell(row, col);
        spectrum_render_square_mark_with_hint(square_spec);
    } else {
        spectrum_render_square_mark(square_spec);
    }
}

uint8_t spectrum_gui_poll_key(void)
{
    return spectrum_key_poll();
}

uint8_t spectrum_gui_handle_menu_key(uint8_t key) NETCHESSZX_FASTCALL
{
    if (!about_visible && key == SPECTRUM_GUI_KEY_MENU) {
        toggle_menu_bar();
        return 0u;
    }
    if (!menu_visible) {
        return key;
    }
    if (key == GUI_KEY_LEFT || key == '5' || key == 'o') {
        uint8_t old_focus = menu_focus;
        menu_focus = menu_focus == 0u ? (MENU_OPTION_COUNT - 1u) : (uint8_t)(menu_focus - 1u);
        render_menu_focus(old_focus);
    } else if (key == GUI_KEY_RIGHT || key == '8' || key == 'p') {
        uint8_t old_focus = menu_focus;
        menu_focus = (uint8_t)(menu_focus + 1u);
        if (menu_focus >= MENU_OPTION_COUNT) {
            menu_focus = 0u;
        }
        render_menu_focus(old_focus);
    } else if (key == 13u || key == 32u) {
        if (menu_focus == 0u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_FILE);
        }
        if (menu_focus == 1u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_DISCC);
        }
        if (menu_focus == 2u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_REST);
        }
        if (menu_focus == 3u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_FLIP);
        }
        if (menu_focus == 4u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_THEME);
        }
        if (menu_focus == 5u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_ABOUT);
        }
    }
    return 0u;
}

static void wait_frames(uint8_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_uart_background_pump();
        spectrum_gui_tick();
        spectrum_uart_background_pump();
    }
}

#ifdef NETCHESSZX_SPECTRANEXT
void spectrum_gui_flash_square(uint8_t row, uint8_t col)
#else
static void flash_square(uint8_t row, uint8_t col)
#endif
{
    uint8_t i;
    char square_spec[3];

    for (i = 0u; i < 2u; ++i) {
        square_spec[0] = (char)display_coord(row);
        square_spec[1] = (char)display_coord(col);
        square_spec[2] = (char)ATTR_FLASH;
        spectrum_render_square_attr(square_spec);
        wait_frames(6u);
        render_square_from_board(row, col);
        wait_frames(6u);
    }
}
#ifdef NETCHESSZX_SPECTRANEXT
#define flash_square spectrum_gui_flash_square
#endif

void spectrum_gui_prepare_move(const char *move) NETCHESSZX_FASTCALL
{
    uint16_t coords;
    uint8_t from_idx;

    if (about_visible) {
        return;
    }
    coords = netchesszx_move_parse_coords(move);
    if (coords == NETCHESSZX_MOVE_COORDS_INVALID) {
        return;
    }
    from_idx = NETCHESSZX_MOVE_FROM_INDEX(coords);
    if (gui_board_cell((uint8_t)(from_idx >> 3),
                       (uint8_t)(from_idx & 7u)) != '.') {
        flash_square((uint8_t)(from_idx >> 3),
                     (uint8_t)(from_idx & 7u));
    }
}

#ifndef NETCHESSZX_SPECTRANEXT
void spectrum_gui_apply_move(const char *move) NETCHESSZX_FASTCALL
{
    uint16_t coords;
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    char piece;

    if (about_visible) {
        return;
    }
    coords = netchesszx_move_parse_coords(move);
    if (coords == NETCHESSZX_MOVE_COORDS_INVALID) {
        return;
    }
    from_col = NETCHESSZX_MOVE_FROM_INDEX(coords);
    to_col = NETCHESSZX_MOVE_TO_INDEX(coords);
    from_row = (uint8_t)(from_col >> 3);
    to_row = (uint8_t)(to_col >> 3);
    from_col &= 7u;
    to_col &= 7u;

    render_square_from_board(from_row, from_col);
    render_square_from_board(to_row, to_col);
    flash_square(to_row, to_col);

    piece = gui_board_cell(to_row, to_col);
    if ((piece == 'P' || piece == 'p') && from_col != to_col) {
        render_square_from_board(from_row, to_col);
    }
    if ((piece == 'K' || piece == 'k') &&
        from_row == to_row && from_col == 4u) {
        if (to_col == 6u) {
            render_square_from_board(from_row, 7u);
            render_square_from_board(from_row, 5u);
        } else if (to_col == 2u) {
            render_square_from_board(from_row, 0u);
            render_square_from_board(from_row, 3u);
        }
    }
}
#endif

void spectrum_gui_draw_board(void)
{
    about_visible = 0u;
    spectrum_render_board(gui_live_board);
    /* render_board no longer calls hide_menu; keep C menu state in sync. */
    menu_visible = 0u;
    active_coord_valid = 0u;
    side_panels_visible = 0u;
    board_coords_dirty = 0u;
    clock_force_redraw = 1u;
    spectrum_gui_set_connected(connected_state);
    timer_force_redraw = 1u;
    render_game_timer_only();
    spectrum_gui_set_input("");
}

void spectrum_gui_redraw_board_view(void)
{
    spectrum_render_board_coords();
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
    spectrum_gui_redraw_board_flip_squares();
}

void spectrum_gui_restore_board_area(void)
{
    about_visible = 0u;
    spectrum_render_board_area(gui_live_board);
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
}

#ifndef NETCHESSZX_NEXT_BANKING
void spectrum_gui_restore_game_center(void)
{
    about_visible = 0u;
    spectrum_restore_game_center(gui_live_board);
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
    side_panels_visible = 1u;
    spectrum_info_show_game();
    spectrum_render_moves(move_lines);
    spectrum_render_chat(chat_lines);
    spectrum_gui_draw_status();
}
#endif

#ifndef NETCHESSZX_SPECTRANEXT
static void reveal_board_piece_pairs(uint8_t step, uint8_t pause)
{
    uint8_t i;

    for (i = 0u; i < 8u; ++i) {
        render_square_from_board(0u, i);
        render_square_from_board(7u, (uint8_t)(7u - i));
        wait_frames(step);
    }
    wait_frames(pause);
    for (i = 0u; i < 8u; ++i) {
        render_square_from_board(1u, (uint8_t)(7u - i));
        render_square_from_board(6u, i);
        wait_frames(step);
    }
}

#endif

#ifdef NETCHESSZX_SPECTRANEXT
void spectrum_gui_sync_board_coords(void)
#else
static void sync_board_coords(void)
#endif
{
    if (!board_coords_dirty) {
        return;
    }
    spectrum_render_board_coords();
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
}

#ifndef NETCHESSZX_SPECTRANEXT
void spectrum_gui_animate_board_pieces(void)
{
    sync_board_coords();
    if (spectrum_gui_board_pieces_visible) {
        spectrum_gui_hide_board_pieces();
    }
    spectrum_gui_set_board_pieces_visible(1u);
    reveal_board_piece_pairs(1u, 0u);
}

void spectrum_gui_morph_board_pieces(void)
{
    if (!spectrum_gui_board_pieces_visible) {
        spectrum_gui_animate_board_pieces();
        return;
    }
    sync_board_coords();
    reveal_board_piece_pairs(0u, 0u);
}
#endif

void spectrum_gui_draw_status(void)
{
    render_clock_only();
    if (notice_error) {
        spectrum_render_notice_error(notice_text);
    } else if (notice_success) {
        spectrum_render_notice_success(notice_text);
    } else {
        spectrum_render_notice(notice_text);
    }
}

#ifndef NETCHESSZX_SPECTRANEXT
void spectrum_gui_restore_side_panels(void)
{
    about_visible = 0u;
    side_panels_visible = 1u;
    spectrum_info_show_game();
    spectrum_render_moves(move_lines);
    spectrum_render_chat(chat_lines);
}
#endif

uint8_t spectrum_gui_side_panels_visible(void)
{
    return side_panels_visible;
}
