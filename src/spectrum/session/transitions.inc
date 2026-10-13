#define SESSION_DISPATCH_UNHANDLED 0u
#define SESSION_DISPATCH_HANDLED 1u
#define SESSION_DISPATCH_EXIT 2u
#define SESSION_DISPATCH_HANDLED_LIVE 3u
#define SESSION_DISPATCH_RETRY_RESET 4u

#define SESSION_BEGIN_EVENT(event) do { \
    if ((event) != netchesszx_session_view()->answer_replay_event) { \
        netchesszx_session_set_answer_replay_polls(0u); \
    } \
} while (0)
#define SESSION_PRESENCE_RESET_PING(event, ping) do { \
    if (netchesszx_transport_is_mqtt() && \
        ((event) == NETCHESSZX_SESSION_EVENT_BYE || \
         (event) == NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE)) { \
        netchesszx_session_ping_reset(ping); \
    } \
} while (0)
#define SESSION_TEXT_EVENT(event, payload) do { \
    if ((event) == NETCHESSZX_SESSION_EVENT_MQTT_TEXT) { \
        SESSION_APP_NOTICE_INFO(payload); \
    } \
} while (0)

static void session_mqtt_failure_cleanup(void)
{
    if (netchesszx_transport_is_mqtt()) {
        (void)netchesszx_session_mqtt_publish_offline(
            NETCHESSZX_SESSION_ROUTE_PRESENCE);
    }
}

static uint16_t parse_u16(const char *text)
{
    uint16_t value;
    return netchess_mqtt_session_parse_u16_token(text, &value) == 0 ? 0u : value;
}

static void disconnect_to_setup(void)
{
    SESSION_APP_BEGIN_SETUP_EXIT();
    (void)netchesszx_session_send_text(msg_bye);
    if (netchesszx_transport_is_mqtt()) {
        /* Only an activated seat has retained presence to release. */
        if (netchesszx_session_identity_mqtt_session_id() != 0u) {
            session_mqtt_failure_cleanup();
        }
        spectrum_net_mqtt_stop();
    }
    SESSION_APP_FINISH_SETUP_EXIT();
}

static uint8_t tcp_required(uint8_t sent) __z88dk_fastcall
{
    if (sent) {
        return 1u;
    }
    SESSION_APP_DISCONNECT();
    return 0u;
}

static uint8_t session_begin_link(void)
{
    if (!netchesszx_session_direct_send_hello()) {
        SESSION_APP_DISCONNECT();
        return 0u;
    }
    return 1u;
}

static uint8_t session_tcp_send(const char *text) __z88dk_fastcall
{
    return tcp_required(netchesszx_session_send_text(text));
}

static uint8_t session_tcp_reply(const char *text) __z88dk_fastcall
{
    return tcp_required(netchesszx_session_send_ack_text(text));
}

static uint8_t session_tcp_control_reply(const char *text) __z88dk_fastcall
{
    return tcp_required(netchesszx_session_send_control_text(text));
}

static uint8_t session_tcp_move_reply(uint8_t ack, uint16_t ply) NETCHESSZX_CALLEE
{
    char ply_text[8];

    (void)spectrum_append_u16(ply_text, ply);
    return tcp_required(ack
        ? netchesszx_session_send_ack_move(ply_text)
        : netchesszx_session_send_nack_move(ply_text));
}

static uint8_t send_draw_reply(uint8_t accepted) __z88dk_fastcall
{
    return tcp_required(accepted ? netchesszx_session_send_ack_move(msg_draw) :
        netchesszx_session_send_nack_move(msg_draw));
}

static void accepted_draw_end(void)
{
    SESSION_APP_END_DRAW();
    netchesszx_session_set_last_control_accept(CONTROL_ACCEPT_DRAW);
}

/* Remembers that this side answered the peer request `event`, for its
   crossing retries. */
static void note_answer_replay(uint8_t event) __z88dk_fastcall
{
#ifndef NETCHESSZX_HOST_SESSION_TEST
    netchesszx_session_set_answer_replay(event);
    netchesszx_session_set_answer_replay_polls(NETCHESSZX_SESSION_DIRECT_3S_POLLS);
#else
    (void)event;
#endif
}

static void accepted_reset_start(void)
{
    SESSION_APP_CLEAR_CONFIRMATION();
    netchesszx_session_set_control_pending(0u);
    SESSION_APP_RESET_START();
    netchesszx_session_set_last_control_accept(CONTROL_ACCEPT_RESET);
    /* The reset already restarted the game at ply 0. */
    note_answer_replay(NETCHESSZX_SESSION_EVENT_RESET);
#ifndef NETCHESSZX_HOST_SESSION_TEST
    netchesszx_session_set_answer_replay_ack();
#endif
}

static uint8_t send_takeback_wire(uint16_t ply)
{
    char payload[24];
    char *p = spectrum_append_text(payload,
        netchesszx_session_view()->control_pending == CONTROL_PENDING_TAKEBACK_CANCEL
            ? NETCHESS_PROTO_CANCEL_TAKEBACK_PREFIX : msg_takeback_wire);

    p = spectrum_append_u16(p, ply);
    *p = '\0';
    return netchesszx_session_send_text(payload);
}

static uint8_t send_move_wire(uint16_t ply, const char *move)
{
    char payload[24];
    char *p = spectrum_append_text(payload, msg_move_prefix);

    p = spectrum_append_u16(p, ply);
    *p++ = ' ';
    (void)spectrum_append_text(p, move);
    return netchesszx_session_send_text(payload);
}

static uint8_t restore_send_chunks(void);

static uint8_t local_action_ready(void)
{
    if (netchesszx_session_view()->resign_pending) {
        SESSION_APP_SHOW_WAITING_RESIGN_ACK();
        return 0u;
    }
    if (!netchesszx_session_view()->game_status_active) {
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
        return 0u;
    }
    if (netchesszx_session_view()->control_pending ||
        netchesszx_session_view()->takeback_pending_ply != 0u) {
        SESSION_APP_WAIT_FOR_ACK();
        return 0u;
    }
    return 1u;
}

static uint8_t game_start_local(uint8_t start_key)
{
    if (netchesszx_session_view()->game_status_active) {
        return 1u;
    }
    if (!netchesszx_session_can_start_game()) {
        SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_OPPONENT_STARTS_GAME);
        return 1u;
    }
    if (netchesszx_session_view()->start_pending) {
        SESSION_APP_WAIT_FOR_ACK();
        return 1u;
    }
    SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_STARTING_GAME);
    if (!netchesszx_session_send_start_game()) {
        SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_START_FAILED);
        return 0u;
    }
    netchesszx_session_set_game_over(0u);
    netchesszx_session_set_start_pending(1u);
    SESSION_APP_SUPPRESS_KEY_UNTIL_RELEASE(start_key);
    SESSION_APP_SHOW_CURSOR();
    return 1u;
}

static uint8_t send_local_move(const char *move)
{
    uint16_t ply;

    if (!netchesszx_session_view()->game_status_active) {
        SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
        return LOCAL_MOVE_REJECTED;
    }
    if (netchesszx_session_view()->control_pending &&
        !CONTROL_IS_CANCEL(netchesszx_session_view()->control_pending)) {
        goto wait_opponent_ack;
    }
    if (netchesszx_session_view()->pending_local_ply != 0u ||
        netchesszx_session_view()->takeback_pending_ply != 0u) {
        goto wait_opponent_ack;
    }
    if (netchesszx_session_view()->game_ply == 65535u ||
        !SESSION_APP_MOVE_IS_LEGAL(move)) {
        SESSION_APP_REJECT_MOVE();
        return LOCAL_MOVE_REJECTED;
    }
    ply = (uint16_t)(netchesszx_session_view()->game_ply + 1u);
    SESSION_APP_SHOW_LOCAL_MOVE(move);
    if (!send_move_wire(ply, move)) {
        session_mqtt_failure_cleanup();
        return LOCAL_MOVE_NET_FAIL;
    }
    SESSION_APP_CLEAR_MOVE_HINTS();
    netchesszx_session_set_pending_local_ply(ply);
    netchesszx_session_store_pending_move(move);
    return LOCAL_MOVE_SENT;

wait_opponent_ack:
    SESSION_APP_WAIT_FOR_ACK();
    return LOCAL_MOVE_REJECTED;
}

static uint8_t retry_pending_outgoing(void)
{
    if (netchesszx_session_view()->resign_pending) {
        return netchesszx_session_send_text(msg_resign_wire);
    }
    if (netchesszx_session_view()->pending_local_ply != 0u) {
        return send_move_wire(netchesszx_session_view()->pending_local_ply, netchesszx_session_view()->pending_local_move);
    }
    if (netchesszx_session_view()->start_pending) {
        return netchesszx_session_send_start_game();
    }
    if (CONTROL_IS_RESET(netchesszx_session_view()->control_pending)) {
        return netchesszx_session_send_text(
            netchesszx_session_view()->control_pending == CONTROL_PENDING_RESET_CANCEL
                ? NETCHESS_PROTO_CANCEL_RESET : msg_reset_wire);
    }
    if (CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending)) {
        return netchesszx_session_send_text(
            netchesszx_session_view()->control_pending == CONTROL_PENDING_DRAW_CANCEL
                ? NETCHESS_PROTO_CANCEL_DRAW : msg_draw);
    }
    if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_PENDING) != 0u) {
        return netchesszx_session_send_text(msg_restore_rq);
    }
    if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_AWAIT_ACK) != 0u) {
        return restore_send_chunks();
    }
    /* Last on purpose: both callers reach this only with a reset/draw or a
       local_retry_pending() item, and every earlier item has returned, so a
       pending ply here is this side's own TAKEBACK, never the peer's request
       awaiting Y/N (SESSION_LOCAL_TAKEBACK_PENDING). */
    if (netchesszx_session_view()->takeback_pending_ply != 0u) {
        return send_takeback_wire(netchesszx_session_view()->takeback_pending_ply);
    }
    return 1u;
}

