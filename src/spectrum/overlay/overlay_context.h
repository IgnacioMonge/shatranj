#ifndef NETCHESSZX_SPECTRUM_OVERLAY_CONTEXT_H
#define NETCHESSZX_SPECTRUM_OVERLAY_CONTEXT_H

#include <stdint.h>

#include "spectrum/lowram_map.h"

#define SPECTRUM_OVERLAY_CONTEXT_SIZE NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE

/* Wire formats for byte-indexed overlay calls. Keep writer and reader using
   these offsets; abi-check guards the symbol/size, not per-entry fields. */
#define SPECTRUM_OVL_CTX_PTR_LO 0u
#define SPECTRUM_OVL_CTX_PTR_HI 1u

#define SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO 0u
#define SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI 1u
#define SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_LO 2u
#define SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_HI 3u
#define SPECTRUM_OVL_CTX_GUI_RENDER 4u

#define SPECTRUM_OVL_CTX_GUI_CHAT_WHO 0u
#define SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_LO 1u
#define SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_HI 2u

#define SPECTRUM_OVL_CTX_CONNECTION_LINE_LO 0u
#define SPECTRUM_OVL_CTX_CONNECTION_LINE_HI 1u
#define SPECTRUM_OVL_CTX_CONNECTION_MODE 2u

#define SPECTRUM_OVL_CTX_STATUS_PHASE 0u
#define SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE 53u
#define SPECTRUM_OVL_STATUS_LINE_SIZE (SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE + 1u)

/* Resident code writes this buffer, overlay entries mutate it, then resident
   code reads it again. Keep it volatile so C never caches values across the
   overlay call boundary. */
#define spectrum_overlay_context ((volatile uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR)

#endif
