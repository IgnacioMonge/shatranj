/* Compile the production coordinator without app.c or GUI/board stubs.
   Semantic effects are inert except those observed by the ordering checks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "spectrum/session/state.h"
#include "spectrum/session/identity.h"
#include "spectrum/session/direct.h"
#include "spectrum/session/event.h"
#include "spectrum/session/mqtt.h"
#include "spectrum/session/mqtt_wire.h"
#include "spectrum/session/outgoing.h"
#include "spectrum/session/ping.h"
#include "spectrum/session/poll.h"
#include "spectrum/transport/link.h"
#include "spectrum/platform/platform.h"
#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/savegame/savegame_format.h"
#include "spectrum/session/transitions.h"
#include "common/ui_messages.h"
#include "spectrum/config/session.h"

char netchesszx_player_name_line[NETCHESS_PROTO_NAME_LINE_SIZE] =
    NETCHESS_PROTO_NAME_PREFIX;
char netchesszx_peer_name[NETCHESS_PLAYER_NAME_SIZE];
/* Notice identifiers only; no GUI implementation is linked or replaced. */
#include "spectrum/ui/gui.h"

static uint8_t fixture_confirmation;
static uint16_t fixture_frame_count;
static uint8_t fixture_takeback_local;
static uint16_t fixture_takeback_ply;
static uint8_t fixture_chunk;
static uint8_t fixture_disconnected;
static uint8_t fixture_ended;
static uint8_t fixture_send_count;
static uint8_t fixture_fail_send;
static uint8_t fixture_offline;
static uint8_t fixture_started;
static uint8_t fixture_peer_left;
static uint8_t fixture_setup_exit;
static char fixture_sends[2][24];
static uint8_t fixture_finish_sends;
static uint8_t fixture_finish_disconnects;
uint8_t netchesszx_transport;
/* The BYE vectors reach the presence handler: its peer/role state. */
uint8_t netchesszx_session_peer_ready_state;
uint8_t netchesszx_session_role;

#define SESSION_APP_APPLY_LOADED(meta) (1u)
#define SESSION_APP_BUILD_LOADED(meta) 1u
#define SESSION_APP_BEGIN_SETUP_EXIT() ((void)0)
#define SESSION_APP_FINISH_SETUP_EXIT() ((void)0)