static uint8_t local_retry_pending(void)
{
    return (uint8_t)(netchesszx_session_view()->pending_local_ply != 0u || netchesszx_session_view()->start_pending ||
        netchesszx_session_view()->resign_pending || CONTROL_IS_RESET(netchesszx_session_view()->control_pending) ||
        CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending) ||
        SESSION_LOCAL_TAKEBACK_PENDING() ||
        (netchesszx_session_view()->restore_rx_mask &
         (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK)) != 0u);
}

static uint8_t restore_transfer_pending(void)
{
    return (uint8_t)((netchesszx_session_view()->restore_rx_mask &
        (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK | RESTORE_RX_RECEIVE)) != 0u);
}

/* A control, move or takeback awaits its reply, or a Y/N prompt is up. */
static uint8_t session_busy(void)
{
    return (uint8_t)(netchesszx_session_view()->control_pending ||
                     netchesszx_session_view()->pending_local_ply != 0u ||
                     netchesszx_session_view()->takeback_pending_ply != 0u ||
                     SESSION_APP_CONFIRMATION() != CONFIRM_NONE);
}

/* Hide the menu and raise a Y/N prompt: kind in the low byte, error
   message id in the high byte, so each caller loads one register pair. */
static void session_confirm_prompt(uint16_t kind_msg) NETCHESSZX_FASTCALL
{
    SESSION_APP_HIDE_MENU();
    SESSION_APP_CONFIRM((uint8_t)kind_msg);
    SESSION_APP_NOTICE_ERROR_MSG((uint8_t)(kind_msg >> 8));
}
#define SESSION_CONFIRM_PROMPT(kind, msg) \
    session_confirm_prompt((uint16_t)((kind) | ((uint16_t)(msg) << 8)))

static uint8_t restore_send_chunks(void)
{
    uint8_t chunk;

    if ((netchesszx_session_view()->restore_rx_mask &
         (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK)) == 0u) {
        return 0u;
    }
    for (chunk = 0u; chunk < 2u; ++chunk) {
        SESSION_APP_RESTORE_PREPARE_FRAME(chunk);
        if (!netchesszx_session_send_text(SESSION_APP_RESTORE_FRAME())) {
            netchesszx_session_set_restore_rx_mask(0u);
            if (chunk != 0u) {
                session_mqtt_failure_cleanup();
            }
            return 0u;
        }
    }
    netchesszx_session_set_restore_rx_mask(RESTORE_TX_AWAIT_ACK);
    SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_RESET_WAIT);
    return 1u;
}

static uint8_t restore_cancel_pending_request(void)
{
    if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_PENDING) == 0u) {
        SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_RESET_WAIT);
        SESSION_APP_SUPPRESS_KEY();
        return 1u;
    }
    if (!netchesszx_session_send_text(msg_restore_rn)) {
        return 0u;
    }
    netchesszx_session_set_restore_rx_mask(0u);
    SESSION_APP_NOTICE_LOAD_CANCELLED();
    SESSION_APP_SUPPRESS_KEY();
    return 1u;
}


#ifndef NETCHESSZX_HOST_SESSION_TEST
#define SESSION_REPLAY_DECAY() do { \
    if (netchesszx_session_view()->answer_replay_polls != 0u) { \
        netchesszx_session_decay_answer_replay(); \
    } \
} while (0)
/* Nonzero while a retry of the request answered last may still cross that
   answer: SESSION_BEGIN_EVENT already ended the window for any other event. */
#define answer_replay_live() (netchesszx_session_view()->answer_replay_polls != 0u)
/* Shared verbatim with the production-configuration replay regression. A
   refusal changes nothing, so it is replayed as is; an ACK only while the new
   game it started has no move, or the peer would reset a game this side
   continued. */
#define SESSION_REPLAY_RESET() do { \
    if (answer_replay_live() && \
        (!netchesszx_session_view()->answer_replay_ack || \
         netchesszx_session_view()->game_ply == 0u)) { \
        if (!session_tcp_reply(netchesszx_session_view()->answer_replay_ack \
                                   ? msg_ack_reset : msg_nack_reset)) { \
            goto dispatch_exit; \
        } \
        goto dispatch_handled; \
    } \
} while (0)
#else
#define SESSION_REPLAY_DECAY() ((void)0)
#define SESSION_REPLAY_RESET() ((void)0)
#define answer_replay_live() 0u
#endif

/* The approval interval is below half the 16-bit frame range. */
#define SESSION_DEADLINE_REACHED(now, deadline) \
    ((uint16_t)((now) - (deadline)) < 32768u)

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
#include <stddef.h>
typedef char session_deadline_layout_check[
    offsetof(netchesszx_session_state_t, takeback_pending_ply) == 4u &&
    offsetof(netchesszx_session_state_t, last_accepted_takeback_ply) == 6u &&
    offsetof(netchesszx_session_state_t, last_control_accept) == 8u &&
    offsetof(netchesszx_session_state_t, restore_rx_mask) == 10u &&
    offsetof(netchesszx_session_state_t, game_over) == 12u &&
    offsetof(netchesszx_session_state_t, control_pending) == 14u &&
    offsetof(netchesszx_session_schedule_t, control_cancel_wait) == 4u &&
    offsetof(netchesszx_session_schedule_t, decision_kind) == 6u &&
    CONTROL_PENDING_RESET == 1u && CONTROL_PENDING_RESET_WAIT == 4u &&
    CONTROL_PENDING_DRAW_SENT == 2u && CONTROL_PENDING_DRAW_WAIT == 6u &&
    CONTROL_PENDING_DRAW_INCOMING == 3u && CONFIRM_RESET_ACCEPT == 3u &&
    CONTROL_PENDING_RESET_CANCEL == 0x81u && CONTROL_PENDING_DRAW_CANCEL == 0x82u &&
    CONTROL_PENDING_TAKEBACK_CANCEL == 0x83u &&
    CONFIRM_TAKEBACK_ACCEPT == 8u && CONFIRM_RESTORE_ACCEPT == 9u &&
    SPECTRUM_GUI_MSG_PACK(SPECTRUM_GUI_MSG_LOAD_DECLINED, SPECTRUM_GUI_MSG_KIND_ERROR) == 0x0123u &&
    NETCHESS_PROTO_DECISION_SECONDS == 60u ? 1 : -1];

static uint8_t session_decision_kind(void) __naked
{
#asm
    ld a,(_netchesszx_session_state+14)
    bit 7,a
    jr nz,session_kind_none
    cp 4
    ld l,1
    ret z
    and 0xfb
    dec a
    cp 2
    jr nc,session_kind_other
    inc a
    ld l,a
    ret
session_kind_other:
    ld a,(_takeback_snapshot_local)
    or a
    jr z,session_kind_restore
    ld hl,(_netchesszx_session_state+4)
    ld a,h
    or l
    jr z,session_kind_restore
    ld l,3
    ret
session_kind_restore:
    ld a,(_netchesszx_session_state+10)
    bit 7,a
    jr z,session_kind_confirmation
    ld l,4
    ret
session_kind_confirmation:
    ld a,(_confirm_action)
    sub 8
    cp 2
    jr nc,session_kind_remote_control
    add a,7
    ld l,a
    ret
session_kind_remote_control:
    cp 251
    jr nz,session_kind_none
    ld a,(_netchesszx_session_state+14)
    cp 3
    ld l,5
    ret nz
    ld l,6
    ret
session_kind_none:
    ld l,0
    ret
#endasm
}
#else
static uint8_t session_decision_kind(void)
{
    uint8_t pending = netchesszx_session_view()->control_pending;

    if (CONTROL_IS_CANCEL(pending)) { return 0u; }
    if (pending == CONTROL_PENDING_RESET || pending == CONTROL_PENDING_RESET_WAIT) { return 1u; }
    if (pending == CONTROL_PENDING_DRAW_SENT || pending == CONTROL_PENDING_DRAW_WAIT) { return 2u; }
    if (SESSION_LOCAL_TAKEBACK_PENDING()) { return 3u; }
    if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_PENDING) != 0u) { return 4u; }
    if (SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_ACCEPT) { return 7u; }
    if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT) { return 8u; }
    if (SESSION_APP_CONFIRMATION() == CONFIRM_RESET_ACCEPT) {
        return pending == CONTROL_PENDING_DRAW_INCOMING ? 6u : 5u;
    }
    return 0u;
}

#endif

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_SPECTRANEXT) && NETCHESSZX_SESSION_FRAME_HZ == 60
#define SESSION_ASM_DEFAULT_FRAMES 3600
#define SESSION_ASM_DEFAULT_REPLY 75
#else
#define SESSION_ASM_DEFAULT_FRAMES 3000
#define SESSION_ASM_DEFAULT_REPLY 63
#endif

#ifdef NETCHESSZX_NEXT
#define SESSION_ASM_NEXT 1
#else
#define SESSION_ASM_NEXT 0
#endif
#ifdef NETCHESSZX_SPECTRANEXT
#define SESSION_ASM_SPECTRANEXT 1
#else
#define SESSION_ASM_SPECTRANEXT 0
#endif

