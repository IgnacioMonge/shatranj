#ifndef NETCHESSZX_SPECTRUM_CONFIG_SETUP_MENU_H
#define NETCHESSZX_SPECTRUM_CONFIG_SETUP_MENU_H

#include <stdint.h>

void netchesszx_setup_update_room_code(void);
void netchesszx_setup_render_edit_line(uint8_t row,
                                       uint8_t flags,
                                       const char *text,
                                       uint8_t max_len);
uint16_t netchesszx_setup_compute_visible(uint16_t defined_mask) __z88dk_fastcall;
uint8_t netchesszx_setup_step_row(uint8_t row,
                                  uint8_t next,
                                  uint16_t visible_mask);
void netchesszx_setup_paint_attrs(uint8_t values,
                                  uint16_t visible_mask,
                                  uint16_t defined_mask,
                                  uint8_t cursor,
                                  uint8_t focus_values,
                                  uint8_t piece_set);
void netchesszx_setup_render_rows(uint8_t values,
                                  uint16_t visible_mask,
                                  uint16_t dirty_mask);
uint8_t netchesszx_setup_room_editable(void);
uint8_t netchesszx_setup_room_backspace(void);
uint8_t netchesszx_setup_room_append(uint8_t key) __z88dk_fastcall;
uint8_t netchesszx_setup_move_focus(uint8_t key) __z88dk_fastcall;
uint8_t netchesszx_setup_validate_ip(const char *text) __z88dk_fastcall;

#endif
