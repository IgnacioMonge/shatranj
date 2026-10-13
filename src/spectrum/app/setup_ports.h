#ifndef SPECTRUM_APP_SETUP_PORTS_H
#define SPECTRUM_APP_SETUP_PORTS_H
#include <stdint.h>
#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

/* Synchronous effects requested by Setup, not access to application storage. */
extern const char spectrum_app_overlay_load_error[];
uint8_t spectrum_setup_start(uint8_t key) NETCHESSZX_FASTCALL;
uint8_t spectrum_setup_save_config(void);
void spectrum_setup_preview_piece_set(uint8_t set) NETCHESSZX_FASTCALL;
void spectrum_setup_apply_timezone(int8_t previous) NETCHESSZX_FASTCALL;
#endif