/* HL is the seven-byte stack scheduler; return the expired kind in L. */
static uint8_t session_deadline_step(netchesszx_session_schedule_t *schedule) __z88dk_fastcall __naked
{
    (void)schedule;
#asm
    push hl
    call _session_decision_kind
    ld b,l
    pop hl
    ld de,6
    add hl,de
    ld a,b
    or a
    jr z,session_deadline_none
    ld a,(hl)
    cp b
    jr z,session_deadline_same
    ld (hl),b
    push hl
    ld de,(0x5c78)
    ld hl,SESSION_ASM_DEFAULT_FRAMES
    ld c,SESSION_ASM_DEFAULT_REPLY
IF SESSION_ASM_NEXT
    ld a,(_net_uart_direct_idle_ticks)
    cp 90
    jr nz,session_deadline_rate_ready
    ld hl,3600
    ld c,75
ENDIF
IF SESSION_ASM_SPECTRANEXT
    ld a,(_net_uart_direct_idle_ticks)
    cp 180
    jr nz,session_deadline_rate_ready
    ld hl,3600
    ld c,75
ENDIF
session_deadline_rate_ready:
IF SESSION_ASM_SPECTRANEXT
    ld a,(_netchesszx_transport)
    or a
    jr nz,session_deadline_reply_ready
    ld a,c
    cp 75
    ld c,125
    jr nz,session_deadline_reply_ready
    ld c,150
session_deadline_reply_ready:
ENDIF
    add hl,de
    ex de,hl
    pop hl
    dec hl
    ld (hl),d
    dec hl
    ld (hl),e
    dec hl
    xor a
    ld (hl),a
    dec hl
    ld (hl),c
    ld l,a
    ret
session_deadline_same:
    dec hl
    ld d,(hl)
    dec hl
    ld e,(hl)
    ld hl,(0x5c78)
    or a
    sbc hl,de
    add hl,hl
    ld l,0
    ret c
    ld l,b
    ret
session_deadline_none:
    xor a
    ld (hl),a
    ld l,a
    ret
#endasm
}
#else
static uint8_t session_deadline_step(netchesszx_session_schedule_t *schedule,
                                    uint8_t pending_period, uint16_t decision_period)
{
    uint8_t kind = session_decision_kind();
    uint16_t now;

    if (kind == 0u) {
        netchesszx_session_schedule_set_decision_kind(schedule, 0u);
        return 0u;
    }
    now = SESSION_APP_FRAME_COUNT();
    if (kind != netchesszx_session_schedule_view(schedule)->decision_kind) {
        netchesszx_session_schedule_set_decision_kind(schedule, kind);
        netchesszx_session_schedule_set_control_cancel_wait(schedule, (uint16_t)(now + decision_period));
        SESSION_RETRY_RESET(schedule, pending_period);
        return 0u;
    }
    return SESSION_DEADLINE_REACHED(now, netchesszx_session_schedule_view(schedule)->control_cancel_wait)
        ? kind : 0u;
}

#endif

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
#define SESSION_DEADLINE_STEP(schedule, pending, decision) session_deadline_step(schedule)
#else
#define SESSION_DEADLINE_STEP(schedule, pending, decision) session_deadline_step(schedule, pending, decision)
#endif

#define SESSION_PENDING_STEP(schedule, pending_period, decision_period, ok) do { \
    uint8_t session_kind = SESSION_DEADLINE_STEP(schedule, pending_period, decision_period); \
    (ok) = 1u; \
    if (session_kind != 0u) { \
        if (!session_expire_decision(session_kind)) { (ok) = 0u; break; } \
        netchesszx_session_schedule_set_decision_kind(schedule, 0u); \
        SESSION_RETRY_RESET((schedule), (pending_period)); \
    } \
} while (0)

#define SESSION_IDLE_STEP(schedule, pending_period, ok) do { \
    SESSION_REPLAY_DECAY(); \
    (ok) = 1u; \
    if (local_retry_pending()) { \
        if (netchesszx_session_schedule_view(schedule)->pending_retry_wait != 0u) { \
            netchesszx_session_schedule_tick_pending_retry_wait(schedule); \
        } \
        if (netchesszx_session_schedule_view(schedule)->pending_retry_wait == 0u) { \
            if (netchesszx_session_schedule_view(schedule)->control_retry_count >= CONTROL_REPLY_RETRIES) { \
                if (netchesszx_session_schedule_view(schedule)->decision_kind == 0u) { \
                    session_mqtt_failure_cleanup(); \
                    SESSION_APP_DISCONNECT(); \
                    (ok) = 0u; \
                    break; \
                } else if (netchesszx_session_schedule_view(schedule)->decision_kind == 1u) { \
                    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET_WAIT); \
                } else if (netchesszx_session_schedule_view(schedule)->decision_kind == 2u) { \
                    netchesszx_session_set_control_pending(CONTROL_PENDING_DRAW_WAIT); \
                } \
            } else { \
                if (!tcp_required(retry_pending_outgoing())) { (ok) = 0u; break; } \
                if (!CONTROL_IS_CANCEL(netchesszx_session_view()->control_pending) || \
                    netchesszx_session_view()->pending_local_ply != 0u) { \
                    netchesszx_session_schedule_inc_control_retry_count(schedule); \
                } \
            } \
            netchesszx_session_schedule_set_pending_retry_wait((schedule), (pending_period)); \
        } \
    } else { \
        SESSION_RETRY_RESET((schedule), (pending_period)); \
    } \
    if (!session_presence_reannounce(schedule)) { (ok) = 0u; } \
} while (0)

#define SESSION_MOVE_EVENT(event, payload, ply, move, notation, ping, result) do { \
    (result) = SESSION_DISPATCH_UNHANDLED; \
    if ((event) == NETCHESSZX_SESSION_EVENT_MOVE && \
        netchess_proto_parse_move((payload), (ply), sizeof(ply), (move), \
                                  sizeof(move), (notation), sizeof(notation))) { \
        uint16_t session_incoming_ply = parse_u16(ply); \
        uint8_t session_move_applied; \
        SESSION_APP_REMOTE_MOVE_WORKSPACE(); \
        (result) = SESSION_DISPATCH_HANDLED; \
        netchesszx_session_ping_rx_data(ping); \
        if (session_incoming_ply == 0u) { \
            SESSION_APP_REJECT_MOVE(); \
            goto session_nack_move; \
        } \
        if (session_incoming_ply <= netchesszx_session_view()->game_ply) { \
            if (!tcp_required(netchesszx_session_send_ack_move(ply))) { \
                (result) = SESSION_DISPATCH_EXIT; \
            } \
            break; \
        } \
        if ((!CONTROL_IS_CANCEL(netchesszx_session_view()->control_pending) && \
             (netchesszx_session_view()->control_pending || \
              netchesszx_session_view()->takeback_pending_ply != 0u)) || \
            SESSION_APP_CONFIRMATION() != CONFIRM_NONE) { \
            SESSION_APP_WAIT_FOR_ACK(); \
            if (!SESSION_APP_SEND_BUSY_MOVE_REPLY(ply, 0u)) { \
                (result) = SESSION_DISPATCH_EXIT; \
            } \
            break; \
        } \
        if (netchesszx_session_view()->game_over) { \
            if (!tcp_required(netchesszx_session_send_nack_move(ply))) { \
                (result) = SESSION_DISPATCH_EXIT; \
            } \
            break; \
        } \
        if (!netchesszx_session_view()->game_status_active) { \
            SESSION_APP_GAME_NOT_STARTED(); \
            goto session_nack_move; \
        } \
        if (netchesszx_session_view()->pending_local_ply != 0u) { \
            if (session_incoming_ply == netchesszx_session_view()->pending_local_ply && \
                spectrum_streq((move), netchesszx_session_view()->pending_local_move)) { \
                SESSION_APP_WAIT_FOR_ACK(); \
                if (!SESSION_APP_SEND_BUSY_MOVE_REPLY(ply, 0u)) { \
                    (result) = SESSION_DISPATCH_EXIT; \
                } \
                break; \
            } \
            if (session_incoming_ply == \
                (uint16_t)(netchesszx_session_view()->pending_local_ply + 1u)) { \
                SESSION_APP_APPLY_PENDING_MOVE(0); \
            } \
            if (netchesszx_session_view()->pending_local_ply != 0u) { \
                SESSION_APP_WAIT_FOR_ACK(); \
                goto session_nack_move; \
            } \
        } \
        if (SESSION_APP_HAS_LOCAL_TURN()) { \
            SESSION_APP_OPPONENT_TURN(); \
            goto session_nack_move; \
        } \
        if (session_incoming_ply != \
            (uint16_t)(netchesszx_session_view()->game_ply + 1u)) { \
            SESSION_APP_REJECT_MOVE(); \
            memcpy((payload), "NACK", 4u); \
            memcpy((payload) + 6u + netchesszx_asm_mqtt_strlen8(ply), \
                   "SYNC", 5u); \
            if (!session_tcp_reply(payload)) { \
                (result) = SESSION_DISPATCH_EXIT; \
            } \
            break; \
        } \
        if (!SESSION_APP_MOVE_IS_LEGAL(move)) { \
            SESSION_APP_REJECT_MOVE(); \
            goto session_nack_move; \
        } \
        SESSION_APP_APPLY_REMOTE_MOVE(session_incoming_ply, move, notation, \
                                      session_move_applied); \
        if (session_move_applied) { \
            /* ACK once the board holds the move. A failed send still \
               finishes (log, journal, checkpoint) before the link drops. */ \
            session_move_applied = netchesszx_session_send_ack_move(ply); \
            SESSION_APP_FINISH_REMOTE_MOVE(ply, move); \
            if (!tcp_required(session_move_applied)) { \
                (result) = SESSION_DISPATCH_EXIT; \
            } \
            break; \
        } \
session_nack_move: \
        if (!tcp_required(netchesszx_session_send_nack_move(ply))) { \
            (result) = SESSION_DISPATCH_EXIT; \
        } \
    } \
} while (0)

