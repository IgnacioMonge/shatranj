#ifndef NETCHESSZX_SPECTRUM_UI_INFO_PANEL_H
#define NETCHESSZX_SPECTRUM_UI_INFO_PANEL_H

#include <stdint.h>

void spectrum_info_show_game(void);
void spectrum_info_show_setup(void);
void spectrum_info_show_game_setup(void);
void spectrum_info_show_preflight(void);
uint8_t spectrum_info_panel_overlay_line(const char *line, uint8_t mode);
void spectrum_info_clear_tail(uint8_t row) __z88dk_fastcall;
void spectrum_info_line(const char *line) __z88dk_fastcall;
void spectrum_setup_board_swatches(uint8_t focus) __z88dk_fastcall;

#endif
