#ifndef NETCHESSZX_SPECTRUM_OVERLAY_API_H
#define NETCHESSZX_SPECTRUM_OVERLAY_API_H

#include <stdint.h>
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/render_status.h"

#if !defined(__SDCC) || !defined(NETCHESSZX_SDCC_IY)
#error "Shatranj Spectrum overlays must be built with the SDCC/IY ABI."
#endif

#define SPECTRUM_OVL_BLOCK_SIZE 2048u

extern uint8_t overlay_code_slot[];
extern uint8_t spectrum_overlay_loaded_id;

uint8_t spectrum_overlay_exec(uint8_t ovl_id, uint8_t entry_id);
uint8_t spectrum_overlay_exec_cached(uint8_t ovl_id, uint8_t entry_id);

void spectrum_frame_wait(void);
void spectrum_gui_tick(void);
uint8_t spectrum_key_poll(void);
void spectrum_uart_flush(uint16_t frames) __z88dk_fastcall;
void spectrum_uart_send_string(const char *s) __z88dk_fastcall;
void spectrum_uart_send_bytes(const uint8_t *data, uint16_t len);
void spectrum_uart_send_crlf(void);
uint8_t spectrum_uart_ready(void);
uint8_t spectrum_uart_read(void);

char *spectrum_append_text(char *dst, const char *src);
char *spectrum_append_u16(char *dst, uint16_t value);

void spectrum_gui_set_status(const char *text) __z88dk_fastcall;
void spectrum_gui_set_status_error(const char *text) __z88dk_fastcall;
void spectrum_gui_draw_status(void);
void spectrum_gui_set_connected(uint8_t connected) __z88dk_fastcall;
void spectrum_gui_notify(const char *text, uint8_t is_error);
void spectrum_gui_notify_success(const char *text) __z88dk_fastcall;
void spectrum_gui_add_move(const char *ply, const char *move);
void spectrum_gui_add_chat(char who, const char *text);
void spectrum_gui_prepare_move(const char *move) __z88dk_fastcall;
void spectrum_gui_apply_move(const char *move) __z88dk_fastcall;
void spectrum_gui_redraw_board_squares(void);
void spectrum_gui_redraw_square(uint8_t row, uint8_t col);
void spectrum_board_view_redraw_square(uint8_t row, uint8_t col);
extern uint8_t spectrum_board_view_flipped;

void spectrum_render_board(const char *board) __z88dk_fastcall;
void spectrum_render_status(const char *text) __z88dk_fastcall;
void spectrum_render_connection(uint8_t connected) __z88dk_fastcall;
void spectrum_render_moves(const char *moves) __z88dk_fastcall;
void spectrum_render_move_at(const char *line) __z88dk_fastcall;
void spectrum_render_moves_scroll(void);
void spectrum_render_chat(const char *chat) __z88dk_fastcall;
void spectrum_render_chat_at(const char *line) __z88dk_fastcall;
void spectrum_render_chat_scroll(void);
void spectrum_render_square(const char *spec) __z88dk_fastcall;
void spectrum_render_square_attr(const char *spec) __z88dk_fastcall;
void spectrum_render_square_mark(const char *spec) __z88dk_fastcall;

void spectrum_board_reset(void);
const char *spectrum_board_cells(void);
char spectrum_board_cell(uint8_t row, uint8_t col);
uint8_t spectrum_board_apply_trusted_move(const char *move) __z88dk_fastcall;

uint8_t spectrum_net_runtime_clock_ready(void);
void spectrum_net_runtime_set_clock(uint8_t hour,
                                    uint8_t minute,
                                    uint8_t second);

#endif