/* OVER: only the host offers a new game, so requests never cross between
   two current builds. The guest waits for that offer with the statistics
   one key away; BREAK leaves on either side. */
static void session_game_over_idle(void)
{
    if (netchesszx_session_is_host()) {
        SESSION_APP_CONFIRM(CONFIRM_RESTART_GAME);
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_RESTART_GAME_CONFIRM);
    } else {
        SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_RESTART_WAIT);
    }
}
#define SESSION_GAME_OVER_IDLE() session_game_over_idle()

#define SESSION_ACCEPT_CONFIRM(done, failed) do { \
        if (SESSION_APP_CONFIRMATION() == CONFIRM_RESET_SEND || \
            SESSION_APP_CONFIRMATION() == CONFIRM_RESTART_GAME) { \
            if (!netchesszx_session_send_text(msg_reset_wire)) { \
                session_mqtt_failure_cleanup(); \
                goto failed; \
            } \
            netchesszx_session_set_control_pending(CONTROL_PENDING_RESET); \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_WAIT_FOR_ACK(); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_RESET_ACCEPT) { \
            if (netchesszx_session_view()->control_pending == \
                CONTROL_PENDING_DRAW_INCOMING) { \
                if (!send_draw_reply(1u)) { goto done; } \
                accepted_draw_end(); \
            } else if (!session_tcp_reply(msg_ack_reset)) { \
                goto done; \
            } else { \
                accepted_reset_start(); \
            } \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_RESIGN_SEND) { \
            if (!session_tcp_send(msg_resign_wire)) { goto done; } \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_END_LOCAL_RESIGN(); \
            netchesszx_session_set_resign_pending(1u); \
            SESSION_APP_SHOW_WAITING_RESIGN_ACK(); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_DRAW_SEND) { \
            if (!session_tcp_send(msg_draw)) { goto done; } \
            netchesszx_session_set_control_pending(CONTROL_PENDING_DRAW_SENT); \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_WAIT_FOR_ACK(); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_SEND) { \
            if (!tcp_required(send_takeback_wire(SESSION_APP_TAKEBACK_PLY()))) { \
                goto done; \
            } \
            netchesszx_session_set_takeback_pending_ply(SESSION_APP_TAKEBACK_PLY()); \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_TAKEBACK_SENT); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT) { \
            if (!session_tcp_send(msg_restore_ry)) { goto done; } \
            netchesszx_session_set_restore_rx_mask(RESTORE_RX_RECEIVE); \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_LOAD_ACCEPTED); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_ACCEPT) { \
            uint16_t session_accepted_ply = \
                netchesszx_session_view()->takeback_pending_ply; \
            SESSION_APP_BOARD_APPLY(SESSION_APP_APPLY_TAKEBACK(), \
                SESSION_APP_BOARD_LOST(); \
                goto failed); \
            if (!session_tcp_move_reply(1u, session_accepted_ply)) { goto done; } \
            netchesszx_session_set_last_accepted_takeback_ply(session_accepted_ply); \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_TAKEBACK_DONE); \
        } else { \
            /* fall through to application-only confirmation */ \
            break; \
        } \
        SESSION_APP_SUPPRESS_KEY(); \
        goto done; \
} while (0)

#define SESSION_REJECT_CONFIRM(done, failed) do { \
        uint8_t session_was_restart = \
            (uint8_t)(SESSION_APP_CONFIRMATION() == CONFIRM_RESTART_GAME); \
        if (SESSION_APP_CONFIRMATION() == CONFIRM_RESET_ACCEPT) { \
            if (netchesszx_session_view()->control_pending == \
                CONTROL_PENDING_DRAW_INCOMING) { \
                netchesszx_session_set_control_pending(0u); \
                netchesszx_session_set_last_control_accept(CONTROL_REJECT_DRAW); \
                if (!send_draw_reply(0u)) { goto done; } \
            } else if (!session_tcp_reply(msg_nack_reset)) { \
                goto done; \
            } else if (netchesszx_session_view()->game_over) { \
                /* Declining a new game ends the session, as N on the \
                   host's own offer does. */ \
                SESSION_APP_CLEAR_CONFIRMATION(); \
                SESSION_APP_DISCONNECT_SETUP(); \
                session_was_restart = 1u; \
            } else { \
                note_answer_replay(NETCHESSZX_SESSION_EVENT_RESET); \
                SESSION_APP_REFRESH_STATUS(); \
            } \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTART_GAME) { \
            SESSION_APP_CLEAR_CONFIRMATION(); \
            SESSION_APP_DISCONNECT_SETUP(); \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT) { \
            if (!netchesszx_session_send_text(msg_restore_rn)) { \
                session_mqtt_failure_cleanup(); \
                goto failed; \
            } \
            netchesszx_session_set_restore_rx_mask(0u); \
            note_answer_replay(NETCHESSZX_SESSION_EVENT_RESTORE_RQ); \
            SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_DECLINED); \
            session_was_restart = 1u; \
        } else if (SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_ACCEPT) { \
            if (!session_tcp_move_reply( \
                    0u, netchesszx_session_view()->takeback_pending_ply)) { \
                goto done; \
            } \
            note_answer_replay(NETCHESSZX_SESSION_EVENT_TAKEBACK); \
            netchesszx_session_set_takeback_pending_ply(0u); \
        } \
        SESSION_APP_CLEAR_CONFIRMATION(); \
        if (!session_was_restart) { \
            SESSION_APP_CLEAR_NOTICE(); \
            SESSION_APP_REFRESH_STATUS(); \
        } \
        SESSION_APP_SUPPRESS_KEY(); \
        goto done; \
} while (0)

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
static uint8_t session_cancel_remote_control(uint8_t reset) __z88dk_fastcall __naked
{
    (void)reset;
#asm
    ld c,l
    ld a,(_netchesszx_session_state+14)
    ld b,a
    ld a,l
    or a
    jr z,session_cancel_ctrl_draw
    ld a,b
    or a
    jr nz,session_cancel_ctrl_unmatched
    ld a,(_confirm_action)
    cp 3
    jr nz,session_cancel_ctrl_unmatched
    jr session_cancel_ctrl_matched
session_cancel_ctrl_draw:
    ld a,b
    cp 3
    jr nz,session_cancel_ctrl_unmatched
session_cancel_ctrl_matched:
    set 1,c
    xor a
    ld (_confirm_action),a
    ld (_netchesszx_session_state+14),a
    jr session_cancel_ctrl_reply
session_cancel_ctrl_unmatched:
    ld a,2
    sub c
    ld b,a
    ld a,(_netchesszx_session_state+8)
    cp b
    jr nz,session_cancel_ctrl_reply
    set 2,c
session_cancel_ctrl_reply:
    ld a,c
    push af
    bit 0,c
    jr z,session_cancel_ctrl_draw_reply
    ld hl,_NETCHESS_PROTO_NACK_RESET
    bit 2,c
    jr z,session_cancel_ctrl_reset_reply
    inc hl
session_cancel_ctrl_reset_reply:
    call _session_tcp_reply
    jr session_cancel_ctrl_sent
session_cancel_ctrl_draw_reply:
    ld a,c
    and 4
    rrca
    rrca
    ld l,a
    call _send_draw_reply
session_cancel_ctrl_sent:
    ld a,l
    or a
    pop bc
    jr z,session_cancel_ctrl_exit
    bit 1,b
    jr z,session_cancel_ctrl_handled
    bit 0,b
    jr z,session_cancel_ctrl_notice
    ld a,(_netchesszx_session_state+12)
    or a
    jr z,session_cancel_ctrl_notice
    call _session_game_over_idle
    jr session_cancel_ctrl_handled
session_cancel_ctrl_notice:
    ld a,b
    and 1
    xor 3
    dec a
    ld l,a
    call _notify_control_timeout
session_cancel_ctrl_handled:
    ld l,1
    ret
session_cancel_ctrl_exit:
    ld l,2
    ret
#endasm
}
#else
static uint8_t session_cancel_remote_control(uint8_t reset) __z88dk_fastcall
{
    uint8_t matched = reset
                          ? (uint8_t)(SESSION_APP_CONFIRMATION() == CONFIRM_RESET_ACCEPT &&
                                      netchesszx_session_view()->control_pending == 0u)
                          : (uint8_t)(netchesszx_session_view()->control_pending ==
                                      CONTROL_PENDING_DRAW_INCOMING);

    if (!matched && netchesszx_session_view()->last_control_accept ==
        (reset ? CONTROL_ACCEPT_RESET : CONTROL_ACCEPT_DRAW)) {
        if (!(reset ? session_tcp_reply(msg_ack_reset) : send_draw_reply(1u))) {
            goto dispatch_exit;
        }
        goto dispatch_handled;
    }
    if (matched) {
        SESSION_APP_CLEAR_CONFIRMATION();
        netchesszx_session_set_control_pending(0u);
    }
    if (!(reset ? session_tcp_reply(msg_nack_reset)
                : send_draw_reply(0u))) {
        goto dispatch_exit;
    }
    if (matched) {
        if (reset && netchesszx_session_view()->game_over) {
            /* OVER: the request expired on the peer; back to waiting
               (guest) or to this player's own offer (host). */
            SESSION_GAME_OVER_IDLE();
        } else {
            SESSION_APP_NOTICE_CONTROL_TIMEOUT(reset ? 1u : 2u);
        }
    }
    goto dispatch_handled;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
}
#endif

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
static uint8_t session_cancel_remote_takeback(uint16_t ply) __z88dk_fastcall __naked
{
    (void)ply;
#asm
    ld a,h
    or l
    jr z,session_cancel_tk_handled
    push hl
    ld de,(_netchesszx_session_state+6)
    or a
    sbc hl,de
    ld b,0
    jr nz,session_cancel_tk_accepted
    inc b
session_cancel_tk_accepted:
    pop hl
    ld c,0
    ld a,(_confirm_action)
    cp 8
    jr nz,session_cancel_tk_reply
    push hl
    ld de,(_netchesszx_session_state+4)
    or a
    sbc hl,de
    pop hl
    jr nz,session_cancel_tk_reply
    inc c
    xor a
    ld (_confirm_action),a
    ld (_netchesszx_session_state+4),a
    ld (_netchesszx_session_state+5),a
session_cancel_tk_reply:
    ld a,c
    rlca
    or b
    push af
    push hl
    ld a,b
    push af
    inc sp
    call _session_tcp_move_reply
    ld a,l
    or a
    pop bc
    jr z,session_cancel_tk_exit
    bit 1,b
    jr z,session_cancel_tk_handled
    ld l,3
    call _notify_control_timeout
session_cancel_tk_handled:
    ld l,1
    ret
session_cancel_tk_exit:
    ld l,2
    ret
#endasm
}
#else
static uint8_t session_cancel_remote_takeback(uint16_t ply) __z88dk_fastcall
{
    uint8_t accepted;
    uint8_t matched;

    if (ply == 0u) {
        goto dispatch_handled;
    }
    accepted = (uint8_t)(ply == netchesszx_session_view()->last_accepted_takeback_ply);
    matched = (uint8_t)(SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_ACCEPT &&
        ply == netchesszx_session_view()->takeback_pending_ply);
    if (matched) {
        SESSION_APP_CLEAR_CONFIRMATION();
        netchesszx_session_set_takeback_pending_ply(0u);
    }
    if (!session_tcp_move_reply(accepted, ply)) {
        goto dispatch_exit;
    }
    if (matched) {
        SESSION_APP_NOTICE_CONTROL_TIMEOUT(3u);
    }
    goto dispatch_handled;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
}

