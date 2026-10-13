#ifndef SPECTRUM_EDITOR_FIXTURE_H
#define SPECTRUM_EDITOR_FIXTURE_H

#include "spectrum/ui/editor.h"

/* Host overlay storage only. Production storage is private ASM resident data. */
uint8_t local_input_len;
uint8_t local_input_cursor;
uint8_t local_input_mode;
uint8_t input_history_count;
uint8_t input_history_pos;
#define spectrum_editor_active local_input_mode

#endif
