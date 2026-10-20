#include "spectrum/board/board.h"
#include "spectrum/board/san.h"
#include "spectrum/app/input_cmd.h"
#include "spectrum/app/time.h"
#include "spectrum/app/game_stats.h"
#include "spectrum/saveload/saveload.h"
#include "spectrum/fileui/fileui.h"
#include "spectrum/restore/restore.h"
#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/render.h"
#include "spectrum/ui/presentation.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/config/session.h"
#include "spectrum/config/setup_menu.h"
#include "spectrum/ui/setup.h"
#include "spectrum/app/setup_ports.h"
#include "spectrum/session/state.h"
#include "spectrum/session/transitions.h"
#include "spectrum/session/direct.h"
#include "spectrum/session/event.h"
#include "spectrum/session/mqtt.h"
#include "spectrum/session/mqtt_wire.h"
#include "spectrum/session/outgoing.h"
#include "spectrum/session/ping.h"
#include "spectrum/session/poll.h"
#include "spectrum/transport/link.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/input.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/text.h"
#include "spectrum/render_status.h"
#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/savegame/savegame_format.h"
#include "common/ui_messages.h"

#include <stddef.h>
#include <string.h>

#ifdef NETCHESSZX_FIXED_LOW_RAM
__at(NETCHESSZX_LOWRAM_GAME_STATS_ADDR)
#endif
uint8_t spectrum_game_stats[GAME_STATS_SIZE];
#ifdef NETCHESSZX_HOST_SESSION_TEST
static const char *spectrum_game_stats_result;
#endif

uint8_t netchesszx_asm_mqtt_strlen8(const char *text) NETCHESSZX_FASTCALL;
uint8_t netchesszx_setup_nav_key_alias(uint8_t key) NETCHESSZX_FASTCALL;
#ifdef NETCHESSZX_HOST_SESSION_TEST
static uint8_t netchesszx_confirm_key_alias(uint8_t key)
{
    if (key == 13u || key == ' ') {
        return 'y';
    }
    return key == 0x8au ? 'n' : key;
}
#else
uint8_t netchesszx_confirm_key_alias(uint8_t key) NETCHESSZX_FASTCALL;
#endif
#if defined(NETCHESSZX_JOURNAL_HOOK) && !defined(NETCHESSZX_HOST_SESSION_TEST)
void spectrum_journal_after_move(const char *move) NETCHESSZX_FASTCALL;
void spectrum_journal_after_takeback(void);
void spectrum_journal_notify(void);
/* History switches for the next append; see asm/spectrum/journal.asm. */
extern uint8_t spectrum_journal_pend;
extern uint8_t spectrum_journal_warning;
#define spectrum_journal_new_game() do { \
    spectrum_journal_pend = 0x80u; spectrum_journal_warning = 0u; \
} while (0)
#define spectrum_journal_arm_load() do { \
    spectrum_journal_pend |= 0x80u; spectrum_journal_warning = 0u; \
} while (0)
#define spectrum_journal_remote_restore() (spectrum_journal_pend = 0xc0u)
#else
#define spectrum_journal_after_move(move) ((void)(move))
#define spectrum_journal_after_takeback() ((void)0)
#define spectrum_journal_notify() ((void)0)
#define spectrum_journal_new_game() ((void)0)
#define spectrum_journal_arm_load() ((void)0)
#define spectrum_journal_remote_restore() ((void)0)
#endif


#define KEY_UP 0x81u
#define KEY_DOWN 0x82u
#define KEY_LEFT 0x83u
#define KEY_RIGHT 0x84u
#define KEY_HOME 0x88u
#define KEY_END 0x89u
#define KEY_CANCEL 0x8au
#define NO_SQUARE 0xffu
#define LOCAL_KEY_OK 1u
#define LOCAL_KEY_CHAT_SENT 2u
#define LOCAL_CHAT_TEXT_MAX NETCHESSZX_CHAT_MESSAGE_TEXT_MAX
#define LOCAL_INPUT_MAX LOCAL_CHAT_TEXT_MAX
#define INPUT_HISTORY_SIZE 2u
#define INPUT_HISTORY_TEXT_MAX 31u
#define STATUS_PHASE_CONNECTION_SETUP 0u
#define STATUS_PHASE_GAME_SETUP 1u
#define STATUS_PHASE_CONNECTING 2u
#define STATUS_PHASE_CONNECTED 3u
#define STATUS_PHASE_GAME 4u
#define INPUT_HISTORY_NONE 0xffu
#define CONFIRM_DISCONNECT 1u
#define CONFIRM_FILE_ERASE 10u
#define side_to_move_is_white() ((uint8_t)((netchesszx_session_view()->game_ply & 1u) == 0u))

#ifdef NETCHESSZX_HOST_SESSION_TEST
static char session_test_local_input[NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE];
static char session_test_input_history[NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE];
#define local_input session_test_local_input
#define input_history session_test_input_history
void netchesszx_host_session_observe_move_result(const char *notation);
#define HOST_SESSION_OBSERVE_MOVE_RESULT(notation) \
    netchesszx_host_session_observe_move_result(notation)
void netchesszx_host_session_observe_move_rejection(const char *reason);
#define HOST_SESSION_OBSERVE_MOVE_REJECTION(reason) \
    netchesszx_host_session_observe_move_rejection(reason)
/* A remote MOVE is delivered when the board commits it, before its ACK;
   the log, flash and journal/checkpoint I/O follow that ACK. */
void netchesszx_host_session_observe_remote_move(const char *move);
#define HOST_SESSION_OBSERVE_REMOTE_MOVE(move) \
    netchesszx_host_session_observe_remote_move(move)
extern netchesszx_session_ping_t *netchesszx_host_session_ping;
#define HOST_SESSION_EXPOSE_PING(ping) \
    (netchesszx_host_session_ping = (ping))
#else
#define local_input ((char *)NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR)
#define input_history ((char *)NETCHESSZX_LOWRAM_INPUT_HISTORY_ADDR)
#define HOST_SESSION_OBSERVE_MOVE_RESULT(notation) ((void)0)
#define HOST_SESSION_OBSERVE_MOVE_REJECTION(reason) ((void)0)
#define HOST_SESSION_OBSERVE_REMOTE_MOVE(move) ((void)0)
#define HOST_SESSION_EXPOSE_PING(ping) ((void)0)
#endif
#if LOCAL_INPUT_MAX + 1u > NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE
#error "local input exceeds fixed low-RAM region"
#endif
#if INPUT_HISTORY_SIZE * (INPUT_HISTORY_TEXT_MAX + 1u) > NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE
#error "input history exceeds fixed low-RAM region"
#endif
#define input_history_slot(index) \
    (input_history + ((uint16_t)(index) * (INPUT_HISTORY_TEXT_MAX + 1u)))
#include "spectrum/ui/editor.h"
#ifdef NETCHESSZX_HOST_SESSION_TEST
uint8_t spectrum_editor_append_hot(uint8_t key)
{
    if (key < 32u || key >= 127u || local_input_len >= LOCAL_INPUT_MAX ||
        local_input_cursor != local_input_len) {
        return 0u;
    }
    local_input[local_input_len++] = (char)key;
    local_input[local_input_len] = '\0';
    local_input_cursor = local_input_len;
    input_history_pos = INPUT_HISTORY_NONE;
    return 1u;
}
#endif
static uint8_t confirm_action;
static uint8_t setup_restart_requested;
#ifdef NETCHESSZX_RECOVERY
#define RECOVERY_WARNING 0x02u
#define RECOVERY_NOTICE_PENDING 0x08u
/* Host-only resume offer, raised when the peer becomes ready. */
#define CONFIRM_RESUME 11u
static uint8_t recovery_state;
#endif
#include "spectrum/ui/interaction.h"
#include "spectrum/app/ux_ports.h"
static uint8_t hints_selected_row = NO_SQUARE;
static uint8_t hints_selected_col = NO_SQUARE;
static uint8_t local_turn;
spectrum_board_undo_t spectrum_stats_undo;
static uint16_t takeback_snapshot_ply;
static uint8_t takeback_snapshot_local;
static uint8_t takeback_snapshot_check_state;
static uint8_t game_check_state;
/* Low bits are the UI phase; high bits hold optional MACH peer identity. */
static uint8_t status_phase_current;

static void session_send_mach(void);

/* Restore, application config, and Setup are mutually exclusive phases. The
   CONFIG overlay may overwrite Setup while saving; Setup then reconstructs
   itself from the committed globals before the next render. */
typedef union {
    char restore_b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    spectrum_board_snapshot_t restore_snapshot;
#ifdef NETCHESSZX_HOST_SESSION_TEST
    spectrum_setup_view_t setup;
#else
    uint8_t setup[SPECTRUM_SETUP_WORKSPACE_SIZE];
#endif
    uint8_t config_record[SPECTRUM_CONFIG_RECORD_SIZE];
} app_workspace_t;

app_workspace_t app_workspace;

#ifndef NETCHESSZX_HOST_SESSION_TEST
typedef char app_workspace_size_check[
    sizeof(app_workspace_t) == sizeof(spectrum_board_snapshot_t) ? 1 : -1];
typedef char app_config_record_size_check[
    sizeof(app_workspace_t) >= SPECTRUM_CONFIG_RECORD_SIZE ? 1 : -1];
#endif

#define restore_b64_pending app_workspace.restore_b64
#define restore_snapshot app_workspace.restore_snapshot
#define setup_config_record app_workspace.config_record

