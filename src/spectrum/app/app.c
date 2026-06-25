#include "spectrum/board/board.h"
#include "spectrum/board/san.h"
#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/config/session.h"
#include "spectrum/config/setup_menu.h"
#include "spectrum/session/direct.h"
#include "spectrum/session/event.h"
#include "spectrum/session/mqtt.h"
#include "spectrum/session/outgoing.h"
#include "spectrum/session/ping.h"
#include "spectrum/session/poll.h"
#include "spectrum/transport/esp_at.h"
#include "spectrum/transport/link.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/input.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/text.h"
#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/ui_messages.h"

#include <string.h>


#define KEY_UP 0x81u
#define KEY_DOWN 0x82u
#define KEY_LEFT 0x83u
#define KEY_RIGHT 0x84u
#define KEY_HOME 0x88u
#define KEY_END 0x89u
#define KEY_CANCEL 0x8au
#define NO_SQUARE 0xffu
#define LOCAL_MOVE_NET_FAIL 0u
#define LOCAL_MOVE_REJECTED 1u
#define LOCAL_MOVE_SENT 2u
#define LOCAL_CHAT_TEXT_MAX (SPECTRUM_LINK_PAYLOAD_MAX - 6u)
#define LOCAL_INPUT_MAX LOCAL_CHAT_TEXT_MAX
#define INPUT_HISTORY_SIZE 2u
#define INPUT_HISTORY_TEXT_MAX 31u
#define STATUS_PHASE_CONNECTION_SETUP 0u
#define STATUS_PHASE_GAME_SETUP 1u
#define STATUS_PHASE_CONNECTING 2u
#define STATUS_PHASE_CONNECTED 3u
#define STATUS_PHASE_GAME 4u
#define STATUS_LINE_MAX 53u
#define INPUT_HISTORY_NONE 0xffu
#define EDIT_NET_POLL_PERIOD 5u
#define MQTT_SETUP_REANNOUNCE_TICKS 200u
#define DIRECT_HELLO_REANNOUNCE_TICKS 80u
#define PENDING_RETRY_TICKS 120u
#define SETUP_ROW_GAME 0u
#define SETUP_ROW_LINK 1u
#define SETUP_ROW_ROOM 2u
#define SETUP_ROW_MQTT 3u
#define SETUP_ROW_SIDE 4u
#define SETUP_ROW_NOTATION 5u
#define SETUP_ROW_BOARD 6u
#define SETUP_ROW_SET 7u
#define SETUP_ROW_HINTS 8u
#define SETUP_ROW_ACTION 9u
#define SETUP_ROW_COUNT 10u
#define SETUP_MASK_GAME 0x0001u
#define SETUP_MASK_LINK 0x0002u
#define SETUP_MASK_ROOM 0x0004u
#define SETUP_MASK_MQTT 0x0008u
#define SETUP_MASK_SIDE 0x0010u
#define SETUP_MASK_NOTATION 0x0020u
#define SETUP_MASK_BOARD 0x0040u
#define SETUP_MASK_SET 0x0080u
#define SETUP_MASK_HINTS 0x0100u
#define SETUP_MASK_ACTION 0x0200u
#define SETUP_MASK_ALL 0x03ffu
#define SETUP_MASK_REQUIRED_JOIN (SETUP_MASK_GAME | SETUP_MASK_LINK | SETUP_MASK_ROOM | SETUP_MASK_MQTT | SETUP_MASK_NOTATION | SETUP_MASK_BOARD | SETUP_MASK_HINTS)
#define SETUP_MASK_REQUIRED_HOST_DIRECT (SETUP_MASK_GAME | SETUP_MASK_LINK | SETUP_MASK_MQTT | SETUP_MASK_SIDE | SETUP_MASK_NOTATION | SETUP_MASK_BOARD | SETUP_MASK_HINTS)
#define SETUP_MASK_REQUIRED_HOST_MQTT (SETUP_MASK_REQUIRED_JOIN | SETUP_MASK_SIDE)
#define SETUP_VALUE_ROLE_JOIN 0x01u
#define SETUP_VALUE_TRANSPORT_MQTT 0x02u
#define SETUP_VALUE_COLOR_BLACK 0x04u
#define SETUP_VALUE_NOTATION_SAN 0x08u
#define SETUP_VALUE_HINTS_ON 0x10u
#define SETUP_CHOICE_ROLE 0u
#define SETUP_CHOICE_TRANSPORT 1u
#define SETUP_CHOICE_COLOR 2u
#define SETUP_CHOICE_NOTATION 3u
#define SETUP_CHOICE_HINTS 4u
#define SETUP_CHOICE_SET 5u
#define SETUP_DIRTY_FULL 0x80u
#define SETUP_ATTR_TEXT 0x07u
#define SETUP_ATTR_HEADER 0x03u
#define SETUP_ATTR_CURSOR 0x38u
#define SETUP_ATTR_SELECTED 0x06u
#define SETUP_ATTR_SELECTED_CURSOR 0x39u
#define SETUP_ATTR_START 0x44u
#define SETUP_ATTR_START_CURSOR 0x38u
#define SETUP_ATTR_BASE NETCHESSZX_ATTR_BASE
#define SETUP_CLEAR_NONE 0xffu
#define CONFIRM_NONE 0u
#define CONFIRM_DISCONNECT 1u
#define CONFIRM_RESET_SEND 2u
#define CONFIRM_RESET_ACCEPT 3u
#define CONFIRM_RESTART_GAME 4u
#define CONFIRM_RESIGN_SEND 5u
#define CONFIRM_DRAW_SEND 6u
#define SETUP_ROOM_INPUT_MAX 6u
#define SETUP_PORT_INPUT_MAX 5u
#define setup_row_bit(row) ((uint16_t)((uint16_t)1u << (row)))
#define setup_screen_row(row) ((uint8_t)((row) == SETUP_ROW_ACTION ? NETCHESSZX_SETUP_ACTION_ROW_WITH_SIDE : (NETCHESSZX_SETUP_SCREEN_ROW_BASE + (row) + ((row) >= SETUP_ROW_SIDE ? 3u : 0u))))
#define setup_clear_row(row) ((uint8_t)((row) == SETUP_ROW_ACTION ? (NETCHESSZX_SETUP_ACTION_ROW_WITH_SIDE - NETCHESSZX_INFO_SETUP_LINE_BASE_ROW) : ((row) + ((row) >= SETUP_ROW_SIDE ? 3u : 0u))))
#define setup_role setup_choice[SETUP_CHOICE_ROLE]
#define setup_transport setup_choice[SETUP_CHOICE_TRANSPORT]
#define setup_host_color setup_choice[SETUP_CHOICE_COLOR]
#define setup_notation setup_choice[SETUP_CHOICE_NOTATION]
#define setup_hints setup_choice[SETUP_CHOICE_HINTS]
#define setup_focus_role setup_focus_choice[SETUP_CHOICE_ROLE]
#define setup_focus_transport setup_focus_choice[SETUP_CHOICE_TRANSPORT]
#define setup_focus_color setup_focus_choice[SETUP_CHOICE_COLOR]
#define setup_focus_notation setup_focus_choice[SETUP_CHOICE_NOTATION]
#define setup_focus_hints setup_focus_choice[SETUP_CHOICE_HINTS]
#define setup_focus_piece_set setup_focus_choice[SETUP_CHOICE_SET]
#define side_to_move_is_white() ((uint8_t)((game_ply & 1u) == 0u))
#define is_input_text_char(key) ((uint8_t)((key) >= 32u && (key) < 127u))
#define SETUP_EDIT_FLAG_MQTT 0x01u
#define SETUP_EDIT_FLAG_CURSOR 0x02u
#define SETUP_EDIT_FLAG_LOCAL 0x04u

#define local_input ((char *)NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR)
#define input_history ((char *)NETCHESSZX_LOWRAM_INPUT_HISTORY_ADDR)
#if LOCAL_INPUT_MAX + 1u > NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE
#error "local input exceeds fixed low-RAM region"
#endif
#if INPUT_HISTORY_SIZE * (INPUT_HISTORY_TEXT_MAX + 1u) > NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE
#error "input history exceeds fixed low-RAM region"
#endif
#define input_history_slot(index) \
    (input_history + ((uint16_t)(index) * (INPUT_HISTORY_TEXT_MAX + 1u)))
static uint8_t local_input_len;
static uint8_t local_input_cursor;
static uint8_t local_input_mode;
static uint8_t input_history_count;
static uint8_t input_history_pos = INPUT_HISTORY_NONE;
static uint8_t suppress_key;
static uint8_t edit_key_handled;
static uint8_t edit_net_poll_wait;
static uint8_t confirm_action;
static uint8_t setup_restart_requested;
static uint8_t cursor_row = 6u;
static uint8_t cursor_col = 4u;
static uint8_t selected_row = NO_SQUARE;
static uint8_t selected_col = NO_SQUARE;
static uint8_t hints_selected_row = NO_SQUARE;
static uint8_t hints_selected_col = NO_SQUARE;
static uint8_t local_turn;
static uint16_t game_ply;
static uint16_t pending_local_ply;
static char pending_local_move[6];
static uint8_t game_status_active;
static uint8_t game_check_state;
static uint8_t game_over;
static uint8_t start_pending;
static uint8_t reset_pending;
static uint8_t status_phase_current;
uint8_t setup_choice[6];
uint8_t setup_focus_choice[6];
uint8_t setup_focus_board_theme;
static uint16_t setup_defined_mask;
static uint16_t setup_visible_mask;
uint8_t setup_cursor;
static uint8_t setup_room_editing;
uint8_t setup_edit_row;
char setup_port_text[SETUP_PORT_INPUT_MAX + 1u];

/* User-facing notice strings (<= 28 chars). Transport-agnostic and framed as
   PLAYER (local) vs OPPONENT (remote); never device identity or role-specific
   host/peer wording. Sentence case, with CAPS reserved for critical events. */
