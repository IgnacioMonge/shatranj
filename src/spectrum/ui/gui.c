#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/presentation.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/ui/render.h"

#include "common/chess/move_coords.h"
#include "spectrum/app/time_view.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/text.h"
#include "spectrum/platform/uart.h"
#include <string.h>

static void render_clock_only(void);
static void render_game_timer_only(void);

#ifdef NETCHESSZX_SDCC_IY
void netchesszx_asm_put_timer_digit(char *dst, uint8_t value);
#define put_timer_digit netchesszx_asm_put_timer_digit
#endif

#define TIMER_ROLL_BUFFER_SIZE 18u
#define TIMER_ROLL_STRIP 8u
#define TIMER_ROLL_STRIP_BASE 10u
#define CLOCK_IKKLE_ATTR 0x38u
#define CLOCK_DIGIT_TEXT_SIZE 5u

#if NETCHESSZX_MOVE_ROWS * NETCHESSZX_MOVE_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_MOVE_LOG_SIZE
#error "move log size must match low-RAM map"
#endif
#if NETCHESSZX_CHAT_ROWS * NETCHESSZX_CHAT_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_CHAT_LOG_SIZE
#error "chat log size must match low-RAM map"
#endif
#if NETCHESSZX_CLOCK_TEXT_SAVE_SIZE != NETCHESSZX_LOWRAM_CLOCK_SAVE_SIZE
#error "clock save size must match low-RAM map"
#endif
#if NETCHESSZX_GAME_TIMER_SAVE_SIZE != NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_SIZE
#error "game timer save size must match low-RAM map"
#endif
#if TIMER_ROLL_BUFFER_SIZE != NETCHESSZX_LOWRAM_TIMER_ROLL_SIZE
#error "timer roll buffer must fill its fixed low-RAM arena"
#endif
#if TIMER_ROLL_STRIP_BASE + TIMER_ROLL_STRIP > \
    TIMER_ROLL_BUFFER_SIZE
#error "timer roll glyph strips exceed the fixed low-RAM arena"
#endif
#if NETCHESSZX_NOTICE_TEXT_SIZE != NETCHESSZX_LOWRAM_NOTICE_TEXT_SIZE
#error "notice text must fill its fixed low-RAM slot"
#endif
#define move_lines ((char *)NETCHESSZX_LOWRAM_MOVE_LOG_ADDR)
#define chat_lines ((char *)NETCHESSZX_LOWRAM_CHAT_LOG_ADDR)
#ifdef NETCHESSZX_HOST_TEST
static char host_last_clock_line[NETCHESSZX_LOWRAM_CLOCK_SAVE_SIZE];
static char host_notice_text[NETCHESSZX_NOTICE_TEXT_SIZE];
#define last_clock_line host_last_clock_line
#define notice_text host_notice_text
#else
#define last_clock_line ((char *)NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR)
#define notice_text ((char *)NETCHESSZX_LOWRAM_NOTICE_TEXT_ADDR)
#endif
#define last_game_timer_line ((char *)NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_ADDR)
#define timer_roll_state ((uint8_t *)NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_ADDR)
#define timer_roll_phase timer_roll_state[0]
#define clock_roll_phase timer_roll_state[1]
static uint8_t clock_force_redraw;
static uint8_t timer_force_redraw;
#define menu_visible timer_roll_state[5]
#define menu_focus timer_roll_state[6]
#define menu_item timer_roll_state[7]
#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spectrum_gui_about_visible_state;
uint8_t spectrum_gui_side_panels_visible_state;
#define side_panels_visible spectrum_gui_side_panels_visible_state
#else
uint8_t spectrum_gui_about_visible_state;
static uint8_t side_panels_visible;
#endif
#define about_visible spectrum_gui_about_visible_state
#ifdef NETCHESSZX_FIXED_LOW_RAM
static __at(NETCHESSZX_LOWRAM_NOTICE_TICKS_ADDR) uint8_t notice_ticks;
static __at(NETCHESSZX_LOWRAM_NOTICE_ERROR_ADDR) uint8_t notice_error;
static __at(NETCHESSZX_LOWRAM_NOTICE_SUCCESS_ADDR) uint8_t notice_success;
static __at(NETCHESSZX_LOWRAM_LAST_PLY_SEEN_ADDR) uint16_t last_ply_seen;
static __at(NETCHESSZX_LOWRAM_MOVE_LINE_COUNT_ADDR) uint8_t move_line_count;
static __at(NETCHESSZX_LOWRAM_CHAT_LINE_COUNT_ADDR) uint8_t chat_line_count;
#else
static uint8_t notice_ticks;
static uint8_t notice_error;
static uint8_t notice_success;
static uint16_t last_ply_seen;
static uint8_t move_line_count;
static uint8_t chat_line_count;
#endif
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

void netchesszx_asm_timer_game(const char *text) NETCHESSZX_FASTCALL;
void netchesszx_asm_timer_clock(const char *text) NETCHESSZX_FASTCALL;
uint8_t netchesszx_asm_menu_key(uint8_t key) NETCHESSZX_FASTCALL;