#define SESSION_APP_CONFIRMATION() fixture_confirmation
#define SESSION_APP_FRAME_COUNT() fixture_frame_count
#define SESSION_APP_CONFIRM(kind) (fixture_confirmation = (kind))
#define SESSION_APP_CLEAR_CONFIRMATION() (fixture_confirmation = CONFIRM_NONE)
#define SESSION_APP_HAS_LOCAL_TURN() 0u
#define SESSION_APP_TAKEBACK_IS_LOCAL() fixture_takeback_local
#define SESSION_APP_TAKEBACK_PLY() fixture_takeback_ply
#define SESSION_APP_RESTORE_CHUNK(frame) RESTORE_CHUNK_PARTIAL
#define SESSION_APP_RESTORE_WORKSPACE() ((void)0)
#define SESSION_APP_RESTORE_DECODE_VALID() 0u
#define SESSION_APP_RESTORE_APPLY() (1u)
#define SESSION_APP_RESTORE_CACHE() 0u
#define SESSION_APP_RESTORE_PREPARE_FRAME(chunk) (fixture_chunk = (chunk))
#define SESSION_APP_RESTORE_FRAME() (fixture_chunk ? "RS01" : "RS00")
#define SESSION_APP_DISCONNECT() (++fixture_disconnected)
#define SESSION_APP_DISCONNECT_WITH(message) ((void)0)
#define SESSION_APP_MQTT_PEER_LOST() ((void)0)
#define SESSION_APP_PEER_LEFT() (++fixture_peer_left)
#define SESSION_APP_SHOW_READY(message) ((void)0)
#define SESSION_APP_SHOW_PEER_MACHINE(platform) ((void)0)
#define SESSION_APP_SHOW_PEER_NAME(name) ((void)(name))
#define SESSION_APP_DIRECT_READY(host) ((void)0)
#define SESSION_APP_PREPARE_ROOM_CONFLICT() ((void)0)
#define SESSION_APP_SHOW_ROOM_CONFLICT() ((void)0)
#define SESSION_APP_REFRESH_SIDE() ((void)0)
#define SESSION_APP_WAITING_OPPONENT() ((void)0)
#define SESSION_APP_HOST_BUSY_EXIT() ((void)0)
#define SESSION_APP_NOTICE_ERROR(text) ((void)0)
#define SESSION_APP_NOTICE_ERROR_MSG(id) ((void)0)
#define SESSION_APP_NOTICE_INFO(text) ((void)0)
#define SESSION_APP_NOTICE_INFO_MSG(id) ((void)0)
#define SESSION_APP_NOTICE_SUCCESS_MSG(id) ((void)0)
#define SESSION_APP_NOTICE_WAIT() ((void)0)
#define SESSION_APP_NOTICE_CONTROL_TIMEOUT(kind) ((void)0)
#define SESSION_APP_REFRESH_STATUS() ((void)0)
#define SESSION_APP_SHOW_ENDPOINT() ((void)0)
#define SESSION_APP_SHOW_PERSISTENT(text) ((void)0)
#define SESSION_APP_HIDE_MENU() ((void)0)
#define SESSION_APP_SHOW_CURSOR() ((void)0)
#define SESSION_APP_SUPPRESS_KEY() ((void)0)
#define SESSION_APP_SUPPRESS_KEY_UNTIL_RELEASE(key) ((void)0)
#define SESSION_APP_DISCONNECT_SETUP() (++fixture_setup_exit)
#define SESSION_APP_CLEAR_NOTICE() ((void)0)
#define SESSION_APP_NOTICE_WAIT_MSG(id) ((void)0)
#define SESSION_APP_START_GAME() ((void)0)
#define SESSION_APP_RESET_START() (++fixture_started)
#define SESSION_APP_END_GAME(message) ((void)0)
#define SESSION_APP_END_DRAW() do { \
    ++fixture_ended; \
    netchesszx_session_game_ended(); \
} while (0)
#define SESSION_APP_END_OPPONENT_RESIGN() ((void)0)
#define SESSION_APP_END_LOCAL_RESIGN() ((void)0)
#define SESSION_APP_APPLY_TAKEBACK() (1u)
/* Classic's ports: an apply that returns 0 lost BOARD. */
#define SESSION_APP_BOARD_APPLY(apply, lost) do { \
    if (!(apply)) { \
        lost; \
    } \
} while (0)
#define SESSION_APP_BOARD_LOST() ((void)0)
#define SESSION_APP_APPLY_PENDING_MOVE(notation) ((void)0)
#define SESSION_APP_CLEAR_PENDING_MOVE() ((void)0)
#define SESSION_APP_REJECT_MOVE() ((void)0)
#define SESSION_APP_WAIT_FOR_ACK() ((void)0)
#define SESSION_APP_OPPONENT_TURN() ((void)0)
#define SESSION_APP_GAME_NOT_STARTED() ((void)0)
#define SESSION_APP_SEND_BUSY_MOVE_REPLY(ply, control) 1u
#define SESSION_APP_MOVE_IS_LEGAL(move) 1u
#define SESSION_APP_SHOW_LOCAL_MOVE(move) ((void)0)
#define SESSION_APP_CLEAR_MOVE_HINTS() ((void)0)
#define SESSION_APP_SHOW_LOCAL_RESIGN() ((void)0)
#define SESSION_APP_SHOW_WAITING_RESIGN_ACK() ((void)0)
#define SESSION_APP_NOTICE_LOAD_CANCELLED() ((void)0)
#define SESSION_APP_STOP_INPUT_SHOW_CURSOR() ((void)0)
#define SESSION_APP_STOP_EDIT_CLEAR() ((void)0)
#define SESSION_APP_ADD_INPUT_HISTORY(text) ((void)0)
#define SESSION_APP_REMOTE_MOVE_WORKSPACE() ((void)0)
#define SESSION_APP_APPLY_REMOTE_MOVE(incoming_ply, move, notation, applied) ((applied) = 1u)
#define SESSION_APP_FINISH_REMOTE_MOVE(ply_text, move) \
    (fixture_finish_sends = fixture_send_count, \
     fixture_finish_disconnects = fixture_disconnected)
