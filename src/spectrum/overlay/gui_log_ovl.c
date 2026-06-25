#include "spectrum/overlay/overlay_api.h"
#include "spectrum/lowram_map.h"
#include "spectrum/ui/layout.h"


#define move_lines ((char *)NETCHESSZX_LOWRAM_MOVE_LOG_ADDR)
#define chat_lines ((char *)NETCHESSZX_LOWRAM_CHAT_LOG_ADDR)

extern uint16_t last_ply_seen;
extern uint8_t move_line_count;
extern uint8_t chat_line_count;

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

    if (clock[0] == '\0') {
        clock = "--:-- ";
    }
    for (i = 0u; i < 6u; ++i) {
        *out++ = clock[i];
    }
    for (i = 0u; i < 7u && move[i] != '\0'; ++i) {
        *out++ = move[i];
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
            line[0] = (who != '\0') ? who : '?';
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
    const char *ply =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI] << 8));
    const char *move =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_HI] << 8));

    gui_log_add_move(ply, move,
                     (uint8_t)(ctx[SPECTRUM_OVL_CTX_GUI_RENDER] != 0u));
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

uint8_t connection_panel_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *line =
        (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_CONNECTION_LINE_LO] |
                       ((uint16_t)ctx[SPECTRUM_OVL_CTX_CONNECTION_LINE_HI] << 8));

    if (ctx[SPECTRUM_OVL_CTX_CONNECTION_MODE] != NETCHESSZX_INFO_MODE_KEEP) {
        spectrum_info_show_preflight();
    }
    spectrum_info_line(line);
    spectrum_frame_wait();
    spectrum_gui_tick();
    return 1u;
}