#endif

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
static uint8_t session_expire_decision(uint8_t kind) __z88dk_fastcall __naked
{
    (void)kind;
#asm
    ld a,l
    cp 5
    jr nc,session_expire_remote
    cp 4
    jr z,session_expire_local_load
    or 0x80
    ld (_netchesszx_session_state+14),a
    push af
    call _retry_pending_outgoing
    call _tcp_required
    ld a,l
    or a
    pop bc
    jr z,session_expire_failed
    ld l,b
    call _notify_control_timeout
    jr session_expire_ok
session_expire_local_load:
    ld hl,_msg_restore_rn
    call _session_tcp_send
    ld a,l
    or a
    ret z
    xor a
    ld (_netchesszx_session_state+10),a
    ld hl,_msg_load_cancelled
    push hl
    call _notify_info
    pop bc
    jr session_expire_ok
session_expire_remote:
    cp 7
    jr nz,session_expire_not_tk
    ld hl,(_netchesszx_session_state+4)
    call _session_cancel_remote_takeback
    jr session_expire_result
session_expire_not_tk:
    cp 8
    jr z,session_expire_load
    push af
    ld l,0
    cp 5
    jr nz,session_expire_control
    inc l
session_expire_control:
    call _session_cancel_remote_control
    ld a,l
    cp 2
    pop bc
    jr z,session_expire_failed
    ld a,b
    cp 6
    jr nz,session_expire_ok
    ld a,3
    ld (_netchesszx_session_state+8),a
    jr session_expire_ok
session_expire_load:
    ld hl,_msg_restore_rn
    call _session_tcp_reply
    ld a,l
    or a
    ret z
    xor a
    ld (_confirm_action),a
    ld (_netchesszx_session_state+10),a
    ld hl,0x0123
    call _spectrum_gui_notify_msg
session_expire_ok:
    ld l,1
    ret
session_expire_result:
    ld a,2
    sub l
    ld l,a
    ret
session_expire_failed:
    ld l,0
    ret
#endasm
}
#else
static uint8_t session_expire_decision(uint8_t kind) __z88dk_fastcall
{
    if (kind < 4u) {
        netchesszx_session_set_control_pending((uint8_t)(kind | 0x80u));
        if (!tcp_required(retry_pending_outgoing())) { return 0u; }
        SESSION_APP_NOTICE_CONTROL_TIMEOUT((uint8_t)(kind | 0x80u));
        return 1u;
    }
    if (kind == 4u) {
        if (!session_tcp_send(msg_restore_rn)) { return 0u; }
        netchesszx_session_set_restore_rx_mask(0u);
        SESSION_APP_NOTICE_LOAD_CANCELLED();
        return 1u;
    }
    if (kind == 7u) {
        return (uint8_t)(session_cancel_remote_takeback(
            netchesszx_session_view()->takeback_pending_ply) != SESSION_DISPATCH_EXIT);
    }
    if (kind == 8u) {
        if (!session_tcp_reply(msg_restore_rn)) { return 0u; }
        SESSION_APP_CLEAR_CONFIRMATION();
        netchesszx_session_set_restore_rx_mask(0u);
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_DECLINED);
        return 1u;
    }
    if (session_cancel_remote_control((uint8_t)(kind == 5u)) == SESSION_DISPATCH_EXIT) {
        return 0u;
    }
    if (kind == 6u) { netchesszx_session_set_last_control_accept(CONTROL_REJECT_DRAW); }
    return 1u;
}
#endif

/* Local input outcomes: ok = handled (result 1); cursor_ok = handled after
   stopping input and showing the cursor; done = result already set. */
#define SESSION_LOCAL_MOVE_REQUEST(input, move, result, done, ok, cursor_ok) do { \
    if (netchesszx_session_view()->game_over) { \
        SESSION_APP_GAME_NOT_STARTED(); \
        goto cursor_ok; \
    } \
    if ((netchesszx_session_view()->control_pending && \
         !CONTROL_IS_CANCEL(netchesszx_session_view()->control_pending)) || \
        netchesszx_session_view()->takeback_pending_ply != 0u) { \
        SESSION_APP_WAIT_FOR_ACK(); \
        goto cursor_ok; \
    } \
    if (!netchesszx_session_view()->game_status_active) { \
        if (!game_start_local(0u)) { goto ok; } \
        if (!netchesszx_session_view()->game_status_active) { \
            goto cursor_ok; \
        } \
    } \
    if (!SESSION_APP_HAS_LOCAL_TURN()) { \
        SESSION_APP_OPPONENT_TURN(); \
        goto cursor_ok; \
    } \
    if (netchesszx_session_view()->pending_local_ply != 0u) { \
        SESSION_APP_WAIT_FOR_ACK(); \
        goto cursor_ok; \
    } \
    SESSION_APP_ADD_INPUT_HISTORY(input); \
    (result) = send_local_move(move); \
    if ((result) == LOCAL_MOVE_REJECTED) { \
        SESSION_APP_SHOW_CURSOR(); \
    } else { \
        SESSION_APP_STOP_EDIT_CLEAR(); \
    } \
    (result) = (uint8_t)((result) != LOCAL_MOVE_NET_FAIL); \
    goto done; \
} while (0)

#define SESSION_LOCAL_COMMAND(input, ok, cursor_ok) do { \
    if (spectrum_streq((input), command_resign)) { \
        if (!local_action_ready()) { goto ok; } \
        SESSION_APP_CONFIRM(CONFIRM_RESIGN_SEND); \
        SESSION_APP_STOP_EDIT_CLEAR(); \
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_RESIGN_CONFIRM); \
    } else if (spectrum_streq((input), command_draw)) { \
        if (netchesszx_session_view()->pending_local_ply != 0u) { \
            SESSION_APP_WAIT_FOR_ACK(); \
            goto ok; \
        } \
        if (!local_action_ready()) { goto ok; } \
        SESSION_APP_CONFIRM(CONFIRM_DRAW_SEND); \
        SESSION_APP_STOP_EDIT_CLEAR(); \
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_DRAW_REQUEST); \
    } else if (spectrum_streq((input), command_takeback)) { \
        if (!local_action_ready()) { \
            goto cursor_ok; \
        } \
        if (netchesszx_session_view()->pending_local_ply != 0u) { \
            SESSION_APP_WAIT_FOR_ACK(); \
            goto cursor_ok; \
        } \
        if (!SESSION_APP_TAKEBACK_IS_LOCAL() || \
            SESSION_APP_TAKEBACK_PLY() != netchesszx_session_view()->game_ply) { \
            SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_NO_TAKEBACK); \
            goto cursor_ok; \
        } \
        SESSION_APP_CONFIRM(CONFIRM_TAKEBACK_SEND); \
        SESSION_APP_STOP_EDIT_CLEAR(); \
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_TAKEBACK_CONFIRM); \
    } else { \
        break; \
    } \
    goto ok; \
} while (0)

#define SESSION_LOCAL_CHAT_READY(ok) do { \
    if (netchesszx_session_view()->pending_local_ply != 0u) { \
        SESSION_APP_WAIT_FOR_ACK(); \
        goto ok; \
    } \
} while (0)

#define SESSION_PEER_GATE(cancel, done) do { \
    if (!netchesszx_session_peer_view()) { \
        if (cancel) { \
            SESSION_APP_DISCONNECT_SETUP(); \
            SESSION_APP_SUPPRESS_KEY(); \
        } else { \
            SESSION_APP_NOTICE_WAIT(); \
        } \
        goto done; \
    } \
} while (0)