#define HOST_SESSION_OBSERVE_MOVE_REJECTION(payload) ((void)0)

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "spectrum/session/transitions.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

uint8_t netchesszx_session_send_text(const char *text)
{
    assert(fixture_send_count < 2u);
    strcpy(fixture_sends[fixture_send_count++], text);
    return fixture_send_count != fixture_fail_send;
}

char *spectrum_net_payload_scratch(void)
{
    static char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    return payload;
}

uint8_t netchesszx_session_direct_send_hello(void)
{
    assert(!"unexpected HELLO in decision-clock tests");
    return 0u;
}

uint8_t spectrum_net_mqtt_publish_setup(uint8_t flags)
{
    (void)flags;
    assert(!"unexpected setup publication in decision-clock tests");
    return 0u;
}

uint8_t netchesszx_session_send_start_game(void)
{
    assert(!"unexpected START in decision-clock tests");
    return 0u;
}

uint8_t netchesszx_session_mqtt_publish_offline(uint8_t route)
{
    assert(route == NETCHESSZX_SESSION_ROUTE_PRESENCE);
    assert(fixture_disconnected == 0u);
    ++fixture_offline;
    return 1u;
}

uint8_t netchesszx_session_send_ack_text(const char *text)
{
    return netchesszx_session_send_text(text);
}

uint8_t netchesszx_session_send_ack_move(const char *ply)
{
    char reply[16] = "ACK ";

    return netchesszx_session_send_text(strcat(reply, ply));
}

uint8_t netchesszx_session_send_nack_move(const char *ply)
{
    char reply[16] = "NACK ";

    return netchesszx_session_send_text(strcat(reply, ply));
}

uint8_t netchesszx_asm_mqtt_strlen8(const char *text)
{
    return (uint8_t)strlen(text);
}

const char *netchess_mqtt_session_parse_u16_token(const char *p, uint16_t *out)
{
    *out = 0u;
    while (*p >= '0' && *p <= '9') {
        *out = (uint16_t)(*out * 10u + (uint16_t)(*p++ - '0'));
    }
    return p;
}

void netchesszx_session_ping_reset(netchesszx_session_ping_t *ping)
{
    (void)ping;
}

static void fixture_reset(uint8_t failed_send)
{
    memset(&netchesszx_session_state, 0, sizeof(netchesszx_session_state));
    fixture_send_count = 0u;
    fixture_takeback_local = 0u;
    fixture_takeback_ply = 0u;
    fixture_disconnected = 0u;
    fixture_ended = 0u;
    fixture_fail_send = failed_send;
    fixture_offline = 0u;
    fixture_started = 0u;
    fixture_peer_left = 0u;
    fixture_setup_exit = 0u;
    fixture_confirmation = CONFIRM_NONE;
}

/* A BYE the peer sent ends a live game for recovery before the link is torn
   down; MQTT's empty logical BYE after a lost PING never does. */
static void check_peer_bye_retires_recovery(void)
{
    char wire_bye[] = "BYE";
    char logical_bye[] = "";

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    fixture_reset(0u);
    netchesszx_session_state.game_status_active = 1u;
    assert(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_BYE,
                                         wire_bye, 0u) == SESSION_DISPATCH_EXIT);
    assert(fixture_peer_left == 1u && fixture_disconnected == 1u);

    netchesszx_transport = NETCHESSZX_TRANSPORT_MQTT;
    fixture_reset(0u);
    netchesszx_session_state.game_status_active = 1u;
    assert(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_BYE,
                                         logical_bye, 0u) == SESSION_DISPATCH_HANDLED);
    assert(fixture_peer_left == 0u && fixture_disconnected == 0u);

    /* Guest: the host path would also release the peer's retained seat. */
    fixture_reset(0u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    netchesszx_session_state.game_status_active = 1u;
    assert(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_BYE,
                                         wire_bye, 0u) == SESSION_DISPATCH_HANDLED);
    assert(fixture_peer_left == 1u && fixture_disconnected == 0u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
}

