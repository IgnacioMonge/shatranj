#ifndef SPECTRUM_UI_EDITOR_H
#define SPECTRUM_UI_EDITOR_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

/* Live read-only view. */
extern const volatile uint8_t spectrum_editor_active;
/* Keep ordinary append-at-end typing resident. INPUT_EDIT owns cold edits. */
uint8_t spectrum_editor_append_hot(uint8_t key) NETCHESSZX_FASTCALL;

#endif