/* User-facing notice strings (<= 28 chars). Transport-agnostic and framed as
   PLAYER (local) vs OPPONENT (remote); never device identity or role-specific
   host/peer wording. Sentence case, with CAPS reserved for critical events. */
static const char msg_connect_failed[] = NETCHESSZX_UI_ERROR_CONNECTION_FAILED;
static const char msg_preflight_failed[] = NETCHESSZX_UI_ERROR_PREFLIGHT;
const char spectrum_app_overlay_load_error[] = NETCHESSZX_UI_ERROR_OVERLAY_LOAD;
static const char msg_connection_lost[] = NETCHESSZX_UI_ERROR_CONNECTION_LOST;
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
/* The next "opponent lost" teardown's notice: a BOARD change that could not
   load (SESSION_APP_BOARD_LOST) names that cause instead. */
static const char *disconnect_message = msg_connection_lost;
#endif
static const char msg_host_busy[] = NETCHESSZX_UI_SPECTRUM_ERROR_HOST_BUSY;
static const char msg_checkmate_won[] = NETCHESSZX_UI_EVENT_CHECKMATE_WON;
static const char msg_checkmate_lost[] = NETCHESSZX_UI_EVENT_CHECKMATE_LOST;
static const char msg_stalemate[] = NETCHESSZX_UI_EVENT_STALEMATE;
static const char command_resign[] = "/resign";
static const char command_draw[] = "/draw";
static const char command_takeback[] = "/takeback";
static const char msg_draw_agreed[] = NETCHESSZX_UI_EVENT_DRAW_AGREED;
static const char msg_resignation_lost[] = NETCHESSZX_UI_EVENT_RESIGNATION_LOST;
static const char msg_opponent_resign[] = NETCHESSZX_UI_EVENT_OPPONENT_RESIGN;
static const char msg_retrying[] = "RETRYING...";
static const char msg_control_cancelled[] = " cancelled: no response";
static const char msg_control_expired[] = " request withdrawn";
static const char msg_waiting_resign_ack[] =
    NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK;
static const char msg_load_cancelled[] = "Load cancelled";
#ifdef NETCHESSZX_RECOVERY
static const char msg_recovery_failed[] = "Recovery failed";
#endif
static const char *preflight_retry_msg;

static void handle_opponent_disconnected(void);
static void handle_opponent_disconnected_with(const char *message);
static void mqtt_peer_disconnected_wait(void);
static void end_game_over(const char *message);
static void local_controls_reset(uint8_t is_local_turn);
static uint8_t tcp_required(uint8_t sent) __z88dk_fastcall;
static uint8_t session_tcp_send(const char *text) __z88dk_fastcall;
static uint8_t session_tcp_reply(const char *text) __z88dk_fastcall;
static uint8_t session_tcp_control_reply(const char *text) __z88dk_fastcall;
static uint8_t session_tcp_move_reply(uint8_t ack, uint16_t ply) NETCHESSZX_CALLEE;
static uint8_t send_takeback_wire(uint16_t ply);
static uint8_t send_move_wire(uint16_t ply, const char *move);
static uint8_t restore_transfer_pending(void);
static uint8_t restore_cancel_pending_request(void);
static void disconnect_to_setup(void);
static void local_save_game(const char *name);
#ifdef NETCHESSZX_RECOVERY
static void recovery_checkpoint(void);
#else
#define recovery_checkpoint() ((void)0)
#endif
static uint8_t local_action_ready(void);
static uint8_t game_start_local(uint8_t start_key);
static uint8_t send_local_move(const char *move);
static void about_restore_full_board(void);
void notify_info(const char *text)
{
    /* Existing application entry bindings and phase-shared CONFIG buffer. */
#ifndef NETCHESSZX_HOST_SESSION_TEST
#asm
    EXTERN _netchesszx_config_save_overlay
    PUBLIC _spectrum_setup_save_config
    defc _spectrum_setup_save_config = _netchesszx_config_save_overlay
    PUBLIC _setup_config_record
    defc _setup_config_record = _app_workspace
#endasm
#endif
    spectrum_gui_notify(text, 0u);
}

void notify_error(const char *text)
{
    spectrum_gui_notify(text, 1u);
}

#define notify_info_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_INFO))
#define notify_error_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_ERROR))
#define notify_wait_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_WAIT))
#define notify_success_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_SUCCESS))
static void notify_wait_opponent(void)
{
    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
}

/* READY_GO is the host's peer-ready notice. Before any game or clock starts,
   the host offers its newest checkpoint instead; accepting reuses LOAD. */
static void ready_notice(uint8_t message) NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_RECOVERY
    if (message == SPECTRUM_GUI_MSG_OPPONENT_READY_GO &&
        spectrum_recovery_read(0)) {
        confirm_action = CONFIRM_RESUME;
        notify_error_msg(SPECTRUM_GUI_MSG_RESUME_OFFER);
        return;
    }
#endif
    notify_wait_msg(message);
}

static void notify_wait_opponent_ack(void)
{
    notify_wait_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
}

static void notify_move_rejected(void)
{
    notify_error_msg(SPECTRUM_GUI_MSG_MOVE_REJECTED);
}

#if defined(NETCHESSZX_SDCC_IY) && !defined(NETCHESSZX_HOST_TEST)
static void notify_control_timeout(uint8_t kind) __z88dk_fastcall __naked
{
    (void)kind;
#asm
    ld a,l
    push af
    call _spectrum_net_payload_scratch
    pop bc
    push hl
    push bc
    ld a,b
    and 3
    ld de,_NETCHESS_PROTO_TAKEBACK_PREFIX
    cp 3
    jr z,control_notice_prefix
    ld de,_NETCHESS_PROTO_NACK_RESET+5
    cp 1
    jr z,control_notice_prefix
    ld de,_NETCHESS_PROTO_DRAW
control_notice_prefix:
    push de
    push hl
    call _spectrum_append_text
    pop bc
    ld a,b
    and 3
    cp 3
    jr nz,control_notice_end
    dec hl
control_notice_end:
    ld de,_msg_control_expired
    bit 7,b
    jr z,control_notice_suffix
    ld de,_msg_control_cancelled
control_notice_suffix:
    push de
    push hl
    call _spectrum_append_text
    pop hl
    push hl
    call _notify_info
    pop bc
    ret
#endasm
}
#else
static void notify_control_timeout(uint8_t kind) __z88dk_fastcall
{
    char *payload = spectrum_link_payload_scratch();
    uint8_t name = (uint8_t)(kind & 3u);
    char *end = spectrum_append_text(payload, name == 3u ? NETCHESS_PROTO_TAKEBACK :
                                            name == 1u ? msg_reset_wire : msg_draw);

    (void)spectrum_append_text(end, (kind & 0x80u) ? msg_control_cancelled : msg_control_expired);
    notify_info(payload);
}
#endif

static void wait_after_notice(void);