static uint8_t fixture_replay_reset(void)
{
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_RESET);
    SESSION_REPLAY_RESET();
    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
}

static void check_production_answer_replay(void)
{
    const uint8_t window = NETCHESSZX_SESSION_DIRECT_3S_POLLS;
    uint8_t polls;

    fixture_reset(0u);
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    accepted_reset_start();
    assert(fixture_started == 1u && fixture_confirmation == CONFIRM_NONE);
    assert(netchesszx_session_view()->control_pending == 0u);
    /* Longer than one retry period: the peer's next retry still meets it. */
    assert(window > NETCHESSZX_SESSION_DIRECT_REPLY_POLLS &&
           window > NETCHESSZX_SESSION_MQTT_REPLY_POLLS_AT(NETCHESSZX_SESSION_FRAME_HZ));
    assert(netchesszx_session_view()->answer_replay_polls == window);
    for (polls = 0u; polls < window; ++polls) {
        fixture_send_count = 0u;
        assert(fixture_replay_reset() == SESSION_DISPATCH_HANDLED);
        assert(strcmp(fixture_sends[0], "ACK RESET") == 0);
        assert(fixture_started == 1u && fixture_confirmation == CONFIRM_NONE);
        /* Received retransmissions do not consume or extend the idle window. */
        assert(netchesszx_session_view()->answer_replay_polls == window - polls);
        SESSION_REPLAY_DECAY();
    }
    fixture_send_count = 0u;
    assert(fixture_replay_reset() == SESSION_DISPATCH_UNHANDLED);
    assert(fixture_send_count == 0u);
    SESSION_REPLAY_DECAY();
    assert(netchesszx_session_view()->answer_replay_polls == 0u);

    note_answer_replay(NETCHESSZX_SESSION_EVENT_RESET);
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_CHAT);
    assert(netchesszx_session_view()->answer_replay_polls == 0u);
    assert(fixture_replay_reset() == SESSION_DISPATCH_UNHANDLED);

    /* The new game's first move ends the ACK replay. */
    note_answer_replay(NETCHESSZX_SESSION_EVENT_RESET);
    netchesszx_session_set_answer_replay_ack();
    netchesszx_session_set_game_ply(1u);
    assert(fixture_replay_reset() == SESSION_DISPATCH_UNHANDLED);
    assert(fixture_send_count == 0u);

    fixture_reset(1u);
    note_answer_replay(NETCHESSZX_SESSION_EVENT_RESET);
    netchesszx_session_set_answer_replay_ack();
    assert(fixture_replay_reset() == SESSION_DISPATCH_EXIT);
    assert(fixture_disconnected == 1u && fixture_started == 0u);
}

/* After the game the local S:STATS/RESTART offer must not refuse the peer's
   RESET: the request replaces it with this player's Y/N. Other pending
   confirmations still answer NACK RESET. */
/* Inline the static FSM at each constant event so dead handler branches
   (and their unlinked helpers) drop out regardless of call count. */
#define FIXTURE_FLATTEN __attribute__((flatten))

FIXTURE_FLATTEN static void check_game_over_reset_supersedes_restart_offer(void)
{
    char payload[] = "";

    fixture_reset(0u);
    netchesszx_session_game_ended();
    fixture_confirmation = CONFIRM_RESTART_GAME;
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_RESET);
    assert(session_control_handle_event(NETCHESSZX_SESSION_EVENT_RESET,
                                        payload) == SESSION_DISPATCH_HANDLED);
    assert(fixture_send_count == 0u);
    assert(fixture_confirmation == CONFIRM_RESET_ACCEPT);
    assert(fixture_started == 0u && fixture_disconnected == 0u);

    fixture_reset(0u);
    netchesszx_session_game_ended();
    fixture_confirmation = CONFIRM_RESET_SEND;
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_RESET);
    assert(session_control_handle_event(NETCHESSZX_SESSION_EVENT_RESET,
                                        payload) == SESSION_DISPATCH_HANDLED);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    assert(fixture_confirmation == CONFIRM_RESET_SEND);
}

