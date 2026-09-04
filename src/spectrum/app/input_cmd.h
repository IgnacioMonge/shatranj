#ifndef NETCHESSZX_SPECTRUM_APP_INPUT_CMD_H
#define NETCHESSZX_SPECTRUM_APP_INPUT_CMD_H

#include <stdint.h>


void netchesszx_input_edit_render_overlay(void);
void netchesszx_input_edit_begin_empty_overlay(void);
void netchesszx_input_edit_stop_clear_overlay(void);
void netchesszx_input_edit_key_overlay(uint8_t key) __z88dk_fastcall;
void netchesszx_input_edit_history_add_overlay(const char *text) __z88dk_fastcall;

#endif