static uint8_t wait_notice_frames(uint8_t cancel_key)
{
    uint16_t wait_frames = 250u;

    while (wait_frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_link_background_drain();
        spectrum_app_tick();
        spectrum_link_background_drain();
        if (cancel_key && spectrum_input_poll_event() == KEY_CANCEL) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t retry_after_error(const char *msg)
{
    notify_error(msg);
    return wait_notice_frames(1u);
}

static void wait_after_notice(void)
{
    (void)wait_notice_frames(0u);
}

#if defined(NETCHESSZX_NEXT) || defined(NETCHESSZX_SPECTRANEXT) || \
    defined(NETCHESSZX_HOST_SESSION_TEST)
#define clock_sync_run spectrum_link_sync_time
#else
static uint8_t clock_sync_run(void)
{
    if (netchesszx_timezone == NETCHESSZX_TIME_RTC) {
        if (netchesszx_setup_time_init(0xffu)) {
            netchesszx_rtc_available = 1u;
            spectrum_net_runtime_set_clock(
                netchesszx_setup_overlay_context[2],
                netchesszx_setup_overlay_context[1],
                netchesszx_setup_overlay_context[0]);
            return 1u;
        }
        netchesszx_rtc_available = 0u;
        netchesszx_timezone = netchesszx_timezone_last;
    }
    return spectrum_link_sync_time();
}
#endif

#ifndef NETCHESSZX_SPECTRANEXT
static void preflight_clock_line(const char *line)
{
    spectrum_info_line(line);
    spectrum_frame_wait();
    spectrum_app_tick();
}

static void preflight_clock_run(void)
{
    preflight_clock_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK WAIT");
    (void)clock_sync_run();
    preflight_clock_line(spectrum_net_runtime_clock_ready()
        ? NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK OK  "
        : NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK FAIL");
}
#endif

static uint8_t connection_preflight_run(uint8_t quiet)
{
    uint8_t rc = spectrum_link_preflight_run(quiet);

    if (rc == SPECTRUM_LINK_PREFLIGHT_OK) {
        if (!quiet && !spectrum_net_runtime_clock_ready()) {
#ifdef NETCHESSZX_SPECTRANEXT
            (void)clock_sync_run();
#else
            preflight_clock_run();
#endif
        }
        return 1u;
    }
    if (rc == SPECTRUM_LINK_PREFLIGHT_RETRYING) {
        preflight_retry_msg = msg_retrying;
    } else if (rc == SPECTRUM_LINK_PREFLIGHT_OVL_FAIL) {
        preflight_retry_msg = spectrum_app_overlay_load_error;
        spectrum_gui_set_status_error(spectrum_app_overlay_load_error);
        spectrum_gui_draw_status();
    } else {
        preflight_retry_msg = msg_preflight_failed;
    }
    return 0u;
}

static void status_show_phase(uint8_t phase)
{
    status_phase_current = SPECTRUM_STATUS_WITH_PHASE(status_phase_current,
                                                      phase);
    if (!spectrum_gui_status_phase(status_phase_current)) {
        status_phase_current |= SPECTRUM_STATUS_STALE;
    }
}

static void status_show_endpoint(void)
{
    status_show_phase(STATUS_PHASE_CONNECTED);
}

static void status_show_connecting(void)
{
    status_show_phase(STATUS_PHASE_CONNECTING);
}

static void status_show_connection_setup(void)
{
    status_show_phase(STATUS_PHASE_CONNECTION_SETUP);
}

static void status_refresh_game(void)
{
    uint8_t black_to_move;

    if (!netchesszx_session_view()->game_status_active) {
        return;
    }
    /* A recorded GAME phase is already on the status row: every other row
       writer either repaints the recorded phase or is followed by a phase
       change. Skipping the identical repaint saves a Classic STATUS overlay
       load on every ply; a paint whose overlay failed to load is retried. */
    if ((status_phase_current &
         (SPECTRUM_STATUS_STALE | SPECTRUM_STATUS_PHASE_MASK)) !=
        STATUS_PHASE_GAME) {
        status_show_phase(STATUS_PHASE_GAME);
    }
    black_to_move = (uint8_t)!side_to_move_is_white();
    if (game_check_state == SPECTRUM_BOARD_CHECK) {
        spectrum_gui_set_turn_label(black_to_move
                                    ? SPECTRUM_GUI_TURN_BLACK_CHECK
                                    : SPECTRUM_GUI_TURN_WHITE_CHECK);
    } else {
        spectrum_gui_set_turn_label(black_to_move
                                    ? SPECTRUM_GUI_TURN_BLACK
                                    : SPECTRUM_GUI_TURN_WHITE);
    }
}

static void pending_local_clear(void)
{
    netchesszx_session_pending_move_clear();
}

static void takeback_clear(void)
{
    takeback_snapshot_ply = 0u;
    takeback_snapshot_local = 0u;
    netchesszx_session_takeback_history_clear();
}

static void pending_takeback_clear(void)
{
    pending_local_clear();
    takeback_clear();
}

static void takeback_snapshot_save(uint16_t ply, uint8_t local_move)
{
    takeback_snapshot_ply = ply;
    takeback_snapshot_local = local_move;
    takeback_snapshot_check_state = game_check_state;
    netchesszx_session_new_move_committed();
}

static void restore_about_full_board_if_visible(void)
{
    if (spectrum_gui_about_visible()) {
        about_restore_full_board();
    }
}

static void reset_board_moves_chat(void)
{
    spectrum_board_reset();
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    spectrum_gui_reset_logs();
}

static void clear_disconnected_session_state(void)
{
    local_turn = 0u;
    netchesszx_session_disconnected();
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    netchesszx_session_peer_reset();
    status_phase_current &= SPECTRUM_STATUS_PHASE_MASK;
    SESSION_FORGET_DISCONNECTED_COLOR();
    pending_takeback_clear();
    spectrum_input_flush_until_release();
    spectrum_app_game_timer_stop();
}

#define nav_key_alias netchesszx_setup_nav_key_alias

static void suppress_current_key(void)
{
    spectrum_input_flush_until_release();
}

static void suppress_key_until_release(uint8_t key);

static uint16_t mqtt_new_session_id(void)
{
    uint16_t id = *(volatile uint16_t *)0x5c78u;
    return id == 0u ? 1u : id;
}

uint8_t spectrum_setup_start(uint8_t key) NETCHESSZX_FASTCALL
{
    if (!netchesszx_setup_time_commit()) {
        return 0u;
    }
    if (spectrum_setup_view.choice[SETUP_CHOICE_TRANSPORT] == NETCHESSZX_TRANSPORT_MQTT &&
        spectrum_setup_view.choice[SETUP_CHOICE_ROLE] == NETCHESSZX_SESSION_ROLE_HOST) {
        netchesszx_session_set_id(mqtt_new_session_id());
    } else {
        netchesszx_session_set_id(0u);
    }
    if (spectrum_setup_view.choice[SETUP_CHOICE_ROLE] == NETCHESSZX_SESSION_ROLE_JOIN) {
        netchesszx_session_forget_color();
    }
    netchesszx_session_set_mqtt_seat_probed(0u);
    suppress_key_until_release(key);
    return 1u;
}

void spectrum_setup_preview_piece_set(uint8_t set) NETCHESSZX_FASTCALL
{
    if (set == netchesszx_piece_set_index) {
        return;
    }
    if (!netchesszx_piece_set_load(set)) {
        notify_error(NETCHESSZX_UI_ERROR_SET_LOAD_FAILED);
        return;
    }
    netchesszx_piece_set_index = set;
#ifndef NETCHESSZX_HOST_SESSION_TEST
    if (spectrum_gui_are_board_pieces_visible()) {
        spectrum_gui_morph_board_pieces();
    }
#endif
}

void spectrum_setup_apply_timezone(int8_t previous) NETCHESSZX_FASTCALL
{
    if (netchesszx_setup_time_commit()) {
        if (spectrum_net_runtime_clock_ready()) {
            if (previous != NETCHESSZX_TIME_RTC &&
                netchesszx_timezone != NETCHESSZX_TIME_RTC) {
                spectrum_app_shift_clock(
                    (int8_t)(netchesszx_timezone - previous));
            } else if (netchesszx_timezone == NETCHESSZX_TIME_RTC) {
                (void)clock_sync_run();
            } else {
                spectrum_link_clock_retry_start();
            }
        } else {
            spectrum_link_clock_retry_start();
        }
    }
}

#ifdef NETCHESSZX_HOST_SESSION_TEST
uint8_t spectrum_setup_save_config(void) { return 1u; }
#endif

static uint8_t session_setup_run(uint8_t config_state)
{
    uint8_t key;
    uint8_t requested_set = netchesszx_piece_set_index;

    netchesszx_piece_set_index = 0xffu;
    if (!netchesszx_piece_set_load(requested_set)) {
        requested_set = NETCHESSZX_PIECE_SET_STD;
        (void)netchesszx_piece_set_load(requested_set);
    }
    netchesszx_piece_set_index = requested_set;
    spectrum_setup_open(config_state);
    spectrum_board_reset();
    spectrum_gui_animate_board_pieces();
    status_show_phase(STATUS_PHASE_GAME_SETUP);
    if (config_state == SPECTRUM_CONFIG_STATE_INVALID) {
        notify_error_msg(SPECTRUM_GUI_MSG_CONFIG_INVALID);
    } else if (config_state == SPECTRUM_CONFIG_STATE_IO) {
        notify_error_msg(SPECTRUM_GUI_MSG_CONFIG_IO);
    } else {
        notify_info_msg(SPECTRUM_GUI_MSG_SELECT_OPTIONS);
    }
    suppress_current_key();

    while (1) {
        key = spectrum_input_poll_event();
        if (key == 0u) {
            spectrum_net_runtime_wait_frame();
            continue;
        }
        if (spectrum_setup_step(key)) {
            return (uint8_t)(SPECTRUM_CONFIG_STATE_SAVED ^ spectrum_setup_view.config_dirty);
        }
    }
}

static uint8_t input_has_text(const char *text)
{
    while (*text != '\0') {
        if (*text != ' ') {
            return 1u;
        }
        ++text;
    }
    return 0u;
}

static void edit_stop_clear(void)
{
    netchesszx_input_edit_stop_clear_overlay();
}


uint8_t spectrum_ux_can_navigate(void)
{
    if (netchesszx_session_active_peer_ready() &&
        netchesszx_session_view()->pending_local_ply == 0u) {
        return local_turn;
    }
    return 0u;
}

uint8_t spectrum_ux_local_turn(void) { return local_turn; }
uint16_t spectrum_ux_move_pending(void)
{
    return netchesszx_session_view()->pending_local_ply;
}
static uint8_t is_local_piece(char piece)
{
    if (netchesszx_local_is_white()) {
        return (uint8_t)(piece >= 'A' && piece <= 'Z');
    }
    return (uint8_t)(piece >= 'a' && piece <= 'z');
}

uint8_t spectrum_ux_selectable(uint8_t row, uint8_t col)
{
    return is_local_piece(spectrum_board_cell(row, col));
}

void spectrum_ux_selection_notice(uint16_t position) NETCHESSZX_FASTCALL
{
    uint8_t row = (uint8_t)position;
    uint8_t col = (uint8_t)(position >> 8);
    char piece = spectrum_board_cell(row, col);
    char square[3];
    char msg[16];

    square[0] = (char)('a' + col);
    square[1] = (char)('8' - row);
    square[2] = '\0';
    if (piece == '.') {
        (void)spectrum_append_text(spectrum_append_text(msg, "Empty "), square);
    } else if (!is_local_piece(piece)) {
        (void)spectrum_append_text(spectrum_append_text(msg, "You play "),
                                   netchesszx_local_side_name());
    } else {
        (void)spectrum_append_text(spectrum_append_text(msg, "Sel "), square);
    }
    notify_info(msg);
}

uint8_t spectrum_ux_move_intent(uint16_t from, uint16_t to)
{
    char move[6];
    uint8_t from_row = (uint8_t)from;
    uint8_t to_row = (uint8_t)to;
    char piece = spectrum_board_cell(from_row, (uint8_t)(from >> 8));

    move[0] = (char)('a' + (uint8_t)(from >> 8));
    move[1] = (char)('8' - from_row);
    move[2] = (char)('a' + (uint8_t)(to >> 8));
    move[3] = (char)('8' - to_row);
    move[4] = '\0';
    /* Preserve the existing board-input policy: automatic queen promotion.
       The normal move path still validates legality before any board effect. */
    if ((piece == 'P' && to_row == 0u) || (piece == 'p' && to_row == 7u)) {
        move[4] = 'q';
        move[5] = '\0';
    }
    return send_local_move(move);
}

static uint8_t cursor_try_square(uint8_t row, uint8_t col)
{
    if (!spectrum_ux_selectable(row, col)) {
        return 0u;
    }
    cursor_home((uint16_t)(row | ((uint16_t)col << 8)));
    return 1u;
}

static void cursor_reset_default_square(void)
{
    uint8_t row;
    uint8_t col;

    if (netchesszx_local_is_white()) {
        if (cursor_try_square(6u, 4u) || cursor_try_square(4u, 4u)) {
            return;
        }
    } else {
        if (cursor_try_square(1u, 4u) || cursor_try_square(3u, 4u)) {
            return;
        }
    }
    for (row = 0u; row < 8u; ++row) {
        for (col = 0u; col < 8u; ++col) {
            if (cursor_try_square(row, col)) {
                return;
            }
        }
    }
    cursor_home(0x0406u);
}

typedef char spectrum_ux_move_results_match[
    UX_MOVE_FAILED == LOCAL_MOVE_NET_FAIL &&
    UX_MOVE_REJECTED == LOCAL_MOVE_REJECTED &&
    UX_MOVE_ACCEPTED == LOCAL_MOVE_SENT ? 1 : -1];

void movement_hints_clear(void)
{
    uint8_t row;
    uint8_t has_hints = 0u;

    for (row = 0u; row < 8u; ++row) {
        if (netchesszx_hinted_rows[row] != 0u) {
            has_hints = 1u;
            break;
        }
    }
    if (has_hints) {
        spectrum_board_clear_legal_hints();
    }
    hints_selected_row = NO_SQUARE;
    hints_selected_col = NO_SQUARE;
}

void movement_hints_show(uint16_t square) NETCHESSZX_FASTCALL
{
    uint8_t row = (uint8_t)square;
    uint8_t col = (uint8_t)(square >> 8);
    if (netchesszx_movement_hints == 0u ||
        local_turn == 0u || netchesszx_session_view()->pending_local_ply != 0u) {
        return;
    }
    if (hints_selected_row == row && hints_selected_col == col) {
        return;
    }
    spectrum_board_show_legal_hints(row, col);
    hints_selected_row = row;
    hints_selected_col = col;
}

static void input_stop_and_show_cursor(void)
{
    edit_stop_clear();
    cursor_show();
}

static void about_restore_game(void)
{
    spectrum_presentation_restore_about(spectrum_board_cells());
    if (spectrum_editor_active) {
        netchesszx_input_edit_render_overlay();
    } else if (local_turn) {
        cursor_request_hints();
        cursor_show();
    }
    suppress_current_key();
}

static void about_restore_full_board(void)
{
    spectrum_presentation_close_about();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_draw_board();
    status_show_phase(status_phase_current);
    spectrum_gui_restore_side_panels();
    if (spectrum_editor_active) {
        netchesszx_input_edit_render_overlay();
    }
}

/* Statistics page over the board; on overlay failure a hidden local offer
   is disarmed so the next key re-shows it. */
static void game_stats_open(void)
{
    spectrum_presentation_hide_board();
    spectrum_gui_show_board_page(3u);
    if (!spectrum_render_game_stats()) {
        about_restore_game();
        if (confirm_action == CONFIRM_RESTART_GAME) {
            confirm_action = CONFIRM_NONE;
        }
        notify_error(spectrum_app_overlay_load_error);
    }
    suppress_current_key();
}

static void about_open(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    if (!spectrum_gui_show_about()) {
        about_restore_game();
        notify_error(spectrum_app_overlay_load_error);
        return;
    }
    suppress_current_key();
}

static void turn_set_notice(uint8_t is_local_turn, uint8_t show_notice)
{
    cursor_hide();
    if (is_local_turn) {
        cursor_reset_default_square();
        local_turn = 1u;
        status_refresh_game();
        if (show_notice) {
            notify_info(NETCHESSZX_UI_PHASE_YOUR_TURN);
        }
        cursor_show();
    } else {
        local_turn = 0u;
        status_refresh_game();
        if (show_notice) {
            notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        }
    }
}

static void local_controls_reset(uint8_t is_local_turn)
{
    edit_stop_clear();
    spectrum_input_flush_until_release();
    cursor_home(0x0406u);
    if (is_local_turn) {
        turn_set_notice(1u, 1u);
    } else {
        local_turn = 0u;
        if (netchesszx_session_view()->game_status_active) {
            status_refresh_game();
            notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        }
    }
}

static void suppress_key_until_release(uint8_t key)
{
    spectrum_input_suppress_until_release(key);
}

/* Whether the side to move is the local player. */
static uint8_t local_turn_now(void)
{
    return netchesszx_session_has_local_turn(side_to_move_is_white());
}

/* Classic's snapshot applies return 0 when BOARD could not load, with the
   board, ply and history unchanged; elsewhere BOARD always loads and they
   return nothing (SESSION_APP_BOARD_APPLY). */
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
#define BOARD_APPLY_RESULT uint8_t
#define BOARD_APPLIED 1u
#else
#define BOARD_APPLY_RESULT void
#define BOARD_APPLIED
#endif

static BOARD_APPLY_RESULT apply_takeback_snapshot(void)
{
    if (takeback_snapshot_ply == 0u) {
        return BOARD_APPLIED;
    }
    movement_hints_clear();
    pending_local_clear();
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
    if (!spectrum_board_undo_restore(&spectrum_stats_undo)) {
        return 0u;
    }
#else
    (void)spectrum_board_undo_restore(&spectrum_stats_undo);
#endif
    netchesszx_session_takeback_committed(
        (uint16_t)(takeback_snapshot_ply - 1u));
    game_check_state = takeback_snapshot_check_state;
    spectrum_gui_add_chat(
        (char)((uint8_t)(takeback_snapshot_local
                             ? netchesszx_local_side_char()
                             : netchesszx_remote_side_char()) |
               NETCHESSZX_CHAT_EVENT_SIDE_FLAG),
        NETCHESS_PROTO_TAKEBACK);
    spectrum_gui_remove_last_move(takeback_snapshot_ply);
    spectrum_journal_after_takeback();
    takeback_clear();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_redraw_board_squares();
    spectrum_app_move_timer_reset();
    turn_set_notice(local_turn_now(),
                    (uint8_t)(game_check_state != SPECTRUM_BOARD_CHECK));
    recovery_checkpoint();
    return BOARD_APPLIED;
}

static uint8_t saveload_flags(void)
{
    uint8_t flags = 0u;

    if (netchesszx_session_view()->game_status_active) {
        flags |= NETCHESSZX_SAVE_FLAG_ACTIVE;
    }
    if (netchesszx_session_view()->game_over) {
        flags |= NETCHESSZX_SAVE_FLAG_GAME_OVER;
    }
    if (game_check_state == SPECTRUM_BOARD_CHECK) {
        flags |= NETCHESSZX_SAVE_FLAG_CHECK;
    }
    return flags;
}

#ifdef NETCHESSZX_RECOVERY
/* RECOVERY encodes the live position itself, so an applied snapshot is
   checkpointed as the board now stands. */
static void recovery_checkpoint(void)
{
    if (!spectrum_recovery_capture(saveload_flags(), 0) &&
        !(recovery_state & RECOVERY_WARNING)) {
        recovery_state |= RECOVERY_WARNING | RECOVERY_NOTICE_PENDING;
    }
}

static void recovery_notify(void)
{
    if (recovery_state & RECOVERY_NOTICE_PENDING) {
        recovery_state &= (uint8_t)~RECOVERY_NOTICE_PENDING;
        notify_error(msg_recovery_failed);
    }
}
#else
#define recovery_notify() ((void)0)
#endif

static void local_save_game(const char *name)
{
#ifndef NETCHESSZX_RECOVERY
    netchesszx_save_meta_t meta;

    spectrum_board_snapshot_save(&restore_snapshot);
    meta.ply = netchesszx_session_view()->game_ply;
    meta.flags = saveload_flags();
    meta.host_color = netchesszx_session_identity_host_color();
    meta.view_flags = spectrum_gui_is_board_flipped()
        ? NETCHESSZX_SAVE_VIEW_FLIPPED
        : 0u;
    spectrum_time_game_timer_save(meta.timers);
#endif
    if (
#ifdef NETCHESSZX_RECOVERY
        spectrum_recovery_capture(saveload_flags(), restore_b64_pending)
#else
        spectrum_restore_build_b64(&restore_snapshot, &meta,
                                   restore_b64_pending)
#endif
        &&
        spectrum_saveload_write(name, restore_b64_pending)) {
        notify_info_msg(SPECTRUM_GUI_MSG_SAVE_OK);
    } else {
        notify_error_msg(SPECTRUM_GUI_MSG_SAVE_FAIL);
    }
}

/* Applies and checkpoints the decoded snapshot; on Classic 0 when BOARD
   could not load, with the board, ply and history unchanged. */
static BOARD_APPLY_RESULT saveload_apply_snapshot(
    const netchesszx_save_meta_t *meta, uint8_t board_flipped)
{
    movement_hints_clear();
    pending_takeback_clear();
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
    if (!spectrum_board_snapshot_restore(&restore_snapshot)) {
        return 0u;
    }
#else
    (void)spectrum_board_snapshot_restore(&restore_snapshot);
#endif
    spectrum_gui_set_board_view(board_flipped);
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_presentation_hide_board();
    netchesszx_session_restore_committed(
        meta->ply,
        (uint8_t)(meta->flags & NETCHESSZX_SAVE_FLAG_ACTIVE),
        (uint8_t)(meta->flags & NETCHESSZX_SAVE_FLAG_GAME_OVER));
    game_check_state = (meta->flags & NETCHESSZX_SAVE_FLAG_CHECK)
        ? SPECTRUM_BOARD_CHECK
        : SPECTRUM_BOARD_CHECK_NONE;
    confirm_action = CONFIRM_NONE;
    /* Arm the staged history; the append call below consumes it at once
       (asm/spectrum/journal.asm), before the post-apply checkpoint. */
    spectrum_game_stats_result = 0;
    spectrum_journal_arm_load();
    spectrum_gui_reset_logs();
#if defined(NETCHESSZX_JOURNAL_HOOK) && !defined(NETCHESSZX_HOST_SESSION_TEST) && \
    !defined(NETCHESSZX_SPECTRANEXT)
    /* Classic and Next: SAVELOAD replays SHATJRNL.NEW from ply zero when the
       staged body is complete (pend bit 0 set, bit 6 clear), otherwise it
       paints the RESTORED marker (3 = odd ply, white column; 1 = even ply,
       black column). The same call then consumes the armed switch. */
    spectrum_journal_after_move((const char *)(uintptr_t)(
        (meta->ply & 1u) != 0u ? 3u : 1u));
#else
#if defined(NETCHESSZX_SPECTRANEXT) && defined(NETCHESSZX_RECOVERY) && \
    defined(NETCHESSZX_JOURNAL_HOOK) && !defined(NETCHESSZX_HOST_SESSION_TEST)
    /* SpectraNext: SAVELOAD has no room for the replay; RECOVERY paints the
       history (or the marker) before the switch is consumed below. */
    if (!spectrum_recovery_replay((meta->ply & 1u) != 0u))
#endif
    {
        /* GUI_LOG maps a null move to the clockless RESTORED marker. */
        spectrum_gui_add_move((meta->ply & 1u) != 0u ? "1" : "2", 0);
    }
#if defined(NETCHESSZX_RECOVERY) && defined(NETCHESSZX_JOURNAL_HOOK) && \
    !defined(NETCHESSZX_HOST_SESSION_TEST)
    /* Consume the armed switch now, as Classic's replay call does, so the
       checkpoint written after this apply already names the live TMP. */
    spectrum_journal_after_move((const char *)1);
#endif
#endif
    spectrum_gui_set_board_pieces_visible(1u);
    spectrum_gui_redraw_board_squares();
    if (netchesszx_session_view()->game_status_active && !netchesszx_session_view()->game_over) {
        spectrum_app_game_timer_start();
        turn_set_notice(local_turn_now(), 0u);
    } else {
        spectrum_app_game_timer_stop();
        cursor_hide();
        local_turn = 0u;
        status_refresh_game();
    }
    /* Start clears a new game's timers; loaded values replace them afterwards. */
    spectrum_app_game_timer_restore(meta->timers);
#ifdef NETCHESSZX_RECOVERY
    recovery_state = 0u;
#endif
    recovery_checkpoint();
    return BOARD_APPLIED;
}

static uint8_t restore_host_color_ok(const netchesszx_save_meta_t *meta)
{
    return (uint8_t)(meta->host_color == netchesszx_session_identity_host_color());
}

#include "spectrum/app/session_ports.h"

static void local_load_game(const char *name)
{
    netchesszx_save_meta_t meta;

    if (SESSION_LOCAL_LOAD_BLOCKED()) {
        notify_wait_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
        return;
    }
    /* name == 0 stages the resume checkpoint; its history rides in
       SHATJRNL.NEW exactly like a JOURNAL LOAD body. */
    if (!(name == 0 ? spectrum_recovery_read(restore_b64_pending)
                    : spectrum_saveload_read(name, restore_b64_pending)) ||
        !spectrum_restore_decode(restore_b64_pending, &restore_snapshot,
                                 &meta) ||
        !restore_host_color_ok(&meta)) {
        notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
        return;
    }
    SESSION_LOAD_PREPARED(&meta, load_done);
load_done:
    return;
}

#define FILEUI_CLOSE_KEY 0x8au

static void fileui_open(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    spectrum_presentation_hide_board();
    spectrum_gui_show_fileui();
    if (!spectrum_fileui_open_render()) {
        spectrum_gui_set_board_snapshot(spectrum_board_cells());
        spectrum_gui_restore_board_area();
        notify_error(spectrum_app_overlay_load_error);
        return;
    }
    suppress_current_key();
}

static uint8_t fileui_process_key(uint8_t key) NETCHESSZX_FASTCALL
{
    uint8_t action;

    if (key == 0u) {
        return 1u;
    }
    if (key == FILEUI_CLOSE_KEY || key == SPECTRUM_GUI_KEY_MENU_FILE ||
        key == SPECTRUM_GUI_KEY_MENU) {
        about_restore_game();
        return 1u;
    }
    action = spectrum_fileui_send_key(key);
    if (action == SPECTRUM_FILEUI_ACT_LOAD) {
        about_restore_game();
        local_load_game(spectrum_fileui_selected_name());
    } else if (action == SPECTRUM_FILEUI_ACT_SAVE) {
        local_save_game(spectrum_fileui_selected_name());
        spectrum_fileui_rerender();
    } else if (action == SPECTRUM_FILEUI_ACT_ERASE) {
        confirm_action = CONFIRM_FILE_ERASE;
        notify_error_msg(SPECTRUM_GUI_MSG_ERASE_CONFIRM);
    }
    suppress_current_key();
    return 1u;
}

static const char *move_display_text(const char *move, const char *notation)
{
    if (netchesszx_notation_is_san() &&
        notation != 0 &&
        notation[0] != '\0') {
        return notation;
    }
    return move;
}

static const char *move_display_with_check_suffix(const char *move,
                                                   char *out,
                                                   uint8_t state)
{
    char *p = out;
    uint8_t count = 0u;

    while (*move != '\0' && count < 5u) {
        *p++ = *move++;
        ++count;
    }
    *p++ = state == SPECTRUM_BOARD_CHECK_MATE ? '#' : '+';
    *p = '\0';
    return out;
}

static uint8_t move_san_prepare(const char *move, char *out)
{
    return (uint8_t)(netchesszx_notation_is_san() &&
                     spectrum_board_move_san_base(move, out) != 0u);
}

static void finish_applied_move(const char *ply_text,
                                 const char *move,
                                 char *san,
                                 uint8_t san_ready,
                                 const char *display,
                                 uint8_t next_local_turn)
{
    char move_check_display[7];
    uint8_t state;

    if (san_ready) {
        state = spectrum_board_san_append_suffix(san);
        display = san;
    } else {
        state = spectrum_board_check_state();
        if (!netchesszx_notation_is_san() &&
            (state == SPECTRUM_BOARD_CHECK || state == SPECTRUM_BOARD_CHECK_MATE)) {
            display = move_display_with_check_suffix(move,
                                                      move_check_display,
                                                      state);
        }
    }
    spectrum_gui_add_move(ply_text, display);
    /* Open White's next row while GUI_LOG still owns the Classic overlay
       slot; a finished game gets no new row. */
    if (state <= SPECTRUM_BOARD_CHECK && side_to_move_is_white()) {
        spectrum_gui_prepare_move_row();
    }
    /* The opponent's-turn notice is a GUI_LOG message too: show it now, not
       after the journal I/O has evicted that overlay. */
    if (!(next_local_turn | state)) {           /* opponent's turn, no check */
        notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
    }
    spectrum_journal_after_move(move);
    /* The next turn starts once this one is captured: the seconds spent
       animating and checkpointing the move belong to the player now on the
       clock, as the peer counts them, not to nobody. */
    spectrum_app_move_timer_reset();
    spectrum_gui_apply_move(move);
    pending_local_clear();
    game_check_state = state == SPECTRUM_BOARD_CHECK
        ? SPECTRUM_BOARD_CHECK
        : SPECTRUM_BOARD_CHECK_NONE;
    if (state == SPECTRUM_BOARD_CHECK_MATE) {
        end_game_over(next_local_turn == 0u ? msg_checkmate_won : msg_checkmate_lost);
        return;
    }
    if (state == SPECTRUM_BOARD_STALEMATE) {
        end_game_over(msg_stalemate);
        return;
    }
    turn_set_notice(next_local_turn,
                    (uint8_t)(next_local_turn &
                              (state != SPECTRUM_BOARD_CHECK)));
    recovery_checkpoint();
}

static const char *move_san_or_fallback(const char *move,
                                        const char *notation,
                                        char *san)
{
    if (move_san_prepare(move, san)) {
        return san;
    }
    return move_display_text(move, notation);
}

static void end_game_over(const char *message)
{
    const char *chat_event = 0;
    char chat_who = '\0';

    if (message == msg_opponent_resign) {
        chat_event = message;
        chat_who = netchesszx_remote_side_char();
    } else if (message == msg_resignation_lost) {
        chat_event = message;
        chat_who = netchesszx_local_side_char();
    } else if (message == msg_draw_agreed) {
        chat_event = message;
        chat_who = CONTROL_IS_LOCAL_DRAW(netchesszx_session_view()->control_pending)
                       ? netchesszx_local_side_char()
                       : netchesszx_remote_side_char();
    }
    if (chat_event != 0) {
        spectrum_gui_add_chat(
            (char)((uint8_t)chat_who | NETCHESSZX_CHAT_EVENT_SIDE_FLAG),
            chat_event);
    }
    edit_stop_clear();
    cursor_hide();
    local_turn = 0u;
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    pending_takeback_clear();
    netchesszx_session_game_ended();
    confirm_action = CONFIRM_NONE;
    spectrum_app_game_timer_stop();
    status_show_endpoint();
    spectrum_game_stats_result = message;
    notify_error(message);
    /* Mate, stalemate, draw and resignation: an OVER checkpoint is never
       offered, so a finished game cannot come back as a live one. */
    recovery_checkpoint();
}

#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
static uint8_t send_busy_move_reply(const char *ply, uint8_t control)
{
    char *payload;

    if (!netchesszx_session_format_busy(ply)) {
        return 0u;
    }
    payload = spectrum_link_payload_scratch();
    spectrum_append_text(payload,
        (const char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR);
    return control ? session_tcp_control_reply(payload) : session_tcp_reply(payload);
}
#else
static uint8_t send_busy_move_reply(const char *ply, uint8_t control)
{
    char *payload = spectrum_link_payload_scratch();
    char *out = payload;

    out = spectrum_append_text(out, NETCHESS_PROTO_NACK_PREFIX);
    out = spectrum_append_text(out, ply);
    out = spectrum_append_text(out, " BUSY");
    return control ? session_tcp_control_reply(payload) : session_tcp_reply(payload);
}
#endif

static void game_start_state(void)
{
    uint8_t setup_pieces_visible = (uint8_t)(!netchesszx_session_view()->game_status_active && !netchesszx_session_view()->game_over &&
        spectrum_gui_are_board_pieces_visible());
    uint8_t was_flipped = spectrum_gui_is_board_flipped();
    uint8_t local_black = (uint8_t)!netchesszx_local_is_white();

    if (spectrum_gui_are_board_pieces_visible() && was_flipped != local_black) {
        spectrum_gui_hide_board_pieces();
    }
    spectrum_gui_set_board_view(local_black);
    spectrum_board_reset();
    spectrum_journal_new_game();
#ifdef NETCHESSZX_RECOVERY
    recovery_state = 0u;
#endif
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_moves();
    pending_takeback_clear();
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    netchesszx_session_game_started();
    spectrum_app_game_timer_start();
    if (!setup_pieces_visible || was_flipped != spectrum_gui_is_board_flipped()) {
        spectrum_gui_animate_board_pieces();
    }
    spectrum_gui_set_connected(2u);
    /* A ply-zero checkpoint retires the previous game's resume offer. Finish
       its synchronous I/O before exposing the playable cursor and resetting
       input. Failure only raises the once-per-game "Recovery failed" notice. */
    recovery_checkpoint();
    /* Every start, rematch or RESET included, repaints the status row once;
       per-ply refreshes then keep the unchanged GAME status. */
    status_show_phase(STATUS_PHASE_GAME);
    local_controls_reset(local_turn_now());
    cursor_show();
}

/* Accepted RESET/rematch starts immediately on both peers; GAME START is only
   for the initial game start path. */
static void reset_auto_start(void)
{
    /* The peer may accept while this player reads the statistics. */
    restore_about_full_board_if_visible();
    game_start_state();
    notify_success_msg(SPECTRUM_GUI_MSG_GAME_STARTED);
}

static uint8_t send_local_chat(const char *text)
{
    char *payload = spectrum_link_payload_scratch();

    if (!input_has_text(text)) {
        return 1u;
    }
    {
        char *p = spectrum_append_text(payload, NETCHESS_PROTO_CHAT_PREFIX);
        char *end = payload + SPECTRUM_LINK_PAYLOAD_MAX - 1u;

        while (*text != '\0' && p < end) {
            *p++ = *text++;
        }
        *p = '\0';
    }
    if (!netchesszx_session_send_text(payload)) {
        return 0u;
    }
    spectrum_gui_add_chat(netchesszx_local_side_char(), payload + 5u);
    return 1u;
}

static void apply_pending_local_move(const char *notation)
{
    char ply_text[8];
    char san[SPECTRUM_SAN_TEXT_MAX];
    const char *display;
    uint8_t san_ready;

    if (netchesszx_session_view()->pending_local_ply == 0u) {
        return;
    }

    (void)spectrum_append_u16(ply_text, netchesszx_session_view()->pending_local_ply);
    san_ready = move_san_prepare(netchesszx_session_view()->pending_local_move, san);
    if (san_ready) {
        display = san;
    } else {
        display = move_display_text(netchesszx_session_view()->pending_local_move, notation);
    }
    /* No source flash for our own move: the player picked that square, and
       the 0.48 s would only stretch the wait after the ACK. */
    if (!spectrum_board_apply_trusted_move_with_undo(netchesszx_session_view()->pending_local_move,
                                                      &spectrum_stats_undo)) {
        pending_local_clear();
        notify_move_rejected();
        cursor_show();
        return;
    }
    takeback_snapshot_save(netchesszx_session_view()->pending_local_ply, 1u);
    spectrum_gui_set_board_snapshot(spectrum_board_cells());

    netchesszx_session_move_committed(
        netchesszx_session_view()->pending_local_ply);
    finish_applied_move(ply_text, netchesszx_session_view()->pending_local_move, san, san_ready, display, 0u);
    HOST_SESSION_OBSERVE_MOVE_RESULT(notation);
}

static uint8_t cursor_select_or_move(uint8_t key)
{
    if (!netchesszx_session_view()->game_status_active) {
        if (!game_start_local(key)) {
            return 1u;
        }
        return 1u;
    }
    if (netchesszx_session_view()->control_pending &&
        !CONTROL_IS_CANCEL(netchesszx_session_view()->control_pending)) {
        notify_wait_opponent_ack();
        return 1u;
    }

    if (!local_turn) {
        notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        return 1u;
    }
    if (netchesszx_session_view()->pending_local_ply != 0u) {
        notify_wait_opponent_ack();
        return 1u;
    }

    return spectrum_ux_board_activate();
}

#include "spectrum/session/transitions.inc"

static uint8_t input_submit(void)
{
    char move[6];
    uint8_t rc;

    if (!input_has_text(local_input)) {
        goto input_cursor_ok;
    }

    if (spectrum_input_parse_move(local_input, move)) {
        SESSION_LOCAL_MOVE_REQUEST(local_input, move, rc, input_done,
                                   input_ok, input_cursor_ok);
    }

    SESSION_LOCAL_COMMAND(local_input, input_ok, input_cursor_ok);
    SESSION_LOCAL_CHAT_READY(input_ok);
    rc = send_local_chat(local_input);
    if (rc) {
        netchesszx_input_edit_history_add_overlay(local_input);
        input_stop_and_show_cursor();
        return LOCAL_KEY_CHAT_SENT;
    }
    return 0u;
input_cursor_ok:
    input_stop_and_show_cursor();
input_ok:
    rc = LOCAL_KEY_OK;
input_done:
    return rc;
}

static uint8_t menu_game_action(uint8_t key) NETCHESSZX_FASTCALL
{
    const char *command = key == SPECTRUM_GUI_KEY_MENU_TAKEBACK ? command_takeback :
        (key == SPECTRUM_GUI_KEY_MENU_DRAW ? command_draw : command_resign);

    local_input[0] = '\0';
    (void)spectrum_append_text(local_input, command);
    return input_submit();
}

static uint8_t process_local_key(uint8_t key) NETCHESSZX_FASTCALL
{
    if (key == 0u) {
        return 1u;
    }
    /* Full-width About hides pending notices, so dismiss it before handling
       a confirmation. FILE leaves the notice panel visible and stays behind
       the confirmation gate below. */
    if (spectrum_gui_about_visible() & 1u) {
#if NETCHESSZX_ABOUT_HAS_CHANGELOG
        /* C on About (not on statistics) swaps in the changelog page; on
           that page, or after a failed load, C closes like any other key. */
        if (spectrum_gui_about_visible() == 1u && (key | 0x20u) == 'c' &&
            *(volatile uint8_t *)NETCHESSZX_CHLOG_TITLE_ATTR_ADDR !=
                NETCHESSZX_CHLOG_TITLE_ATTR &&
            spectrum_render_chlog()) {
            goto key_suppressed;
        }
#endif
        /* Any key only closes a page: an accidental Y/N must never restart
           or leave. Re-show the prompt still waiting behind it, including
           a peer restart request that arrived while the page was open. */
        about_restore_game();
        if (confirm_action == CONFIRM_RESTART_GAME) {
            notify_error_msg(SPECTRUM_GUI_MSG_RESTART_GAME_CONFIRM);
        } else if (confirm_action == CONFIRM_RESET_ACCEPT) {
            notify_error_msg(netchesszx_session_view()->game_over
                                 ? SPECTRUM_GUI_MSG_RESTART_REQUEST
                                 : SPECTRUM_GUI_MSG_RESET_REQUEST);
        } else if (netchesszx_session_view()->game_over) {
            notify_wait_msg(SPECTRUM_GUI_MSG_RESTART_WAIT);
        }
        return 1u;
    }
    /* Key-priority layers: CONFIRM > FILE > MENU > GAME. A pending Y/N
       prompt lives in the right-hand panel and is answered with the
       browser still on screen. Accepting an erase refreshes the list in
       place; every other accepted action closes the browser first so it
       repaints a visible board. Declining leaves the browser untouched. */
    /* After the game S opens the statistics from the host's offer, from an
       opponent's restart request (a 1.2 peer sends one after every DRAW)
       and from the guest's wait for the offer. Not while a request of this
       player's own awaits its answer. */
    if (netchesszx_session_view()->game_over) {
        if (key == 's' &&
            (confirm_action == CONFIRM_RESTART_GAME ||
             confirm_action == CONFIRM_RESET_ACCEPT ||
             (confirm_action == CONFIRM_NONE &&
              !netchesszx_session_view()->control_pending &&
              !netchesszx_session_is_host()))) {
            game_stats_open();
            return 1u;
        }
        /* BREAK answers either restart prompt with N: both leave. */
        if (key == KEY_CANCEL && (confirm_action == CONFIRM_RESTART_GAME ||
                                  confirm_action == CONFIRM_RESET_ACCEPT)) {
            key = 'n';
        }
    }
    if (confirm_action == CONFIRM_RESTART_GAME && key == 13u) {
        /* The offer is local: chat stays open after the game and the
           next ordinary key re-arms it through SESSION_GAME_OVER_GATE. */
        confirm_action = CONFIRM_NONE;
    }
    if (confirm_action != CONFIRM_NONE) {
        if (confirm_action == CONFIRM_FILE_ERASE) {
            key = netchesszx_confirm_key_alias(key);
        }
#ifdef NETCHESSZX_RECOVERY
        /* Y resumes through the FILE LOAD consent/RESTORE path; N keeps
           every file and GAME START retires the offer. */
        if (confirm_action == CONFIRM_RESUME) {
            if (key == 'y' || key == 'n') {
                confirm_action = CONFIRM_NONE;
                if (key == 'y') {
                    local_load_game(0);
                } else {
                    notify_wait_msg(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
                }
                goto key_suppressed;
            }
            return 1u;
        }
#endif
        if (key == 'y') {
            if (confirm_action == CONFIRM_FILE_ERASE) {
                if (!spectrum_saveload_erase(
                        spectrum_fileui_selected_name())) {
                    confirm_action = CONFIRM_NONE;
                    notify_error_msg(SPECTRUM_GUI_MSG_SAVE_FAIL);
                    goto key_suppressed;
                }
                /* Erase is list maintenance: keep the browser open. */
                (void)spectrum_fileui_rerender();
                goto reject_confirm;
            }
            if (confirm_action == CONFIRM_DISCONNECT) {
                confirm_action = CONFIRM_NONE;
                /* A chosen exit ends a live game for recovery. */
                spectrum_recovery_retire();
                disconnect_to_setup();
                goto key_suppressed;
            }
            SESSION_ACCEPT_CONFIRM(confirm_handled, confirm_failed);
        } else if (key == 'n') {
reject_confirm:
            SESSION_REJECT_CONFIRM(confirm_handled, confirm_failed);
        }
        return 1u;
    }

    /* Every local move, command, chat and start below relies on this gate
       for its peer check. */
    SESSION_PEER_GATE((uint8_t)(key == KEY_CANCEL), confirm_handled);

    /* Restore borrows app_workspace while TX or partial RX is pending. Keep
       FILE, editor and cursor input behind this gate until the transfer ends. */
    SESSION_TRANSFER_GATE((uint8_t)(key == KEY_CANCEL), confirm_handled, confirm_failed);

    if (spectrum_gui_fileui_visible()) {
        return fileui_process_key(key);
    }

    if (key == SPECTRUM_GUI_KEY_MENU && spectrum_editor_active) {
        input_stop_and_show_cursor();
    }
    key = spectrum_gui_handle_menu_key(key);
    if (key == 0u) {
        return 1u;
    }

    if (spectrum_editor_active) {
        if (key == KEY_CANCEL) {
            input_stop_and_show_cursor();
            return 1u;
        }
        if (key == 13u) {
            return input_submit();
        }
        if (!spectrum_editor_append_hot(key)) {
            netchesszx_input_edit_key_overlay(key);
        }
        return 1u;
    }

    /* ENTER precedes the OVER gate: chatting stays possible after a game. */
    if (key == 13u) {
        SESSION_EDITOR_GATE(confirm_handled);
        cursor_hide();
        netchesszx_input_edit_begin_empty_overlay();
        notify_info(NETCHESSZX_UI_NOTICE_TYPE_MOVE_CHAT);
        return 1u;
    }

    /* After the game BREAK leaves to Setup, also while the host waits for
       the answer to its restart request. */
    if (netchesszx_session_view()->game_over && key == KEY_CANCEL) {
        disconnect_to_setup();
        goto key_suppressed;
    }

    SESSION_GAME_OVER_GATE(confirm_handled);

    if (key == SPECTRUM_GUI_KEY_MENU_ABOUT) {
        about_open();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_FILE) {
        fileui_open();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_DISCC) {
        SESSION_CONFIRM_PROMPT(CONFIRM_DISCONNECT,
                               SPECTRUM_GUI_MSG_DISCONNECT_CONFIRM);
        goto key_suppressed;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_REST) {
        SESSION_RESET_REQUEST();
        return 1u;
    }

    if (key >= SPECTRUM_GUI_KEY_MENU_TAKEBACK &&
        key <= SPECTRUM_GUI_KEY_MENU_RESIGN) {
        return menu_game_action(key);
    }

    if (key == SPECTRUM_GUI_KEY_MENU_FLIP) {
        cursor_refresh();
        spectrum_gui_toggle_board_view();
        cursor_request_hints();
        cursor_show();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_HINTS) {
        netchesszx_movement_hints ^= 1u;
        if (netchesszx_movement_hints == 0u) {
            movement_hints_clear();
        }
        cursor_request_hints();
        cursor_show();
        /* HINTS_ON == HINTS_OFF + 1: one cold catalog call, no resident text. */
        notify_info_msg((uint8_t)(SPECTRUM_GUI_MSG_HINTS_OFF +
                                  netchesszx_movement_hints));
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_THEME ||
        key == SPECTRUM_GUI_KEY_MENU_ASSET) {
        movement_hints_clear();
        spectrum_gui_clear_cursor_coords();
        if (key == SPECTRUM_GUI_KEY_MENU_THEME) {
            spectrum_presentation_apply_theme(
                (uint8_t)(netchesszx_board_theme_index + 1u));
        } else {
            uint8_t next_set = (uint8_t)(netchesszx_piece_set_index + 1u);
            if (next_set >= NETCHESSZX_PIECE_SET_COUNT) {
                next_set = 0u;
            }
            spectrum_setup_preview_piece_set(next_set);
        }
        cursor_request_hints();
        cursor_show();
        return 1u;
    }

    key = nav_key_alias(key);

    if (key == 32u) {
        return cursor_select_or_move(key);
    }
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT ||
        key == KEY_RIGHT || key == 'q' || key == 'a' ||
        key == 'o' || key == 'p') {
        cursor_move(key);
    }
confirm_handled:
    return 1u;
confirm_failed:
    return 0u;
key_suppressed:
    suppress_current_key();
    return 1u;
}

static void handle_opponent_disconnected_with(const char *message)
{
    confirm_action = CONFIRM_NONE;
    clear_disconnected_session_state();
    edit_stop_clear();
    reset_board_moves_chat();
    /* The peer name is gone; the listen/reconnect wait that follows never
       restores the side panels, so repaint the MOVES header here. */
    spectrum_gui_moves_header();
    spectrum_gui_hide_board_pieces();
    restore_about_full_board_if_visible();
    spectrum_gui_set_connected(0u);
    spectrum_gui_set_status_error(message);
    notify_error(message);
}

static void handle_opponent_disconnected(void)
{
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
    handle_opponent_disconnected_with(disconnect_message);
    disconnect_message = msg_connection_lost;
#else
    handle_opponent_disconnected_with(msg_connection_lost);
#endif
}

static void mqtt_peer_reset_wait_state(void)
{
    status_phase_current &= SPECTRUM_STATUS_PHASE_MASK;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_hide_menu();
    if (local_turn) {
        cursor_hide();
    }
    netchesszx_session_peer_reset();
    spectrum_gui_moves_header();
    netchesszx_session_peer_wait();
    pending_takeback_clear();
    spectrum_app_game_timer_stop();
    reset_board_moves_chat();
    spectrum_gui_hide_board_pieces();
    local_controls_reset(0u);
    restore_about_full_board_if_visible();
    spectrum_gui_set_connected(1u);
    status_show_endpoint();
}

static void mqtt_peer_disconnected_wait(void)
{
    mqtt_peer_reset_wait_state();
    notify_error(msg_connection_lost);
    wait_after_notice();
    notify_wait_opponent();
}


static void game_message_loop(void)
{
    char *payload = spectrum_link_payload_scratch();
    char ply[6];
    char move[6];
    char notation[8];
    netchesszx_session_ping_t ping;
    uint8_t poll_status;
    uint8_t dispatch_status;
    const uint8_t pending_retry_period = PENDING_RETRY_POLLS;
    const uint16_t control_cancel_period = CONTROL_DECISION_FRAMES;
    netchesszx_session_schedule_t schedule;

    netchesszx_session_event_t event;
    netchesszx_session_poll_result_t poll;

    SESSION_SCHEDULE_BEGIN(&schedule, pending_retry_period, control_cancel_period);

    /* The panels are normally on screen already (Setup exit, or the previous
       session's teardown): rebuilding them here blanked and repainted the
       whole MOVES header at the instant the opponent connected. */
    if (!spectrum_gui_side_panels_visible()) {
        spectrum_gui_restore_side_panels();
    }
    reset_board_moves_chat();
    netchesszx_session_loop_begin();
    clear_disconnected_session_state();
    if (netchesszx_session_identity_host_color_ready()) {
        spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    }
    spectrum_gui_set_connected(netchesszx_transport_is_mqtt() ? 1u : 2u);
    status_show_endpoint();
    /* Peer is not confirmed at connect time: show "waiting" until the direct
       guest HELLO arrives (host) or the host HELLO arrives (guest). The
       "press SPACE" prompt is raised later, once peer ready is set. */
    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
    local_controls_reset(0u);
    netchesszx_session_ping_reset(&ping);
    HOST_SESSION_EXPOSE_PING(&ping);
    if (!session_begin_link()) {
        return;
    }

    while (1) {
        spectrum_app_tick();
        spectrum_journal_notify();
        recovery_notify();
        SESSION_PENDING_STEP(&schedule, pending_retry_period,
                              control_cancel_period, dispatch_status);
        if (!dispatch_status) {
            return;
        }
        dispatch_status = process_local_key(spectrum_input_poll_event());
        if (!dispatch_status) {
            goto disconnected;
        }
        if (dispatch_status == LOCAL_KEY_CHAT_SENT) {
            netchesszx_session_ping_reset(&ping);
        }
        if (setup_restart_requested) {
            return;
        }
        poll_status = netchesszx_session_poll(&ping,
                                               payload,
                                               SPECTRUM_LINK_PAYLOAD_MAX,
                                               &poll);
        if (poll_status == NETCHESSZX_SESSION_POLL_DISCONNECTED) {
disconnected:
            handle_opponent_disconnected();
            return;
        }
        if (poll_status != NETCHESSZX_SESSION_POLL_EVENT) {
            SESSION_IDLE_STEP(&schedule, pending_retry_period, dispatch_status);
            if (!dispatch_status) {
                return;
            }
            continue;
        }
        event = poll.event;
#ifndef NETCHESSZX_HOST_SESSION_TEST
        SESSION_BEGIN_EVENT(event);
#endif
        dispatch_status = session_presence_handle_event(event,
                                                        payload,
                                                        poll.retained);
        if (dispatch_status == SESSION_DISPATCH_EXIT) {
            return;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED) {
            SESSION_PRESENCE_RESET_PING(event, &ping);
            continue;
        }
        if (!SESSION_CAN_DISPATCH_CONTROL()) {
            continue;
        }
        dispatch_status = session_control_handle_event(event, payload);
        if (dispatch_status == SESSION_DISPATCH_EXIT) {
            return;
        }
        if (dispatch_status == SESSION_DISPATCH_RETRY_RESET) {
            SESSION_RETRY_RESET(&schedule, pending_retry_period);
            continue;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED_LIVE) {
            netchesszx_session_ping_rx_data(&ping);
            continue;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED) {
            continue;
        }
        SESSION_MOVE_EVENT(event, payload, ply, move, notation, &ping,
                           dispatch_status);
        if (dispatch_status == SESSION_DISPATCH_EXIT) {
            return;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED) {
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_CHAT &&
                   netchess_proto_parse_chat(payload,
                                             payload,
                                             SPECTRUM_LINK_PAYLOAD_MAX)) {
            spectrum_gui_add_chat(netchesszx_remote_side_char(), payload);
        }
        SESSION_TEXT_EVENT(event, payload);
    }
}

#ifndef NETCHESSZX_HOST_SESSION_TEST
int main(void)
{
    uint8_t warm_restart;
    uint8_t config_state;

    if (!spectrum_assets_load()) {
        spectrum_assets_fatal();
    }
#ifndef NETCHESSZX_CLASSIC_BOOT
#if !defined(NETCHESSZX_NEXT_BANKING) && !defined(NETCHESSZX_SPECTRANEXT)
    spectrum_frame_clock_init();
#endif
    spectrum_app_tick_init();
#endif
    config_state = netchesszx_config_load_overlay();
    /* Cold boot probes and prefers a hardware RTC. The clock path falls back
       to the saved numeric UTC offset when the device has no usable RTC. */
    netchesszx_timezone = NETCHESSZX_TIME_RTC;

connection_setup:
    confirm_action = CONFIRM_NONE;
    netchesszx_session_begin_setup();
    spectrum_gui_set_board_view(0u);
    spectrum_gui_set_board_pieces_visible(0u);
    /* The hint bitmap lives at a fixed low-RAM address and so has no
       cold-boot initializer. movement_hints_clear() dispatches the HINTS
       overlay for any non-zero row, which would repaint arbitrary squares
       from garbage. */
    memset(netchesszx_hinted_rows, 0, sizeof(netchesszx_hinted_rows));
    spectrum_board_clear();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_logs();
    spectrum_gui_set_connected(0u);
    notify_info("");
    warm_restart = setup_restart_requested;
    if (warm_restart) {
        setup_restart_requested = 0u;
        spectrum_gui_restore_board_area();
        status_show_connection_setup();
    } else {
        spectrum_gui_draw_board();
        status_show_connection_setup();
    }
    /* Every return to Setup must replenish the one-shot ESP hard-reset budget
       and force a fresh CIFSR into resident last_ip; skipping this strands a
       wedged link with no way back. A warm return does that silently: the
       cold-boot panel misdescribes the work and the reconnect needs neither the
       UART restart nor a clock it already has. Anything failing drops the quiet
       flag so the retry shows where it is stuck. */
    while (!connection_preflight_run(warm_restart)) {
        warm_restart = 0u;
        retry_after_error(preflight_retry_msg);
    }
    if (!spectrum_net_runtime_clock_ready()) {
        spectrum_link_clock_retry_start();
    }
    config_state = session_setup_run(config_state);
    spectrum_link_clock_retry_cancel();
    if (netchesszx_session_identity_host_color_ready()) {
        spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    }
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_restore_side_panels();
    spectrum_gui_set_connected(1u);
    status_show_endpoint();

    if (netchesszx_transport_is_mqtt()) {
        while (1) {
            netchesszx_session_seat_probe_reset();
            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
            if (netchesszx_session_mqtt_start()) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
            } else {
                if (retry_after_error(msg_connect_failed)) {
                    goto cancel_to_setup;
                }
            }
        }
    } else if (netchesszx_session_is_host()) {
        while (1) {
            uint8_t listen_rc;

            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
            listen_rc = spectrum_link_listen();
            if (listen_rc) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                while (1) {
                    uint8_t wait_rc;

                    status_show_endpoint();
                    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
                    wait_rc = spectrum_link_wait_pc_connect();
                    if (wait_rc == SPECTRUM_LINK_CANCELLED) {
                        goto cancel_to_setup;
                    }
#ifdef NETCHESSZX_SPECTRANEXT
                    /* MATCH lobby or pairing failure: say so; the next wait
                       registers again. */
                    if (wait_rc == SPECTRUM_LINK_FAILED) {
                        if (retry_after_error(msg_connect_failed)) {
                            goto cancel_to_setup;
                        }
                        continue;
                    }
#endif
                    if (wait_rc) {
                        game_message_loop();
                        if (setup_restart_requested) {
                            goto connection_setup;
                        }
                        wait_after_notice();
                        /* Session over (peer lost or left): restart the listen
                           from scratch so the ESP server drops stale links
                           before the next accept. */
                        break;
                    }
                }
            } else {
                if (retry_after_error(msg_connect_failed)) {
                    goto cancel_to_setup;
                }
            }
        }
    } else {
        while (1) {
            uint8_t connect_rc;

            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
#ifdef NETCHESSZX_SPECTRANEXT
            if (netchesszx_link_match) {
                connect_rc = spectrum_net_select_host();
                if (connect_rc == 1u) {
                    spectrum_gui_restore_side_panels();
                    status_show_connecting();
                    notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
                    connect_rc = spectrum_net_join_host();
                }
            } else {
                connect_rc = spectrum_link_connect_host();
            }
#else
            connect_rc = spectrum_link_connect_host();
#endif
            if (connect_rc == SPECTRUM_LINK_CANCELLED) {
                goto cancel_to_setup;
            }
#ifdef NETCHESSZX_SPECTRANEXT
            /* Another guest picked this MATCH guest from its own menu. Setup
               clears the notice, so hold it before returning there. */
            if (connect_rc == SPECTRUM_LINK_NOT_HOST) {
                retry_after_error(NETCHESSZX_UI_ERROR_RIVAL_NOT_HOST);
                goto cancel_to_setup;
            }
            if (netchesszx_link_match && connect_rc != 1u) {
                retry_after_error(msg_connect_failed);
                goto cancel_to_setup;
            }
#endif
            if (connect_rc) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
                wait_after_notice();
            } else {
                spectrum_gui_notify_persistent(NETCHESSZX_UI_PHASE_WAITING_HOST);
                if (wait_notice_frames(1u)) {
                    goto cancel_to_setup;
                }
            }
        }
    }

    /* The connection loops never fall out; every cancel lands here. */
cancel_to_setup:
    suppress_key_until_release(KEY_CANCEL);
    setup_restart_requested = 1u;
    goto connection_setup;
}
#endif