static uint8_t fixture_reject_confirm(void)
{
    SESSION_REJECT_CONFIRM(reject_done, reject_failed);
reject_done:
    return 1u;
reject_failed:
    return 0u;
}

/* A constant event keeps -O1 from linking every unrelated handler branch. */
#define FIXTURE_CONTROL_STATUS(event, status) do { \
    char fixture_payload[] = ""; \
    SESSION_BEGIN_EVENT(event); \
    assert(session_control_handle_event((event), fixture_payload) == (status)); \
} while (0)
#define FIXTURE_CONTROL(event) \
    FIXTURE_CONTROL_STATUS(event, SESSION_DISPATCH_HANDLED)

/* A declined request's retry gets the same refusal, not a second prompt;
   another event or a new ply makes the next request a genuine one. */
FIXTURE_FLATTEN static void check_production_declined_replay(void)
{
    char takeback[] = "TAKEBACK 5";

    fixture_reset(0u);
    netchesszx_session_peer_ready_state = 1u;
    netchesszx_session_game_started();
    netchesszx_session_set_game_ply(5u);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_confirmation == CONFIRM_RESET_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    fixture_send_count = 0u;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_confirmation == CONFIRM_NONE);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    /* A refusal is replayed at any ply. */
    netchesszx_session_set_game_ply(6u);
    fixture_send_count = 0u;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_confirmation == CONFIRM_NONE);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    netchesszx_session_set_game_ply(5u);
    /* A declined RESET is not replayed to another request. */
    fixture_send_count = 0u;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESTORE_RQ);
    assert(fixture_confirmation == CONFIRM_RESTORE_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "RN") == 0);
    fixture_send_count = 0u;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESTORE_RQ);
    assert(fixture_confirmation == CONFIRM_NONE);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "RN") == 0);
    fixture_send_count = 0u;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_confirmation == CONFIRM_RESET_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);

    fixture_takeback_ply = 5u;
    fixture_send_count = 0u;
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_TAKEBACK);
    assert(session_control_handle_event(NETCHESSZX_SESSION_EVENT_TAKEBACK,
                                        takeback) == SESSION_DISPATCH_HANDLED);
    assert(fixture_confirmation == CONFIRM_TAKEBACK_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK 5") == 0);
    fixture_send_count = 0u;
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_TAKEBACK);
    assert(session_control_handle_event(NETCHESSZX_SESSION_EVENT_TAKEBACK,
                                        takeback) == SESSION_DISPATCH_HANDLED);
    assert(fixture_confirmation == CONFIRM_NONE);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK 5") == 0);
    /* After another event the same request is a genuine one again. */
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_CHAT);
    fixture_send_count = 0u;
    SESSION_BEGIN_EVENT(NETCHESSZX_SESSION_EVENT_TAKEBACK);
    assert(session_control_handle_event(NETCHESSZX_SESSION_EVENT_TAKEBACK,
                                        takeback) == SESSION_DISPATCH_HANDLED);
    assert(fixture_confirmation == CONFIRM_TAKEBACK_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);
    fixture_reset(0u);
    netchesszx_session_peer_ready_state = 0u;
}

/* Restart after the game: only the host asks, the guest answers. A refusal
   ends the session on both sides; an expired request keeps OVER. */
