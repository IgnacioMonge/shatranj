#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_HOST_SESSION_TEST 1
#define NETCHESSZX_RECOVERY 1
#define __z88dk_fastcall

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "spectrum/board/board.h"
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/session/state.h"

volatile uint8_t netchesszx_host_overlay_context[SPECTRUM_OVERLAY_CONTEXT_SIZE];
/* The product encodes into the overlay-scratch record and stages the live
   position beside it; the host test owns both. */
static char test_record_snapshot[64];
#define RECOVERY_RECORD_SNAPSHOT test_record_snapshot
static spectrum_board_snapshot_t test_live;
#define RECOVERY_LIVE_SNAPSHOT (&test_live)
static char test_cells[64];
#define RECOVERY_BOARD_CELLS test_cells
uint8_t side_to_move;
uint8_t castle_rights;
int8_t ep_square;

#include "../../src/spectrum/overlay/restore_ovl.c"
#include "../../src/spectrum/overlay/recovery_ovl.c"
#include "../../src/spectrum/saveload/saveload.c"

netchesszx_session_state_t netchesszx_session_state;
static spectrum_time_view_t test_time;
static uint8_t test_flipped;
static uint8_t *test_ctx;
static uint8_t test_writes;
static char test_saved[NETCHESSZX_SAVE_WIRE_B64_SIZE];
static uint8_t test_overlay;
static uint8_t test_entry;

uint8_t netchesszx_host_color;
uint8_t netchesszx_session_role;
uint8_t netchesszx_local_color;
uint8_t spectrum_board_view_flipped;
static uint16_t test_write_source;
static uint8_t test_link;

const spectrum_time_view_t *spectrum_time_snapshot(void)
{
    return &test_time;
}

uint8_t recovery_write_ovl_entry(void)
{
    /* The product context carries 16-bit Spectrum pointers. */
    test_write_source = (uint16_t)(
        (uint16_t)test_ctx[SPECTRUM_OVL_CTX_RECOVERY_BUF_LO] |
        ((uint16_t)test_ctx[SPECTRUM_OVL_CTX_RECOVERY_BUF_HI] << 8));
    test_link = test_ctx[SPECTRUM_OVL_CTX_RECOVERY_LINK];
    memcpy(test_saved, test_record_snapshot, sizeof test_saved);
    test_ctx[SPECTRUM_OVL_CTX_RECOVERY_RESULT] = 0u;
    ++test_writes;
    return 1u;
}

