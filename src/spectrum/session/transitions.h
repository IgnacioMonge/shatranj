#ifndef NETCHESSZX_SPECTRUM_SESSION_TRANSITIONS_H
#define NETCHESSZX_SPECTRUM_SESSION_TRANSITIONS_H

#include "common/protocol/game_protocol.h"
#include "spectrum/session/identity.h"
#include "spectrum/session/state.h"
#include "spectrum/session/event.h"
#include "spectrum/session/timing.h"

static uint8_t netchesszx_session_active_peer_ready(void)
{
    return (uint8_t)(!netchesszx_session_view()->game_status_active ||
                     netchesszx_session_peer_view());
}

#define LOCAL_MOVE_NET_FAIL 0u
#define LOCAL_MOVE_REJECTED 1u
#define LOCAL_MOVE_SENT 2u

/* takeback_pending_ply holds either this side's TAKEBACK awaiting its reply
   or the peer's TAKEBACK awaiting Y/N. This side only asks to take back its
   own move and the peer only its own, so the snapshot owner tells them
   apart: only our own request is retransmitted or answered by ACK/NACK. */
#define SESSION_LOCAL_TAKEBACK_PENDING() \
    (netchesszx_session_view()->takeback_pending_ply != 0u && \
     SESSION_APP_TAKEBACK_IS_LOCAL())

#define SESSION_LOCAL_LOAD_BLOCKED() \
    (netchesszx_session_view()->pending_local_ply != 0u || \
     netchesszx_session_view()->start_pending || \
     netchesszx_session_view()->resign_pending || \
     netchesszx_session_view()->control_pending || \
     netchesszx_session_view()->takeback_pending_ply != 0u || \
     (netchesszx_session_view()->restore_rx_mask & \
      (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK | RESTORE_RX_RECEIVE)) != 0u)

/* The caller supplies the exit label and therefore owns the return value. */
#define SESSION_LOAD_PREPARED(meta, done) do { \
    if (!netchesszx_session_peer_view()) { \
        SESSION_APP_BOARD_APPLY(SESSION_APP_APPLY_LOADED(meta), \
            SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_FAIL); \
            goto done); \
        SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_LOAD_OK); \
        goto done; \
    } \
    if (!SESSION_APP_BUILD_LOADED(meta)) { \
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_FAIL); \
        goto done; \
    } \
    netchesszx_session_set_restore_rx_mask(RESTORE_TX_PENDING); \
    if (!session_tcp_send(msg_restore_rq)) { \
        netchesszx_session_set_restore_rx_mask(0u); \
        goto done; \
    } \
    SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL); \
    goto done; \
} while (0)

#define MQTT_SETUP_REANNOUNCE_TICKS 200u
#define DIRECT_HELLO_REANNOUNCE_POLLS NETCHESSZX_SESSION_DIRECT_3S_POLLS
#define PENDING_RETRY_POLLS \
    (netchesszx_transport_is_mqtt() \
         ? NETCHESSZX_SESSION_MQTT_REPLY_POLLS \
         : NETCHESSZX_SESSION_DIRECT_REPLY_POLLS)
#define CONTROL_REPLY_RETRIES 5u
#define SESSION_CAN_DISPATCH_CONTROL() \
    (netchesszx_transport_is_mqtt() || netchesszx_session_peer_view())
#define SESSION_FORGET_DISCONNECTED_COLOR() do { \
    if (!netchesszx_transport_is_mqtt() && !netchesszx_session_is_host()) { \
        netchesszx_session_forget_color(); \
    } \
} while (0)
#define SESSION_RETRY_RESET(schedule, period) do { \
    netchesszx_session_schedule_set_control_retry_count((schedule), 0u); \
    netchesszx_session_schedule_set_pending_retry_wait((schedule), (period)); \
} while (0)
#define SESSION_SCHEDULE_BEGIN(schedule, pending, cancel) do { \
    netchesszx_session_schedule_set_mqtt_setup_reannounce_wait((schedule), MQTT_SETUP_REANNOUNCE_TICKS); \
    netchesszx_session_schedule_set_direct_hello_reannounce_wait((schedule), 0u); \
    SESSION_RETRY_RESET((schedule), (pending)); \
    netchesszx_session_schedule_set_control_cancel_wait((schedule), 0u); \
    netchesszx_session_schedule_set_decision_kind((schedule), 0u); \
} while (0)
#define CONTROL_DECISION_FRAMES NETCHESSZX_SESSION_DECISION_FRAMES

#if defined(NETCHESSZX_SPECTRANEXT)
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "SPCX"
#elif defined(NETCHESSZX_NEXT)
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "NXT"
#else
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "ZX"
#endif

#define CONFIRM_NONE 0u
#define CONFIRM_RESET_SEND 2u
#define CONFIRM_RESET_ACCEPT 3u
#define CONFIRM_RESTART_GAME 4u
#define CONFIRM_RESIGN_SEND 5u
#define CONFIRM_DRAW_SEND 6u
#define CONFIRM_TAKEBACK_SEND 7u
#define CONFIRM_TAKEBACK_ACCEPT 8u
#define CONFIRM_RESTORE_ACCEPT 9u

#define msg_move_prefix NETCHESS_PROTO_MOVE_PREFIX
#define msg_reset_wire NETCHESS_PROTO_RESET
#define msg_ack_reset NETCHESS_PROTO_ACK_RESET
#define msg_nack_reset NETCHESS_PROTO_NACK_RESET
#define msg_ack_resign NETCHESS_PROTO_ACK_RESIGN
#define msg_takeback_wire NETCHESS_PROTO_TAKEBACK_PREFIX
#define msg_bye NETCHESS_PROTO_BYE
#define msg_draw NETCHESS_PROTO_DRAW
#define msg_resign_wire NETCHESS_PROTO_RESIGN
static const char msg_nack_reset_busy[] = "NACK RESET BUSY";
static const char msg_ack_game_start[] = "ACK GAME START";
static const char msg_restore_rq[] = NETCHESS_PROTO_RESTORE_RQ;
static const char msg_restore_ry[] = NETCHESS_PROTO_RESTORE_RY;
static const char msg_restore_rn[] = NETCHESS_PROTO_RESTORE_RN;
static const char msg_restore_ra[] = NETCHESS_PROTO_RESTORE_RA;

#endif