FIXTURE_FLATTEN static void check_game_over_restart_flow(void)
{
    /* Host: the opponent declines, so this side leaves to Setup. */
    fixture_reset(0u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    netchesszx_session_game_ended();
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    FIXTURE_CONTROL_STATUS(NETCHESSZX_SESSION_EVENT_NACK_RESET,
                           SESSION_DISPATCH_EXIT);
    assert(fixture_setup_exit == 1u && fixture_started == 0u);
    assert(netchesszx_session_view()->control_pending == 0u);

    /* A refusal that arrives after the retries ran out is still one. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET_WAIT);
    FIXTURE_CONTROL_STATUS(NETCHESSZX_SESSION_EVENT_NACK_RESET,
                           SESSION_DISPATCH_EXIT);
    assert(fixture_setup_exit == 1u);

    /* Expired: the NACK answers our automatic CANCEL; OVER stays and the
       next key offers again through the OVER gate. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET_CANCEL);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_NACK_RESET);
    assert(fixture_setup_exit == 0u && fixture_confirmation == CONFIRM_NONE);
    assert(netchesszx_session_view()->control_pending == 0u);

    /* Accepted: both start; a late duplicate ACK starts nothing more. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_ACK_RESET);
    assert(fixture_started == 1u);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_ACK_RESET);
    assert(fixture_started == 1u);

    /* A 1.2 guest's automatic RESET crossing the host's request agrees. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_started == 1u && strcmp(fixture_sends[0], "ACK RESET") == 0);

    /* Guest: the host's request is asked; N sends NACK and leaves too. */
    fixture_reset(0u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    netchesszx_session_game_ended();
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_confirmation == CONFIRM_RESET_ACCEPT && fixture_send_count == 0u);
    assert(fixture_reject_confirm() == 1u);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    assert(fixture_setup_exit == 1u && fixture_confirmation == CONFIRM_NONE);

    /* Guest: the host's request expired; the prompt goes, OVER stays. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_CANCEL_RESET);
    assert(fixture_confirmation == CONFIRM_NONE && fixture_setup_exit == 0u);
    assert(fixture_send_count == 1u && strcmp(fixture_sends[0], "NACK RESET") == 0);
    /* A repeated CANCEL is only NACKed. */
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_CANCEL_RESET);
    assert(fixture_started == 0u && fixture_confirmation == CONFIRM_NONE);

    /* Host with a 1.2 guest: its expired request restores the offer. */
    fixture_reset(0u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    netchesszx_session_game_ended();
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_CANCEL_RESET);
    assert(fixture_confirmation == CONFIRM_RESTART_GAME);

    /* During play a refused or withdrawn reset keeps the session. */
    fixture_reset(0u);
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_NACK_RESET);
    assert(fixture_setup_exit == 0u && fixture_confirmation == CONFIRM_NONE);
    fixture_reset(0u);
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_CANCEL_RESET);
    assert(fixture_confirmation == CONFIRM_NONE);
    fixture_reset(0u);
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    assert(fixture_reject_confirm() == 1u);
    assert(fixture_setup_exit == 0u && strcmp(fixture_sends[0], "NACK RESET") == 0);

    /* A duplicate RESET while our prompt is up neither replies nor restarts. */
    fixture_reset(0u);
    netchesszx_session_game_ended();
    fixture_confirmation = CONFIRM_RESET_ACCEPT;
    FIXTURE_CONTROL(NETCHESSZX_SESSION_EVENT_RESET);
    assert(fixture_send_count == 0u && fixture_started == 0u);
    assert(fixture_confirmation == CONFIRM_RESET_ACCEPT);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
}

/* A remote MOVE is ACKed before its log, journal and checkpoint; a failed
   ACK still finishes the committed move before the link is dropped. */
static void check_remote_move_acks_before_finish(void)
{
    char payload[] = "MOVE 1 e2e4";
    char ply[6];
    char move[6];
    char notation[8];
    netchesszx_session_ping_t ping;
    uint8_t failed_send;
    uint8_t result;

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    for (failed_send = 0u; failed_send <= 1u; ++failed_send) {
        fixture_reset(failed_send);
        fixture_finish_sends = 0xffu;
        netchesszx_session_state.game_status_active = 1u;
        SESSION_MOVE_EVENT(NETCHESSZX_SESSION_EVENT_MOVE, payload, ply, move,
                           notation, &ping, result);
        assert(result == (failed_send ? SESSION_DISPATCH_EXIT
                                      : SESSION_DISPATCH_HANDLED));
        assert(fixture_send_count == 1u &&
               strcmp(fixture_sends[0], "ACK 1") == 0);
        assert(fixture_finish_sends == 1u && fixture_finish_disconnects == 0u);
        assert(fixture_disconnected == failed_send);
    }
}