#ifdef NETCHESSZX_SDCC_IY
/* Share the step, not the phase: each consumer still owns its five frames. */
static void timer_roll_tick(void) __naked
{
#asm
    INCLUDE "asm/spectrum/timer_roll_layout.inc"
    ld hl,timer_roll_state_addr
    ld de,_render_game_timer_only
    call gui_roll_step
    inc hl
    ld de,_render_clock_only
    call gui_roll_step
    ret
gui_roll_step:
    ld a,(hl)
    or a
    ret z
    inc (hl)
    push hl
    ex de,hl
    call gui_roll_render
    pop hl
    ld a,(hl)
    and 3
    ret nz
    ld (hl),a
    ret
gui_roll_render:
    jp (hl)
#endasm
}

static void timer_roll_finish(void) __naked
{
#asm
    ld hl,0x0404
    ld (timer_roll_state_addr),hl
    call _render_game_timer_only
    call _render_clock_only
    ld hl,0
    ld (timer_roll_state_addr),hl
    ret
#endasm
}
#else
static void timer_roll_tick(void)
{
    uint8_t phase;

    phase = timer_roll_phase;
    if (phase != 0u) {
        phase = (uint8_t)((phase & 7u) + 1u);
        timer_roll_phase = phase;
        render_game_timer_only();
        if (phase == 4u) {
            timer_roll_phase = 0u;
        }
    }
    phase = clock_roll_phase;
    if (phase != 0u) {
        phase = (uint8_t)((phase & 7u) + 1u);
        clock_roll_phase = phase;
        render_clock_only();
        if (phase == 4u) {
            clock_roll_phase = 0u;
        }
    }
}

static void timer_roll_finish(void)
{
    timer_roll_phase = clock_roll_phase = 4u;
    render_game_timer_only();
    render_clock_only();
    timer_roll_phase = clock_roll_phase = 0u;
}
#endif

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

static void set_status(const char *text, uint8_t error)
{
    char status_line[NETCHESSZX_STATUS_LEFT_TEXT_SIZE + 1u];

    build_status_line(status_line, text);
    if (error) {
        spectrum_render_status_error(status_line);
    } else {
        spectrum_render_status(status_line);
    }
    render_clock_only();
}