static const char msg_connecting[] = NETCHESSZX_UI_PHASE_CONNECTING;
static const char msg_connected[] = NETCHESSZX_UI_PHASE_CONNECTED;
static const char msg_connect_failed[] = NETCHESSZX_UI_ERROR_CONNECTION_FAILED;
static const char msg_preflight_failed[] = NETCHESSZX_UI_ERROR_PREFLIGHT;
static const char msg_overlay_failed[] = NETCHESSZX_UI_ERROR_OVERLAY_LOAD;
static const char msg_select_options[] = NETCHESSZX_UI_NOTICE_SELECT_OPTIONS;
static const char msg_waiting_opponent[] = NETCHESSZX_UI_PHASE_WAITING_OPPONENT;
static const char msg_opponent_ready_go[] = NETCHESSZX_UI_NOTICE_OPPONENT_READY_GO;
static const char msg_opponent_ready_wait[] = NETCHESSZX_UI_NOTICE_OPPONENT_READY_WAIT;
static const char msg_opponent_turn[] = NETCHESSZX_UI_PHASE_OPPONENT_TURN;
static const char msg_game_not_started[] = NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED;
static const char msg_connection_lost[] = NETCHESSZX_UI_ERROR_CONNECTION_LOST;
static const char msg_disconnect_confirm[] = NETCHESSZX_UI_CONFIRM_DISCONNECT;
static const char msg_reset_confirm[] = NETCHESSZX_UI_CONFIRM_RESET;
static const char msg_reset_request[] = NETCHESSZX_UI_CONFIRM_RESET_REQUEST;
static const char msg_restart_request[] = NETCHESSZX_UI_CONFIRM_RESTART_REQUEST;
static const char msg_reset_rejected[] = NETCHESSZX_UI_ERROR_RESET_REJECTED;
static const char msg_reset_wait[] = NETCHESSZX_UI_NOTICE_WAITING_ACK;
static const char msg_reset_confirmed[] = NETCHESSZX_UI_EVENT_RESET_CONFIRMED;
static const char msg_game_started[] = NETCHESSZX_UI_EVENT_GAME_STARTED;
static const char msg_checkmate_won[] = NETCHESSZX_UI_EVENT_CHECKMATE_WON;
static const char msg_checkmate_lost[] = NETCHESSZX_UI_EVENT_CHECKMATE_LOST;
static const char msg_game_start_wire[] = "GAME START";
static const char msg_move_prefix[] = "MOVE ";
static const char msg_reset_wire[] = "RESET";
static const char msg_draw[] = NETCHESSZX_UI_EVENT_DRAW;
static const char msg_draw_rejected[] = NETCHESSZX_UI_ERROR_DRAW_REJECTED;
static const char msg_resign_wire[] = "RESIGN";
static const char msg_draw_request[] = NETCHESSZX_UI_CONFIRM_DRAW;
static const char msg_opponent_draw_request[] = NETCHESSZX_UI_CONFIRM_OPPONENT_DRAW;
static const char msg_resign_confirm[] = NETCHESSZX_UI_CONFIRM_RESIGN;
static const char msg_opponent_resign[] = NETCHESSZX_UI_EVENT_OPPONENT_RESIGN;
static const char msg_restart_game_confirm[] = NETCHESSZX_UI_CONFIRM_RESTART_GAME;
static const char msg_room_conflict[] = NETCHESSZX_UI_ERROR_ROOM_CONFLICT;
static const char msg_bye[] = "BYE";
static const char preflight_esp_fail[] = NETCHESSZX_PREFLIGHT_ROW_ESP_PREFIX "ESP FAIL";
static const char preflight_wifi_fail[] = NETCHESSZX_PREFLIGHT_ROW_WIFI_PREFIX "WIFI FAIL";
static const char preflight_ip_fail[] = NETCHESSZX_PREFLIGHT_ROW_IP_PREFIX "IP FAIL";
static const char *preflight_retry_msg = msg_preflight_failed;

static void handle_opponent_disconnected(void);
static void handle_opponent_disconnected_with(const char *message);
static void end_game_over(const char *message);
static void local_controls_reset(uint8_t is_local_turn);
static void cursor_hide(void);
static void about_restore_full_board(void);

static void notify_info(const char *text)
{
    spectrum_gui_notify(text, 0u);
}

static void notify_error(const char *text)
{
    spectrum_gui_notify(text, 1u);
}

static void notify_wait(const char *text)
{
    spectrum_gui_notify_persistent(text);
}

static void notify_wait_opponent(void)
{
    notify_wait(msg_waiting_opponent);
}

static void notify_wait_opponent_ack(void)
{
    notify_wait(msg_reset_wait);
}

static void notify_move_rejected(void)
{
    notify_error(NETCHESSZX_UI_ERROR_MOVE_REJECTED);
}

static void wait_after_notice(void);

static void retry_after_error(const char *msg)
{
    notify_error(msg);
    wait_after_notice();
}

static void wait_after_notice(void)
{
    uint16_t wait_frames = 250u;

    while (wait_frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_link_background_drain();
        spectrum_gui_tick();
        spectrum_link_background_drain();
    }
}

static uint8_t info_panel_overlay_line(const char *line, uint8_t mode)
{
    if (!spectrum_info_panel_overlay_line(line, mode)) {
        preflight_retry_msg = msg_overlay_failed;
        spectrum_gui_set_status_error(msg_overlay_failed);
        spectrum_gui_draw_status();
        return 0u;
    }
    return 1u;
}

static uint8_t preflight_line(const char *line)
{
    return info_panel_overlay_line(line, NETCHESSZX_INFO_MODE_KEEP);
}

static uint8_t preflight_fail(const char *line)
{
    preflight_retry_msg = line + 1u;
    (void)preflight_line(line);
    return 0u;
}

static uint8_t connection_preflight_run(void)
{
    preflight_retry_msg = msg_preflight_failed;

    if (!info_panel_overlay_line(NETCHESSZX_PREFLIGHT_ROW_UART_PREFIX "UART WAIT",
                                 NETCHESSZX_INFO_MODE_PREFLIGHT)) {
        return 0u;
    }
    spectrum_link_start_uart();
    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_UART_PREFIX "UART OK  ")) {
        return 0u;
    }

    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_ESP_PREFIX "ESP WAIT")) {
        return 0u;
    }
    if (!spectrum_esp_at_ensure_command_mode()) {
        return preflight_fail(preflight_esp_fail);
    }
    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_ESP_PREFIX "ESP OK  ")) {
        return 0u;
    }

    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_WIFI_PREFIX "WIFI WAIT")) {
        return 0u;
    }
    if (!spectrum_esp_at_prepare_radio()) {
        return preflight_fail(preflight_wifi_fail);
    }
    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_WIFI_PREFIX "WIFI OK  ")) {
        return 0u;
    }

    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_IP_PREFIX "IP WAIT")) {
        return 0u;
    }
    if (!spectrum_esp_at_query_ip_with_retry(8u)) {
        return preflight_fail(preflight_ip_fail);
    }
    spectrum_gui_set_connected(1u);
    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_IP_PREFIX "IP OK  ")) {
        return 0u;
    }

    if (!preflight_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK WAIT")) {
        return 0u;
    }
    if (spectrum_net_runtime_clock_ready() || spectrum_esp_at_sync_time()) {
        (void)preflight_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK OK  ");
    } else {
        (void)preflight_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK FAIL");
    }

    return 1u;
}

static void status_show_phase(uint8_t phase)
{
    status_phase_current = phase;
    spectrum_gui_status_phase(phase);
}

static void status_redraw_current(void)
{
    status_show_phase(status_phase_current);
}

static void status_show_endpoint(void)
{
    status_show_phase(STATUS_PHASE_CONNECTED);
}

static void status_show_connecting(void)
{
    status_show_phase(STATUS_PHASE_CONNECTING);
}

static void status_show_game_setup(void)
{
    status_show_phase(STATUS_PHASE_GAME_SETUP);
}

static void status_show_connection_setup(void)
{
    status_show_phase(STATUS_PHASE_CONNECTION_SETUP);
}

static void status_refresh_game(void)
{
    uint8_t black_to_move;

    if (!game_status_active) {
        return;
    }
    status_show_phase(STATUS_PHASE_GAME);
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
    pending_local_ply = 0u;
    pending_local_move[0] = '\0';
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
    game_status_active = 0u;
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    game_over = 0u;
    start_pending = 0u;
    reset_pending = 0u;
    netchesszx_session_peer_reset();
    pending_local_clear();
    spectrum_gui_clear_control_latches();
    spectrum_gui_game_timer_stop();
}

static void restore_game_state(void)
{
    uint8_t peer_ready = netchesszx_session_peer_ready();
    cursor_hide();
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_logs();
    game_ply = 0u;
    pending_local_clear();
    game_status_active = 0u;
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    game_over = 0u;
    start_pending = 0u;
    reset_pending = 0u;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_game_timer_stop();
    spectrum_gui_hide_board_pieces();
    spectrum_gui_set_connected(peer_ready ? 2u : 1u);
    status_show_endpoint();
    local_controls_reset(0u);
    restore_about_full_board_if_visible();
    if (!peer_ready) {
        notify_wait_opponent();
    } else if (netchesszx_session_can_start_game()) {
        notify_wait(msg_opponent_ready_go);
    } else {
        notify_wait(msg_opponent_ready_wait);
    }
}

static uint16_t parse_u16(const char *text)
{
    uint16_t value;
    return netchess_mqtt_session_parse_u16_token(text, &value) == 0 ? 0u : value;
}

static uint8_t parse_move_input(const char *text, char *move)
{
    return spectrum_input_parse_move(text, move);
}

static uint8_t filter_repeating_key(uint8_t key)
{
    if (key == 0u) {
        suppress_key = 0u;
        return 0u;
    }

    if (suppress_key != 0u) {
        if (key == suppress_key) {
            return 0u;
        }
        suppress_key = 0u;
    }
    return key;
}

static uint8_t poll_repeating_key(void)
{
    return filter_repeating_key(spectrum_gui_poll_key());
}

static uint8_t nav_key_alias(uint8_t key)
{
    if (key == '5') {
        return KEY_LEFT;
    }
    if (key == '6') {
        return KEY_DOWN;
    }
    if (key == '7') {
        return KEY_UP;
    }
    if (key == '8') {
        return KEY_RIGHT;
    }
    return key;
}

static void suppress_current_key(void)
{
    suppress_key = spectrum_gui_poll_key();
}

static uint8_t setup_has(uint8_t row)
{
    return (uint8_t)((setup_defined_mask & setup_row_bit(row)) != 0u);
}

static uint8_t session_setup_values(void)
{
    uint8_t values = 0u;

    if (setup_role == NETCHESSZX_SESSION_ROLE_JOIN) {
        values |= SETUP_VALUE_ROLE_JOIN;
    }
    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT) {
        values |= SETUP_VALUE_TRANSPORT_MQTT;
    }
    if (setup_host_color == NETCHESSZX_COLOR_BLACK) {
        values |= SETUP_VALUE_COLOR_BLACK;
    }
    if (setup_notation == NETCHESSZX_NOTATION_SAN) {
        values |= SETUP_VALUE_NOTATION_SAN;
    }
    if (setup_hints != 0u) {
        values |= SETUP_VALUE_HINTS_ON;
    }
    return values;
}

static uint16_t mqtt_new_session_id(void)
{
    uint16_t id = *(volatile uint16_t *)0x5c78u;
    return id == 0u ? 1u : id;
}

static void session_setup_default_room(void)
{
    strncpy(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE, NETCHESSZX_MQTT_CODE_MAX);
    netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX] = '\0';
}

static void session_setup_update_room_code(void)
{
    netchesszx_setup_update_room_code();
}

static char *session_setup_endpoint_text(uint8_t row)
{
    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT) {
        return netchesszx_mqtt_code;
    }
    if (row == SETUP_ROW_ROOM) {
        return netchesszx_direct_host;
    }
    return setup_port_text;
}