static void check_real_decision_clock(void)
{
    static const char *const cancellations[] = {
        "CANCEL RESET", "CANCEL DRAW", "CANCEL TAKEBACK 3", "RN",
        "NACK RESET", "NACK DRAW", "NACK 3", "RN"
    };
    uint8_t hz;
    uint8_t kind;

    for (hz = 50u; hz <= 60u; hz += 10u) {
        for (kind = 0u; kind < 8u; ++kind) {
            netchesszx_session_schedule_t schedule;
            uint8_t ok;
            uint8_t reply = NETCHESSZX_SESSION_DIRECT_REPLY_POLLS_AT(hz);
            uint16_t deadline = (uint16_t)(hz * NETCHESS_PROTO_DECISION_SECONDS);

            fixture_reset(0u);
            netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
            fixture_frame_count = 65530u;
            fixture_takeback_local = (uint8_t)(kind < 4u);
            netchesszx_session_set_game_status_active(1u);
            if (kind >= 4u) {
                fixture_confirmation = kind == 6u ? CONFIRM_TAKEBACK_ACCEPT :
                    kind == 7u ? CONFIRM_RESTORE_ACCEPT : CONFIRM_RESET_ACCEPT;
                if (kind == 5u) { netchesszx_session_set_control_pending(CONTROL_PENDING_DRAW_INCOMING); }
                if (kind == 6u) { netchesszx_session_set_takeback_pending_ply(3u); }
            } else if (kind < 2u) {
                netchesszx_session_set_control_pending(kind == 0u
                    ? CONTROL_PENDING_RESET : CONTROL_PENDING_DRAW_SENT);
            } else if (kind == 2u) {
                netchesszx_session_set_takeback_pending_ply(3u);
            } else {
                netchesszx_session_set_restore_rx_mask(RESTORE_TX_PENDING);
            }
            SESSION_SCHEDULE_BEGIN(&schedule, reply, deadline);
            (void)SESSION_DEADLINE_STEP(&schedule, reply, deadline);
            assert((uint16_t)(netchesszx_session_schedule_view(&schedule)->control_cancel_wait -
                              fixture_frame_count) == deadline);
            /* Polling repeatedly inside one frame consumes no decision time. */
            (void)SESSION_DEADLINE_STEP(&schedule, reply, deadline);
            assert((uint16_t)(netchesszx_session_schedule_view(&schedule)->control_cancel_wait -
                              fixture_frame_count) == deadline);
            /* Busy work crosses the 16-bit wrap; no empty polls are needed. */
            fixture_frame_count = (uint16_t)(fixture_frame_count + deadline - 1u);
            (void)SESSION_DEADLINE_STEP(&schedule, reply, deadline);
            assert((uint16_t)(netchesszx_session_schedule_view(&schedule)->control_cancel_wait -
                              fixture_frame_count) == 1u);
            assert(!SESSION_DEADLINE_REACHED(fixture_frame_count,
                netchesszx_session_schedule_view(&schedule)->control_cancel_wait));
            assert(fixture_send_count == 0u);
            ++fixture_frame_count;
            SESSION_PENDING_STEP(&schedule, reply, deadline, ok);
            assert(ok && fixture_send_count == 1u && fixture_disconnected == 0u);
            assert(strcmp(fixture_sends[0], cancellations[kind]) == 0);
            if (kind >= 4u) {
                assert(fixture_confirmation == CONFIRM_NONE &&
                    netchesszx_session_view()->control_pending == 0u &&
                    netchesszx_session_view()->takeback_pending_ply == 0u);
            } else if (kind == 2u) {
                uint8_t retry;
                for (retry = 0u; retry <= CONTROL_REPLY_RETRIES; ++retry) {
                    fixture_send_count = 0u;
                    netchesszx_session_schedule_set_pending_retry_wait(&schedule, 1u);
                    SESSION_IDLE_STEP(&schedule, reply, ok);
                    assert(ok && fixture_disconnected == 0u && fixture_send_count == 1u);
                    assert(strcmp(fixture_sends[0], "CANCEL TAKEBACK 3") == 0);
                }
            }
        }
    }
}

