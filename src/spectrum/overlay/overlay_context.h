#ifndef NETCHESSZX_SPECTRUM_OVERLAY_CONTEXT_H
#define NETCHESSZX_SPECTRUM_OVERLAY_CONTEXT_H

#include <stdint.h>

#include "spectrum/lowram_map.h"

#define SPECTRUM_OVERLAY_CONTEXT_SIZE NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE

/* Wire formats for byte-indexed overlay calls. Keep writer and reader using
   these offsets; abi-check guards the symbol/size, not per-entry fields. */
#define SPECTRUM_OVL_CTX_PTR_LO 0u
#define SPECTRUM_OVL_CTX_PTR_HI 1u

/* MQTT TX consumes the payload during the synchronous overlay call. Route and
   retain share one byte so no mutable request state outlives that call. */
#define NETCHESSZX_MQTT_TX_CTX_PAYLOAD_LO 0u
#define NETCHESSZX_MQTT_TX_CTX_PAYLOAD_HI 1u
#define NETCHESSZX_MQTT_TX_CTX_FLAGS 2u
#define NETCHESSZX_MQTT_TX_ROUTE_MASK 0x0fu
#define NETCHESSZX_MQTT_TX_CLEAR_META 0x20u
#define NETCHESSZX_MQTT_TX_OFFLINE 0x40u
#define NETCHESSZX_MQTT_TX_RETAIN 0x80u

#define NETCHESSZX_MQTT_CONNECT_CTX_FLAGS 0u

#define SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO 0u
#define SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI 1u
#define SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_LO 2u
#define SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_HI 3u
/* ADD_MOVE packs render in bit 0; bit 7 requests a row reservation. */
#define SPECTRUM_OVL_CTX_GUI_RENDER 4u

#define SPECTRUM_OVL_CTX_GUI_CHAT_WHO 0u
#define SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_LO 1u
#define SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_HI 2u

#define SPECTRUM_OVL_CTX_GUI_MSG_ID 0u
#define SPECTRUM_OVL_CTX_GUI_MSG_KIND 1u

#define SPECTRUM_OVL_CTX_INPUT_KEY 0u
#define SPECTRUM_OVL_CTX_INPUT_CMD_TEXT_LO 0u
#define SPECTRUM_OVL_CTX_INPUT_CMD_TEXT_HI 1u
#define SPECTRUM_OVL_CTX_INPUT_CMD_ARG_LO 2u
#define SPECTRUM_OVL_CTX_INPUT_CMD_ARG_HI 3u
#define SPECTRUM_OVL_CTX_SAVELOAD_NAME_LO 0u
#define SPECTRUM_OVL_CTX_SAVELOAD_NAME_HI 1u
#define SPECTRUM_OVL_CTX_SAVELOAD_BUF_LO 2u
#define SPECTRUM_OVL_CTX_SAVELOAD_BUF_HI 3u
#define SPECTRUM_OVL_CTX_SAVELOAD_RESULT 4u
#define SPECTRUM_OVL_CTX_RECOVERY_BUF_LO SPECTRUM_OVL_CTX_SAVELOAD_BUF_LO
#define SPECTRUM_OVL_CTX_RECOVERY_BUF_HI SPECTRUM_OVL_CTX_SAVELOAD_BUF_HI
#define SPECTRUM_OVL_CTX_RECOVERY_OUT_LO SPECTRUM_OVL_CTX_SAVELOAD_NAME_LO
#define SPECTRUM_OVL_CTX_RECOVERY_OUT_HI SPECTRUM_OVL_CTX_SAVELOAD_NAME_HI
#define SPECTRUM_OVL_CTX_RECOVERY_RESULT SPECTRUM_OVL_CTX_SAVELOAD_RESULT
#define SPECTRUM_OVL_CTX_RECOVERY_FLAGS 5u
/* Checkpoint link flags, stored in the SRC2 record (rcv_layout.inc). */
#define SPECTRUM_OVL_CTX_RECOVERY_LINK 6u
#define SPECTRUM_RECOVERY_LINK_PARTIAL 0x01u
#define SPECTRUM_RECOVERY_LINK_UNLINKED 0x02u
#define SPECTRUM_RECOVERY_LINK_HOST 0x04u
#define SPECTRUM_RECOVERY_LINK_BLACK 0x08u
#define SPECTRUM_RECOVERY_LINK_PLAYED 0x10u
#define SPECTRUM_OVL_CTX_JOURNAL_REC_LO 0u
#define SPECTRUM_OVL_CTX_JOURNAL_REC_HI 1u
#define SPECTRUM_OVL_CTX_JOURNAL_FD 2u
/* Append-only: nonzero until the statistics prologue has executed. */
#define SPECTRUM_OVL_CTX_JOURNAL_CAPTURE_PENDING 7u

#define SPECTRUM_OVL_CTX_RESTORE_SNAP_LO 0u
#define SPECTRUM_OVL_CTX_RESTORE_SNAP_HI 1u
#define SPECTRUM_OVL_CTX_RESTORE_META_LO 2u
#define SPECTRUM_OVL_CTX_RESTORE_META_HI 3u
#define SPECTRUM_OVL_CTX_RESTORE_TEXT_LO 4u
#define SPECTRUM_OVL_CTX_RESTORE_TEXT_HI 5u
#define SPECTRUM_OVL_CTX_RESTORE_CHUNK 6u