void spectrum_gui_set_status(const char *text) NETCHESSZX_FASTCALL
{
    set_status(text, 0u);
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

static void build_game_timer_line(char *game_timer_line)
{
    const spectrum_time_view_t *time = spectrum_time_snapshot();
    const uint8_t *timers = time->game_timers;

    if (time->game_timer_active) {
        memcpy(game_timer_line, "GAME:00h00m TURN:00h00m", 24u);
        put_timer_digit(game_timer_line + 5u, timers[0]);
        put_timer_digit(game_timer_line + 8u, timers[1]);
        timers += 3u;
        if (timers[0] == 0u) {
            ++timers;
            game_timer_line[19] = 'm';
            game_timer_line[22] = 's';
        }
        put_timer_digit(game_timer_line + 17u, timers[0]);
        put_timer_digit(game_timer_line + 20u, timers[1]);
    } else {
        memset(game_timer_line, ' ', NETCHESSZX_GAME_TIMER_TEXT_SIZE);
        game_timer_line[NETCHESSZX_GAME_TIMER_TEXT_SIZE] = '\0';
    }
}

static void render_game_timer_only(void)
{
    char game_timer_line[24];
    uint8_t force = 0u;

    /* Menu open/close only retints shared timer pixels; retain a pending force. */
    if (!menu_visible) {
        force = timer_force_redraw;
        timer_force_redraw = 0u;
    }
    build_game_timer_line(game_timer_line);
    if (force) {
        memcpy(last_game_timer_line, game_timer_line,
               NETCHESSZX_GAME_TIMER_SAVE_SIZE);
        spectrum_render_game_timer_clear(game_timer_line);
        return;
    }
    netchesszx_asm_timer_game(game_timer_line);
}

static void render_clock_only(void)
{
    char clock_line[8];
    char *clock_time = clock_line + 1u;
    const spectrum_time_view_t *time = spectrum_time_snapshot();

    if (time->clock_valid) {
        put_timer_digit(clock_time, time->clock_hour);
        clock_time[2] = ':';
        put_timer_digit(clock_time + 3u, time->clock_minute);
    } else {
        memcpy(clock_time, "--:--", 5u);
    }
    clock_time[5] = ' ';
    clock_time[6] = '\0';

    if (clock_force_redraw || last_clock_line[0] == '\0') {
        memcpy(last_clock_line, clock_time, 7u);
        clock_line[0] = '[';
        clock_line[6] = ']';
        clock_line[7] = '\0';
        spectrum_render_clock(clock_line);
        clock_force_redraw = 0u;
        return;
    }
    if (spectrum_streq(clock_time, last_clock_line)) {
        return;
    }
    netchesszx_asm_timer_clock(clock_time);
}

#ifdef NETCHESSZX_SDCC_IY
void spectrum_gui_hide_menu(void) __naked
{
#asm
    ld a,(taboption_visible_addr)
    or a
    ret z
    xor a
    ld (taboption_visible_addr),a
    ld l,a
    jp _spectrum_render_menu
#endasm
}
#else
void spectrum_gui_hide_menu(void)
{
    if (menu_visible) {
        menu_visible = 0u;
        spectrum_render_menu(0u);
    }
}
#endif

void spectrum_gui_set_status_error(const char *text) NETCHESSZX_FASTCALL
{
    set_status(text, 1u);
}

void spectrum_gui_time_changed(uint8_t dirty) NETCHESSZX_FASTCALL
{
    if ((dirty & SPECTRUM_GUI_TIME_FORCE) != 0u) {
        /* Let the full paint finish the timer without a preceding roll blit. */
        timer_force_redraw = 1u;
        timer_roll_finish();
    }
    if ((dirty & SPECTRUM_GUI_TIME_GAME) != 0u) {
        render_game_timer_only();
    }
    if ((dirty & SPECTRUM_GUI_TIME_CLEAR_TURN) != 0u) {
        spectrum_gui_set_turn_label(SPECTRUM_GUI_TURN_CLEAR);
    }
    if ((dirty & SPECTRUM_GUI_TIME_CLOCK) != 0u) {
        render_clock_only();
    }
}

static void move_marker_render(uint8_t visible)
{
    char spec[5];

#if NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER
    if (about_visible != 0u) {
        return;
    }
#endif
    spec[0] = (char)move_marker_y;
    spec[1] = (char)move_marker_col;
    spec[2] = (char)NETCHESSZX_ATTR_TEXT;
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
#if NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER
    /* About covers the label row; closing it repaints the recorded mode. */
    if (about_visible != 1u)
#endif
    {
        spectrum_render_turn_label(mode);
    }
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
        spectrum_gui_hide_menu();
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
#if NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER
    /* FILE (2) only covers the board area; its prompts must reach the notice row. */
    if (about_visible == 1u) {
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

void spectrum_gui_present_tick(uint8_t dirty) NETCHESSZX_FASTCALL
{
    timer_roll_tick();

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
#if NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER
            if (about_visible != 1u) {
#endif
                spectrum_render_notice(notice_text);
#if NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER
            }
#endif
        }
    }

    spectrum_gui_time_changed(dirty);
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

uint8_t spectrum_gui_are_board_pieces_visible(void)
{
    return spectrum_gui_board_pieces_visible;
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
    timer_roll_finish();
    active_coord_valid = 0u;
    spectrum_gui_hide_menu();
#if NETCHESSZX_ABOUT_HIDES_SIDE_PANELS
    side_panels_visible = 0u;
#endif
    about_visible = 1u;
    if (!spectrum_render_about()) {
        about_visible = 0u;
        return 0u;
    }
    return 1u;
}

#ifndef NETCHESSZX_HIGH_SCRATCH
/* Classic includes this cache in its resident DATA (overlay_loader.asm). */
char spectrum_moves_header_shown[2][NETCHESS_PLAYER_NAME_SIZE];
#endif

uint8_t spectrum_gui_about_visible(void)
{
    return about_visible;
}

/* FILE (2) and statistics (3) only cover the board. */
uint8_t spectrum_gui_show_board_page(uint8_t page) NETCHESSZX_FASTCALL
{
    active_coord_valid = 0u;
    spectrum_gui_hide_menu();
    about_visible = page;
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

uint8_t spectrum_gui_handle_menu_key(uint8_t key) NETCHESSZX_FASTCALL
{
    return netchesszx_asm_menu_key(key);
}

static void wait_frames(uint8_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_uart_background_pump();
        spectrum_app_tick();
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
        square_spec[2] = (char)NETCHESSZX_ATTR_FLASH;
        spectrum_render_square_attr(square_spec);
        wait_frames(6u);
        render_square_from_board(row, col);
        wait_frames(6u);
    }
}
#ifdef NETCHESSZX_SPECTRANEXT
#define flash_square spectrum_gui_flash_square
#endif

#include "spectrum/ui/chess_move_view.inc"

void spectrum_gui_draw_board(void)
{
    timer_roll_phase = clock_roll_phase = 0u;
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
    /* The About band also covered the turn-label row. */
    spectrum_render_turn_label(move_marker_mode);
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    side_panels_visible = 1u;
    spectrum_info_show_game();
    spectrum_render_moves(move_lines);
    spectrum_render_chat(chat_lines);
    spectrum_gui_moves_header();
#else
    spectrum_gui_restore_side_panels();
#endif
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
    spectrum_gui_moves_header();
}
#endif

uint8_t spectrum_gui_side_panels_visible(void)
{
    return side_panels_visible;
}
