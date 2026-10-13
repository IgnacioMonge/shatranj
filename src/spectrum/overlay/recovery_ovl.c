#include <stdint.h>

#include "common/savegame/savegame_format.h"
#include "spectrum/app/time_view.h"
#include "spectrum/board/board.h"
#include "spectrum/lowram_map.h"
#include "spectrum/overlay/overlay_api.h"
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/saveload/saveload.h"
#include "spectrum/session/identity.h"
#include "spectrum/session/state.h"

/* The checkpoint record is assembled in the overlay scratch (see
   asm/overlay/recovery/rcv_layout.inc): encode its snapshot in place. */
#ifndef RECOVERY_RECORD_SNAPSHOT
#define RECOVERY_RECORD_SNAPSHOT \
    ((char *)(NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR + 6u))
#endif
/* CAPTURE reads the live position itself: spectrum_board_snapshot_save()
   dispatches BOARD, and a nested overlay call would do nothing. */
#ifndef RECOVERY_BOARD_CELLS
#define RECOVERY_BOARD_CELLS \
    ((const char *)NETCHESSZX_LOWRAM_CHESS_BOARD_ADDR)
#endif
/* The position is staged at scratch +73, clear of the record at +6. On
   Classic and SpectraNext that is RCV_TMP, which WRITE only uses after the
   encode; Next's store keeps RCV_TMP in overlay BSS and never touches this
   scratch (asm/overlay/recovery/rcv_layout.inc). */
#ifndef RECOVERY_LIVE_SNAPSHOT
#define RECOVERY_LIVE_SNAPSHOT \
    ((spectrum_board_snapshot_t *)(NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR + 73u))
typedef char recovery_live_snapshot_fits[
    73u + sizeof(spectrum_board_snapshot_t) <=
        NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE ? 1 : -1];
#endif
extern uint8_t side_to_move;
extern uint8_t castle_rights;
extern int8_t ep_square;

uint8_t restore_build_b64_ovl(const spectrum_board_snapshot_t *snap,
                              const netchesszx_save_meta_t *meta,
                              char *b64);
uint8_t recovery_write_ovl_entry(void);

/* OUT == 0 writes a checkpoint of the live position; otherwise its payload
   goes to OUT (SAVE) and nothing is written. */
static uint8_t recovery_capture_live_ovl(uint8_t flags, uint8_t *ctx)
{
    spectrum_board_snapshot_t *snapshot = RECOVERY_LIVE_SNAPSHOT;
    const spectrum_time_view_t *time;
    netchesszx_save_meta_t meta;
    uint16_t b64_ptr;
    char *b64 = RECOVERY_RECORD_SNAPSHOT;
    uint8_t i;

    for (i = 0u; i < sizeof snapshot->cells; ++i) {
        snapshot->cells[i] = RECOVERY_BOARD_CELLS[i];
    }
    snapshot->side = side_to_move;
    snapshot->castle = castle_rights;
    snapshot->ep = ep_square;

    meta.ply = netchesszx_session_view()->game_ply;
    meta.flags = flags;
    meta.host_color = netchesszx_session_identity_host_color();
    meta.view_flags = spectrum_board_view_flipped
        ? NETCHESSZX_SAVE_VIEW_FLIPPED
        : 0u;
    time = spectrum_time_snapshot();
    for (i = 0u; i < sizeof meta.timers; ++i) {
        meta.timers[i] = time->game_timers[i];
    }
    b64_ptr = (uint16_t)ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_LO] |
        ((uint16_t)ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_HI] << 8);
    if (b64_ptr != 0u) {
        b64 = (char *)b64_ptr;
    }
    if (!restore_build_b64_ovl(snapshot, &meta, b64)) {
        ctx[SPECTRUM_OVL_CTX_RECOVERY_RESULT] = 1u;
        return 0u;
    }
    if (b64_ptr != 0u) {
        ctx[SPECTRUM_OVL_CTX_RECOVERY_RESULT] = 0u;
        return 1u;
    }
    /* Only resumability is decided here (a played, unfinished game); WRITE
       adds role, colour and journal bits in ASM because z88dk SDCC dropped
       the stores of a conditional |= chain on this local. */
    ctx[SPECTRUM_OVL_CTX_RECOVERY_LINK] =
        (meta.ply != 0u && !(flags & NETCHESSZX_SAVE_FLAG_GAME_OVER))
            ? SPECTRUM_RECOVERY_LINK_PLAYED : 0u;
    b64_ptr = (uint16_t)b64;
    ctx[SPECTRUM_OVL_CTX_RECOVERY_BUF_LO] = (uint8_t)b64_ptr;
    ctx[SPECTRUM_OVL_CTX_RECOVERY_BUF_HI] = (uint8_t)(b64_ptr >> 8);
    return recovery_write_ovl_entry();
}

/* Entry 3: a chosen exit (menu disconnect or the peer's BYE) ends a live
   game for recovery. The newest checkpoint becomes the exact position marked
   OVER, which is never offered; outside a live game there is nothing to do. */
uint8_t recovery_retire_ovl(uint8_t *ctx) __z88dk_fastcall
{
    if (!netchesszx_session_view()->game_status_active ||
        netchesszx_session_view()->game_over) {
        return 1u;
    }
    ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_LO] = 0u;
    ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_HI] = 0u;
    return recovery_capture_live_ovl(
        NETCHESSZX_SAVE_FLAG_ACTIVE | NETCHESSZX_SAVE_FLAG_GAME_OVER, ctx);
}

uint8_t recovery_capture_ovl(uint8_t *ctx) __z88dk_fastcall
{
    return recovery_capture_live_ovl(ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS], ctx);
}