#define SPECTRUM_OVL_CTX_HINTS_BOARD_LO 0u
#define SPECTRUM_OVL_CTX_HINTS_BOARD_HI 1u
#define SPECTRUM_OVL_CTX_HINTS_SIDE_TO_MOVE 2u
#define SPECTRUM_OVL_CTX_HINTS_SQUARE 3u
#define SPECTRUM_OVL_CTX_HINTS_CASTLE 4u
#define SPECTRUM_OVL_CTX_HINTS_EP 5u

#define SPECTRUM_OVL_SAVELOAD_OK 0u
#define SPECTRUM_OVL_SAVELOAD_ERR_NAME 1u
#define SPECTRUM_OVL_SAVELOAD_ERR_OPEN 2u
#define SPECTRUM_OVL_SAVELOAD_ERR_IO 3u
#define SPECTRUM_OVL_SAVELOAD_ERR_DATA 4u

#define SPECTRUM_OVL_CTX_FILEUI_KEY 0u
#define SPECTRUM_OVL_CTX_FILEUI_SEL 1u
#define SPECTRUM_OVL_CTX_FILEUI_ACTION 3u
#define SPECTRUM_OVL_CTX_FILEUI_NAME_LO 4u
#define SPECTRUM_OVL_CTX_FILEUI_NAME_HI 5u

#define SPECTRUM_OVL_FILEUI_ACT_NONE 0u
#define SPECTRUM_OVL_FILEUI_ACT_LOAD 1u
#define SPECTRUM_OVL_FILEUI_ACT_SAVE 2u
#define SPECTRUM_OVL_FILEUI_ACT_ERASE 3u
#define SPECTRUM_OVL_FILEUI_ACT_FAIL 0xffu
#define SPECTRUM_OVL_CTX_PREFLIGHT_OK 0u
#define SPECTRUM_OVL_CTX_PREFLIGHT_RETRY 1u
/* Warm return to Setup: do the ESP work without repainting the cold-boot
   panel, and skip the steps a reconnect does not need. */
#define SPECTRUM_OVL_CTX_PREFLIGHT_QUIET 2u
#define SPECTRUM_OVL_PREFLIGHT_RETRY_DEFAULT 0u
#define SPECTRUM_OVL_PREFLIGHT_RETRY_AGAIN 1u

#define SPECTRUM_OVL_CTX_STATUS_PHASE 0u
#define SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE 53u
#define SPECTRUM_OVL_STATUS_LINE_SIZE (SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE + 1u)

#define SPECTRUM_OVL_CTX_SETUP_KEY 0u
#define SPECTRUM_OVL_CTX_SETUP_FORCE_LO 1u
#define SPECTRUM_OVL_CTX_SETUP_FORCE_HI 2u
#define SPECTRUM_OVL_CTX_SETUP_CLEAR_FROM 3u
#define SPECTRUM_OVL_CTX_SETUP_ACTION 4u
#define SPECTRUM_OVL_CTX_SETUP_NOTICE 5u
#define SPECTRUM_OVL_CTX_SETUP_FLAGS 6u

#define SPECTRUM_OVL_SETUP_FLAG_RENDER 0x01u
#define SPECTRUM_OVL_SETUP_FLAG_PAINT 0x02u
#define SPECTRUM_OVL_SETUP_FLAG_EDIT 0x04u
#define SPECTRUM_OVL_SETUP_FLAG_SUPPRESS 0x08u

#define SPECTRUM_OVL_SETUP_ACTION_NONE 0u
#define SPECTRUM_OVL_SETUP_ACTION_BOARD 1u
#define SPECTRUM_OVL_SETUP_ACTION_SET 2u
#define SPECTRUM_OVL_SETUP_ACTION_START 3u
#define SPECTRUM_OVL_SETUP_ACTION_SAVE 4u

#define SPECTRUM_OVL_SETUP_NOTICE_NONE 0u
#define SPECTRUM_OVL_SETUP_NOTICE_SELECT 1u
#define SPECTRUM_OVL_SETUP_NOTICE_PENDING 2u
#define SPECTRUM_OVL_SETUP_NOTICE_BAD_IP 3u
#define SPECTRUM_OVL_SETUP_NOTICE_BAD_ROOM 4u
#define SPECTRUM_OVL_SETUP_NOTICE_BAD_PORT 5u
#define SPECTRUM_OVL_SETUP_NOTICE_BAD_TIMEZONE 6u

/* Resident code writes this buffer, overlay entries mutate it, then resident
   code reads it again. Keep it volatile so C never caches values across the
   overlay call boundary. */
#ifdef NETCHESSZX_HOST_SESSION_TEST
#define spectrum_overlay_context netchesszx_host_overlay_context
#else
#define spectrum_overlay_context \
    ((volatile uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR)
#endif

/* Store a 16-bit field whose bytes sit at lo and hi == lo + 1: one word
   store on the Z80 (little-endian), two byte stores elsewhere. */
#ifdef NETCHESSZX_SDCC_IY
#define SPECTRUM_OVL_CTX_SET16(lo, hi, value) do { \
    (void)sizeof(char[((hi) == (lo) + 1u) ? 1 : -1]); \
    *(volatile uint16_t *)(NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR + (lo)) = \
        (uint16_t)(value); \
} while (0)
#else
#define SPECTRUM_OVL_CTX_SET16(lo, hi, value) do { \
    uint16_t spectrum_ovl_ctx_word = (uint16_t)(value); \
    (void)sizeof(char[((hi) == (lo) + 1u) ? 1 : -1]); \
    spectrum_overlay_context[(lo)] = (uint8_t)spectrum_ovl_ctx_word; \
    spectrum_overlay_context[(hi)] = (uint8_t)(spectrum_ovl_ctx_word >> 8); \
} while (0)
#endif

#endif