#define SESSION_TRANSFER_GATE(cancel, done, failed) do { \
    if (restore_transfer_pending()) { \
        if (cancel) { \
            if (restore_cancel_pending_request()) { goto done; } \
            goto failed; \
        } else { \
            SESSION_APP_NOTICE_WAIT_MSG(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL); \
            SESSION_APP_SUPPRESS_KEY(); \
        } \
        goto done; \
    } \
} while (0)

#define SESSION_EDITOR_GATE(done) do { \
    if (netchesszx_session_view()->pending_local_ply != 0u) { \
        SESSION_APP_WAIT_FOR_ACK(); \
        goto done; \
    } \
} while (0)

#define SESSION_GAME_OVER_GATE(done) do { \
    if (netchesszx_session_view()->game_over) { \
        SESSION_APP_HIDE_MENU(); \
        if (netchesszx_session_view()->resign_pending) { \
            SESSION_APP_SHOW_WAITING_RESIGN_ACK(); \
        } else if (netchesszx_session_view()->control_pending) { \
            SESSION_APP_WAIT_FOR_ACK(); \
        } else { \
            SESSION_GAME_OVER_IDLE(); \
        } \
        SESSION_APP_SUPPRESS_KEY(); \
        goto done; \
    } \
} while (0)

/* Reached only with no Y/N prompt up, so session_busy() adds no case. */
#define SESSION_RESET_REQUEST() do { \
    if (!netchesszx_session_view()->game_status_active) { \
        SESSION_APP_GAME_NOT_STARTED(); \
    } else if (session_busy()) { \
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_RESET_WAIT); \
    } else { \
        SESSION_CONFIRM_PROMPT(CONFIRM_RESET_SEND, SPECTRUM_GUI_MSG_RESET_CONFIRM); \
        SESSION_APP_SUPPRESS_KEY(); \
    } \
} while (0)

static void session_send_mach(void)
{
#ifndef NETCHESSZX_HOST_SESSION_TEST
    /* Advisory identity never changes session control flow. */
    (void)netchesszx_session_send_text(LOCAL_MACH_TEXT);
    if (netchesszx_player_name[0] != '\0') {
        (void)netchesszx_session_send_text(netchesszx_player_name_line);
    }
#endif
}

static uint8_t session_has_live_peer(void)
{
    return (uint8_t)(netchesszx_session_view()->game_status_active || netchesszx_session_view()->start_pending ||
                     netchesszx_session_peer_view());
}

static uint8_t session_show_ready(uint8_t wait_msg)
{
    session_send_mach();
    SESSION_APP_SHOW_READY(wait_msg);
    return SESSION_DISPATCH_HANDLED;
}

static uint8_t session_presence_reannounce(netchesszx_session_schedule_t *schedule)
{
    uint8_t mqtt_is_host = netchesszx_session_is_host();

    if (netchesszx_transport_is_mqtt() &&
        !netchesszx_session_view()->game_status_active &&
        !netchesszx_session_view()->game_over &&
        mqtt_is_host != netchesszx_session_peer_view()) {
        if (netchesszx_session_schedule_view(schedule)->mqtt_setup_reannounce_wait != 0u) {
            netchesszx_session_schedule_tick_mqtt_setup_reannounce_wait(schedule);
        }
        if (netchesszx_session_schedule_view(schedule)->mqtt_setup_reannounce_wait == 0u) {
            if (!netchesszx_session_mqtt_publish_setup(
                    mqtt_is_host ? NETCHESSZX_SESSION_MQTT_SETUP_RETAINED : 0u)) {
                SESSION_APP_DISCONNECT();
                return 0u;
            }
            netchesszx_session_schedule_set_mqtt_setup_reannounce_wait(schedule, MQTT_SETUP_REANNOUNCE_TICKS);
        }
    } else {
        netchesszx_session_schedule_set_mqtt_setup_reannounce_wait(schedule, (uint8_t)(!mqtt_is_host &&
                                     !netchesszx_session_view()->game_status_active &&
                                     !netchesszx_session_view()->game_over)
            ? MQTT_SETUP_REANNOUNCE_TICKS : 0u);
    }

    if (!netchesszx_transport_is_mqtt() &&
        !netchesszx_session_view()->game_status_active &&
        !netchesszx_session_peer_view()) {
        if (netchesszx_session_schedule_view(schedule)->direct_hello_reannounce_wait != 0u) {
            netchesszx_session_schedule_tick_direct_hello_reannounce_wait(schedule);
        }
        if (netchesszx_session_schedule_view(schedule)->direct_hello_reannounce_wait == 0u) {
            if (!netchesszx_session_direct_send_hello()) {
                SESSION_APP_DISCONNECT();
                return 0u;
            }
            netchesszx_session_schedule_set_direct_hello_reannounce_wait(schedule, DIRECT_HELLO_REANNOUNCE_POLLS);
        }
    } else {
        netchesszx_session_schedule_set_direct_hello_reannounce_wait(schedule, 0u);
    }
    return 1u;
}