static void session_setup_render_edit_line(uint8_t row)
{
    const char *text;
    uint8_t flags = 0u;
    uint8_t max_len;

    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT) {
        flags = SETUP_EDIT_FLAG_MQTT;
        text = session_setup_endpoint_text(row);
        max_len = SETUP_ROOM_INPUT_MAX;
    } else if (row == SETUP_ROW_ROOM) {
        if (setup_role == NETCHESSZX_SESSION_ROLE_HOST) {
            text = spectrum_esp_at_last_ip();
            flags = SETUP_EDIT_FLAG_LOCAL;
        } else {
            text = netchesszx_direct_host;
        }
        max_len = NETCHESSZX_DIRECT_HOST_MAX;
    } else {
        text = setup_port_text;
        max_len = SETUP_PORT_INPUT_MAX;
    }
    if (setup_room_editing && setup_edit_row == row) {
        flags |= SETUP_EDIT_FLAG_CURSOR;
    }
    netchesszx_setup_render_edit_line(row, flags, text, max_len);
}

static uint16_t session_setup_compute_visible(void)
{
    return netchesszx_setup_compute_visible(setup_defined_mask);
}

static uint16_t setup_rows_from(uint8_t row)
{
    return (uint16_t)(SETUP_MASK_ALL & (uint16_t)~(setup_row_bit(row) - 1u));
}

static uint8_t setup_first_row(uint16_t mask)
{
    uint8_t row = 0u;

    while (row < SETUP_ROW_COUNT) {
        if (mask & setup_row_bit(row)) {
            return row;
        }
        ++row;
    }
    return SETUP_CLEAR_NONE;
}

static uint8_t setup_step_row(uint8_t row, uint8_t next)
{
    return netchesszx_setup_step_row(row, next, setup_visible_mask);
}

#define setup_next_row(row) setup_step_row((row), 1u)
#define setup_prev_row(row) setup_step_row((row), 0u)

static uint8_t session_setup_focus_values(void)
{
    return (uint8_t)(setup_focus_role |
                     (uint8_t)(setup_focus_transport << 1) |
                     (uint8_t)(setup_focus_color << 2) |
                     (uint8_t)(setup_focus_notation << 3) |
                     (uint8_t)(setup_focus_hints << 4));
}

static void session_setup_paint_attrs(void)
{
    netchesszx_setup_paint_attrs(
        session_setup_values(),
        setup_visible_mask,
        setup_defined_mask,
        setup_cursor,
        (uint8_t)(session_setup_focus_values() |
                  (uint8_t)(setup_focus_board_theme << 5)),
        (uint8_t)((uint8_t)(setup_choice[SETUP_CHOICE_SET] << 4) |
                  setup_focus_piece_set));
}
static void session_setup_render(uint8_t full,
                                 uint16_t force_dirty,
                                 uint8_t clear_from)
{
    uint16_t old_visible = setup_visible_mask;
    uint8_t clear_panel;
    uint16_t dirty_rows;
    uint16_t overlay_dirty;
    uint16_t edit_dirty;

    setup_visible_mask = session_setup_compute_visible();
    if ((setup_visible_mask & setup_row_bit(setup_cursor)) == 0u) {
        setup_cursor = SETUP_ROW_GAME;
    }
    clear_panel = full;
    dirty_rows = clear_panel ? setup_visible_mask
                             : (uint16_t)(setup_visible_mask & (uint16_t)~old_visible);
    dirty_rows |= (uint16_t)(setup_visible_mask & force_dirty);
    if (clear_panel) {
        spectrum_info_show_setup();
    } else if (clear_from != SETUP_CLEAR_NONE) {
        spectrum_info_clear_tail(setup_clear_row(clear_from));
    }
    edit_dirty = 0u;
    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT) {
        edit_dirty = (uint16_t)(dirty_rows & SETUP_MASK_ROOM);
    } else {
        edit_dirty = (uint16_t)(dirty_rows & (SETUP_MASK_ROOM |
                                              SETUP_MASK_MQTT));
    }
    overlay_dirty = (uint16_t)(dirty_rows & (uint16_t)~edit_dirty);
    if (overlay_dirty != 0u) {
        netchesszx_setup_render_rows(session_setup_values(),
                                     setup_visible_mask,
                                     overlay_dirty);
    }
    if (edit_dirty & SETUP_MASK_ROOM) {
        session_setup_render_edit_line(SETUP_ROW_ROOM);
    }
    if (edit_dirty & SETUP_MASK_MQTT) {
        session_setup_render_edit_line(SETUP_ROW_MQTT);
    }
    session_setup_paint_attrs();
}

static void session_setup_begin_room_edit(void)
{
    setup_edit_row = setup_cursor;
    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT &&
        strcmp(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE) == 0) {
        netchesszx_mqtt_code[0] = '\0';
    }
    setup_room_editing = 1u;
    session_setup_render_edit_line(setup_edit_row);
}

static void session_setup_room_backspace(void)
{
    if (netchesszx_setup_room_backspace()) {
        session_setup_render_edit_line(setup_edit_row);
    }
}

static void session_setup_room_edit_key(uint8_t key)
{
    char *text = session_setup_endpoint_text(setup_edit_row);

    if (key == KEY_CANCEL) {
        setup_room_editing = 0u;
        if (netchesszx_mqtt_code[0] == '\0' &&
            setup_role != NETCHESSZX_SESSION_ROLE_JOIN) {
            session_setup_default_room();
        }
        if (setup_transport == NETCHESSZX_TRANSPORT_DIRECT &&
            setup_edit_row == SETUP_ROW_MQTT) {
            (void)spectrum_append_u16(setup_port_text, netchesszx_direct_port);
        }
        session_setup_render_edit_line(setup_edit_row);
        notify_info(msg_select_options);
        suppress_key = key;
        return;
    }
    if (key == 8u) {
        session_setup_room_backspace();
        return;
    }
    if (key == 13u || key == 32u) {
        if (text[0] == '\0') {
            return;
        }
        if (setup_transport == NETCHESSZX_TRANSPORT_DIRECT &&
            setup_role == NETCHESSZX_SESSION_ROLE_JOIN &&
            setup_edit_row == SETUP_ROW_ROOM &&
            !netchesszx_setup_validate_ip(netchesszx_direct_host)) {
            notify_error(NETCHESSZX_UI_ERROR_BAD_IP);
            return;
        }
        if (setup_transport == NETCHESSZX_TRANSPORT_DIRECT &&
            setup_edit_row == SETUP_ROW_MQTT) {
            netchesszx_direct_port = parse_u16(text);
            if (netchesszx_direct_port == 0u) {
                return;
            }
        }
        setup_room_editing = 0u;
        setup_defined_mask |= setup_row_bit(setup_edit_row);
        session_setup_render(0u, setup_row_bit(setup_edit_row), SETUP_CLEAR_NONE);
        if (setup_edit_row == SETUP_ROW_ROOM) {
            setup_cursor = SETUP_ROW_MQTT;
        } else {
            setup_cursor = (setup_visible_mask & SETUP_MASK_SIDE) ? SETUP_ROW_SIDE
                                                                  : SETUP_ROW_NOTATION;
        }
        session_setup_paint_attrs();
        notify_info(msg_select_options);
        suppress_key = key;
        return;
    }

    if (netchesszx_setup_room_append(key)) {
        session_setup_render_edit_line(setup_edit_row);
    }
}

static void setup_clear_after(uint8_t row)
{
    setup_defined_mask &= (uint16_t)(setup_row_bit((uint8_t)(row + 1u)) - 1u);
}

static void session_setup_move_focus(uint8_t key)
{
    if (netchesszx_setup_move_focus(key)) {
        session_setup_paint_attrs();
    }
}

static uint8_t session_setup_can_start(void)
{
    uint16_t required;

    if (setup_role == NETCHESSZX_SESSION_ROLE_JOIN) {
        required = SETUP_MASK_REQUIRED_JOIN;
    } else if (setup_transport == NETCHESSZX_TRANSPORT_DIRECT) {
        required = SETUP_MASK_REQUIRED_HOST_DIRECT;
    } else {
        required = SETUP_MASK_REQUIRED_HOST_MQTT;
    }
    if ((setup_defined_mask & required) != required) {
        notify_info(NETCHESSZX_UI_NOTICE_SETUP_PENDING);
        return 0u;
    }
    return 1u;
}

static uint8_t session_setup_select(uint8_t key)
{
    uint8_t changed = 0u;
    uint8_t clear_from = SETUP_CLEAR_NONE;
    uint16_t old_visible = setup_visible_mask;
    uint16_t new_visible;
    uint8_t next_cursor = SETUP_CLEAR_NONE;
    uint8_t first_define;

    if (setup_cursor == SETUP_ROW_GAME) {
        first_define = (uint8_t)!setup_has(SETUP_ROW_GAME);
        changed = (uint8_t)(setup_role != setup_focus_role);
        setup_role = setup_focus_role;
        setup_defined_mask |= setup_row_bit(SETUP_ROW_GAME);
        if (first_define || changed) {
            session_setup_update_room_code();
        }
        if (changed) {
            setup_clear_after(SETUP_ROW_GAME);
            clear_from = SETUP_ROW_LINK;
        }
    } else if (setup_cursor == SETUP_ROW_LINK) {
        first_define = (uint8_t)!setup_has(SETUP_ROW_LINK);
        changed = (uint8_t)(setup_transport != setup_focus_transport);
        setup_transport = setup_focus_transport;
        setup_defined_mask |= setup_row_bit(SETUP_ROW_LINK);
        if (first_define || changed) {
            session_setup_update_room_code();
        }
        if (changed) {
            setup_clear_after(SETUP_ROW_LINK);
            clear_from = SETUP_ROW_ROOM;
        }
        if (setup_transport == NETCHESSZX_TRANSPORT_DIRECT &&
            setup_role == NETCHESSZX_SESSION_ROLE_HOST) {
            setup_defined_mask |= setup_row_bit(SETUP_ROW_ROOM);
        }
    } else if (setup_cursor == SETUP_ROW_ROOM) {
        if (netchesszx_setup_room_editable()) {
            session_setup_begin_room_edit();
            suppress_key = key;
            return 0u;
        }
        setup_defined_mask |= setup_row_bit(SETUP_ROW_ROOM);
    } else if (setup_cursor == SETUP_ROW_MQTT) {
        if (netchesszx_setup_room_editable()) {
            session_setup_begin_room_edit();
            suppress_key = key;
            return 0u;
        }
        setup_defined_mask |= setup_row_bit(SETUP_ROW_MQTT);
    } else if (setup_cursor == SETUP_ROW_SIDE) {
        setup_host_color = setup_focus_color;
        setup_defined_mask |= setup_row_bit(SETUP_ROW_SIDE);
    } else if (setup_cursor == SETUP_ROW_NOTATION) {
        setup_notation = setup_focus_notation;
        setup_defined_mask |= setup_row_bit(SETUP_ROW_NOTATION);
    } else if (setup_cursor == SETUP_ROW_BOARD) {
        netchesszx_board_theme_apply(setup_focus_board_theme);
        spectrum_gui_redraw_board_squares();
        setup_defined_mask |= setup_row_bit(SETUP_ROW_BOARD);
    } else if (setup_cursor == SETUP_ROW_SET) {
        if (setup_focus_piece_set != netchesszx_piece_set_index &&
            !netchesszx_piece_set_load(setup_focus_piece_set)) {
            notify_error(NETCHESSZX_UI_ERROR_SET_LOAD_FAILED);
            return 0u;
        }
        netchesszx_piece_set_index = setup_focus_piece_set;
        setup_choice[SETUP_CHOICE_SET] = setup_focus_piece_set;
        spectrum_gui_redraw_board_squares();
        setup_defined_mask |= setup_row_bit(SETUP_ROW_SET);
    } else if (setup_cursor == SETUP_ROW_HINTS) {
        setup_hints = setup_focus_hints;
        setup_defined_mask |= setup_row_bit(SETUP_ROW_HINTS);
    } else if (setup_cursor == SETUP_ROW_ACTION) {
        if (session_setup_can_start()) {
            netchesszx_notation = setup_notation;
            netchesszx_movement_hints = setup_hints;
            netchesszx_board_theme_apply(setup_focus_board_theme);
            netchesszx_session_configure(setup_role,
                                          setup_transport,
                                          setup_host_color);
            if (setup_transport == NETCHESSZX_TRANSPORT_MQTT &&
                setup_role == NETCHESSZX_SESSION_ROLE_HOST) {
                netchesszx_mqtt_session_id = mqtt_new_session_id();
            } else {
                netchesszx_mqtt_session_id = 0u;
            }
            if (setup_role == NETCHESSZX_SESSION_ROLE_JOIN) {
                netchesszx_host_color_ready = 0u;
            }
            suppress_key = key;
            return 1u;
        }
    }

    new_visible = session_setup_compute_visible();
    if (clear_from == SETUP_ROW_ROOM &&
        setup_transport == NETCHESSZX_TRANSPORT_DIRECT &&
        setup_role == NETCHESSZX_SESSION_ROLE_HOST &&
        (new_visible & SETUP_MASK_MQTT)) {
        next_cursor = SETUP_ROW_MQTT;
    } else if (clear_from != SETUP_CLEAR_NONE &&
               (new_visible & setup_row_bit(clear_from))) {
        next_cursor = clear_from;
    } else {
        next_cursor = setup_first_row((uint16_t)(new_visible & (uint16_t)~old_visible));
    }
    session_setup_render(0u,
                         clear_from == SETUP_CLEAR_NONE
                             ? 0u
                             : setup_rows_from(clear_from),
                         clear_from);
    if (next_cursor != SETUP_CLEAR_NONE) {
        setup_cursor = next_cursor;
        session_setup_paint_attrs();
    }
    if ((setup_cursor == SETUP_ROW_ROOM || setup_cursor == SETUP_ROW_MQTT) &&
        netchesszx_setup_room_editable()) {
        session_setup_begin_room_edit();
    }
    suppress_key = key;
    return 0u;
}

