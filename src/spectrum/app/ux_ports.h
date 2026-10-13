#ifndef SPECTRUM_APP_UX_PORTS_H
#define SPECTRUM_APP_UX_PORTS_H

#include "spectrum/ui/interaction.h"

/* Application facts and synchronous intents for valid 8x8 coordinates.
   No application/session storage or implementation is visible to interaction. */
uint8_t spectrum_ux_can_navigate(void);
uint8_t spectrum_ux_local_turn(void);
uint16_t spectrum_ux_move_pending(void);
uint8_t spectrum_ux_selectable(uint8_t row, uint8_t col);
void spectrum_ux_selection_notice(uint16_t square) NETCHESSZX_FASTCALL;
void movement_hints_clear(void);
/* Row in low byte, column in high byte; passed in HL on Z80. */
void movement_hints_show(uint16_t square) NETCHESSZX_FASTCALL;
/* Squares use the same row-low/column-high encoding; no piece or wire format. */
uint8_t spectrum_ux_move_intent(uint16_t from, uint16_t to);

#endif
