#ifndef NETCHESSZX_SPECTRUM_SESSION_STATE_H
#define NETCHESSZX_SPECTRUM_SESSION_STATE_H

#include <stdint.h>
#include "spectrum/platform/text.h"

#define CONTROL_PENDING_RESET 1u
#define CONTROL_PENDING_DRAW_SENT 2u
#define CONTROL_PENDING_DRAW_INCOMING 3u
#define CONTROL_PENDING_RESET_WAIT 4u
#define CONTROL_PENDING_RESET_CANCEL 0x81u
#define CONTROL_PENDING_DRAW_WAIT 6u
#define CONTROL_PENDING_DRAW_CANCEL 0x82u
#define CONTROL_PENDING_TAKEBACK_CANCEL 0x83u
#define CONTROL_IS_CANCEL(value) \
    (((value) & 0x80u) != 0u)
#define CONTROL_IS_RESET(value) \
    ((value) == CONTROL_PENDING_RESET || \
     (value) == CONTROL_PENDING_RESET_WAIT || \
     (value) == CONTROL_PENDING_RESET_CANCEL)
#define CONTROL_IS_LOCAL_DRAW(value) \
    ((value) == CONTROL_PENDING_DRAW_SENT || \
     (value) == CONTROL_PENDING_DRAW_WAIT || \
     (value) == CONTROL_PENDING_DRAW_CANCEL)
#define CONTROL_IS_WAIT(value) \
    ((value) == CONTROL_PENDING_RESET_WAIT || \
     (value) == CONTROL_PENDING_DRAW_WAIT)
#define CONTROL_ACCEPT_NONE 0u
#define CONTROL_ACCEPT_RESET 1u
#define CONTROL_ACCEPT_DRAW 2u
/* Declined incoming DRAW; cleared with the accept latch by the next
   committed move, takeback, reset, start or restore. */
#define CONTROL_REJECT_DRAW 3u
/* This side's last answer to a peer request (RESET, RQ, TAKEBACK). The peer
   resends a request every 2.5 s until the answer arrives, so a retry can
   cross it; that retry gets the same reply again instead of a second prompt.
   The window is the 3-second DIRECT poll count of empty polls, and any
   other event ends it, so a later genuine request still asks. Host
   transcripts omit the window so a repeated request keeps prompting. */

#define RESTORE_RX_ALL 0x03u
#define RESTORE_RX_RECEIVE 0x10u
#define RESTORE_RX_APPLIED 0x20u
#define RESTORE_TX_AWAIT_ACK 0x40u
#define RESTORE_TX_PENDING 0x80u
#define RESTORE_CHUNK_REJECT 0u
#define RESTORE_CHUNK_PARTIAL 1u
#define RESTORE_CHUNK_COMPLETE 2u
#define RESTORE_CHUNK_REACK 3u

/* Persistent protocol bookkeeping, not board undo or UI state.
   No pointer to the shared Setup/RESTORE workspace is retained here. */
typedef struct netchesszx_session_state {
    uint16_t game_ply;
    uint16_t pending_local_ply;
    uint16_t takeback_pending_ply;
    uint16_t last_accepted_takeback_ply;
    uint8_t last_control_accept;
    uint8_t resign_pending;
    uint8_t restore_rx_mask;
    uint8_t game_status_active;
    uint8_t game_over;
    uint8_t start_pending;
    uint8_t control_pending;
    uint8_t mqtt_seat_probed;
    char pending_local_move[6];
#ifndef NETCHESSZX_HOST_SESSION_TEST
    uint8_t answer_replay_polls;
    uint8_t answer_replay_event;
    uint8_t answer_replay_ack;
#endif
} netchesszx_session_state_t;

/* Private storage: product callers use the const view and operations below.
   The source guard rejects direct access outside session/. */
extern netchesszx_session_state_t netchesszx_session_state;
#define netchesszx_session_view() \
    ((const netchesszx_session_state_t *)&netchesszx_session_state)

/* Primitive writes are private to session coordination. Application effects
   report committed domain facts through the semantic operations below. */
#define netchesszx_session_set_game_ply(value) \
    (netchesszx_session_state.game_ply = (value))
