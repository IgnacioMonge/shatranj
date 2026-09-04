#include "spectrum/overlay/overlay_api.h"
#include "spectrum/lowram_map.h"
#include "spectrum/ui/layout.h"
#include "common/ui_messages.h"
#include "common/chess/move_coords.h"


#define move_lines ((char *)NETCHESSZX_LOWRAM_MOVE_LOG_ADDR)
#define chat_lines ((char *)NETCHESSZX_LOWRAM_CHAT_LOG_ADDR)

extern uint16_t last_ply_seen;
extern uint8_t move_line_count;
extern uint8_t chat_line_count;
#ifdef NETCHESSZX_SPECTRANEXT
void spectrum_uart_background_pump(void);
void spectrum_gui_sync_board_coords(void);
void spectrum_gui_set_board_pieces_visible(uint8_t visible) __z88dk_fastcall;
void spectrum_gui_hide_board_pieces(void);
void spectrum_gui_flash_square(uint8_t row, uint8_t col);
extern uint8_t spectrum_gui_board_pieces_visible;
extern uint8_t spectrum_gui_about_visible_state;
extern uint8_t spectrum_gui_side_panels_visible_state;
#endif

char chat_clean_char(uint8_t c) __z88dk_fastcall;
uint8_t chat_word_len(const char *text) __z88dk_fastcall;
void chat_copy_clock_line(char *line) __z88dk_fastcall;
uint16_t gui_log_parse_ply(const char *text) __z88dk_fastcall;
void clear_move_line(char *line) __z88dk_fastcall;
void clear_log_line(char *line) __z88dk_fastcall;
void scroll_move_lines(char *base) __z88dk_fastcall;
void scroll_chat_lines(char *base) __z88dk_fastcall;
char *move_line_at(char *line, uint8_t index);
char *log_line_at(char *line, uint8_t index);

#ifdef NETCHESSZX_SPECTRANEXT
static void gui_log_animation_wait(uint8_t frames)
{
    while (frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_uart_background_pump();
        spectrum_gui_tick();
        spectrum_uart_background_pump();
    }
}

static void gui_log_reveal_board(uint8_t step)
{
    uint8_t i;

    for (i = 0u; i < 8u; ++i) {
        spectrum_gui_redraw_square(0u, i);
        spectrum_gui_redraw_square(7u, (uint8_t)(7u - i));
        gui_log_animation_wait(step);
    }
    for (i = 0u; i < 8u; ++i) {
        spectrum_gui_redraw_square(1u, (uint8_t)(7u - i));
        spectrum_gui_redraw_square(6u, i);
        gui_log_animation_wait(step);
    }
}

uint8_t gui_log_animate_board_ovl(uint8_t *ctx) __z88dk_fastcall
{
    (void)ctx;
    spectrum_gui_sync_board_coords();
    if (spectrum_gui_board_pieces_visible) {
        spectrum_gui_hide_board_pieces();
    }
    spectrum_gui_set_board_pieces_visible(1u);
    gui_log_reveal_board(1u);
    return 1u;
}

uint8_t gui_log_morph_board_ovl(uint8_t *ctx) __z88dk_fastcall
{
    (void)ctx;
    if (!spectrum_gui_board_pieces_visible) {
        return gui_log_animate_board_ovl(ctx);
    }
    spectrum_gui_sync_board_coords();
    gui_log_reveal_board(0u);
    return 1u;
}

uint8_t gui_log_restore_side_panels_ovl(uint8_t *ctx) __z88dk_fastcall
{
    (void)ctx;
    spectrum_gui_about_visible_state = 0u;
    spectrum_gui_side_panels_visible_state = 1u;
    spectrum_info_show_game();
    spectrum_render_moves(move_lines);
    spectrum_render_chat(chat_lines);
    return 1u;
}

uint8_t gui_log_apply_move_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *move = (const char *)((uint16_t)ctx[0] |
                                      ((uint16_t)ctx[1] << 8));
    const char *board = (const char *)NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR;
    uint16_t coords;
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    char piece;

    if (spectrum_gui_about_visible_state) {
        return 1u;
    }
    coords = netchesszx_move_parse_coords(move);
    if (coords == NETCHESSZX_MOVE_COORDS_INVALID) {
        return 1u;
    }
    from_col = NETCHESSZX_MOVE_FROM_INDEX(coords);
    to_col = NETCHESSZX_MOVE_TO_INDEX(coords);
    from_row = (uint8_t)(from_col >> 3);
    to_row = (uint8_t)(to_col >> 3);
    from_col &= 7u;
    to_col &= 7u;

    spectrum_gui_redraw_square(from_row, from_col);
    spectrum_gui_redraw_square(to_row, to_col);
    spectrum_gui_flash_square(to_row, to_col);

    piece = board[(uint8_t)((to_row << 3) + to_col)];
    if ((piece == 'P' || piece == 'p') && from_col != to_col) {
        spectrum_gui_redraw_square(from_row, to_col);
    }
    if ((piece == 'K' || piece == 'k') &&
        from_row == to_row && from_col == 4u) {
        if (to_col == 6u) {
            spectrum_gui_redraw_square(from_row, 7u);
            spectrum_gui_redraw_square(from_row, 5u);
        } else if (to_col == 2u) {
            spectrum_gui_redraw_square(from_row, 0u);
            spectrum_gui_redraw_square(from_row, 3u);
        }
    }
    return 1u;
}
#endif

static char *new_move_line(uint8_t index, uint8_t render)
{
    char *base = move_lines;

    if (index >= NETCHESSZX_MOVE_ROWS) {
        scroll_move_lines(base);
        if (render) {
            spectrum_render_moves_scroll();
        }
        index = NETCHESSZX_MOVE_ROWS - 1u;
    }
    base = move_line_at(base, index);
    clear_move_line(base);
    return base;
}