static void session_setup_run(void)
{
    uint8_t key;

    setup_role = NETCHESSZX_SESSION_ROLE_HOST;
    setup_transport = NETCHESSZX_TRANSPORT_MQTT;
    setup_host_color = NETCHESSZX_COLOR_WHITE;
    setup_notation = netchesszx_notation;
    setup_hints = netchesszx_movement_hints;
    setup_choice[SETUP_CHOICE_SET] = netchesszx_piece_set_index;
    setup_focus_role = setup_role;
    setup_focus_transport = setup_transport;
    setup_focus_color = setup_host_color;
    setup_focus_notation = setup_notation;
    setup_focus_hints = setup_hints;
    setup_focus_piece_set = netchesszx_piece_set_index;
    setup_defined_mask = 0u;
    setup_visible_mask = 0u;
    setup_cursor = SETUP_ROW_GAME;
    setup_room_editing = 0u;
    setup_edit_row = SETUP_ROW_ROOM;
    session_setup_default_room();
    (void)spectrum_append_u16(setup_port_text, netchesszx_direct_port);
    session_setup_render(1u, 0u, SETUP_CLEAR_NONE);
    status_show_game_setup();
    notify_info(msg_select_options);
    suppress_current_key();

    while (1) {
        key = poll_repeating_key();
        if (key == 0u) {
            spectrum_frame_wait();
            spectrum_gui_tick();
            continue;
        }
        if (setup_room_editing) {
            session_setup_room_edit_key(key);
            continue;
        }
        key = nav_key_alias(key);
        if (key == KEY_UP || key == 'q') {
            setup_cursor = setup_prev_row(setup_cursor);
            session_setup_paint_attrs();
            continue;
        }
        if (key == KEY_DOWN || key == 'a') {
            setup_cursor = setup_next_row(setup_cursor);
            session_setup_paint_attrs();
            continue;
        }
        if (key == KEY_LEFT || key == KEY_RIGHT ||
            key == 'o' || key == 'p') {
            session_setup_move_focus(key);
            continue;
        }
        if (key == 32u || key == 13u) {
            if (session_setup_select(key)) {
                return;
            }
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

static void edit_render(void)
{
    spectrum_gui_set_input_edit(local_input, local_input_len, local_input_cursor);
}

static char edit_char_at(uint8_t pos)
{
    return pos < local_input_len ? local_input[pos] : ' ';
}

static void edit_draw_cell(uint8_t pos, uint8_t cursor)
{
    spectrum_gui_input_cell(pos, edit_char_at(pos), cursor);
}

static void edit_cursor_show(void)
{
    edit_draw_cell(local_input_cursor, 1u);
}

static void edit_redraw_from(uint8_t start, uint8_t old_len)
{
    uint8_t i;

    for (i = start; i < local_input_len; ++i) {
        edit_draw_cell(i, 0u);
    }
    while (i <= old_len && i <= LOCAL_INPUT_MAX) {
        spectrum_gui_input_cell(i, ' ', 0u);
        ++i;
    }
    edit_cursor_show();
}

static void edit_history_nav_reset(void)
{
    input_history_pos = INPUT_HISTORY_NONE;
}

static void edit_set_text(const char *text)
{
    strncpy(local_input, text, LOCAL_INPUT_MAX);
    local_input[LOCAL_INPUT_MAX] = '\0';
    local_input_len = (uint8_t)strlen(local_input);
    local_input_cursor = local_input_len;
    edit_render();
}

static void edit_stop_clear(void)
{
    local_input[0] = '\0';
    local_input_len = 0u;
    local_input_cursor = 0u;
    local_input_mode = 0u;
    edit_history_nav_reset();
    spectrum_gui_set_input("");
}

static void disconnect_to_setup(void)
{
    setup_restart_requested = 1u;
    (void)spectrum_link_send_text(msg_bye);
    clear_disconnected_session_state();
    edit_stop_clear();
    spectrum_gui_set_connected(0u);
}

static void edit_begin_empty(void)
{
    local_input[0] = '\0';
    local_input_len = 0u;
    local_input_cursor = 0u;
    local_input_mode = 1u;
    edit_history_nav_reset();
    edit_render();
}

static uint8_t active_peer_ready(void)
{
    return (uint8_t)(!game_status_active || netchesszx_session_peer_ready());
}

static void edit_history_add(const char *text)
{
    char *slot;

    if (!input_has_text(text)) {
        return;
    }
    if (input_history_count != 0u &&
        strcmp(input_history_slot(input_history_count - 1u), text) == 0) {
        return;
    }

    if (input_history_count < INPUT_HISTORY_SIZE) {
        slot = input_history_slot(input_history_count);
        ++input_history_count;
    } else {
        memmove(input_history_slot(0u),
                input_history_slot(1u),
                (INPUT_HISTORY_SIZE - 1u) * (INPUT_HISTORY_TEXT_MAX + 1u));
        slot = input_history_slot(INPUT_HISTORY_SIZE - 1u);
    }

    strncpy(slot, text, INPUT_HISTORY_TEXT_MAX);
    slot[INPUT_HISTORY_TEXT_MAX] = '\0';
}

static void edit_history_up(void)
{
    uint8_t index;

    if (input_history_count == 0u) {
        return;
    }
    if (input_history_pos == INPUT_HISTORY_NONE) {
        input_history_pos = 0u;
    } else if ((uint8_t)(input_history_pos + 1u) < input_history_count) {
        ++input_history_pos;
    }

    index = (uint8_t)(input_history_count - 1u - input_history_pos);
    edit_set_text(input_history_slot(index));
}

static void edit_history_down(void)
{
    if (input_history_pos == INPUT_HISTORY_NONE) {
        return;
    }
    if (input_history_pos == 0u) {
        input_history_pos = INPUT_HISTORY_NONE;
        edit_set_text("");
        return;
    }

    --input_history_pos;
    edit_set_text(input_history_slot(input_history_count - 1u - input_history_pos));
}

static uint8_t edit_insert_char(uint8_t key)
{
    char c = (char)key;
    uint8_t old_cursor;
    uint8_t old_len;

    if (!is_input_text_char((uint8_t)c)) {
        return 0u;
    }
    if (local_input_len >= LOCAL_INPUT_MAX) {
        return 0u;
    }

    old_cursor = local_input_cursor;
    old_len = local_input_len;
    memmove(local_input + local_input_cursor + 1u,
            local_input + local_input_cursor,
            (uint16_t)(local_input_len - local_input_cursor + 1u));
    local_input[local_input_cursor] = c;
    ++local_input_len;
    ++local_input_cursor;
    edit_history_nav_reset();
    if (old_cursor == old_len) {
        spectrum_gui_input_cell(old_cursor, c, 0u);
        edit_cursor_show();
    } else {
        edit_redraw_from(old_cursor, old_len);
    }
    return 1u;
}

static void edit_backspace(void)
{
    uint8_t old_len;

    if (local_input_cursor == 0u) {
        return;
    }
    old_len = local_input_len;
    memmove(local_input + local_input_cursor - 1u,
            local_input + local_input_cursor,
            (uint16_t)(local_input_len - local_input_cursor + 1u));
    --local_input_cursor;
    --local_input_len;
    edit_history_nav_reset();
    edit_redraw_from(local_input_cursor, old_len);
}

static void edit_cursor_to(uint8_t cursor)
{
    uint8_t old_cursor = local_input_cursor;

    if (old_cursor != cursor) {
        local_input_cursor = cursor;
        spectrum_gui_input_cell(old_cursor, edit_char_at(old_cursor), 0u);
        edit_cursor_show();
    }
}

static void edit_move_left(void)
{
    if (local_input_cursor > 0u) {
        edit_cursor_to((uint8_t)(local_input_cursor - 1u));
    }
}

static void edit_move_right(void)
{
    if (local_input_cursor < local_input_len) {
        edit_cursor_to((uint8_t)(local_input_cursor + 1u));
    }
}

static void edit_move_home(void)
{
    edit_cursor_to(0u);
}

static void edit_move_end(void)
{
    edit_cursor_to(local_input_len);
}

static void square_from_cursor(char *square, uint8_t row, uint8_t col)
{
    square[0] = (char)('a' + col);
    square[1] = (char)('8' - row);
    square[2] = '\0';
}

static uint8_t is_spectrum_piece(char piece)
{
    if (netchesszx_local_is_white()) {
        return (uint8_t)(piece >= 'A' && piece <= 'Z');
    }
    return (uint8_t)(piece >= 'a' && piece <= 'z');
}

static uint8_t cursor_try_square(uint8_t row, uint8_t col)
{
    if (is_spectrum_piece(spectrum_board_cell(row, col))) {
        cursor_row = row;
        cursor_col = col;
        return 1u;
    }
    return 0u;
}

static void cursor_reset_default_square(void)
{
    uint8_t row;
    uint8_t col;

    selected_row = NO_SQUARE;
    selected_col = NO_SQUARE;

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

    cursor_row = 6u;
    cursor_col = 4u;
}

static void movement_hints_clear(void)
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

static void movement_hints_show(void)
{
    if (netchesszx_movement_hints == 0u || selected_row == NO_SQUARE ||
        local_turn == 0u || pending_local_ply != 0u) {
        return;
    }
    if (hints_selected_row == selected_row && hints_selected_col == selected_col) {
        return;
    }
    spectrum_board_show_legal_hints(selected_row, selected_col);
    hints_selected_row = selected_row;
    hints_selected_col = selected_col;
}

static void cursor_redraw_square(uint8_t row, uint8_t col)
{
    spectrum_gui_redraw_square(row, col);
    if (local_turn && selected_row == row && selected_col == col) {
        spectrum_gui_mark_cursor(row, col, 1u);
    }
}

static void cursor_hide(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    if (selected_row != NO_SQUARE) {
        spectrum_gui_redraw_square(selected_row, selected_col);
        if (selected_row != cursor_row || selected_col != cursor_col) {
            spectrum_gui_redraw_square(cursor_row, cursor_col);
        }
    } else {
        spectrum_gui_redraw_square(cursor_row, cursor_col);
    }
    selected_row = NO_SQUARE;
    selected_col = NO_SQUARE;
}

static void cursor_show(void)
{
    if (!active_peer_ready() || !local_turn || pending_local_ply != 0u) {
        return;
    }
    if (selected_row == cursor_row && selected_col == cursor_col) {
        spectrum_gui_mark_cursor(cursor_row, cursor_col, 1u);
        return;
    }
    if (selected_row != NO_SQUARE) {
        spectrum_gui_mark_cursor(selected_row, selected_col, 1u);
    }
    spectrum_gui_mark_cursor(cursor_row, cursor_col, 0u);
}

static void about_restore_game(void)
{
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_restore_board_area();
    if (local_input_mode) {
        edit_render();
    } else if (local_turn) {
        movement_hints_show();
        cursor_show();
    }
    suppress_current_key();
}

static void about_restore_full_board(void)
{
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_draw_board();
    status_redraw_current();
    spectrum_gui_restore_side_panels();
    if (local_input_mode) {
        edit_render();
    }
}

static void about_open(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    if (!spectrum_gui_show_about()) {
        spectrum_gui_set_board_snapshot(spectrum_board_cells());
        spectrum_gui_restore_board_area();
        notify_error(msg_overlay_failed);
        return;
    }
    suppress_current_key();
}

static uint8_t about_process_key(void)
{
    uint8_t key = poll_repeating_key();

    if (key != 0u) {
        about_restore_game();
    }
    return 1u;
}

static void turn_set_notice(uint8_t is_local_turn, uint8_t show_notice)
{
    if (is_local_turn) {
        if (local_turn) {
            cursor_hide();
        } else {
            selected_row = NO_SQUARE;
            selected_col = NO_SQUARE;
        }
        cursor_reset_default_square();
        local_turn = 1u;
        status_refresh_game();
        if (show_notice) {
            notify_info(NETCHESSZX_UI_PHASE_YOUR_TURN);
        }
        cursor_show();
    } else {
        cursor_hide();
        local_turn = 0u;
        status_refresh_game();
        if (show_notice) {
            notify_info(msg_opponent_turn);
        }
    }
}

static void turn_set(uint8_t is_local_turn)
{
    turn_set_notice(is_local_turn, 1u);
}

static void local_controls_reset(uint8_t is_local_turn)
{
    edit_stop_clear();
    suppress_key = 0u;
    edit_net_poll_wait = 0u;
    cursor_row = 6u;
    cursor_col = 4u;
    selected_row = NO_SQUARE;
    selected_col = NO_SQUARE;
    if (is_local_turn) {
        turn_set(1u);
    } else {
        local_turn = 0u;
        if (game_status_active) {
            status_refresh_game();
            notify_info(msg_opponent_turn);
        }
    }
}

static void suppress_key_until_release(uint8_t key)
{
    suppress_key = key;
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
        if (!netchesszx_notation_is_san() && state != SPECTRUM_BOARD_CHECK_NONE) {
            display = move_display_with_check_suffix(move,
                                                      move_check_display,
                                                      state);
        }
    }
    spectrum_gui_add_move(ply_text, display);
    spectrum_gui_apply_move(move);
    pending_local_clear();
    game_check_state = state == SPECTRUM_BOARD_CHECK
        ? SPECTRUM_BOARD_CHECK
        : SPECTRUM_BOARD_CHECK_NONE;
    if (state == SPECTRUM_BOARD_CHECK_MATE) {
        spectrum_gui_mark_last_move_error();
        end_game_over(next_local_turn == 0u ? msg_checkmate_won : msg_checkmate_lost);
        return;
    }
    spectrum_gui_move_timer_reset();
    turn_set_notice(next_local_turn,
                    (uint8_t)(state != SPECTRUM_BOARD_CHECK));
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

static uint8_t tcp_required(uint8_t sent)
{
    if (sent) {
        return 1u;
    }
    handle_opponent_disconnected();
    return 0u;
}

static void end_game_over(const char *message)
{
    edit_stop_clear();
    cursor_hide();
    local_turn = 0u;
    game_status_active = 0u;
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    game_over = 1u;
    pending_local_clear();
    reset_pending = 0u;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_game_timer_stop();
    status_show_endpoint();
    notify_error(message);
}

static uint8_t send_draw_reply(uint8_t accepted)
{
    return tcp_required(accepted ? netchesszx_session_send_ack_move(msg_draw) :
        netchesszx_session_send_nack_move(msg_draw));
}

static uint8_t start_draw_rematch(uint8_t send_reset)
{
    end_game_over(msg_draw);
    reset_pending = 1u;
    if (send_reset && !tcp_required(spectrum_link_send_text(msg_reset_wire))) {
        return 0u;
    }
    return 1u;
}

static uint8_t local_action_ready(void)
{
    if (!game_status_active) {
        notify_error(msg_game_not_started);
        return 0u;
    }
    if (reset_pending) {
        notify_wait_opponent_ack();
        return 0u;
    }
    if (!netchesszx_session_peer_ready()) {
        notify_wait_opponent();
        return 0u;
    }
    return 1u;
}

static void game_start_state(void)
{
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_moves();
    game_ply = 0u;
    pending_local_clear();
    game_status_active = 1u;
    game_check_state = SPECTRUM_BOARD_CHECK_NONE;
    game_over = 0u;
    start_pending = 0u;
    spectrum_gui_clear_control_latches();
    spectrum_gui_game_timer_start();
    spectrum_gui_animate_board_pieces();
    spectrum_gui_set_connected(2u);
    status_refresh_game();
    local_controls_reset(netchesszx_session_has_local_turn(side_to_move_is_white()));
    cursor_show();
}

static uint8_t game_start_local(uint8_t start_key)
{
    if (game_status_active) {
        return 1u;
    }
    if (!netchesszx_session_can_start_game()) {
        notify_wait(NETCHESSZX_UI_NOTICE_OPPONENT_STARTS_GAME);
        return 1u;
    }
    if (!netchesszx_session_peer_ready()) {
        notify_wait_opponent();
        return 1u;
    }
    if (start_pending) {
        notify_wait_opponent_ack();
        return 1u;
    }
    notify_wait(NETCHESSZX_UI_NOTICE_STARTING_GAME);
    if (!netchesszx_session_send_start_game()) {
        notify_error(NETCHESSZX_UI_ERROR_START_FAILED);
        return 0u;
    }
    game_over = 0u;
    start_pending = 1u;
    /* Keep the calm starting-game notice until ACK GAME START flips it to
       "GAME STARTED". Showing the yellow "Waiting opponent ACK" here only
       flashes for one round-trip and reads like an error to the user. */
    suppress_key_until_release(start_key);
    cursor_show();
    return 1u;
}

static uint8_t send_local_chat(const char *text)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];

    if (!input_has_text(text)) {
        return 1u;
    }
    if (reset_pending) {
        notify_wait_opponent_ack();
        return 1u;
    }

    {
        char *p = spectrum_append_text(payload, "CHAT ");
        char *end = payload + sizeof(payload) - 1u;

        while (*text != '\0' && p < end) {
            *p++ = *text++;
        }
        *p = '\0';
        if (*text != '\0') {
            return 0u;
        }
    }
    if (!spectrum_link_send_text(payload)) {
        return 0u;
    }
    spectrum_gui_add_chat(netchesszx_local_side_char(), payload + 5u);
    return 1u;
}

static uint8_t send_local_move(const char *move)
{
    char payload[24];
    char san[SPECTRUM_SAN_TEXT_MAX];
    const char *notice;
    uint16_t ply;

    if (!game_status_active) {
        notify_info(msg_game_not_started);
        return LOCAL_MOVE_REJECTED;
    }
    if (reset_pending) {
        notify_wait_opponent_ack();
        return LOCAL_MOVE_REJECTED;
    }
    if (!active_peer_ready()) {
        notify_wait_opponent();
        return LOCAL_MOVE_REJECTED;
    }

    if (pending_local_ply != 0u) {
        notify_wait_opponent_ack();
        return LOCAL_MOVE_REJECTED;
    }

    if (!spectrum_board_is_legal_move(move)) {
        notify_move_rejected();
        return LOCAL_MOVE_REJECTED;
    }

    ply = (uint16_t)(game_ply + 1u);
    {
        char *p = spectrum_append_text(payload, msg_move_prefix);
        p = spectrum_append_u16(p, ply);
        *p++ = ' ';
        (void)spectrum_append_text(p, move);
    }
    if (!spectrum_link_send_text(payload)) {
        return LOCAL_MOVE_NET_FAIL;
    }

    movement_hints_clear();
    pending_local_ply = ply;
    strncpy(pending_local_move, move, sizeof(pending_local_move) - 1u);
    pending_local_move[sizeof(pending_local_move) - 1u] = '\0';
    notice = move_san_or_fallback(move, "", san);
    notify_wait(notice);
    return LOCAL_MOVE_SENT;
}

static uint8_t retry_pending_outgoing(void)
{
    if (pending_local_ply != 0u) {
        char payload[24];
        char *p = spectrum_append_text(payload, msg_move_prefix);

        p = spectrum_append_u16(p, pending_local_ply);
        *p++ = ' ';
        (void)spectrum_append_text(p, pending_local_move);
        return spectrum_link_send_text(payload);
    }
    if (start_pending) {
        return netchesszx_session_send_start_game();
    }
    return 1u;
}

static void apply_pending_local_move(const char *notation)
{
    char ply_text[8];
    char san[SPECTRUM_SAN_TEXT_MAX];
    const char *display;
    uint8_t san_ready;

    if (pending_local_ply == 0u) {
        return;
    }

    (void)spectrum_append_u16(ply_text, pending_local_ply);
    san_ready = move_san_prepare(pending_local_move, san);
    if (san_ready) {
        display = san;
    } else {
        display = move_display_text(pending_local_move, notation);
    }
    spectrum_gui_prepare_move(pending_local_move);
    if (!spectrum_board_apply_trusted_move(pending_local_move)) {
        pending_local_clear();
        notify_move_rejected();
        cursor_show();
        return;
    }
    spectrum_gui_set_board_snapshot(spectrum_board_cells());

    game_ply = pending_local_ply;
    finish_applied_move(ply_text, pending_local_move, san, san_ready, display, 0u);
}

static void reject_pending_local_move(void)
{
    pending_local_clear();
    notify_move_rejected();
    cursor_show();
}

static void cursor_move(uint8_t key)
{
    uint8_t old_row = cursor_row;
    uint8_t old_col = cursor_col;
    uint8_t flipped = spectrum_gui_is_board_flipped();

    if (!active_peer_ready() || !local_turn || pending_local_ply != 0u) {
        return;
    }

    if (key == KEY_UP || key == 'q') {
        if (flipped) {
            if (cursor_row < 7u) {
                ++cursor_row;
            }
        } else if (cursor_row > 0u) {
            --cursor_row;
        }
    } else if (key == KEY_DOWN || key == 'a') {
        if (flipped) {
            if (cursor_row > 0u) {
                --cursor_row;
            }
        } else if (cursor_row < 7u) {
            ++cursor_row;
        }
    } else if (key == KEY_LEFT || key == 'o') {
        if (flipped) {
            if (cursor_col < 7u) {
                ++cursor_col;
            }
        } else if (cursor_col > 0u) {
            --cursor_col;
        }
    } else if (key == KEY_RIGHT || key == 'p') {
        if (flipped) {
            if (cursor_col > 0u) {
                --cursor_col;
            }
        } else if (cursor_col < 7u) {
            ++cursor_col;
        }
    }

    if (old_row != cursor_row || old_col != cursor_col) {
        cursor_redraw_square(old_row, old_col);
        movement_hints_show();
        if (pending_local_ply == 0u) {
            spectrum_gui_mark_cursor(
                cursor_row,
                cursor_col,
                (uint8_t)(selected_row == cursor_row && selected_col == cursor_col));
        }
    }
}

static uint8_t cursor_select_or_move(uint8_t key)
{
    uint8_t move_rc;
    char square[3];
    char msg[16];
    char move[6];

    if (!game_status_active) {
        if (!game_start_local(key)) {
            return 1u;
        }
        return 1u;
    }
    if (reset_pending) {
        notify_wait_opponent_ack();
        return 1u;
    }
    if (!active_peer_ready()) {
        notify_wait_opponent();
        return 1u;
    }

    if (!local_turn) {
        notify_info(msg_opponent_turn);
        return 1u;
    }
    if (pending_local_ply != 0u) {
        notify_wait_opponent_ack();
        return 1u;
    }

    if (selected_row == NO_SQUARE) {
        if (spectrum_board_cell(cursor_row, cursor_col) == '.') {
            square_from_cursor(square, cursor_row, cursor_col);
            (void)spectrum_append_text(spectrum_append_text(msg, "Empty "), square);
            notify_info(msg);
            return 1u;
        }
        if (!is_spectrum_piece(spectrum_board_cell(cursor_row, cursor_col))) {
            (void)spectrum_append_text(spectrum_append_text(msg, "You play "),
                                       netchesszx_local_side_name());
            notify_info(msg);
            return 1u;
        }
        selected_row = cursor_row;
        selected_col = cursor_col;
        movement_hints_show();
        cursor_show();
        square_from_cursor(square, cursor_row, cursor_col);
        (void)spectrum_append_text(spectrum_append_text(msg, "Sel "), square);
        notify_info(msg);
        return 1u;
    }

    if (selected_row == cursor_row && selected_col == cursor_col) {
        movement_hints_clear();
        selected_row = NO_SQUARE;
        selected_col = NO_SQUARE;
        spectrum_gui_redraw_square(cursor_row, cursor_col);
        cursor_show();
        return 1u;
    }

    move[0] = (char)('a' + selected_col);
    move[1] = (char)('8' - selected_row);
    move[2] = (char)('a' + cursor_col);
    move[3] = (char)('8' - cursor_row);
    move[4] = '\0';
    if ((spectrum_board_cell(selected_row, selected_col) == 'P' && cursor_row == 0u) ||
        (spectrum_board_cell(selected_row, selected_col) == 'p' && cursor_row == 7u)) {
        move[4] = 'q';
        move[5] = '\0';
    }
    move_rc = send_local_move(move);
    if (move_rc == LOCAL_MOVE_NET_FAIL) {
        return 0u;
    }
    if (move_rc == LOCAL_MOVE_REJECTED) {
        cursor_redraw_square(selected_row, selected_col);
        movement_hints_show();
        cursor_show();
        return 1u;
    }
    spectrum_gui_redraw_square(selected_row, selected_col);
    movement_hints_clear();
    selected_row = NO_SQUARE;
    selected_col = NO_SQUARE;
    cursor_show();
    return 1u;
}

static uint8_t input_submit(void)
{
    char move[6];
    uint8_t rc;

    if (!input_has_text(local_input)) {
        edit_stop_clear();
        cursor_show();
        return 1u;
    }

    if (parse_move_input(local_input, move)) {
        if (!game_status_active) {
            if (!game_start_local(0u)) {
                return 1u;
            }
            if (!game_status_active) {
                edit_stop_clear();
                cursor_show();
                return 1u;
            }
        }
        if (!local_turn) {
            notify_error(msg_opponent_turn);
            edit_stop_clear();
            cursor_show();
            return 1u;
        }
        if (pending_local_ply != 0u) {
            notify_wait_opponent_ack();
            edit_stop_clear();
            cursor_show();
            return 1u;
        }
        edit_history_add(local_input);
        rc = send_local_move(move);
        if (rc == LOCAL_MOVE_REJECTED) {
            cursor_show();
        } else {
            edit_stop_clear();
        }
        return (uint8_t)(rc != LOCAL_MOVE_NET_FAIL);
    }

    if (strcmp(local_input, "/resign") == 0) {
        if (!local_action_ready()) {
            return 1u;
        }
        confirm_action = CONFIRM_RESIGN_SEND;
        edit_stop_clear();
        notify_error(msg_resign_confirm);
        return 1u;
    }

    if (strcmp(local_input, "/draw") == 0) {
        if (!local_action_ready()) {
            return 1u;
        }
        confirm_action = CONFIRM_DRAW_SEND;
        edit_stop_clear();
        notify_error(msg_draw_request);
        return 1u;
    }

    if (!netchesszx_session_peer_ready()) {
        notify_wait_opponent();
        return 1u;
    }
    rc = send_local_chat(local_input);
    if (rc) {
        edit_history_add(local_input);
        edit_stop_clear();
        cursor_show();
    }
    return rc;
}

static uint8_t process_local_key(void)
{
    uint8_t key;

    edit_key_handled = 0u;
    if (spectrum_gui_about_visible()) {
        return about_process_key();
    }
    if (confirm_action != CONFIRM_NONE) {
        key = poll_repeating_key();
        if (key == 0u) {
            return 1u;
        }
        if (key == 'y') {
            if (confirm_action == CONFIRM_DISCONNECT) {
                confirm_action = CONFIRM_NONE;
                disconnect_to_setup();
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESET_SEND ||
                confirm_action == CONFIRM_RESTART_GAME) {
                if (!spectrum_link_send_text(msg_reset_wire)) {
                    handle_opponent_disconnected();
                    return 1u;
                }
                reset_pending = 1u;
                confirm_action = CONFIRM_NONE;
                notify_wait_opponent_ack();
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESET_ACCEPT) {
                if (reset_pending == 3u) {
                    if (!send_draw_reply(1u)) {
                        return 1u;
                    }
                    (void)start_draw_rematch(0u);
                } else if (!tcp_required(netchesszx_session_send_ack_reset())) {
                    return 1u;
                } else {
                    if (game_over) {
                        game_start_state();
                        spectrum_gui_notify_success(msg_game_started);
                    } else {
                        restore_game_state();
                    }
                    confirm_action = CONFIRM_NONE;
                    reset_pending = 0u;
                }
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESIGN_SEND) {
                if (!tcp_required(spectrum_link_send_text(msg_resign_wire))) {
                    return 1u;
                }
                confirm_action = CONFIRM_NONE;
                end_game_over(msg_resign_wire);
                suppress_current_key();
            } else if (confirm_action == CONFIRM_DRAW_SEND) {
                if (!tcp_required(spectrum_link_send_text(msg_draw))) {
                    return 1u;
                }
                reset_pending = 2u;
                confirm_action = CONFIRM_NONE;
                notify_wait_opponent_ack();
                suppress_current_key();
            }
        } else if (key == 'n') {
            uint8_t was_restart = (uint8_t)(confirm_action == CONFIRM_RESTART_GAME);

            if (confirm_action == CONFIRM_RESET_ACCEPT) {
                if (reset_pending == 3u) {
                    reset_pending = 0u;
                    if (!send_draw_reply(0u)) {
                        return 1u;
                    }
                } else if (!tcp_required(netchesszx_session_send_nack_reset())) {
                    return 1u;
                } else {
                    if (!game_over) {
                        status_refresh_game();
                    }
                }
            } else if (confirm_action == CONFIRM_RESTART_GAME) {
                disconnect_to_setup();
            }
            confirm_action = CONFIRM_NONE;
            if (!was_restart) {
                notify_info("");
            }
            suppress_current_key();
        }
        return 1u;
    }

    key = filter_repeating_key(spectrum_gui_poll_menu_key());
    if (key == 0u) {
        return 1u;
    }

    if (game_over) {
        spectrum_gui_hide_menu();
        if (reset_pending) {
            notify_wait_opponent_ack();
        } else {
            confirm_action = CONFIRM_RESTART_GAME;
            notify_error(msg_restart_game_confirm);
        }
        suppress_current_key();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_ABOUT) {
        about_open();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_DISCC) {
        spectrum_gui_hide_menu();
        confirm_action = CONFIRM_DISCONNECT;
        notify_error(msg_disconnect_confirm);
        suppress_current_key();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_REST) {
        if (!game_status_active) {
            notify_error(msg_game_not_started);
        } else if (netchesszx_transport_is_mqtt() &&
            !netchesszx_session_peer_ready()) {
            notify_error(NETCHESSZX_UI_ERROR_NO_OPPONENT);
        } else if (reset_pending) {
            notify_error(msg_reset_wait);
        } else {
            spectrum_gui_hide_menu();
            confirm_action = CONFIRM_RESET_SEND;
            notify_error(msg_reset_confirm);
            suppress_current_key();
        }
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_FLIP) {
        spectrum_gui_redraw_square(cursor_row, cursor_col);
        spectrum_gui_toggle_board_view();
        if (!local_input_mode) {
            movement_hints_show();
            cursor_show();
        }
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_THEME) {
        spectrum_gui_clear_cursor_coords();
        netchesszx_board_theme_apply((uint8_t)(netchesszx_board_theme_index + 1u));
        spectrum_gui_redraw_board_squares();
        if (!local_input_mode) {
            cursor_show();
        }
        return 1u;
    }

    if (local_input_mode) {
        if (key == KEY_CANCEL) {
            edit_stop_clear();
            cursor_show();
            goto edit_key_consumed;
        }
        if (key == 13u) {
            return input_submit();
        }
        if (key == 8u) {
            if (local_input_len == 0u) {
                goto edit_key_consumed;
            }
            edit_backspace();
            goto edit_key_consumed;
        }
        if (key == KEY_LEFT) {
            edit_move_left();
            goto edit_key_consumed;
        }
        if (key == KEY_RIGHT) {
            edit_move_right();
            goto edit_key_consumed;
        }
        if (key == KEY_UP) {
            edit_history_up();
            goto edit_key_consumed;
        }
        if (key == KEY_DOWN) {
            edit_history_down();
            goto edit_key_consumed;
        }
        if (key == KEY_HOME) {
            edit_move_home();
            goto edit_key_consumed;
        }
        if (key == KEY_END) {
            edit_move_end();
            goto edit_key_consumed;
        }
        (void)edit_insert_char(key);
edit_key_consumed:
        edit_key_handled = 1u;
        return 1u;
    }

    key = nav_key_alias(key);

    if (key == 13u) {
        if (reset_pending) {
            notify_wait_opponent_ack();
            edit_key_handled = 1u;
            return 1u;
        }
        if (!netchesszx_session_peer_ready()) {
            notify_wait_opponent();
            edit_key_handled = 1u;
            return 1u;
        }
        cursor_hide();
        edit_begin_empty();
        notify_info(NETCHESSZX_UI_NOTICE_TYPE_MOVE_CHAT);
        edit_key_handled = 1u;
        return 1u;
    }

    if (key == 32u) {
        return cursor_select_or_move(key);
    }
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT ||
        key == KEY_RIGHT || key == 'q' || key == 'a' ||
        key == 'o' || key == 'p') {
        cursor_move(key);
    }
    return 1u;
}