#define netchesszx_session_set_pending_local_ply(value) \
    (netchesszx_session_state.pending_local_ply = (value))
#define netchesszx_session_set_takeback_pending_ply(value) \
    (netchesszx_session_state.takeback_pending_ply = (value))
#define netchesszx_session_set_last_accepted_takeback_ply(value) \
    (netchesszx_session_state.last_accepted_takeback_ply = (value))
#define netchesszx_session_set_last_control_accept(value) \
    (netchesszx_session_state.last_control_accept = (value))
#define netchesszx_session_set_resign_pending(value) \
    (netchesszx_session_state.resign_pending = (value))
#define netchesszx_session_set_restore_rx_mask(value) \
    (netchesszx_session_state.restore_rx_mask = (value))
#define netchesszx_session_set_game_status_active(value) \
    (netchesszx_session_state.game_status_active = (value))
#define netchesszx_session_set_game_over(value) \
    (netchesszx_session_state.game_over = (value))
#define netchesszx_session_set_start_pending(value) \
    (netchesszx_session_state.start_pending = (value))
#define netchesszx_session_set_control_pending(value) \
    (netchesszx_session_state.control_pending = (value))
#define netchesszx_session_set_mqtt_seat_probed(value) \
    (netchesszx_session_state.mqtt_seat_probed = (value))
#ifndef NETCHESSZX_HOST_SESSION_TEST
#define netchesszx_session_set_answer_replay_polls(value) \
    (netchesszx_session_state.answer_replay_polls = (value))
#define netchesszx_session_decay_answer_replay() \
    (--netchesszx_session_state.answer_replay_polls)
#define netchesszx_session_set_answer_replay(event) do { \
    netchesszx_session_state.answer_replay_event = (event); \
    netchesszx_session_state.answer_replay_ack = 0u; \
} while (0)
#define netchesszx_session_set_answer_replay_ack() \
    (netchesszx_session_state.answer_replay_ack = 1u)
#endif
#define netchesszx_session_clear_pending_move() \
    (netchesszx_session_state.pending_local_move[0] = '\0')
#define netchesszx_session_store_pending_move(move) \
    ((void)spectrum_append_text(netchesszx_session_state.pending_local_move, (move)))

#define netchesszx_session_pending_move_clear() do { \
    netchesszx_session_state.pending_local_ply = 0u; \
    netchesszx_session_state.pending_local_move[0] = '\0'; \
} while (0)
#define netchesszx_session_takeback_history_clear() do { \
    netchesszx_session_state.takeback_pending_ply = 0u; \
    netchesszx_session_state.last_accepted_takeback_ply = 0u; \
    netchesszx_session_state.last_control_accept = CONTROL_ACCEPT_NONE; \
} while (0)
#define netchesszx_session_new_move_committed() do { \
    netchesszx_session_state.control_pending = 0u; \
    netchesszx_session_state.takeback_pending_ply = 0u; \
    netchesszx_session_state.last_accepted_takeback_ply = 0u; \
    netchesszx_session_state.last_control_accept = CONTROL_ACCEPT_NONE; \
} while (0)
#define netchesszx_session_game_started() do { \
    netchesszx_session_state.game_ply = 0u; \
    netchesszx_session_state.game_status_active = 1u; \
    netchesszx_session_state.game_over = 0u; \
    netchesszx_session_state.start_pending = 0u; \
    netchesszx_session_state.resign_pending = 0u; \
} while (0)
#define netchesszx_session_game_ended() do { \
    netchesszx_session_state.game_status_active = 0u; \
    netchesszx_session_state.game_over = 1u; \
    netchesszx_session_state.control_pending = 0u; \
    netchesszx_session_state.restore_rx_mask = 0u; \
} while (0)
#define netchesszx_session_restore_committed(ply, active, over) do { \
    netchesszx_session_state.game_ply = (ply); \
    netchesszx_session_state.game_status_active = (active); \
    netchesszx_session_state.game_over = (over); \
    netchesszx_session_state.start_pending = 0u; \
    netchesszx_session_state.resign_pending = 0u; \
    netchesszx_session_state.control_pending = 0u; \
} while (0)
/* Phase reset only; the app reports pending-move/undo disposal separately. */
#define netchesszx_session_disconnected() do { \
    netchesszx_session_state.game_status_active = 0u; \
    netchesszx_session_state.game_over = 0u; \
    netchesszx_session_state.start_pending = 0u; \
    netchesszx_session_state.resign_pending = 0u; \
    netchesszx_session_state.control_pending = 0u; \
    netchesszx_session_state.restore_rx_mask = 0u; \
} while (0)
#define netchesszx_session_peer_wait() do { \
    netchesszx_session_state.start_pending = 0u; \
    netchesszx_session_state.control_pending = 0u; \
    netchesszx_session_state.restore_rx_mask = 0u; \
    netchesszx_session_state.game_over = 0u; \
    netchesszx_session_state.game_status_active = 0u; \
    netchesszx_session_state.game_ply = 0u; \
} while (0)
#define netchesszx_session_begin_setup() \
    (netchesszx_session_state.control_pending = 0u)