uint8_t spectrum_overlay_exec_cached(uint8_t overlay, uint8_t entry)
{
    test_overlay = overlay;
    test_entry = entry;
    netchesszx_host_overlay_context[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = 0u;
    return 1u;
}

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    static const char cells[] =
        "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR";
    spectrum_board_snapshot_t source;
    spectrum_board_snapshot_t decoded;
    netchesszx_save_meta_t meta;
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = { 0 };
    uint8_t cache[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    uint8_t before[NETCHESSZX_SAVE_WIRE_B64_SIZE];

    memcpy(source.cells, cells, sizeof source.cells);
    source.side = 1u;
    source.castle = 15u;
    source.ep = -1;
    netchesszx_session_state.game_ply = 17u;
    netchesszx_host_color = NETCHESSZX_SAVE_HOST_BLACK;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    netchesszx_local_color = NETCHESSZX_COLOR_BLACK;
    test_flipped = 1u;
    spectrum_board_view_flipped = test_flipped;
    test_time.game_timers[0] = 1u;
    test_time.game_timers[1] = 2u;
    test_time.game_timers[2] = 3u;
    test_time.game_timers[3] = 4u;
    test_time.game_timers[4] = 5u;
    test_time.game_timers[5] = 6u;
    memset(cache, 0xa5, sizeof cache);
    memcpy(before, cache, sizeof before);
    ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS] =
        NETCHESSZX_SAVE_FLAG_ACTIVE | NETCHESSZX_SAVE_FLAG_CHECK;
    test_ctx = ctx;
    /* The live position: CAPTURE reads it, never a caller's snapshot. */
    memcpy(test_cells, source.cells, sizeof test_cells);
    side_to_move = source.side;
    castle_rights = source.castle;
    ep_square = source.ep;

    if (!recovery_capture_live_ovl(ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS],
                                   ctx) || test_writes != 1u ||
        ctx[SPECTRUM_OVL_CTX_RECOVERY_RESULT] != 0u) {
        return fail("capture does not dispatch one successful WRITE");
    }
    if (!spectrum_restore_decode(test_saved, &decoded, &meta)) {
        return fail("captured payload does not decode");
    }
    if (memcmp(&decoded, &source, sizeof source) != 0 || meta.ply != 17u ||
        meta.flags != ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS] ||
        meta.host_color != NETCHESSZX_SAVE_HOST_BLACK ||
        meta.view_flags != NETCHESSZX_SAVE_VIEW_FLIPPED ||
        memcmp(meta.timers, test_time.game_timers, sizeof meta.timers) != 0) {
        return fail("capture omits board or resident metadata");
    }
    if (test_write_source != (uint16_t)(uintptr_t)test_record_snapshot) {
        return fail("capture does not encode in place in the record");
    }
    if (test_link != SPECTRUM_RECOVERY_LINK_PLAYED) {
        return fail("capture does not flag a played checkpoint");
    }
    if (memcmp(cache, before, sizeof cache) != 0) {
        return fail("capture changes the pending RESTORE cache");
    }
    /* A move on the board is what the next capture records. */
    test_cells[52] = '.';
    test_cells[36] = 'P';
    if (!recovery_capture_live_ovl(ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS],
                                   ctx) ||
        !spectrum_restore_decode(test_saved, &decoded, &meta) ||
        memcmp(decoded.cells, test_cells, sizeof decoded.cells) != 0) {
        return fail("capture does not follow the live board");
    }
    memcpy(test_cells, source.cells, sizeof test_cells);
    if (!spectrum_saveload_erase("MANUAL.NCZS") ||
        test_overlay != SPECTRUM_OVL_SAVELOAD ||
        test_entry != SPECTRUM_SAVELOAD_ERASE) {
        return fail("erase routes to SAVELOAD despite sharing entry 2");
    }
    if (!spectrum_recovery_capture(ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS],
                                   test_saved) ||
        test_overlay != SPECTRUM_OVL_RECOVERY ||
        test_entry != SPECTRUM_RECOVERY_CAPTURE) {
        return fail("manual encode routes to RECOVERY CAPTURE");
    }
    if (!spectrum_recovery_read(0) ||
        test_overlay != SPECTRUM_OVL_CONTROL ||
        test_entry != SPECTRUM_OVL_CONTROL_RECOVERY_RESUME) {
        return fail("resume probe routes to CONTROL RESUME");
    }
    netchesszx_session_state.game_ply = 0u;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    netchesszx_local_color = NETCHESSZX_COLOR_WHITE;
    test_writes = 0u;
    if (!recovery_capture_live_ovl(ctx[SPECTRUM_OVL_CTX_RECOVERY_FLAGS],
                                   ctx) || test_writes != 1u ||
        test_link != 0u) {
        return fail("ply-zero checkpoint is flagged as played");
    }
    netchesszx_session_state.game_ply = 17u;
    if (!recovery_capture_live_ovl(NETCHESSZX_SAVE_FLAG_GAME_OVER, ctx) ||
        test_link != 0u) {
        return fail("finished game is flagged as resumable");
    }

    /* RETIRE writes a live game as the newest OVER checkpoint of the exact
       position, into the record even if a caller left an output pointer. */
    test_writes = 0u;
    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.game_over = 0u;
    if (!recovery_retire_ovl(ctx) || test_writes != 0u) {
        return fail("retire outside a live game writes a checkpoint");
    }
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.game_over = 1u;
    if (!recovery_retire_ovl(ctx) || test_writes != 0u) {
        return fail("retire rewrites a finished game");
    }
    netchesszx_session_state.game_over = 0u;
    test_link = 0xffu;
    ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_LO] = 0x34u;
    ctx[SPECTRUM_OVL_CTX_RECOVERY_OUT_HI] = 0x12u;
    if (!recovery_retire_ovl(ctx) || test_writes != 1u || test_link != 0u ||
        test_write_source != (uint16_t)(uintptr_t)test_record_snapshot) {
        return fail("retire does not write one unoffered record");
    }
    if (!spectrum_restore_decode(test_saved, &decoded, &meta) ||
        memcmp(&decoded, &source, sizeof source) != 0 || meta.ply != 17u ||
        meta.flags != (NETCHESSZX_SAVE_FLAG_ACTIVE |
                       NETCHESSZX_SAVE_FLAG_GAME_OVER)) {
        return fail("retire does not keep the exact position marked OVER");
    }
    spectrum_recovery_retire();
    if (test_overlay != SPECTRUM_OVL_RECOVERY ||
        test_entry != SPECTRUM_OVL_RECOVERY_RETIRE) {
        return fail("retire routes to RECOVERY RETIRE");
    }
    puts("Recovery CAPTURE codec/context vector ok");
    return 0;
}
