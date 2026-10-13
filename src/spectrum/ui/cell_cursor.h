#ifndef SPECTRUM_CELL_CURSOR_H
#define SPECTRUM_CELL_CURSOR_H

#include "common/space/space.h"

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

/* Private cursor owner. The caller supplies a valid home in the selected
   space; presentation chooses neighbors. No selection or game admission. */
void spectrum_cell_cursor_home(space_cell_t cell) NETCHESSZX_FASTCALL;
space_cell_t spectrum_cell_cursor_current(void);
space_cell_t spectrum_cell_cursor_move(uint8_t key) NETCHESSZX_FASTCALL;

#endif