#define netchesszx_session_takeback_committed(ply) do { \
    netchesszx_session_state.game_ply = (ply); \
    netchesszx_session_state.game_over = 0u; \
    netchesszx_session_state.game_status_active = 1u; \
} while (0)
#define netchesszx_session_move_committed(ply) \
    (netchesszx_session_state.game_ply = (ply))
#define netchesszx_session_loop_begin() \
    (netchesszx_session_state.game_ply = 0u)
#define netchesszx_session_seat_probe_reset() \
    (netchesszx_session_state.mqtt_seat_probed = 0u)

uint8_t netchesszx_asm_restore_chunk_step(uint8_t *mask,
                                        char *cache,
                                        const char *frame);
#define netchesszx_session_restore_chunk_step(cache, frame) \
    netchesszx_asm_restore_chunk_step( \
        &netchesszx_session_state.restore_rx_mask, (cache), (frame))

/* Mutable scheduler storage stays on the caller's stack (seven Z80 bytes).
   The compact coordinator owns initialization, scheduling and retry policy. */
typedef struct netchesszx_session_schedule {
    uint8_t mqtt_setup_reannounce_wait;
    uint8_t direct_hello_reannounce_wait;
    uint8_t pending_retry_wait;
    uint8_t control_retry_count;
    uint16_t control_cancel_wait; /* Absolute approval deadline in frames. */
    uint8_t decision_kind;
} netchesszx_session_schedule_t;
#define netchesszx_session_schedule_view(schedule) \
    ((const netchesszx_session_schedule_t *)(schedule))
#define netchesszx_session_schedule_set_mqtt_setup_reannounce_wait(schedule, value) \
    ((schedule)->mqtt_setup_reannounce_wait = (value))
#define netchesszx_session_schedule_set_direct_hello_reannounce_wait(schedule, value) \
    ((schedule)->direct_hello_reannounce_wait = (value))
#define netchesszx_session_schedule_set_pending_retry_wait(schedule, value) \
    ((schedule)->pending_retry_wait = (value))
#define netchesszx_session_schedule_set_control_retry_count(schedule, value) \
    ((schedule)->control_retry_count = (value))
#define netchesszx_session_schedule_set_control_cancel_wait(schedule, value) \
    ((schedule)->control_cancel_wait = (value))
#define netchesszx_session_schedule_set_decision_kind(schedule, value) \
    ((schedule)->decision_kind = (value))
#define netchesszx_session_schedule_tick_mqtt_setup_reannounce_wait(schedule) \
    (--(schedule)->mqtt_setup_reannounce_wait)
#define netchesszx_session_schedule_tick_direct_hello_reannounce_wait(schedule) \
    (--(schedule)->direct_hello_reannounce_wait)
#define netchesszx_session_schedule_tick_pending_retry_wait(schedule) \
    (--(schedule)->pending_retry_wait)
#define netchesszx_session_schedule_inc_control_retry_count(schedule) \
    (++(schedule)->control_retry_count)

#ifdef NETCHESSZX_SDCC_IY
typedef char netchesszx_session_storage_size_check[
    sizeof(netchesszx_session_state_t) == 25u &&
    sizeof(netchesszx_session_schedule_t) == 7u ? 1 : -1];
#endif

#endif