static void handle_opponent_disconnected_with(const char *message)
{
    confirm_action = CONFIRM_NONE;
    clear_disconnected_session_state();
    edit_stop_clear();
    reset_board_moves_chat();
    spectrum_gui_hide_board_pieces();
    restore_about_full_board_if_visible();
    spectrum_gui_set_connected(0u);
    spectrum_gui_set_status_error(message);
    notify_error(message);
}

static void handle_opponent_disconnected(void)
{
    handle_opponent_disconnected_with(msg_connection_lost);
}

static void mqtt_peer_reset_wait_state(void)
{
    confirm_action = CONFIRM_NONE;
    if (local_turn) {
        cursor_hide();
    }
    netchesszx_session_peer_reset();
    start_pending = 0u;
    reset_pending = 0u;
    game_over = 0u;
    if (game_status_active) {
        game_status_active = 0u;
        game_ply = 0u;
        pending_local_clear();
        spectrum_gui_game_timer_stop();
        reset_board_moves_chat();
        spectrum_gui_hide_board_pieces();
    }
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
    uint8_t retained;
    uint8_t host_flags;
    uint8_t bad_color;
    uint8_t poll_status;
    uint8_t mqtt_setup_reannounce_wait = 0u;
    uint8_t direct_hello_reannounce_wait = 0u;
    uint8_t pending_retry_wait = PENDING_RETRY_TICKS;
    netchesszx_session_event_t event;
    netchesszx_session_poll_result_t poll;

    spectrum_gui_restore_side_panels();
    reset_board_moves_chat();
    game_ply = 0u;
    clear_disconnected_session_state();
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    spectrum_gui_set_board_pieces_visible(0u);
    spectrum_gui_set_connected(netchesszx_transport_is_mqtt() ? 1u : 2u);
    status_show_endpoint();
    /* Peer is not confirmed at connect time: show "waiting" until the direct
       guest HELLO arrives (host) or the host HELLO arrives (guest). The
       "press SPACE" prompt is raised later, once peer ready is set. */
    notify_wait(msg_waiting_opponent);
    local_controls_reset(0u);
    netchesszx_session_ping_reset(&ping);
    if (!tcp_required(netchesszx_session_direct_send_hello())) {
        return;
    }

    while (1) {
        if (!process_local_key()) {
            handle_opponent_disconnected();
            return;
        }
        if (setup_restart_requested) {
            return;
        }
        if (edit_key_handled) {
            edit_net_poll_wait = EDIT_NET_POLL_PERIOD;
        } else if (local_input_mode && edit_net_poll_wait != 0u) {
            --edit_net_poll_wait;
            spectrum_frame_wait();
            spectrum_link_background_drain();
            spectrum_gui_tick();
            continue;
        }
        if (local_input_mode) {
            edit_net_poll_wait = EDIT_NET_POLL_PERIOD;
        }

        poll_status = netchesszx_session_poll(&ping,
                                               payload,
                                               SPECTRUM_LINK_PAYLOAD_MAX,
                                               &poll);
        if (poll_status == NETCHESSZX_SESSION_POLL_DISCONNECTED) {
            handle_opponent_disconnected();
            return;
        }
        if (poll_status != NETCHESSZX_SESSION_POLL_EVENT) {
            if (pending_local_ply != 0u || start_pending) {
                if (pending_retry_wait != 0u) {
                    --pending_retry_wait;
                }
                if (pending_retry_wait == 0u) {
                    if (!tcp_required(retry_pending_outgoing())) {
                        return;
                    }
                    pending_retry_wait = PENDING_RETRY_TICKS;
                }
            } else if (reset_pending == 1u) {
                if (pending_retry_wait != 0u) {
                    --pending_retry_wait;
                }
                if (pending_retry_wait == 0u) {
                    handle_opponent_disconnected();
                    return;
                }
            } else {
                pending_retry_wait = PENDING_RETRY_TICKS;
            }
            if (netchesszx_transport_is_mqtt() &&
                netchesszx_session_is_host() &&
                !game_status_active &&
                !netchesszx_session_peer_ready()) {
                if (mqtt_setup_reannounce_wait != 0u) {
                    --mqtt_setup_reannounce_wait;
                }
                if (mqtt_setup_reannounce_wait == 0u) {
                    if (!spectrum_link_mqtt_publish_setup(0u)) {
                        handle_opponent_disconnected();
                        return;
                    }
                    mqtt_setup_reannounce_wait = MQTT_SETUP_REANNOUNCE_TICKS;
                }
            } else {
                mqtt_setup_reannounce_wait = 0u;
            }
            /* Direct: keep re-announcing our HELLO until the peer HELLO is
               received. Both roles must drive this retry on a timer (i.e. only
               while the link is quiet) so a single missed HELLO does not strand
               peer ready forever. Recovery lives here, not in reply-on-receive,
               which would otherwise ping-pong forever. */
            if (!netchesszx_transport_is_mqtt() &&
                !game_status_active &&
                !netchesszx_session_peer_ready()) {
                if (direct_hello_reannounce_wait != 0u) {
                    --direct_hello_reannounce_wait;
                }
                if (direct_hello_reannounce_wait == 0u) {
                    if (!tcp_required(netchesszx_session_direct_send_hello())) {
                        return;
                    }
                    direct_hello_reannounce_wait = DIRECT_HELLO_REANNOUNCE_TICKS;
                }
            } else {
                direct_hello_reannounce_wait = 0u;
            }
            continue;
        }
        event = poll.event;
        retained = poll.retained;

        if (event == NETCHESSZX_SESSION_EVENT_DIRECT_HELLO) {
            uint8_t was_ready;

            if (game_status_active || start_pending) {
                continue;
            }
            if (!netchesszx_session_direct_apply_hello(payload)) {
                notify_error(NETCHESSZX_UI_ERROR_BAD_DIRECT_HELLO);
                (void)spectrum_link_send_text(msg_bye);
                handle_opponent_disconnected();
                return;
            }
            was_ready = netchesszx_session_peer_ready();
            netchesszx_session_peer_mark_ready();
            /* Only act on the not-ready -> ready transition. Replying or
               re-notifying on every peer HELLO would ping-pong forever and keep
               wiping transient notices (e.g. the SPACE start hint). Loss
               recovery is the re-announce timer above. */
            if (was_ready) {
                continue;
            }
            if (!netchesszx_session_is_host()) {
                /* Guest: reply once so a host that missed our earlier announce
                   still completes its handshake. */
                if (!tcp_required(netchesszx_session_direct_send_hello())) {
                    return;
                }
                spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
                notify_wait(msg_opponent_ready_wait);
            } else {
                notify_wait(msg_opponent_ready_go);
            }
            continue;
        }
        if (!netchesszx_transport_is_mqtt() &&
            !netchesszx_session_peer_ready()) {
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_ACK &&
            netchess_after_prefix(payload + 4u, msg_game_start_wire) != 0) {
            if (start_pending) {
                game_start_state();
                spectrum_gui_notify_success(msg_game_started);
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_ACK &&
            netchess_after_prefix(payload + 4u, msg_reset_wire) != 0) {
            if (reset_pending == 1u) {
                reset_pending = 0u;
                if (game_over) {
                    game_start_state();
                    spectrum_gui_notify_success(msg_game_started);
                } else {
                    restore_game_state();
                    spectrum_gui_notify_success(msg_reset_confirmed);
                }
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_ACK &&
            netchess_after_prefix(payload + 4u, msg_draw) != 0) {
            if (reset_pending == 2u) {
                if (!start_draw_rematch(1u)) {
                    return;
                }
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_NACK &&
            netchess_after_prefix(payload + 5u, msg_game_start_wire) != 0) {
            if (start_pending) {
                start_pending = 0u;
                notify_error(NETCHESSZX_UI_ERROR_START_REJECTED);
                status_show_endpoint();
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_NACK &&
            netchess_after_prefix(payload + 5u, msg_reset_wire) != 0) {
            if (reset_pending == 1u) {
                reset_pending = 0u;
                notify_error(msg_reset_rejected);
                if (!game_over) {
                    status_refresh_game();
                }
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_NACK &&
            netchess_after_prefix(payload + 5u, msg_draw) != 0) {
            if (reset_pending == 2u) {
                reset_pending = 0u;
                notify_error(msg_draw_rejected);
                status_refresh_game();
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_ACK &&
            netchess_proto_parse_ack(payload,
                                     ply,
                                     sizeof(ply),
                                     notation,
                                     sizeof(notation))) {
            if (pending_local_ply != 0u &&
                parse_u16(ply) == pending_local_ply) {
                apply_pending_local_move(notation);
            }
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_ACK) {
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_NACK &&
            netchess_proto_parse_nack(payload,
                                      ply,
                                      sizeof(ply),
                                      notation,
                                      0u)) {
            if (pending_local_ply != 0u &&
                parse_u16(ply) == pending_local_ply) {
                reject_pending_local_move();
            }
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_NACK) {
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_MQTT_EMPTY) {
            if (netchesszx_session_is_host() && !game_status_active &&
                !netchesszx_session_peer_ready()) {
                if (!spectrum_link_mqtt_publish_setup(1u)) {
                    handle_opponent_disconnected();
                    return;
                }
                status_show_endpoint();
                notify_wait_opponent();
            }
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE) {
            if (game_status_active || start_pending ||
                netchesszx_session_peer_ready()) {
                mqtt_peer_disconnected_wait();
            }
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST) {
            if (game_status_active || start_pending ||
                netchesszx_session_peer_ready()) {
                continue;
            }
            if (local_turn) {
                cursor_hide();
            }
            netchesszx_session_peer_reset();
            start_pending = 0u;
            spectrum_gui_set_connected(1u);
            status_show_endpoint();
            notify_error(msg_room_conflict);
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_MQTT_HOST) {
            host_flags = netchesszx_session_mqtt_host_flags(payload,
                                                            game_status_active,
                                                            retained,
                                                            &bad_color);
            if (bad_color) {
                notify_error(NETCHESSZX_UI_ERROR_BAD_COLOR);
                continue;
            }
            if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_COLOR_CHANGED) {
                spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
                spectrum_gui_redraw_board_view();
            }
            if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE) &&
                !spectrum_link_mqtt_activate_side()) {
                handle_opponent_disconnected();
                return;
            }
            if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT) {
                spectrum_gui_set_connected(1u);
                status_show_endpoint();
                notify_wait(msg_waiting_opponent);
                continue;
            }
            if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_PUBLISH_SETUP) &&
                !spectrum_link_mqtt_publish_setup(0u)) {
                handle_opponent_disconnected();
                return;
            }
            if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT) {
                spectrum_gui_set_connected(2u);
                status_show_endpoint();
                notify_wait(msg_opponent_ready_wait);
                cursor_show();
            }
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY) {
            if (game_status_active) {
                if (!tcp_required(spectrum_link_send_text(msg_bye))) {
                    return;
                }
                notify_error(NETCHESSZX_UI_ERROR_GAME_ALREADY_ACTIVE);
                continue;
            }
            if (netchesszx_session_peer_ready()) {
                continue;
            }
            netchesszx_session_peer_mark_ready();
            if (!spectrum_link_mqtt_publish_setup(0u)) {
                handle_opponent_disconnected();
                return;
            }
            spectrum_gui_set_connected(2u);
            status_show_endpoint();
            notify_wait(msg_opponent_ready_go);
            cursor_show();
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_BYE) {
            if (netchesszx_transport_is_mqtt()) {
                if (game_status_active || start_pending ||
                    netchesszx_session_peer_ready()) {
                    mqtt_peer_disconnected_wait();
                }
                continue;
            }
            handle_opponent_disconnected();
            return;
        }

        if (event == NETCHESSZX_SESSION_EVENT_RESET) {
            if (game_over) {
                if (reset_pending) {
                    if (!tcp_required(netchesszx_session_send_ack_reset())) {
                        return;
                    }
                    reset_pending = 0u;
                    confirm_action = CONFIRM_NONE;
                    game_start_state();
                    spectrum_gui_notify_success(msg_game_started);
                } else if (confirm_action != CONFIRM_NONE) {
                    if (!tcp_required(netchesszx_session_send_nack_reset())) {
                        return;
                    }
                } else {
                    spectrum_gui_hide_menu();
                    confirm_action = CONFIRM_RESET_ACCEPT;
                    notify_error(msg_restart_request);
                }
            } else if (game_status_active && reset_pending == 1u) {
                if (!tcp_required(netchesszx_session_send_ack_reset())) {
                    return;
                }
                reset_pending = 0u;
                confirm_action = CONFIRM_NONE;
                restore_game_state();
                spectrum_gui_notify_success(msg_reset_confirmed);
            } else if (!game_status_active || reset_pending ||
                confirm_action != CONFIRM_NONE) {
                if (!tcp_required(netchesszx_session_send_nack_reset())) {
                    return;
                }
            } else {
                spectrum_gui_hide_menu();
                confirm_action = CONFIRM_RESET_ACCEPT;
                notify_error(msg_reset_request);
            }
            continue;
        }

        if (strcmp(payload, msg_resign_wire) == 0) {
            if (game_status_active) {
                end_game_over(msg_opponent_resign);
            }
            continue;
        }

        if (strcmp(payload, msg_draw) == 0) {
            if (reset_pending == 2u) {
                if (!send_draw_reply(1u)) {
                    return;
                }
                if (!start_draw_rematch(1u)) {
                    return;
                }
            } else if (!game_status_active || reset_pending || confirm_action != CONFIRM_NONE) {
                if (!send_draw_reply(0u)) {
                    return;
                }
            } else {
                reset_pending = 3u;
                confirm_action = CONFIRM_RESET_ACCEPT;
                notify_error(msg_opponent_draw_request);
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_GAME_START) {
            if (netchesszx_session_is_host()) {
                continue;
            }
            if (netchesszx_transport_is_mqtt()) {
                if (!netchesszx_session_mqtt_can_accept_game_start()) {
                    notify_wait_opponent();
                    continue;
                }
            } else if (!netchesszx_session_peer_ready()) {
                notify_wait_opponent();
                continue;
            }
            if (!netchesszx_session_direct_apply_start_side(payload)) {
                notify_error(NETCHESSZX_UI_ERROR_BAD_START);
                if (!tcp_required(spectrum_link_send_text("NACK GAME START BAD"))) {
                    return;
                }
                continue;
            }
            netchesszx_session_peer_mark_ready();
            if (!game_status_active) {
                game_start_state();
                spectrum_gui_notify_success(msg_game_started);
            }
            if (!tcp_required(netchesszx_session_send_ack_game_start())) {
                return;
            }
            continue;
        }

        if (event == NETCHESSZX_SESSION_EVENT_MOVE &&
            netchess_proto_parse_move(payload,
                                      ply,
                                      sizeof(ply),
                                      move,
                                      sizeof(move),
                                      notation,
                                      sizeof(notation))) {
            uint16_t incoming_ply = parse_u16(ply);

            if (game_over) {
                if (incoming_ply != 0u && incoming_ply <= game_ply) {
                    if (!tcp_required(netchesszx_session_send_ack_move(ply))) {
                        return;
                    }
                } else if (!tcp_required(netchesszx_session_send_nack_move(ply))) {
                    return;
                }
                continue;
            }
            if (!game_status_active) {
                notify_error(msg_game_not_started);
                goto nack_move;
            }
            if (incoming_ply == 0u) {
                notify_move_rejected();
                goto nack_move;
            }
            if (reset_pending) {
                notify_wait_opponent_ack();
                goto nack_move;
            }
            if (incoming_ply <= game_ply) {
                if (!tcp_required(netchesszx_session_send_ack_move(ply))) {
                    return;
                }
                continue;
            }
            if (pending_local_ply != 0u) {
                if (incoming_ply == pending_local_ply &&
                    strcmp(move, pending_local_move) == 0) {
                    continue;
                }
                if (incoming_ply == (uint16_t)(pending_local_ply + 1u)) {
                    apply_pending_local_move(0);
                }
                if (pending_local_ply != 0u) {
                    notify_wait_opponent_ack();
                    goto nack_move;
                }
            }
            if (local_turn) {
                notify_error(msg_opponent_turn);
                goto nack_move;
            }
            if (incoming_ply != (uint16_t)(game_ply + 1u)) {
                notify_move_rejected();
                goto nack_move;
            }
            if (!spectrum_board_is_legal_move(move)) {
                notify_move_rejected();
                goto nack_move;
            }
            {
                char san[SPECTRUM_SAN_TEXT_MAX];
                const char *display;
                uint8_t san_ready;

                san_ready = move_san_prepare(move, san);
                if (san_ready) {
                    display = san;
                } else {
                    display = move_display_text(move, notation);
                }
                spectrum_gui_prepare_move(move);
                if (spectrum_board_apply_trusted_move(move)) {
                    spectrum_gui_set_board_snapshot(spectrum_board_cells());
                    game_ply = incoming_ply;
                    finish_applied_move(ply, move, san, san_ready, display, 1u);
                    if (!tcp_required(netchesszx_session_send_ack_move(ply))) {
                        return;
                    }
                } else {
                    notify_move_rejected();
                    goto nack_move;
                }
            }
            continue;

nack_move:
            if (!tcp_required(netchesszx_session_send_nack_move(ply))) {
                return;
            }
            continue;
        } else if (event == NETCHESSZX_SESSION_EVENT_CHAT &&
                   netchess_proto_parse_chat(payload,
                                             payload,
                                             SPECTRUM_LINK_PAYLOAD_MAX)) {
            spectrum_gui_add_chat(netchesszx_remote_side_char(), payload);
        } else if (event == NETCHESSZX_SESSION_EVENT_MQTT_TEXT) {
            notify_info(payload);
        }
    }
}

