#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char host_move_log[224];
static char host_clock[8];

#define NETCHESSZX_SPECTRUM_LOWRAM_MAP_H
#define NETCHESSZX_LOWRAM_MOVE_LOG_ADDR ((uintptr_t)host_move_log)
#define NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR ((uintptr_t)host_clock)
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR 0u
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE 8u

uint16_t last_ply_seen;
uint8_t move_line_count;

#include "../../src/spectrum/overlay/gui_log_ovl.c"

static char render_events[8];
static uint8_t render_event_count;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

void gui_log_copy_move(char *out, const char *move)
{
    const char *clock = host_clock;
    uint8_t i;

    if (move == 0) {
        (void)spectrum_append_text(out, NETCHESSZX_UI_EVENT_RESTORED);
        return;
    }
    if (clock[0] == '\0') {
        clock = "--:-- ";
    }
    for (i = 0u; i < 6u; ++i) {
        *out++ = clock[i];
    }
    for (i = 0u; i < 7u && move[i] != '\0'; ++i) {
        *out++ = move[i];
    }
    for (; i < 7u; ++i) {
        *out++ = ' ';
    }
    *out = '\0';
}

uint16_t gui_log_parse_ply(const char *text)
{
    uint16_t value = 0u;

    while (*text >= '0' && *text <= '9') {
        value = (uint16_t)(value * 10u + (uint8_t)(*text - '0'));
        ++text;
    }
    return value;
}

void clear_move_line(char *line)
{
    memset(line, ' ', NETCHESSZX_MOVE_SLOT_SIZE);
}

void scroll_move_lines(char *base)
{
    memmove(base, base + NETCHESSZX_MOVE_SLOT_SIZE,
            (NETCHESSZX_MOVE_ROWS - 1u) * NETCHESSZX_MOVE_SLOT_SIZE);
}

char *move_line_at(char *line, uint8_t index)
{
    return line + ((uint16_t)index * NETCHESSZX_MOVE_SLOT_SIZE);
}

char *spectrum_append_text(char *dst, const char *src)
{
    while ((*dst++ = *src++) != '\0') {
    }
    return dst - 1;
}

void spectrum_gui_notify_persistent(const char *text) { (void)text; }
void spectrum_gui_notify_success(const char *text) { (void)text; }
void spectrum_gui_notify(const char *text, uint8_t is_error)
{
    (void)text;
    (void)is_error;
}

void spectrum_render_move_at(const char *line)
{
    (void)line;
    render_events[render_event_count++] = 'M';
}

void spectrum_render_moves_scroll(void)
{
    render_events[render_event_count++] = 'S';
}


static void seed_full_move_log(void)
{
    uint8_t row;

    memset(host_move_log, 0, sizeof(host_move_log));
    memcpy(host_clock, "12:34 ", 7u);
    for (row = 0u; row < NETCHESSZX_MOVE_ROWS; ++row) {
        char *line = move_line_at(host_move_log, row);

        clear_move_line(line);
        line[0] = (char)('0' + row);
        line[NETCHESSZX_MOVE_WHITE_TEXT_SIZE - 1u] = '\0';
        line[NETCHESSZX_MOVE_BLACK_OFFSET +
             NETCHESSZX_MOVE_BLACK_TEXT_SIZE] = '\0';
    }
    move_line_count = NETCHESSZX_MOVE_ROWS;
    last_ply_seen = 13u;
    render_event_count = 0u;
}

int main(void)
{
    char *last;
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};
    uint8_t i;

    seed_full_move_log();
    gui_log_add_move("14", "g8f6", 1u);
    ctx[SPECTRUM_OVL_CTX_GUI_RENDER] = 0x81u;
    check(gui_log_add_move_ovl(ctx) == 1u,
          "packed reserve request is handled");

    check(move_line_count == NETCHESSZX_MOVE_ROWS - 1u,
          "continued black move reserves the next white row");
    check(host_move_log[0] == '1', "reservation drops the oldest row");
    check(move_line_at(host_move_log, 5u)[0] == '6' &&
              strncmp(move_line_at(host_move_log, 5u) +
                          NETCHESSZX_MOVE_BLACK_OFFSET + 6u,
                      "g8f6", 4u) == 0,
          "latest black move survives the early scroll");
    last = move_line_at(host_move_log, NETCHESSZX_MOVE_ROWS - 1u);
    for (i = 0u; i < NETCHESSZX_MOVE_SLOT_SIZE; ++i) {
        check(last[i] == ' ', "reserved row is blank");
    }
    check(render_event_count == 3u && render_events[0] == 'M' &&
              render_events[1] == 'S' && render_events[2] == 'M',
          "black row renders, then scrolls, then clears the reserved row");

    render_event_count = 0u;
    gui_log_add_move("15", "e2e4", 1u);
    check(move_line_count == NETCHESSZX_MOVE_ROWS,
          "next white move consumes the reserved row");
    check(render_event_count == 1u && render_events[0] == 'M',
          "reserved white move does not scroll a second time");

    seed_full_move_log();
    gui_log_add_move("14", "g8f6", 1u);
    check(move_line_count == NETCHESSZX_MOVE_ROWS &&
              render_event_count == 1u && render_events[0] == 'M',
          "black move without reservation keeps the full history in place");

    memset(host_move_log, 0, sizeof(host_move_log));
    move_line_count = 0u;
    last_ply_seen = 0u;
    render_event_count = 0u;
    gui_log_add_move("2", 0, 1u);
    last = move_line_at(host_move_log, 0u);
    check(last[0] == '\0',
          "black-only row terminates the empty white column");
    check(strcmp(last + NETCHESSZX_MOVE_BLACK_OFFSET,
                 NETCHESSZX_UI_EVENT_RESTORED) == 0,
          "black-only restored marker remains in the black column");
    check(move_line_count == 1u && render_event_count == 1u &&
              render_events[0] == 'M',
          "black-only restored marker renders one row");

    memset(host_move_log, 0, sizeof(host_move_log));
    memcpy(host_clock, "--:-- ", 7u);
    move_line_count = 0u;
    last_ply_seen = 0u;
    gui_log_add_move("1", "e2e4", 0u);
    gui_log_add_move("2", "e7e8q", 0u);
    gui_log_add_move("3", "g8f6", 0u);
    ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO] = 3u;
    ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI] = 0u;
    ctx[SPECTRUM_OVL_CTX_GUI_RENDER] = 0u;
    check(gui_log_remove_last_move_ovl(ctx) == 1u, "takeback removes the ply");
    check(move_line_count == 1u && last_ply_seen == 2u,
          "loaded history ends on the second ply");
    check(strncmp(host_move_log, "--:-- e2e4", 10) == 0, "white cell kept");
    check(strncmp(host_move_log + 18, "--:-- e7e8q", 11) == 0,
          "promotion cell kept");
    check(strstr(host_move_log, "g8f6") == 0, "undone move is gone");
    gui_log_add_move("3", "d2d4", 0u);
    check(move_line_count == 2u && last_ply_seen == 3u,
          "next ply follows the loaded history");
    check(strncmp(host_move_log + 32, "--:-- d2d4", 10) == 0,
          "next move is on the following row");
    check(strncmp(host_move_log, "--:-- e2e4", 10) == 0,
          "loaded white move stays in place");

    puts("gui log tests passed");
    return 0;
}