int main(void)
{
    uint8_t failed_send;
    check_production_answer_replay();
    check_production_declined_replay();
    check_game_over_reset_supersedes_restart_offer();
    check_game_over_restart_flow();
    check_peer_bye_retires_recovery();
    check_remote_move_acks_before_finish();

    fixture_reset(0u);
    assert(!SESSION_LOCAL_LOAD_BLOCKED());
    netchesszx_session_set_pending_local_ply(1u);
    assert(SESSION_LOCAL_LOAD_BLOCKED());
    fixture_reset(0u);
    netchesszx_session_set_control_pending(CONTROL_PENDING_RESET);
    assert(SESSION_LOCAL_LOAD_BLOCKED());
    fixture_reset(0u);
    netchesszx_session_set_restore_rx_mask(RESTORE_RX_RECEIVE);
    assert(SESSION_LOCAL_LOAD_BLOCKED());

    /* Board navigation needs the peer only while a game is live. */
    {
        uint8_t saved_peer = netchesszx_session_peer_ready_state;

        fixture_reset(0u);
        netchesszx_session_peer_ready_state = 0u;
        assert(netchesszx_session_active_peer_ready());
        netchesszx_session_state.game_status_active = 1u;
        assert(!netchesszx_session_active_peer_ready());
        netchesszx_session_peer_ready_state = saved_peer;
    }

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    for (failed_send = 0u; failed_send <= 2u; ++failed_send) {
        fixture_reset(failed_send);
        netchesszx_session_set_restore_rx_mask(RESTORE_TX_PENDING);
        assert(restore_send_chunks() == (failed_send == 0u));
        assert(fixture_send_count == (failed_send == 1u ? 1u : 2u));
        assert(strcmp(fixture_sends[0], "RS00") == 0);
        if (fixture_send_count == 2u) {
            assert(strcmp(fixture_sends[1], "RS01") == 0);
        }
        assert(fixture_disconnected == 0u);
        assert(netchesszx_session_view()->restore_rx_mask ==
               (failed_send ? 0u : RESTORE_TX_AWAIT_ACK));
    }
    fixture_reset(0u);
    assert(restore_send_chunks() == 0u);
    assert(fixture_send_count == 0u);

    fixture_reset(1u);
    netchesszx_session_set_restore_rx_mask(RESTORE_TX_PENDING);
    assert(restore_cancel_pending_request() == 0u);
    assert(fixture_disconnected == 0u);

    for (failed_send = 1u; failed_send <= 2u; ++failed_send) {
        fixture_reset(failed_send);
        netchesszx_session_set_restore_rx_mask(RESTORE_TX_PENDING);
        assert(restore_handle_event(NETCHESSZX_SESSION_EVENT_RESTORE_RY,
                                    fixture_sends[0]) == SESSION_DISPATCH_EXIT);
        assert(fixture_disconnected == 1u);
    }

    fixture_reset(1u);
    netchesszx_session_set_control_pending(CONTROL_PENDING_DRAW_INCOMING);
    accepted_draw_end();
    assert(fixture_ended == 1u);
    assert(fixture_send_count == 0u);
    assert(netchesszx_session_view()->control_pending == 0u);
    assert(netchesszx_session_view()->game_over == 1u);
    assert(netchesszx_session_view()->last_control_accept == CONTROL_ACCEPT_DRAW);
    assert(fixture_disconnected == 0u);

    netchesszx_transport = NETCHESSZX_TRANSPORT_MQTT;
    for (failed_send = 1u; failed_send <= 2u; ++failed_send) {
        fixture_reset(failed_send);
        netchesszx_session_set_restore_rx_mask(RESTORE_TX_AWAIT_ACK);
        assert(tcp_required(restore_send_chunks()) == 0u);
        assert(fixture_disconnected == 1u);
        assert(fixture_offline == (failed_send == 2u));
    }
    fixture_reset(1u);
    accepted_draw_end();
    assert(fixture_send_count == 0u);
    assert(fixture_offline == 0u);
    assert(fixture_disconnected == 0u);
    check_real_decision_clock();
    puts("compact transition owner ordering and real-time deadlines ok");
    return 0;
}