int main(void)
{
    if (!spectrum_assets_load()) {
        spectrum_assets_fatal();
    }

connection_setup:
    confirm_action = CONFIRM_NONE;
    reset_pending = 0u;
    spectrum_gui_set_board_view(0u);
    spectrum_gui_set_board_pieces_visible(0u);
    spectrum_board_clear();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_logs();
    spectrum_gui_set_connected(0u);
    notify_info("");
    if (setup_restart_requested) {
        setup_restart_requested = 0u;
        spectrum_gui_restore_board_area();
        status_show_connection_setup();
    } else {
        spectrum_gui_draw_board();
        status_show_connection_setup();
        while (!connection_preflight_run()) {
            retry_after_error(preflight_retry_msg);
        }
    }
    session_setup_run();
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    spectrum_gui_set_board_pieces_visible(0u);
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_restore_side_panels();
    spectrum_gui_set_connected(1u);
    status_show_endpoint();

    if (netchesszx_transport_is_mqtt()) {
        while (1) {
            status_show_connecting();
            notify_wait(msg_connecting);
            if (spectrum_link_mqtt_start()) {
                status_show_endpoint();
                notify_info(msg_connected);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
            } else {
                retry_after_error(msg_connect_failed);
            }
        }
    } else if (netchesszx_session_is_host()) {
        status_show_connecting();
        notify_wait(msg_connecting);
        if (spectrum_link_listen()) {
            status_show_endpoint();
            notify_info(msg_connected);
            while (1) {
                uint8_t wait_rc;

                status_show_endpoint();
                notify_wait(msg_waiting_opponent);
                wait_rc = spectrum_link_wait_pc_connect();
                if (wait_rc == SPECTRUM_LINK_CANCELLED) {
                    suppress_key = KEY_CANCEL;
                    setup_restart_requested = 1u;
                    goto connection_setup;
                }
                if (wait_rc) {
                    game_message_loop();
                    if (setup_restart_requested) {
                        goto connection_setup;
                    }
                    wait_after_notice();
                }
            }
        } else {
            notify_error(msg_connect_failed);
        }
    } else {
        while (1) {
            uint8_t connect_rc;

            status_show_connecting();
            notify_wait(msg_connecting);
            connect_rc = spectrum_link_connect_host();
            if (connect_rc == SPECTRUM_LINK_CANCELLED) {
                suppress_key = KEY_CANCEL;
                setup_restart_requested = 1u;
                goto connection_setup;
            }
            if (connect_rc) {
                status_show_endpoint();
                notify_info(msg_connected);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
                wait_after_notice();
            } else {
                retry_after_error(msg_connect_failed);
            }
        }
    }
    while (1) {
        spectrum_frame_wait();
        spectrum_gui_tick();
    }
}