static uint8_t session_presence_handle_event(netchesszx_session_event_t event,
                                             char *payload,
                                             uint8_t retained)
{
    uint8_t host_flags;
    uint8_t bad_color;

    if (NETCHESSZX_SESSION_EVENT_IS_MACH(event)) {
        uint8_t platform = NETCHESSZX_SESSION_EVENT_MACH_PLATFORM(event);
        if (!netchesszx_session_peer_view()) {
            goto dispatch_handled;
        }
        if (platform == NETCHESSZX_SESSION_EVENT_IDENTITY_NAME) {
            /* The classifier validated the whole "MNAME <name>" payload. */
            SESSION_APP_SHOW_PEER_NAME(payload + NETCHESS_PROTO_NAME_PREFIX_LEN);
        } else {
            SESSION_APP_SHOW_PEER_MACHINE(platform);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_HOST_BUSY) {
        SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_HOST_BUSY);
        goto dispatch_exit;
    }

    if (event == NETCHESSZX_SESSION_EVENT_DIRECT_HELLO) {
        uint8_t was_ready;

        if (!netchesszx_session_direct_apply_hello(payload)) {
            SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_BAD_DIRECT_HELLO);
            (void)netchesszx_session_send_text(msg_bye);
            SESSION_APP_DISCONNECT();
            goto dispatch_exit;
        }
        spectrum_link_direct_peer_mark_valid();
        was_ready = netchesszx_session_peer_view();
        netchesszx_session_peer_mark_ready();
        /* Answer the first valid HELLO after link-up. This recovers either
           side's initial lost HELLO before both peers stop re-announcing.
           Once ready, consume duplicates without creating an echo loop. */
        if (was_ready) {
            goto dispatch_handled;
        }
        if (!tcp_required(netchesszx_session_direct_send_hello())) {
            goto dispatch_exit;
        }
        session_send_mach();
        SESSION_APP_DIRECT_READY(netchesszx_session_is_host());
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_MQTT_EMPTY) {
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE) {
        if (session_has_live_peer()) {
            SESSION_APP_MQTT_PEER_LOST();
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_LOCAL_OFFLINE) {
        if (netchesszx_session_identity_mqtt_session_id() != 0u &&
            (netchesszx_session_view()->game_status_active || netchesszx_session_peer_view())) {
            (void)netchesszx_session_mqtt_publish_presence();
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST) {
        if (session_has_live_peer()) {
            goto dispatch_handled;
        }
        SESSION_APP_PREPARE_ROOM_CONFLICT();
        netchesszx_session_peer_reset();
        netchesszx_session_set_start_pending(0u);
        SESSION_APP_SHOW_ROOM_CONFLICT();
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_HOST) {
        host_flags = netchesszx_session_mqtt_host_flags(payload,
                                                        (uint8_t)(netchesszx_session_view()->game_status_active || netchesszx_session_view()->game_over),
                                                        retained,
                                                        &bad_color);
        if (bad_color) {
            SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_BAD_COLOR);
            goto dispatch_handled;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_NEW_LIVE_SESSION) {
            uint8_t restore_pending = (uint8_t)(
                restore_transfer_pending() ||
                SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT);

            netchesszx_session_set_restore_rx_mask(0u);
            if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT) {
                SESSION_APP_CLEAR_CONFIRMATION();
            }
            if (restore_pending) {
            SESSION_APP_NOTICE_LOAD_CANCELLED();
            }
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_COLOR_CHANGED) {
            SESSION_APP_REFRESH_SIDE();
        }
        if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE) &&
            !netchesszx_session_mqtt_activate_side()) {
            SESSION_APP_DISCONNECT();
            goto dispatch_exit;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT) {
            /* Retained host announcement: the host won't re-announce live
               mid-game, so learn which seat we'd take and probe it (subscribe
               only, no claim). An occupied seat replies with a retained O ->
               MQTT_SEAT_TAKEN -> BUSY, instead of hanging on "waiting". */
            if (!netchesszx_session_view()->mqtt_seat_probed && !netchesszx_session_view()->game_status_active &&
                !netchesszx_session_identity_host_color_ready() &&
                !netchesszx_session_is_host()) {
                uint8_t host_color;
                uint16_t probe_session;

                if (netchesszx_session_mqtt_parse_host_payload(payload,
                                                               &host_color,
                                                               &probe_session)) {
                    /* A retained H supplies the probe identity, not readiness. */
                    netchesszx_session_probe_identity(host_color, probe_session);
                    if (!netchesszx_session_mqtt_probe_seat()) {
                        SESSION_APP_DISCONNECT();
                        goto dispatch_exit;
                    }
                    netchesszx_session_set_mqtt_seat_probed(1u);
                }
            }
            SESSION_APP_WAITING_OPPONENT();
            goto dispatch_handled;
        }
        if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_PUBLISH_SETUP) &&
             !netchesszx_session_mqtt_publish_setup(0u)) {
            SESSION_APP_DISCONNECT();
            goto dispatch_exit;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT) {
            return session_show_ready(SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_SEAT_TAKEN) {
        /* Guest joining a room whose seat is already held (retained O for
           our side): report BUSY and leave; announcing would steal the
           legitimate guest's presence slot. */
        if (netchesszx_session_view()->game_status_active || netchesszx_session_peer_view()) {
            goto dispatch_handled;
        }
        SESSION_APP_HOST_BUSY_EXIT();
        goto dispatch_exit;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY) {
        if (netchesszx_session_view()->game_status_active) {
            /* Don't answer with BYE: the shared out-topic also reaches the
               legitimate guest and would kill the live game. The stray
               joiner self-detects via the retained presence slot. */
            SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_GAME_ALREADY_ACTIVE);
            goto dispatch_handled;
        }
        if (netchesszx_session_peer_view()) {
            if (!netchesszx_session_mqtt_publish_setup(0u)) {
                SESSION_APP_DISCONNECT();
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        netchesszx_session_peer_mark_ready();
        if (!netchesszx_session_mqtt_publish_setup(0u)) {
            SESSION_APP_DISCONNECT();
            goto dispatch_exit;
        }
        return session_show_ready(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    }
    if (event == NETCHESSZX_SESSION_EVENT_BYE) {
        /* Only a BYE the peer sent carries text: a chosen exit, which ends a
           live game for recovery. MQTT's logical BYE after a lost PING is
           empty and keeps the resume offer. */
        if (payload[0] != '\0') {
            SESSION_APP_PEER_LEFT();
        }
        if (netchesszx_transport_is_mqtt()) {
            if (session_has_live_peer()) {
                if (payload[0] != '\0' &&
                    netchesszx_session_is_host() &&
                    !netchesszx_session_mqtt_publish_offline(
                        NETCHESSZX_SESSION_ROUTE_PRESENCE_PEER)) {
                    (void)netchesszx_session_mqtt_publish_offline(
                        NETCHESSZX_SESSION_ROUTE_PRESENCE);
                    SESSION_APP_DISCONNECT();
                    goto dispatch_exit;
                }
                SESSION_APP_MQTT_PEER_LOST();
            }
            goto dispatch_handled;
        }
        SESSION_APP_DISCONNECT();
        goto dispatch_exit;
    }
    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
}

static uint8_t restore_dispatch_reply(const char *text) __z88dk_fastcall
{
    return session_tcp_send(text)
        ? SESSION_DISPATCH_HANDLED : SESSION_DISPATCH_EXIT;
}

static uint8_t restore_handle_event(netchesszx_session_event_t event,
                                    char *payload)
{
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RQ) {
        if (!netchesszx_session_peer_view()) {
            goto dispatch_handled;
        }
        if ((netchesszx_session_view()->restore_rx_mask & RESTORE_RX_APPLIED) != 0u) {
            netchesszx_session_set_restore_rx_mask(0u);
        }
        if (SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT) {
            goto dispatch_handled;
        }
        if ((netchesszx_session_view()->restore_rx_mask & RESTORE_RX_RECEIVE) != 0u) {
            return restore_dispatch_reply(msg_restore_ry);
        }
        /* A non-zero mask also covers restore_transfer_pending(); a retry of
           the RQ just declined gets RN again. */
        if (session_busy() || netchesszx_session_view()->restore_rx_mask != 0u ||
            answer_replay_live()) {
            return restore_dispatch_reply(msg_restore_rn);
        }
        SESSION_CONFIRM_PROMPT(CONFIRM_RESTORE_ACCEPT, SPECTRUM_GUI_MSG_RESTORE_REQUEST);
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RY) {
        if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_PENDING) == 0u) {
            goto dispatch_handled;
        }
        if (restore_send_chunks()) {
            goto dispatch_retry;
        }
        SESSION_APP_DISCONNECT();
        goto dispatch_exit;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RN) {
        if (restore_transfer_pending() ||
            SESSION_APP_CONFIRMATION() == CONFIRM_RESTORE_ACCEPT ||
            (netchesszx_session_view()->restore_rx_mask & RESTORE_RX_RECEIVE) != 0u) {
            SESSION_APP_CLEAR_CONFIRMATION();
            netchesszx_session_set_restore_rx_mask(0u);
            SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_DECLINED);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RA) {
        if ((netchesszx_session_view()->restore_rx_mask & RESTORE_TX_AWAIT_ACK) != 0u) {
            SESSION_APP_RESTORE_WORKSPACE();

            if (!SESSION_APP_RESTORE_DECODE_VALID()) {
                SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_FAIL);
            } else {
                SESSION_APP_BOARD_APPLY(
                    SESSION_APP_APPLY_LOADED(&session_app_meta),
                    goto board_lost);
                SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_LOAD_OK);
            }
            netchesszx_session_set_restore_rx_mask(0u);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RS) {
        uint8_t result;

        result = SESSION_APP_RESTORE_CHUNK(payload);
        if (result == RESTORE_CHUNK_REJECT) {
            return restore_dispatch_reply(msg_restore_rn);
        }
        if (result == RESTORE_CHUNK_REACK) {
            return restore_dispatch_reply(msg_restore_ra);
        }
        if (result == RESTORE_CHUNK_COMPLETE) {
            SESSION_APP_RESTORE_WORKSPACE();

            if (!SESSION_APP_RESTORE_DECODE_VALID()) {
                uint8_t rc = restore_dispatch_reply(msg_restore_rn);

                netchesszx_session_set_restore_rx_mask(0u);
                if (rc != SESSION_DISPATCH_EXIT) {
                    SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_LOAD_FAIL);
                }
                return rc;
            }
            SESSION_APP_BOARD_APPLY(SESSION_APP_RESTORE_APPLY(),
                                    goto board_lost);
            netchesszx_session_set_restore_rx_mask(SESSION_APP_RESTORE_CACHE()
                ? RESTORE_RX_APPLIED : 0u);
            if (restore_dispatch_reply(msg_restore_ra) ==
                SESSION_DISPATCH_EXIT) {
                goto dispatch_exit;
            }
            SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_LOAD_OK);
        }
        goto dispatch_handled;
    }
    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
#ifdef SESSION_APP_BOARD_LOST
board_lost:
    /* BOARD could not load: this board no longer matches the peer's. Only
       ports that can lose BOARD define this exit (SESSION_APP_BOARD_APPLY). */
    SESSION_APP_BOARD_LOST();
    SESSION_APP_DISCONNECT();
#endif
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
dispatch_retry:
    return SESSION_DISPATCH_RETRY_RESET;
}