static void gui_log_reserve_next_move_line(uint8_t render)
{
    char *line;

    if (move_line_count < NETCHESSZX_MOVE_ROWS) {
        return;
    }
    line = new_move_line(move_line_count, render);
    --move_line_count;
    if (render) {
        spectrum_render_move_at(line);
    }
}

static char *new_chat_line(uint8_t index, uint8_t render)
{
    char *base = chat_lines;

    if (index >= NETCHESSZX_CHAT_ROWS) {
        scroll_chat_lines(base);
        if (render) {
            spectrum_render_chat_scroll();
        }
        index = NETCHESSZX_CHAT_ROWS - 1u;
    }
    base = log_line_at(base, index);
    clear_log_line(base);
    return base;
}

static void gui_log_copy_move(char *out, const char *move)
{
    const char *clock = (const char *)NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR;
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

static void gui_log_add_move(const char *ply, const char *move, uint8_t render)
{
    char *line;
    char *out;
    uint16_t ply_num;
    uint8_t is_black;

    ply_num = gui_log_parse_ply(ply);
    if (ply_num == 0u) {
        ++last_ply_seen;
        ply_num = last_ply_seen;
    } else {
        last_ply_seen = ply_num;
    }
    is_black = (uint8_t)((ply_num & 1u) == 0u);

    if (is_black) {
        if (move_line_count == 0u) {
            line = new_move_line(0u, render);
            line[0] = '\0';
            move_line_count = 1u;
        } else {
            line = move_line_at(move_lines, (uint8_t)(move_line_count - 1u));
        }
        out = line + NETCHESSZX_MOVE_BLACK_OFFSET;
    } else {
        line = new_move_line(move_line_count, render);
        if (move_line_count < NETCHESSZX_MOVE_ROWS) {
            ++move_line_count;
        }
        out = line;
    }
    gui_log_copy_move(out, move);
    if (render) {
        spectrum_render_move_at(line);
    }
}

static void gui_log_add_chat(char who, const char *text, uint8_t render)
{
    char *line;
    uint8_t first = 1u;
    uint8_t col;

    do {
        line = new_chat_line(chat_line_count, render);
        if (chat_line_count < NETCHESSZX_CHAT_ROWS) {
            ++chat_line_count;
        }
        if (first) {
            line[0] = who;
            chat_copy_clock_line(line);
            col = NETCHESSZX_CHAT_TEXT_OFFSET;
            first = 0u;
        } else {
            line[0] = '\0';
            col = 1u;
        }

        while (*text == ' ') {
            ++text;
        }
        while (*text != '\0' && col < NETCHESSZX_CHAT_TEXT_LIMIT) {
            uint8_t word_len = chat_word_len(text);
            uint8_t add_space;

            if (line[0] != '\0') {
                add_space = (uint8_t)(col > NETCHESSZX_CHAT_TEXT_OFFSET);
            } else {
                add_space = (uint8_t)(col > 1u);
            }

            if (word_len == 0u) {
                ++text;
                continue;
            }
            if ((uint8_t)(word_len + add_space) >
                (uint8_t)(NETCHESSZX_CHAT_TEXT_LIMIT - col)) {
                if (!add_space) {
                    while (*text != '\0' && *text != ' ' &&
                           col < NETCHESSZX_CHAT_TEXT_LIMIT) {
                        line[col++] = chat_clean_char((uint8_t)*text++);
                    }
                }
                break;
            }
            if (add_space) {
                line[col++] = ' ';
            }
            while (word_len-- != 0u && col < NETCHESSZX_CHAT_TEXT_LIMIT) {
                line[col++] = chat_clean_char((uint8_t)*text++);
            }
            while (*text == ' ') {
                ++text;
            }
        }
        line[NETCHESSZX_CHAT_TEXT_LIMIT] = '\0';
        if (render) {
            spectrum_render_chat_at(line);
        }
    } while (*text != '\0');
}

uint8_t gui_log_add_move_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t render = ctx[SPECTRUM_OVL_CTX_GUI_RENDER];
    const char *ply;
    const char *move;

    if (render & 0x80u) {
        gui_log_reserve_next_move_line((uint8_t)(render & 1u));
        return 1u;
    }
    ply =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI] << 8));
    move =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_HI] << 8));

    gui_log_add_move(ply, move, render);
    return 1u;
}

static void gui_log_clear_black_move(char *line)
{
    uint8_t i;

    line += NETCHESSZX_MOVE_BLACK_OFFSET;
    for (i = 0u; i < NETCHESSZX_MOVE_BLACK_TEXT_SIZE; ++i) {
        *line++ = ' ';
    }
    *line = '\0';
}

uint8_t gui_log_remove_last_move_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint16_t ply = (uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO] |
                   ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI] << 8);
    char *line;

    if (ply == 0u || move_line_count == 0u) {
        return 1u;
    }

    if ((ply & 1u) == 0u) {
        line = move_line_at(move_lines, (uint8_t)(move_line_count - 1u));
        gui_log_clear_black_move(line);
    } else {
        --move_line_count;
        line = move_line_at(move_lines, move_line_count);
        clear_move_line(line);
    }
    last_ply_seen = (uint16_t)(ply - 1u);
    if (ctx[SPECTRUM_OVL_CTX_GUI_RENDER] != 0u) {
        spectrum_render_move_at(line);
    }
    return 1u;
}

uint8_t gui_log_add_chat_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *text =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_HI] << 8));

    gui_log_add_chat((char)ctx[SPECTRUM_OVL_CTX_GUI_CHAT_WHO], text,
                     (uint8_t)(ctx[SPECTRUM_OVL_CTX_GUI_RENDER] != 0u));
    return 1u;
}
