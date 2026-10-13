#ifndef SPECTRUM_UI_INTERACTION_H
#define SPECTRUM_UI_INTERACTION_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define UX_MOVE_FAILED 0u
#define UX_MOVE_REJECTED 1u
#define UX_MOVE_ACCEPTED 2u

/* Resident interaction owner. No writable state crosses this boundary. */
/* Row in low byte, column in high byte; caller supplies a valid 8x8 square. */
void cursor_home(uint16_t square) NETCHESSZX_FASTCALL;
void cursor_hide(void);
void cursor_show(void);
void cursor_move(uint8_t key) NETCHESSZX_FASTCALL;
void cursor_refresh(void);
void cursor_request_hints(void);
uint8_t spectrum_ux_board_activate(void);

#ifdef NETCHESSZX_HOST_TEST
/* Observation only; tests drive state changes through the production operations. */
uint16_t spectrum_ux_cursor_position(void);
uint16_t spectrum_ux_selected_position(void);
#endif

#endif