static uint8_t session_control_handle_event(netchesszx_session_event_t event,
                                           char *payload)
{
    if (event >= NETCHESSZX_SESSION_EVENT_RESTORE_RQ &&
        event <= NETCHESSZX_SESSION_EVENT_RESTORE_RA) {
        return restore_handle_event(event, payload);
    }
    if (event == NETCHESSZX_SESSION_EVENT_ACK_GAME_START) {
        if (netchesszx_session_view()->start_pending) {
            SESSION_APP_START_GAME();
            SESSION_APP_NOTICE_SUCCESS_MSG(SPECTRUM_GUI_MSG_GAME_STARTED);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_RESET) {
        if (CONTROL_IS_RESET(netchesszx_session_view()->control_pending)) {
            netchesszx_session_set_control_pending(0u);
            SESSION_APP_RESET_START();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_DRAW) {
        if (CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending)) {
            accepted_draw_end();
        }
        goto dispatch_handled;
    }

    /* A move reply answers the pending takeback or the pending move of its
       ply; only a takeback this side asked for takes an ACK or a NACK (the
       peer's request waits for Y/N), and a NACK carries no notation. */
    if (event == NETCHESSZX_SESSION_EVENT_ACK_MOVE ||
        event == NETCHESSZX_SESSION_EVENT_NACK_MOVE) {
        uint8_t ack = (uint8_t)(event == NETCHESSZX_SESSION_EVENT_ACK_MOVE);
        char ply[6];
        char notation[8];

        if (netchess_proto_parse_ply_tail(
                netchess_after_prefix(payload, ack ? NETCHESS_PROTO_ACK_PREFIX
                                                   : NETCHESS_PROTO_NACK_PREFIX),
                ply, sizeof(ply), notation,
                ack ? (uint8_t)sizeof(notation) : 0u)) {
            uint16_t reply_ply = parse_u16(ply);

            if (SESSION_LOCAL_TAKEBACK_PENDING() &&
                reply_ply == netchesszx_session_view()->takeback_pending_ply) {
                netchesszx_session_set_control_pending(0u);
                if (ack) {
                    SESSION_APP_BOARD_APPLY(SESSION_APP_APPLY_TAKEBACK(),
                                            goto board_lost);
                    SESSION_APP_NOTICE_INFO_MSG(SPECTRUM_GUI_MSG_TAKEBACK_DONE);
                } else {
                    netchesszx_session_set_takeback_pending_ply(0u);
                    SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_TAKEBACK_REJECTED);
                    SESSION_APP_REFRESH_STATUS();
                }
                goto dispatch_live;
            } else if (netchesszx_session_view()->pending_local_ply != 0u &&
                       reply_ply == netchesszx_session_view()->pending_local_ply) {
                if (ack) {
                    SESSION_APP_APPLY_PENDING_MOVE(notation);
                } else {
                    SESSION_APP_CLEAR_PENDING_MOVE();
                    SESSION_APP_REJECT_MOVE();
                    SESSION_APP_SHOW_CURSOR();
                    HOST_SESSION_OBSERVE_MOVE_REJECTION(payload);
                }
                goto dispatch_live;
            }
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_GAME_START) {
        if (netchesszx_session_view()->start_pending) {
            netchesszx_session_set_start_pending(0u);
            SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_START_REJECTED);
            SESSION_APP_SHOW_ENDPOINT();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_RESET) {
        if (CONTROL_IS_RESET(netchesszx_session_view()->control_pending)) {
            uint8_t pending = netchesszx_session_view()->control_pending;
            uint8_t cancelled = (uint8_t)(pending == CONTROL_PENDING_RESET_CANCEL);
            netchesszx_session_set_control_pending(0u);
            if (netchesszx_session_view()->game_over && !cancelled) {
                /* The opponent declined a new game: the session ends.
                   An expired request (its NACK answers our CANCEL) keeps
                   OVER and the next key offers again. */
                SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_RESET_REJECTED);
                SESSION_APP_DISCONNECT_SETUP();
                goto dispatch_exit;
            } else if (cancelled) {
                SESSION_APP_NOTICE_CONTROL_TIMEOUT(0x81u);
            } else {
                SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_RESET_REJECTED);
            }
            if (!netchesszx_session_view()->game_over) {
                SESSION_APP_REFRESH_STATUS();
            }
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_DRAW) {
        if (CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending)) {
            uint8_t cancelled = (uint8_t)(
                netchesszx_session_view()->control_pending == CONTROL_PENDING_DRAW_CANCEL);

            netchesszx_session_set_control_pending(0u);
            if (cancelled) {
                SESSION_APP_NOTICE_CONTROL_TIMEOUT(0x82u);
            } else {
                SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_DRAW_REJECTED);
            }
            SESSION_APP_REFRESH_STATUS();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_CANCEL_RESET ||
        event == NETCHESSZX_SESSION_EVENT_CANCEL_DRAW) {
        return session_cancel_remote_control(
            (uint8_t)(event == NETCHESSZX_SESSION_EVENT_CANCEL_RESET));
    }
    if (event == NETCHESSZX_SESSION_EVENT_CANCEL_TAKEBACK) {
        return session_cancel_remote_takeback(netchess_proto_takeback_ply(payload));
    }

    if (event == NETCHESSZX_SESSION_EVENT_RESET) {
        SESSION_REPLAY_RESET();
        if (SESSION_APP_CONFIRMATION() == CONFIRM_RESET_ACCEPT && netchesszx_session_view()->control_pending == 0u) {
            goto dispatch_handled;
        }
        if (netchesszx_session_view()->game_over) {
            if (CONTROL_IS_RESET(netchesszx_session_view()->control_pending)) {
                if (!session_tcp_reply(msg_ack_reset)) {
                    goto dispatch_exit;
                }
                accepted_reset_start();
            } else if (SESSION_APP_CONFIRMATION() != CONFIRM_NONE &&
                       SESSION_APP_CONFIRMATION() != CONFIRM_RESTART_GAME) {
                /* The local S:STATS/RESTART prompt is only an offer: the
                   peer's request supersedes it and asks this player. */
                if (!session_tcp_reply(msg_nack_reset)) {
                    goto dispatch_exit;
                }
            } else {
                SESSION_CONFIRM_PROMPT(CONFIRM_RESET_ACCEPT, SPECTRUM_GUI_MSG_RESTART_REQUEST);
            }
        } else if (netchesszx_session_view()->game_status_active &&
                  (CONTROL_IS_RESET(netchesszx_session_view()->control_pending) ||
                    netchesszx_session_view()->pending_local_ply != 0u || netchesszx_session_view()->takeback_pending_ply != 0u)) {
            if (!session_tcp_reply(msg_nack_reset_busy)) {
                goto dispatch_exit;
            }
        } else if (!netchesszx_session_view()->game_status_active || netchesszx_session_view()->control_pending ||
            SESSION_APP_CONFIRMATION() != CONFIRM_NONE) {
            if (!session_tcp_reply(msg_nack_reset)) {
                goto dispatch_exit;
            }
        } else {
            SESSION_CONFIRM_PROMPT(CONFIRM_RESET_ACCEPT, SPECTRUM_GUI_MSG_RESET_REQUEST);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_RESIGN) {
        /* ACK unconditionally: a retransmitted RESIGN (our ACK was lost)
           must be re-ACKed or the peer retries forever. */
        if (!session_tcp_reply(msg_ack_resign)) {
            goto dispatch_exit;
        }
        netchesszx_session_set_resign_pending(0u);
        if (netchesszx_session_view()->game_status_active) {
            SESSION_APP_END_OPPONENT_RESIGN();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_RESIGN) {
        if (netchesszx_session_view()->resign_pending) {
            netchesszx_session_set_resign_pending(0u);
            SESSION_APP_SHOW_LOCAL_RESIGN();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_DRAW) {
        if (netchesszx_session_view()->control_pending == CONTROL_PENDING_DRAW_INCOMING) {
            goto dispatch_handled;
        }
        /* The finished game's accepted DRAW survives later declined RESETs. */
        if (netchesszx_session_view()->last_control_accept == CONTROL_ACCEPT_DRAW &&
            netchesszx_session_view()->game_over) {
            if (!send_draw_reply(1u)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        /* A declined DRAW answers a crossed retransmission the same way,
           ahead of the crossed-offer check, as the canonical reducer does. */
        if (netchesszx_session_view()->last_control_accept == CONTROL_REJECT_DRAW) {
            goto draw_nack;
        }
        if (CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending)) {
            if (!send_draw_reply(1u)) {
                goto dispatch_exit;
            }
            accepted_draw_end();
            goto dispatch_handled;
        } else if (netchesszx_session_view()->game_over || !netchesszx_session_view()->game_status_active ||
                   session_busy()) {
draw_nack:
            if (!send_draw_reply(0u)) {
                goto dispatch_exit;
            }
        } else {
            netchesszx_session_set_control_pending(CONTROL_PENDING_DRAW_INCOMING);
            SESSION_APP_CONFIRM(CONFIRM_RESET_ACCEPT);
            SESSION_APP_NOTICE_ERROR_MSG(SPECTRUM_GUI_MSG_OPPONENT_DRAW_REQUEST);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_TAKEBACK) {
        const char *requested_text = netchess_after_prefix(payload, msg_takeback_wire);
        uint16_t requested_ply = requested_text == 0 ? 0u : parse_u16(requested_text);

        if (requested_ply != 0u && SESSION_APP_CONFIRMATION() == CONFIRM_TAKEBACK_ACCEPT &&
            netchesszx_session_view()->takeback_pending_ply == requested_ply) {
            goto dispatch_handled;
        }

        /* Retransmit of an already-applied takeback (peer missed our ACK
           on the split ack-topic). Re-ACK idempotently instead of NACKing.
           Tracked explicitly: cleared on the next applied move and on
           reset/restore/game-over. */
        if (requested_ply != 0u &&
            requested_ply == netchesszx_session_view()->last_accepted_takeback_ply) {
            if (!session_tcp_move_reply(1u, requested_ply)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }

        /* answer_replay_live(): a retry of the takeback just declined. */
        if (requested_ply == 0u || !netchesszx_session_view()->game_status_active || netchesszx_session_view()->game_over ||
            session_busy() || SESSION_APP_TAKEBACK_IS_LOCAL() || answer_replay_live() ||
            requested_ply != netchesszx_session_view()->game_ply ||
            requested_ply != SESSION_APP_TAKEBACK_PLY()) {
            if (!session_tcp_move_reply(0u, requested_ply)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        /* The menu repaint reads no session state. */
        netchesszx_session_set_takeback_pending_ply(requested_ply);
        SESSION_CONFIRM_PROMPT(CONFIRM_TAKEBACK_ACCEPT, SPECTRUM_GUI_MSG_TAKEBACK_REQUEST);
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_GAME_START) {
        if (netchesszx_session_is_host()) {
            goto dispatch_handled;
        }
        if (netchesszx_transport_is_mqtt() &&
            !netchesszx_session_mqtt_can_accept_game_start()) {
            SESSION_APP_NOTICE_WAIT();
            goto dispatch_handled;
        }
        if (netchesszx_session_view()->start_pending || session_busy() ||
            netchesszx_session_view()->resign_pending || restore_transfer_pending()) {
            if (!SESSION_APP_SEND_BUSY_MOVE_REPLY(NETCHESS_PROTO_GAME_START, 1u)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        if (!netchesszx_session_direct_apply_start_side(payload)) {
            SESSION_APP_NOTICE_ERROR(NETCHESSZX_UI_ERROR_BAD_START);
            if (!session_tcp_control_reply("NACK GAME START BAD")) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        {
            uint8_t was_ready = netchesszx_session_peer_view();

            netchesszx_session_peer_mark_ready();
            if (!was_ready) {
                session_send_mach();
            }
        }
        /* ACK before the blocking piece reveal so the host starts in parallel. */
        if (!session_tcp_control_reply(msg_ack_game_start)) {
            goto dispatch_exit;
        }
        if (!netchesszx_session_view()->game_status_active || netchesszx_session_view()->game_over) {
            SESSION_APP_START_GAME();
            SESSION_APP_NOTICE_SUCCESS_MSG(SPECTRUM_GUI_MSG_GAME_STARTED);
        }
        goto dispatch_handled;
    }

    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
#ifdef SESSION_APP_BOARD_LOST
board_lost:
    /* BOARD could not load: this board no longer matches the peer's. */
    SESSION_APP_BOARD_LOST();
    SESSION_APP_DISCONNECT();
#endif
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
dispatch_live:
    return SESSION_DISPATCH_HANDLED_LIVE;
}
