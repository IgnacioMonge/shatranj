#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_SPECTRANEXT 1
#define __z88dk_fastcall
#define __z88dk_callee
#define __at(address)

#include "spectranext_api_fixture.h"

#define SCRIPT_MAX 16u

#include "../../src/spectrum/overlay/overlay_context.h"
#undef spectrum_overlay_context
static volatile uint8_t host_overlay_context[SPECTRUM_OVERLAY_CONTEXT_SIZE];
#define spectrum_overlay_context host_overlay_context

#include "../../src/spectrum/transport/net.c"
#include "../../src/spectrum/overlay/direct_ovl.c"
#include "../../src/spectrum/overlay/mqtt_connect_ovl.c"
#include "../../src/spectrum/overlay/mqtt_tx_ovl.c"
#include "../../src/spectrum/overlay/time_ovl.c"

void spectrum_mqtt_broker_keepalive_reset(
    spectrum_mqtt_broker_keepalive_t *keepalive)
{
    keepalive->idle_ticks = 0u;
    keepalive->misses = 0u;
}

uint8_t netchesszx_session_role;
uint8_t netchesszx_transport;
uint8_t netchesszx_local_color;
uint8_t netchesszx_host_color;
uint8_t netchesszx_host_color_ready;
uint8_t netchesszx_notation;
uint8_t netchesszx_movement_hints;
uint8_t netchesszx_board_theme_index;
uint8_t netchesszx_piece_set_index;
uint8_t netchesszx_board_light_attr;
uint8_t netchesszx_board_dark_attr;
uint8_t netchesszx_hinted_rows[8];
uint16_t netchesszx_mqtt_session_id;
const char netchesszx_mqtt_host[] = "pool.ntp.org";
char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u] = "NC0000";
const uint16_t netchesszx_mqtt_port = 1883u;
char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u] = "127.0.0.1";
uint16_t netchesszx_direct_port = 5000u;
uint8_t netchesszx_link_match;
char netchesszx_player_name_line[NETCHESS_PROTO_NAME_LINE_SIZE];
int8_t netchesszx_timezone = 0;
int8_t netchesszx_timezone_last = 0;
uint8_t netchesszx_rtc_available;

static int failures;
static uint16_t wait_calls;
static uint8_t close_calls;
static uint8_t listen_calls;
static int16_t listen_result;
static int16_t connect_result;
static uint8_t accept_calls;
volatile uint8_t spectrum_input_pending_event;
static uint16_t key_polls;
static uint16_t key_cancel_at;
static uint8_t send_all_calls;
static const char *send_all_payload = "HELLO";
static uint8_t sends_at_close;
static uint16_t poll_calls;
static uint8_t recv_calls;
static uint8_t detect_calls;
static uint8_t status_calls;
static uint8_t probe_order;
static uint8_t preflight_exec_calls;
uint8_t netchesszx_host_nextreg_peripheral_1;
static int16_t poll_script[SCRIPT_MAX];
static uint8_t poll_script_len;
static uint8_t poll_script_pos;
static int16_t recv_script[SCRIPT_MAX];
static uint8_t recv_script_len;
static uint8_t recv_script_pos;
static const uint8_t *recv_data;
static uint16_t recv_data_len;
static uint16_t recv_data_pos;
static int16_t send_all_script[SCRIPT_MAX];
static uint8_t send_all_script_len;
static uint8_t send_all_script_pos;
static uint8_t clock_calls;
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static uint16_t fat_date;
static uint16_t fat_time;
static uint8_t udp_open_calls;
static uint8_t udp_send_calls;
static uint8_t udp_poll_calls;
static uint8_t udp_close_calls;
static uint8_t udp_ready;
/* MATCH host mocks: a call log ('i' name read, 'c' close, 'o' open,
   'M' MATCH start/poll, 'N' SET_NAME, 'X' MATCH cancel, 'L' MATCH leave)
   and the frame (wait_calls) of every mailbox send and status read. */
#define MATCH_SENDS_MAX 6u
static char match_log[24];
static uint8_t match_log_len;
static int16_t match_open_result;
static int16_t match_socket_index = 2;
static uint8_t match_menu_trace;
static uint8_t match_open_host_ok;
static const char *match_open_host;
static const uint8_t *match_open_ip;
static uint16_t match_open_frame;
static int16_t match_send_result;
static uint16_t match_status_calls;
static int16_t match_info_result;
static uint8_t match_info_id;
static char *match_info_out;
static uint8_t match_info_cap;
/* Guest mocks: texts for id 0 (serial) and id 5 (initiator), 0 = CARTNAME;
   when the result is an error, only match_info_only_id fails (0xff = all). */
static const char *match_info_serial;
static const char *match_info_initiator;
static uint8_t match_info_only_id;
/* Host mock: the cartridge's matchmaking name (id 2), 0 = CARTNAME. */
static const char *match_info_name;
static uint8_t match_send_count;
static uint8_t match_send_socket[MATCH_SENDS_MAX];
static uint8_t match_send_command[MATCH_SENDS_MAX];
static uint8_t match_send_action[MATCH_SENDS_MAX];
static uint8_t match_send_null[MATCH_SENDS_MAX];
static char match_send_text[MATCH_SENDS_MAX][SPXMATCH_NAME_MAX + 1u];
static uint16_t match_send_frame[MATCH_SENDS_MAX];
static int16_t match_status_script[SCRIPT_MAX];
static uint8_t match_status_len;
static uint8_t match_status_pos;
static uint16_t match_status_frame[SCRIPT_MAX];
static int16_t picker_result = 3;
static uint8_t picker_mode, picker_command, picker_index, picker_connects;
static uint16_t picker_revision, picker_frames, picker_clears, picker_rows;
static uint8_t picker_cursor[9];
static char picker_text[9][28];
static uint16_t picker_key_frame;
static uint16_t picker_move_frame;
static uint8_t picker_move_count;
static uint8_t picker_raced, picker_suppressed_key;
static uint8_t picker_connecting, picker_notifications, picker_notice_clears;
static const int16_t *join_replies;
static uint8_t join_reply_count, join_reply_index;
static uint16_t join_cancel_frame;
static int16_t join_select_result;

static void check(int condition, const char *label)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", label);
    }
}

static void match_log_add(char c)
{
    if (match_log_len < sizeof(match_log) - 1u) {
        match_log[match_log_len++] = c;
        match_log[match_log_len] = '\0';
    }
}

static void picker_tick(void)
{
    if (picker_mode == 19 && wait_calls == 2)
        spectrum_input_pending_event = 0x82;
    if (picker_mode == 14 && wait_calls == 52)
        spectrum_input_pending_event = 0x82;
    if (join_cancel_frame && wait_calls == join_cancel_frame)
        spectrum_input_pending_event = 0x8a;
    if (((picker_mode == 10 || picker_mode == 18) && wait_calls == 52) ||
        (picker_mode == 11 && wait_calls == 2))
        spectrum_input_pending_event = picker_mode == 11 ? 32 : 13;
    if (picker_mode == 4 && wait_calls == 120)
        spectrum_input_pending_event = 0x8a;
}

void spectrum_net_runtime_wait_frame_plain(void)
{
    ++wait_calls;
    picker_tick();
}

void spectrum_net_runtime_wait_frame(void)
{
    ++wait_calls;
    picker_tick();
}

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute,
                                    uint8_t second)
{
    ++clock_calls;
    clock_hour = hour;
    clock_minute = minute;
    clock_second = second;
}

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    fat_date = date;
    fat_time = time;
}

uint8_t spectrum_key_poll(void)
{
    ++key_polls;
    if (!picker_mode && picker_result == 3 &&
        picker_command == SPXMATCH_CMD_GET_PEER) return 13;
    if (spectrum_input_pending_event) {
        uint8_t key = spectrum_input_pending_event;
        spectrum_input_pending_event = 0;
        return key;
    }
    if (picker_mode && picker_key_frame != wait_calls) {
        picker_key_frame = wait_calls;
        if (picker_mode == 13 && wait_calls >= 1 && wait_calls <= 3)
            return wait_calls == 2 ? 0x81 : 0x82;
        if (picker_mode == 2) {
            if (wait_calls == 110) {
                check(!picker_cursor[0] && !picker_cursor[1],
                      "removing the selected device clears the cursor");
            }
            if (wait_calls == 1 || wait_calls == 112) return 0x82;
            if (wait_calls == 110 || wait_calls == 114 || wait_calls == 116) return 13;
        }
        if (((picker_mode == 7 || picker_mode == 9) && wait_calls == 60) ||
            (picker_mode == 8 && wait_calls == 110) ||
            (picker_mode == 9 && wait_calls == 70)) {
            check(picker_cursor[0], "first arriving host is selected without navigation");
            return picker_mode == 8 ? 32 : 13;
        }
        if (wait_calls >= (picker_mode == 15 ? 400 : (picker_mode == 2 ? 220 : 120))) return 0x8a;
    }
    return (key_cancel_at != 0u && key_polls == key_cancel_at) ? 0x8au : 0u;
}

char *spectrum_append_text(char *dst, const char *src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

char *spectrum_append_u16(char *dst, uint16_t value)
{
    char digits[6];
    uint8_t count = 0u;

    do {
        digits[count++] = (char)('0' + (value % 10u));
        value = (uint16_t)(value / 10u);
    } while (value != 0u);
    while (count != 0u) {
        *dst++ = digits[--count];
    }
    *dst = '\0';
    return dst;
}

int16_t spxn_poll(void)
{
    ++poll_calls;
    if (poll_script_pos >= poll_script_len) {
        return 0;
    }
    return poll_script[poll_script_pos++];
}

int16_t spxn_detect(void)
{
    ++detect_calls;
    probe_order = 1u;
    return 1;
}

int16_t spxn_status(struct spxn_status *out)
{
    ++status_calls;
    if (spxn_detect() != 1) {
        return -1;
    }
    check(probe_order == 1u, "Spectranext detects before status");
    probe_order = 2u;
    out->ip4host[0] = 192u;
    out->ip4host[1] = 168u;
    out->ip4host[2] = 1u;
    out->ip4host[3] = 20u;
    return SPXN_OK;
}

uint8_t spectrum_overlay_exec_cached(uint8_t overlay_id, uint8_t entry_id)
{
    if (overlay_id == SPECTRUM_OVL_NET_CONNECT &&
        entry_id == SPECTRUM_OVL_NET_PREFLIGHT) {
        ++preflight_exec_calls;
        return net_preflight_ovl();
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_READ) {
        return direct_read_payload_ovl((uint8_t *)spectrum_overlay_context);
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_SEND) {
        return direct_send_text_ovl((uint8_t *)spectrum_overlay_context);
    }
    if (overlay_id == SPECTRUM_OVL_TIME) {
        if (entry_id == SPECTRUM_OVL_TIME_CLOCK_RETRY_START) {
            return spectranext_time_retry_start_ovl();
        }
        if (entry_id == SPECTRUM_OVL_TIME_CLOCK_RETRY_POLL) {
            return spectranext_time_retry_poll_ovl();
        }
        if (entry_id == SPECTRUM_OVL_TIME_CLOCK_RETRY_CANCEL) {
            return spectranext_time_retry_cancel_ovl();
        }
    }
    check(0, "unexpected cached overlay entry");
    return 0u;
}

uint8_t spectrum_overlay_exec(uint8_t overlay_id, uint8_t entry_id)
{
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_CLOSE) {
        return direct_close_wait_ovl((uint8_t *)spectrum_overlay_context);
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_LISTEN) {
        return direct_listen_ovl();
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_CONNECT) {
        return direct_connect_ovl((uint8_t *)spectrum_overlay_context);
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_WAIT_CONNECT) {
        return direct_wait_pc_connect_ovl();
    }
    check(0, "unexpected overlay entry");
    return 0u;
}

int16_t spxn_recv(void *buffer, uint16_t maximum)
{
    uint16_t remaining;
    uint16_t count;
    int16_t scripted;

    ++recv_calls;
    if (recv_script_pos < recv_script_len) {
        scripted = recv_script[recv_script_pos++];
        if (scripted < 0) {
            return scripted;
        }
        count = (uint16_t)scripted;
    } else {
        count = maximum;
    }
    remaining = recv_data_pos < recv_data_len
        ? (uint16_t)(recv_data_len - recv_data_pos) : 0u;
    if (count > maximum) {
        count = maximum;
    }
    if (count > remaining) {
        count = remaining;
    }
    if (count != 0u) {
        memcpy(buffer, recv_data + recv_data_pos, count);
    }
    recv_data_pos = (uint16_t)(recv_data_pos + count);
    return (int16_t)count;
}

int16_t spxn_accept_single(void)
{
    ++accept_calls;
    return SPXN_OK;
}

int16_t spxn_listen(uint16_t port)
{
    (void)port;
    ++listen_calls;
    return listen_result;
}

/* DIRECT CONNECT context for direct calls: zero checks the pairing. */
static uint8_t connect_ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE];

static const char *resolved_host;
static uint8_t connect_calls;

int16_t spxn_resolve(const char *host, uint8_t *ip4be)
{
    if (match_menu_trace) {
        match_log_add('D');
    }
    resolved_host = host;
    ip4be[0] = 127u;
    ip4be[1] = 0u;
    ip4be[2] = 0u;
    ip4be[3] = 1u;
    return SPXN_OK;
}

static const uint8_t *connected_ip4be;

int16_t spxn_connect(const uint8_t *ip4be, uint16_t port)
{
    (void)port;
    connected_ip4be = ip4be;
    ++connect_calls;
    if (match_menu_trace) {
        match_log_add('C');
    }
    return connect_result;
}

void spxn_close(void)
{
    ++close_calls;
    sends_at_close = send_all_calls;
    match_log_add('c');
    match_socket_index = -1;
}

int16_t spxmatch_open(const char *service_host, uint8_t *ip4be)
{
    match_log_add('o');
    match_open_host = service_host;
    match_open_host_ok = strcmp(service_host, NETCHESSZX_MATCH_HOST) == 0;
    match_open_ip = ip4be;
    match_open_frame = wait_calls;
    match_socket_index = match_open_result;
    return match_open_result;
}

int16_t spxmatch_socket_index(void)
{
    return match_socket_index;
}

int16_t spxmatch_send(uint8_t socket, uint8_t command, uint8_t action,
                      const char *text)
{
    uint8_t n = match_send_count;
    picker_command = command;

    match_log_add(command == SPXMATCH_CMD_SET_NAME
                      ? 'N'
                      : (action == SPXMATCH_CANCEL
                             ? 'X'
                             : (action == SPXMATCH_LEAVE ? 'L' : 'M')));
    if (n < MATCH_SENDS_MAX) {
        match_send_socket[n] = socket;
        match_send_command[n] = command;
        match_send_action[n] = action;
        match_send_null[n] = text == 0;
        match_send_text[n][0] = '\0';
        if (text != 0) {
            strncat(match_send_text[n], text, SPXMATCH_NAME_MAX);
        }
        match_send_frame[n] = wait_calls;
        ++match_send_count;
    }
    return match_send_result;
}

int16_t spxmatch_status(void)
{
    if (join_replies && match_phase == MATCH_PHASE_POLL)
        return spxmatch_read(&match_snapshot);
    ++match_status_calls;
    if (match_status_pos >= match_status_len) {
        return SPXMATCH_BUSY;
    }
    match_status_frame[match_status_pos] = wait_calls;
    return match_status_script[match_status_pos++];
}

void spectrum_gui_notify_persistent(const char *text)
{
    check(strcmp(text, "CONNECTING") == 0 || *text == 0,
          "picker uses persistent CONNECTING or clears the notice");
    picker_connecting = *text != 0;
    if (picker_connecting) ++picker_notifications;
}
void match_picker_frame(void) { ++picker_frames; }
void match_picker_clear(void)
{
    if (picker_connecting) ++picker_notice_clears;
    spectrum_gui_notify_persistent("");
    if (picker_mode == 2 && picker_clears == 1)
        check(picker_cursor[1], "down selects the second host across a sparse slot");
    if (picker_mode == 2 && picker_clears == 2)
        check(picker_cursor[0], "selection follows device after rename and reorder");
    ++picker_clears;
    memset(picker_text, 0, sizeof picker_text);
    memset(picker_cursor, 0, sizeof picker_cursor);
}
void match_picker_row(const char *line)
{
    check((uint8_t)line[0] < 9, "picker row is inside the list");
    strcpy(picker_text[(uint8_t)line[0]], line + 1);
    ++picker_rows;
}
void match_picker_finish(void) {}
void match_picker_cursor(uint8_t row)
{
    if (row == 255) return; /* Same no-selection guard as the real ASM. */
    check(row < 9, "cursor is inside the list");
    picker_cursor[row] ^= 1;
    if (row == 1 && picker_cursor[row]) {
        picker_move_frame = wait_calls;
        ++picker_move_count;
    }
}
void spectrum_input_suppress_until_release(uint8_t key)
{
    check(key == 13 || key == 32, "CONNECT suppresses the activation key");
    picker_suppressed_key = key;
}
int16_t spxmatch_select(uint8_t socket, uint8_t command, uint8_t index,
                        uint16_t revision)
{
    (void)socket;
    picker_command = command;
    picker_index = index;
    picker_revision = revision;
    if (command == SPXMATCH_CMD_CONNECT) {
        if (!picker_mode) return join_select_result;
        check(picker_connecting && picker_notifications == picker_connects + 1,
              "CONNECTING is visible before sending each CONNECT");
        ++picker_connects;
        if (picker_mode >= 10) {
            check(index == 0 && revision == 1,
                  "queued activation uses the displayed host");
            check(wait_calls == (picker_mode == 10 ? 55 : 5),
                  "one press during mailbox wait joins when reply arrives");
            return SPXN_OK;
        }
        if (picker_mode >= 7) {
            check(index == 0 && revision == (picker_mode == 8 ? 3 : 2),
                  "arriving host CONNECT uses current identity and revision");
            check(wait_calls == (picker_mode == 8 ? 110 :
                                 (picker_mode == 9 && picker_connects == 2 ? 70 : 60)),
                  "activation joins first arriving host immediately");
            return SPXN_OK;
        }
        check(picker_mode == 2 && index == 0 && revision == (picker_connects == 1 ? 3 : 4),
              "CONNECT uses displayed sparse slot and revision");
        check(wait_calls == (picker_connects == 1 ? 114 : 116),
              "removed selection and stale reply require fresh user action");
    }
    return SPXN_OK;
}
int16_t spxmatch_read(struct spxmatch_snapshot *out)
{
    uint8_t i;
    memset(out, 0, sizeof *out);
    if (join_replies) {
        uint8_t index = join_reply_index;
        if (index + 1u < join_reply_count) ++join_reply_index;
        return join_replies[index];
    }
    if (!picker_mode) {
        if (picker_result != 3 || picker_command == SPXMATCH_CMD_CONNECT)
            return picker_result;
        out->revision = 1;
        out->count = 1;
        strcpy(out->device, "FIRST");
        strcpy(out->name, "HOST (W)");
        return 1;
    }
    if (picker_mode == 19) {
        out->stage = 1;
        out->count = 2;
        out->revision = picker_raced ? 2 : 1;
        if (picker_command == SPXMATCH_CMD_MATCH && picker_raced && wait_calls < 5)
            return SPXMATCH_BUSY;
        if (picker_command == SPXMATCH_CMD_GET_PEER) {
            if (!picker_raced && picker_index == 1) {
                picker_raced = 1;
                out->revision = 2;
                return 1;
            }
            strcpy(out->device, picker_index ? "SECOND" : "FIRST");
            strcpy(out->name, picker_index ? "SECOND (B)" : "FIRST (W)");
        }
        return 1;
    }
    if (picker_mode >= 13) {
        if (picker_command == SPXMATCH_CMD_MATCH && wait_calls >= 50) {
            if (picker_mode == 15) return SPXMATCH_BUSY;
            if (picker_mode == 16) return SPXN_EROM;
            if (picker_mode == 17) return SPXMATCH_STAGE_CONNECTED;
            if (picker_mode == 18)
                return wait_calls < 55 ? SPXMATCH_BUSY : SPXMATCH_STAGE_CONNECTED;
        }
        out->stage = 1;
        out->revision = 1;
        out->count = 2;
        if (picker_mode == 13 && picker_command == SPXMATCH_CMD_GET_PEER &&
            wait_calls >= 1 && wait_calls < 11) return SPXMATCH_BUSY;
        if (picker_mode == 14 && picker_command == SPXMATCH_CMD_MATCH &&
            wait_calls >= 50 && wait_calls < 70) return SPXMATCH_BUSY;
        if (picker_command == SPXMATCH_CMD_GET_PEER) {
            strcpy(out->device, picker_index ? "SECOND" : "FIRST");
            strcpy(out->name, picker_index ? "SECOND (B)" : "FIRST (W)");
        }
        return 1;
    }
    if (picker_mode == 4) return SPXMATCH_BUSY;
    if (picker_mode == 6) return SPXN_EROM;
    if (picker_mode == 12) return SPXMATCH_STAGE_CONNECTED;
    out->stage = 1;
    if (picker_mode >= 10) {
        out->revision = 1;
        out->count = 1;
        if (picker_command == SPXMATCH_CMD_CONNECT) return 3;
        if (picker_command == SPXMATCH_CMD_MATCH &&
            picker_mode == 10 && wait_calls >= 50 && wait_calls < 55)
            return SPXMATCH_BUSY;
        if (picker_command == SPXMATCH_CMD_GET_PEER) {
            if (picker_mode == 11 && wait_calls < 5) return SPXMATCH_BUSY;
            strcpy(out->device, "FIRST");
            strcpy(out->name, "HOST (W)");
        }
        return 1;
    }
    if (picker_mode >= 7) {
        out->revision = wait_calls < 50 ? 1 : (picker_mode == 8 && wait_calls >= 100 ? 3 : 2);
        out->count = picker_mode != 8 ? wait_calls >= 50 : (wait_calls < 50 || wait_calls >= 100);
        if (picker_command == SPXMATCH_CMD_CONNECT) {
            check(picker_connecting, "CONNECTING persists through busy controller reads");
            if (picker_mode == 9 && picker_connects == 1) return 1;
            if (wait_calls < (picker_mode == 8 ? 115 : (picker_mode == 9 ? 75 : 65)))
                return SPXMATCH_BUSY;
            return 3;
        }
        if (picker_command == SPXMATCH_CMD_GET_PEER) {
            check(picker_revision == out->revision, "new host uses current revision");
            strcpy(out->device, wait_calls < 50 ? "OLD" : "ARRIVED");
            strcpy(out->name, "HOST (W)");
        }
        return 1;
    }
    out->revision = picker_mode != 2 ? 1 :
        (picker_connects ? 4 : (wait_calls >= 100 ? 3 : (wait_calls >= 50 ? 2 : 1)));
    if (picker_mode == 5 && picker_raced) out->revision = 2;
    out->count = picker_mode == 3 ? 0 : (picker_mode == 1 ? 9 : (out->revision == 1 ? 4 : 1));
    if (picker_command == SPXMATCH_CMD_CONNECT && picker_connects == 2) return 3;
    if (picker_command == SPXMATCH_CMD_GET_PEER) {
        i = picker_index;
        if (picker_mode == 5 && !picker_raced) {
            picker_raced = 1;
            out->revision = 2;
            return 1;
        }
        check(picker_revision == out->revision, "GET_PEER uses the current revision");
        if (picker_mode == 1) {
            memset(out->device, 'X', 63);
            out->device[63] = (char)('0' + i);
            out->device[64] = 0;
            strcpy(out->name, i % 2 ? "SAME (B)" : "SAME (W)");
        } else if (out->revision == 1) {
            if (i != 1) {
                strcpy(out->device, i == 3 ? "SECOND" : "FIRST");
                strcpy(out->name, i == 2 ? "GUEST" : "SAME (W)");
            }
        } else {
            strcpy(out->device, out->revision == 2 ? "SECOND" : "FIRST");
            strcpy(out->name, "RENAMED (B)");
        }
    }
    if (picker_mode == 2 && out->device[0]) {
        char identity = out->device[0];
        memset(out->device, 'X', 63);
        out->device[63] = identity;
        out->device[64] = 0;
    }
    return out->stage;
}

/* A failed read leaves out untouched (the driver's SPXN_ENOTSUP path), so
   the overlay must not trust what the saved-name bytes held before. */
int16_t spxinfo_string(uint8_t id, char *out, uint8_t capacity)
{
    const char *value = "CARTNAME";

    match_log_add('i');
    match_info_id = id;
    match_info_out = out;
    match_info_cap = capacity;
    if (id == SPXINFO_STRING_SERIAL || id == SPXINFO_STRING_MATCH_INITIATOR ||
        id == SPXINFO_STRING_MATCH_NAME) {
        char *expected_out = direct_serial_arg;

        if (id == SPXINFO_STRING_MATCH_INITIATOR ||
            (id == SPXINFO_STRING_SERIAL &&
             netchesszx_session_role == NETCHESSZX_SESSION_ROLE_HOST)) {
            expected_out += SPXINFO_STRING_SIZE;
        }
        /* The name read compares the serial in the second slot. */
        check((out == expected_out ||
               (id == SPXINFO_STRING_SERIAL &&
                out == direct_serial_arg + SPXINFO_STRING_SIZE)) &&
                  capacity == SPXINFO_STRING_SIZE,
              "MATCH information reads use the existing scratch slots");
    }
    if (match_info_result != SPXN_OK &&
        (match_info_only_id == 0xffu || match_info_only_id == id)) {
        return match_info_result;
    }
    if (id == SPXINFO_STRING_MATCH_NAME && match_info_name != 0) {
        /* Like the driver: a value that does not fit reports carry. */
        if (strlen(match_info_name) >= capacity) {
            out[0] = '\0';
            return SPXN_EROM;
        }
        strcpy(out, match_info_name);
        return SPXN_OK;
    }
    if (id == SPXINFO_STRING_SERIAL && match_info_serial != 0) {
        value = match_info_serial;
    } else if (id == SPXINFO_STRING_MATCH_INITIATOR &&
               match_info_initiator != 0) {
        value = match_info_initiator;
    }
    strcpy(out, value);
    return SPXN_OK;
}

int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget)
{
    const char *text = (const char *)buffer;
    int16_t result = SPXN_OK;

    (void)zero_budget;
    ++send_all_calls;
    if (send_all_calls == 1u) {
        check(length == strlen(send_all_payload) &&
                  memcmp(text, send_all_payload, length) == 0,
              "DIRECT uses send_all for payload");
    } else {
        check(length == 1u && text[0] == '\n',
              "DIRECT uses send_all for newline");
    }
    if (send_all_script_pos < send_all_script_len) {
        result = send_all_script[send_all_script_pos++];
    }
    return result;
}

int16_t spxtime_sntp(const struct spxtime_request *request)
{
    check(strcmp(request->host, "pool.ntp.org") == 0,
          "SNTP uses configured host");
    check(request->scratch_size >= SPXTIME_SNTP_PACKET_SIZE &&
              request->timeout_ticks == 150u,
          "SNTP request has bounded scratch and timeout");
    request->out->hour = 0u;
    request->out->minute = 30u;
    request->out->second = 40u;
    request->out->fat_date = (uint16_t)(((2026u - 1980u) << 9) |
                                        (1u << 5) | 1u);
    request->out->fat_time = (uint16_t)((30u << 5) | (40u >> 1));
    return SPXN_OK;
}

int16_t spxtime_parse_sntp(const uint8_t *packet, uint16_t length,
                           struct spxtime_result *out)
{
    (void)packet;
    check(length == SPXTIME_SNTP_PACKET_SIZE,
          "async SNTP parses one complete datagram");
    out->hour = 0u;
    out->minute = 30u;
    out->second = 40u;
    out->fat_date = (uint16_t)(((2026u - 1980u) << 9) |
                               (1u << 5) | 1u);
    out->fat_time = (uint16_t)((30u << 5) | (40u >> 1));
    return SPXN_OK;
}

int16_t spxudp_open(struct spxudp_socket *socket)
{
    ++udp_open_calls;
    socket->fd = 7u;
    socket->open = 1u;
    socket->poll_in = 0u;
    return SPXN_OK;
}

int16_t spxudp_sendto(struct spxudp_socket *socket, const void *buffer,
                      uint16_t length,
                      const struct spxudp_endpoint *destination)
{
    const uint8_t *packet = (const uint8_t *)buffer;

    ++udp_send_calls;
    check(socket->open && length == SPXTIME_SNTP_PACKET_SIZE &&
              packet[0] == 0x23u && destination->port == SPXTIME_SNTP_PORT,
          "async SNTP sends the canonical request");
    return (int16_t)length;
}

int16_t spxudp_poll(struct spxudp_socket *socket)
{
    ++udp_poll_calls;
    check(socket->open, "async SNTP polls its open socket");
    return udp_ready ? SPXN_POLLIN : 0;
}

int16_t spxudp_recvfrom(struct spxudp_socket *socket, void *buffer,
                        uint16_t maximum,
                        struct spxudp_endpoint *source)
{
    (void)socket;
    memset(buffer, 0, maximum);
    source->ip4be[0] = 127u;
    source->ip4be[1] = 0u;
    source->ip4be[2] = 0u;
    source->ip4be[3] = 1u;
    source->port = SPXTIME_SNTP_PORT;
    return SPXTIME_SNTP_PACKET_SIZE;
}

void spxudp_close(struct spxudp_socket *socket)
{
    ++udp_close_calls;
    socket->open = 0u;
}

static void set_socket_script(const uint8_t *data, uint16_t length,
                               const int16_t *polls, uint8_t poll_count)
{
    uint8_t i;

    poll_calls = 0u;
    recv_calls = 0u;
    poll_script_len = poll_count;
    poll_script_pos = 0u;
    recv_script_len = 0u;
    recv_script_pos = 0u;
    recv_data = data;
    recv_data_len = length;
    recv_data_pos = 0u;
    send_all_script_len = 0u;
    send_all_script_pos = 0u;
    for (i = 0u; i < poll_count; ++i) {
        poll_script[i] = polls[i];
    }
}

static void set_recv_script(const int16_t *results, uint8_t result_count)
{
    uint8_t i;

    recv_script_len = result_count;
    recv_script_pos = 0u;
    for (i = 0u; i < result_count; ++i) {
        recv_script[i] = results[i];
    }
}

static void set_send_all_script(const int16_t *results,
                                uint8_t result_count)
{
    uint8_t i;

    send_all_script_len = result_count;
    send_all_script_pos = 0u;
    for (i = 0u; i < result_count; ++i) {
        send_all_script[i] = results[i];
    }
}

static void reset_socket_script(const uint8_t *data, uint16_t length,
                                const int16_t *polls, uint8_t poll_count)
{
    direct_rx_count = 0u;
    direct_rx_head = 0u;
    direct_rx_payload_len = 0u;
    direct_rx_discard = 0u;
    spxn_direct_pending_cursor = 0u;
    spxn_direct_pending_length = 0u;
    direct_link_closed = 0u;
    active_link = 0u;
    close_calls = 0u;
    listen_calls = 0u;
    listen_result = SPXN_OK;
    accept_calls = 0u;
    wait_calls = 0u;
    key_polls = 0u;
    key_cancel_at = 0u;
    set_socket_script(data, length, polls, poll_count);
    send_all_calls = 0u;
    send_all_payload = "HELLO";
    sends_at_close = 0u;
}

static uint8_t direct_read_host(char *output, uint8_t capacity)
{
    uint8_t context[3] = { 0u, 0u, capacity };

    direct_host_test_ptr = output;
    return direct_read_payload_ovl(context);
}

static int16_t direct_read_net(char *output, uint8_t capacity)
{
    direct_host_test_ptr = output;
    return direct_read_payload(output, capacity);
}

static void test_direct_drain_and_accept(void)
{
    static const uint8_t payload[] = "MOVE\n";
    static const int16_t polls[] = { SPXN_POLLIN | SPXN_POLLHUP,
                                     SPXN_POLLHUP };
    char output[32];
    uint8_t context[3] = { 0u, 0u, sizeof(output) };

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 2u);
    direct_host_test_ptr = output;
    check(direct_read_payload_ovl(context) == 0u,
          "DIRECT returns queued payload link");
    check(strcmp(output, "MOVE") == 0,
          "DIRECT drains data before HUP");
    check(recv_calls == 1u && poll_calls == 1u && !direct_link_closed,
          "DIRECT polls before recv and defers HUP");
    check(direct_read_payload_ovl(context) == 0xfeu && close_calls == 1u,
          "DIRECT closes after queued data is delivered");

    reset_socket_script(0, 0u, (const int16_t[]){ SPXN_POLLCON }, 1u);
    check(direct_wait_pc_connect_ovl() == 1u && accept_calls == 1u,
          "DIRECT accepts only after listener POLLCON");
}

/* The app may load GUI_LOG and STATUS, both overwriting overlay context. */
static uint8_t test_pick_and_join(void)
{
    uint8_t connects = picker_connects;
    uint8_t result = spectrum_net_select_host();

    check(picker_connects == connects, "selection sends no CONNECT before game panels");
    if (result != 1u) return result;
    memset((void *)spectrum_overlay_context, 0xa5, SPECTRUM_OVERLAY_CONTEXT_SIZE);
    spectrum_gui_notify_persistent("CONNECTING");
    return spectrum_net_join_host();
}

static void test_direct_match_guest_connects(void)
{
    reset_socket_script(0, 0u, 0, 0u);
    active_link = 0xffu;
    connect_calls = 0u;
    resolved_host = 0;
    netchesszx_link_match = 1u;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    match_info_result = SPXN_EROM;
    match_open_result = 2;
    match_send_result = SPXN_OK;
    check(test_pick_and_join() == 1u &&
              listen_calls == 0u && connect_calls == 0u &&
              active_link == 0u && match_open_host_ok,
          "MATCH guest connects to the matchmaking service");
    check(match_open_host == direct_match_host_arg,
          "MATCH name is staged outside the overlay page");
    check(match_open_ip == direct_ip4_arg,
          "connect reads the address from scratch, not the stack");
    connect_calls = 0u;
    check(direct_connect_ovl(connect_ctx) == 1u && connect_calls == 0u &&
              active_link == 0u,
          "the MATCH overlay step keeps the resident connection");
    match_info_result = SPXN_OK;
    netchesszx_link_match = 0u;
    check(direct_connect_ovl(connect_ctx) == 1u && resolved_host != 0 &&
              strcmp(resolved_host, "127.0.0.1") == 0,
          "DIRECT guest keeps the configured host");
    check(direct_listen_ovl() == 1u && listen_calls == 1u &&
              connect_calls == 1u,
          "DIRECT host still listens");

    connect_result = SPXN_EROM;
    picker_result = MATCH_PICK_CANCEL;
    netchesszx_link_match = 1u;
    close_calls = 0u;
    check(test_pick_and_join() ==
                  SPECTRUM_LINK_CANCELLED &&
              close_calls == 3u,
          "a failed MATCH connect returns to Setup");
    netchesszx_link_match = 0u;
    check(direct_connect_ovl(connect_ctx) == 0u,
          "a failed DIRECT connect still retries");
    connect_result = SPXN_OK;
    picker_result = 3;
}

static void test_direct_wait_timeout_keeps_listener(void)
{
    static const int16_t accept_poll[] = { SPXN_POLLCON };

    reset_socket_script(0, 0u, 0, 0u);
    check(direct_wait_pc_connect_ovl() == 0u &&
              poll_calls == WAIT_LONG && wait_calls == WAIT_LONG &&
              close_calls == 0u,
          "DIRECT idle wait keeps the listener open");

    set_socket_script(0, 0u, accept_poll, 1u);
    check(direct_wait_pc_connect_ovl() == 1u && accept_calls == 1u &&
              close_calls == 0u,
          "DIRECT accepts after an earlier idle wait batch");
}

static void test_direct_wait_recovers_stale_listener(void)
{
    static const int16_t stale_then_connect[] = {
        SPXN_POLLHUP, SPXN_POLLCON
    };

    reset_socket_script(0, 0u, 0, 0u);
    check(direct_wait_pc_connect_ovl() == 0u &&
              poll_calls == WAIT_LONG && close_calls == 0u &&
              listen_calls == 0u,
          "DIRECT reaches long idle with its listener open");

    set_socket_script(0, 0u, stale_then_connect, 2u);
    check(direct_wait_pc_connect_ovl() == 1u && close_calls == 1u &&
              listen_calls == 1u && accept_calls == 1u,
          "DIRECT recreates a stale listener before accepting a late peer");
}

static void test_direct_wait_retries_failed_relisten(void)
{
    static const int16_t stale_listener[] = { SPXN_POLLNVAL };
    static const int16_t late_connect[] = { SPXN_POLLCON };

    reset_socket_script(0, 0u, stale_listener, 1u);
    listen_result = SPXN_EINVAL;
    check(direct_wait_pc_connect_ovl() == 0u && accept_calls == 0u &&
              poll_calls == 1u && wait_calls == WAIT_LONG - 1u,
          "DIRECT keeps a refused re-listen inside one paced batch");
    check(listen_calls > 1u &&
              listen_calls <= WAIT_LONG / DIRECT_REARM_FRAMES + 1u &&
              close_calls == listen_calls,
          "DIRECT re-arms a refused listener at most once per pace");

    listen_result = SPXN_OK;
    close_calls = 0u;
    listen_calls = 0u;
    set_socket_script(0, 0u, late_connect, 1u);
    check(direct_wait_pc_connect_ovl() == 1u && close_calls == 1u &&
              listen_calls == 1u && accept_calls == 1u,
          "DIRECT retries a failed listener recreation on the next batch");
}

static void test_direct_wait_cancel_during_refused_listen(void)
{
    reset_socket_script(0, 0u, 0, 0u);
    listen_result = SPXN_EINVAL;
    check(direct_listen_ovl() == 0u && listen_calls == 1u,
          "DIRECT host setup reports a refused first LISTEN");
    key_cancel_at = 120u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              key_polls == 120u && poll_calls == 0u &&
              wait_calls == 119u,
          "DIRECT cancel key works while LISTEN is refused");
    check(listen_calls == 4u,
          "DIRECT paces refused LISTEN retries before cancel");
}


static void test_direct_pollin_receive(void)
{
    static const uint8_t payload[] = "POLLIN\n";
    static const int16_t polls[] = { SPXN_POLLIN };
    char output[16];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 1u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "POLLIN") == 0 && poll_calls == 1u &&
              recv_calls == 1u,
          "DIRECT POLLIN receives one complete line");
}

static void test_direct_fragmented_line(void)
{
    static const uint8_t payload[] = "FRAGMENT\n";
    static const int16_t polls[] = { SPXN_POLLIN, SPXN_POLLIN };
    static const int16_t receives[] = { 4, 5 };
    char output[16];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 2u);
    set_recv_script(receives, 2u);
    check(direct_read_host(output, sizeof(output)) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              direct_rx_payload_len == 4u && direct_rx_count == 0u,
          "DIRECT fragmented line waits for LF");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "FRAGMENT") == 0 && poll_calls == 2u &&
              recv_calls == 2u && !direct_link_closed,
          "DIRECT fragmented line is reconstructed");
}

static void test_direct_coalesced_lines_fifo(void)
{
    static const uint8_t payload[] = "ONE\nTWO\nTHREE\n";
    static const int16_t polls[] = { SPXN_POLLIN };
    char output[16];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 1u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "ONE") == 0,
          "DIRECT coalesced lines preserve first payload");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "TWO") == 0,
          "DIRECT coalesced lines preserve second payload");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "THREE") == 0 && poll_calls == 1u &&
              recv_calls == 1u && direct_rx_count == 0u,
          "DIRECT coalesced lines remain FIFO without extra recv");
}

static void test_direct_idle_yields(void)
{
    static const int16_t polls[] = { 0 };
    char output[16];
    uint8_t context[3] = { 0u, 0u, sizeof(output) };

    reset_socket_script(0, 0u, polls, 1u);
    direct_host_test_ptr = output;
    /* The hook counts one logical frame yield; it is not wall-clock timing. */
    check(direct_read_payload_ovl(context) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              poll_calls == 1u && wait_calls == 1u && recv_calls == 0u &&
              close_calls == 0u && !direct_link_closed,
          "DIRECT empty poll yields exactly one frame without recv");

    reset_socket_script(0, 0u, (const int16_t[]){ SPXN_POLLCON }, 1u);
    check(direct_read_payload_ovl(context) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              wait_calls == 1u && recv_calls == 0u && !direct_link_closed,
          "a stale connect flag on a matched stream still yields a frame");
}

static void test_preflight_status(void)
{
    detect_calls = 0u;
    status_calls = 0u;
    probe_order = 0u;
    preflight_exec_calls = 0u;
    netchesszx_host_nextreg_peripheral_1 = 0u;
    check(spectrum_net_preflight_run(1u) == SPECTRUM_LINK_PREFLIGHT_OK &&
              detect_calls == 1u && status_calls == 1u &&
              preflight_exec_calls == 1u &&
              strcmp(spectrum_net_last_ip(), "192.168.1.20") == 0,
          "preflight detects and reports current controller status");
    check(net_uart_direct_idle_ticks == 150u,
          "preflight maps Spectranext 50 Hz timing");
    netchesszx_host_nextreg_peripheral_1 = 0x04u;
    check(spectrum_net_preflight_run(1u) == SPECTRUM_LINK_PREFLIGHT_OK &&
              net_uart_direct_idle_ticks == 180u,
          "preflight maps Spectranext 60 Hz timing");
}

static void test_mqtt_oversized_discard_above_buffer_capacity(void)
{
    static uint8_t payload[225u];
    static const int16_t polls[] = {
        SPXN_POLLIN, SPXN_POLLIN, SPXN_POLLIN, SPXN_POLLIN, SPXN_POLLIN
    };

    memset(payload, 'X', 223u);
    payload[223u] = 0xd0u;
    payload[224u] = 0u;
    mqtt_reset_session_state();
    MQTT_STREAM[0u] = 0x30u;
    MQTT_STREAM[1u] = 0xdfu;
    MQTT_STREAM[2u] = 0x01u;
    mqtt_stream_len = 3u;
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    check(mqtt_take_stream_packet() == 0 && mqtt_discard_remaining == 223u,
          "MQTT oversized packet starts a 223-byte discard");

    close_calls = 0u;
    set_socket_script(payload, sizeof(payload), polls, 5u);
    check(mqtt_drain_uart_budget(64u) == 1u &&
              mqtt_discard_remaining == 159u,
          "MQTT oversized discard passes the stream capacity boundary");
    check(mqtt_drain_uart_budget(64u) == 1u &&
              mqtt_discard_remaining == 95u,
          "MQTT oversized discard drains its second chunk");
    check(mqtt_drain_uart_budget(64u) == 1u &&
              mqtt_discard_remaining == 31u,
          "MQTT oversized discard drains its third chunk");
    check(mqtt_drain_uart_budget(64u) == 1u &&
              mqtt_discard_remaining == 0u,
          "MQTT oversized discard reaches the packet boundary");
    check(mqtt_drain_uart_budget(64u) == 1u && mqtt_stream_len == 2u &&
              close_calls == 0u,
          "MQTT oversized discard preserves the following packet");
    check(mqtt_take_stream_packet() == 2 && MQTT_STREAM[0u] == 0xd0u &&
              MQTT_STREAM[1u] == 0u,
          "MQTT oversized discard resumes at the following packet");
}

static void test_direct_nul_and_send(void)
{
    static const uint8_t malformed[] = { 'A', 0u, 'B', '\n' };
    static const int16_t polls[] = { SPXN_POLLIN };
    char output[16];
    char text[] = "HELLO";
    uint8_t context[3] = { 0u, 0u, sizeof(output) };

    reset_socket_script(malformed, sizeof(malformed), polls, 1u);
    direct_host_test_ptr = output;
    check(direct_read_payload_ovl(context) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              direct_rx_count == 0u,
          "DIRECT rejects embedded NUL before queueing");

    send_all_calls = 0u;
    direct_host_test_ptr = text;
    check(direct_send_text_ovl(context) == 1u && send_all_calls == 2u,
          "DIRECT completes payload and newline sends");
}

static void test_direct_nul_recovers_at_next_line(void)
{
    static const uint8_t payload[] = {
        'B', 'A', 'D', 0u, 'G', 'A', 'R', 'B', 'A', 'G', 'E', '\n',
        'G', 'O', 'O', 'D', '\n'
    };
    static const int16_t polls[] = { SPXN_POLLIN };
    char output[16];

    reset_socket_script(payload, sizeof(payload), polls, 1u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "GOOD") == 0 && direct_rx_count == 0u &&
              direct_rx_payload_len == 0u && !direct_link_closed,
          "DIRECT embedded NUL rejects one line and recovers at next LF");
}

static void test_direct_read_classification(void)
{
    struct direct_read_case {
        const char *label;
        int16_t poll_result;
        int16_t recv_result;
        uint8_t has_recv_script;
        uint8_t expected_result;
        uint8_t expected_yields;
        uint8_t expected_closes;
        uint8_t expected_recvs;
    };
    static const struct direct_read_case cases[] = {
        { "DIRECT poll would-block stays open",
          0, 0, 0, (uint8_t)SPECTRUM_NET_READ_TIMEOUT, 1u, 0u, 0u },
        { "DIRECT fatal poll closes",
          SPXN_EROM, 0, 0, 0xfeu, 0u, 1u, 0u },
        { "DIRECT fatal recv closes",
          SPXN_POLLIN, SPXN_EINVAL, 1u, 0xfeu, 0u, 1u, 1u },
        { "DIRECT EOF closes",
          SPXN_POLLIN, 0, 1u, 0xfeu, 0u, 1u, 1u },
        { "DIRECT HUP without bytes closes",
          SPXN_POLLHUP, 0, 0, 0xfeu, 0u, 1u, 0u }
    };
    uint8_t i;
    char output[16];

    for (i = 0u; i < (uint8_t)(sizeof(cases) / sizeof(cases[0])); ++i) {
        reset_socket_script(0, 0u, &cases[i].poll_result, 1u);
        if (cases[i].has_recv_script) {
            set_recv_script(&cases[i].recv_result, 1u);
        }
        check(direct_read_host(output, sizeof(output)) ==
                  cases[i].expected_result &&
                  wait_calls == cases[i].expected_yields &&
                  close_calls == cases[i].expected_closes &&
                  recv_calls == cases[i].expected_recvs &&
                  (direct_link_closed != 0u) ==
                      (cases[i].expected_closes != 0u),
              cases[i].label);
    }
}

static void test_direct_queue_boundary(void)
{
    static const uint8_t payload[] = "A\nB\nC\nD\n";
    static const int16_t polls[] = { SPXN_POLLIN, 0 };
    char output[16];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 2u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "A") == 0 && direct_rx_count == 2u,
          "DIRECT full queue returns oldest payload");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "B") == 0,
          "DIRECT full queue preserves middle payload");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "C") == 0 && direct_rx_count == 0u &&
              direct_rx_head == 0u,
          "DIRECT full queue preserves newest admitted payload");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "D") == 0 && poll_calls == 1u &&
              recv_calls == 1u && spxn_direct_pending_length == 0u,
          "DIRECT full queue resumes the unread recv suffix");
    check(direct_read_host(output, sizeof(output)) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              direct_rx_payload_len == 0u && direct_rx_discard == 0u &&
              wait_calls == 1u && poll_calls == 2u && recv_calls == 1u,
          "DIRECT resumed recv leaves no ghost payload");
}

static void test_direct_background_drain(void)
{
    static const uint8_t payload[] = "HELLO DIRECT GUEST\n";
    static const int16_t polls[] = { SPXN_POLLIN };
    char output[32];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 1u);
    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    spectrum_net_background_drain();
    spectrum_net_background_drain();
    check(recv_calls == 1u && poll_calls == 1u && direct_rx_count == 0u &&
              spxn_direct_pending_length == sizeof(payload) - 1u,
          "DIRECT background drain stages one recv without parsing twice");
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "HELLO DIRECT GUEST") == 0 &&
              recv_calls == 1u && poll_calls == 1u &&
              spxn_direct_pending_length == 0u,
          "DIRECT overlay consumes the staged recv");

    reset_socket_script(0, 0u, (const int16_t[]){ SPXN_EINVAL }, 1u);
    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    spectrum_net_background_drain();
    check(direct_link_closed == 1u && recv_calls == 0u,
          "DIRECT background poll failure closes the link");
}

static uint8_t direct_send_host(const char *text)
{
    uint8_t context[3] = { 0u, 0u, 0u };

    direct_host_test_ptr = (char *)text;
    return direct_send_text_ovl(context);
}

static void test_direct_send_outcomes(void)
{
    static const int16_t payload_stall[] = { SPXN_ESENDSTALL };
    static const int16_t payload_error[] = { SPXN_EINVAL };
    static const int16_t newline_stall[] = { SPXN_OK, SPXN_ESENDSTALL };

    reset_socket_script(0, 0u, 0, 0u);
    set_send_all_script(payload_stall, 1u);
    check(direct_send_host("HELLO") == 0u && send_all_calls == 1u,
          "DIRECT bounded payload stall is a failed send");

    reset_socket_script(0, 0u, 0, 0u);
    set_send_all_script(payload_error, 1u);
    check(direct_send_host("HELLO") == 0u && send_all_calls == 1u,
          "DIRECT fatal payload send is a failed send");

    reset_socket_script(0, 0u, 0, 0u);
    set_send_all_script(newline_stall, 2u);
    check(direct_send_host("HELLO") == 0u && send_all_calls == 2u,
          "DIRECT newline stall fails after payload handoff");
}

static void test_direct_active_link_zero(void)
{
    static const uint8_t payload[] = "ZERO\n";
    static const int16_t polls[] = { SPXN_POLLIN, 0 };
    char output[16];

    reset_socket_script(payload, (uint16_t)(sizeof(payload) - 1u),
                        polls, 2u);
    check(direct_read_net(output, sizeof(output)) == 0 &&
              strcmp(output, "ZERO") == 0,
          "DIRECT active link zero delivers a real payload");
    check(direct_read_net(output, sizeof(output)) == SPECTRUM_NET_READ_TIMEOUT &&
              output[0] == '\0' && poll_calls == 2u && recv_calls == 1u &&
              wait_calls == 1u && close_calls == 0u,
          "DIRECT active link zero timeout has no ghost payload");
}

static void test_direct_reconnect_resets_state(void)
{
    static const uint8_t stale[] = "OLD\nSTALE\n";
    static const int16_t stale_polls[] = { SPXN_POLLIN };
    static const int16_t accept_polls[] = { SPXN_POLLCON };
    static const uint8_t accepted[] = "ACCEPTED\n";
    static const int16_t accepted_polls[] = {
        SPXN_POLLIN | SPXN_POLLHUP, SPXN_POLLHUP
    };
    static const uint8_t reconnected[] = "RECONNECTED\n";
    static const int16_t reconnect_polls[] = { SPXN_POLLIN };
    char output[24];

    reset_socket_script(stale, (uint16_t)(sizeof(stale) - 1u),
                        stale_polls, 1u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "OLD") == 0 && direct_rx_count == 1u,
          "DIRECT old connection leaves a queued stale line");

    send_all_payload = "BYE";
    direct_host_test_ptr = (char *)send_all_payload;
    check(direct_send_text(send_all_payload) == 1u &&
              send_all_calls == 2u && close_calls == 0u,
          "DIRECT hands BYE and newline to the driver before closing");
    spectrum_net_start_uart();
    check(direct_rx_count == 0u && direct_rx_head == 0u &&
              direct_rx_payload_len == 0u && direct_rx_discard == 0u &&
              spxn_direct_pending_cursor == 0u &&
              spxn_direct_pending_length == 0u &&
              direct_link_closed == 0u && close_calls == 1u &&
              sends_at_close == 2u,
          "DIRECT close/reset closes the driver once after BYE and clears RX");

    set_socket_script(0, 0u, accept_polls, 1u);
    check(spectrum_net_listen() == 1u && active_link == 0xffu &&
              direct_link_closed == 0u,
          "DIRECT fresh listen starts clear");
    check(spectrum_net_wait_pc_connect() == 1u && accept_calls == 1u &&
              active_link == 0u && direct_rx_count == 0u &&
              direct_link_closed == 0u,
          "DIRECT fresh accept resets adapter state");

    set_socket_script(accepted, (uint16_t)(sizeof(accepted) - 1u),
                      accepted_polls, 2u);
    check(direct_read_net(output, sizeof(output)) == 0 &&
              strcmp(output, "ACCEPTED") == 0,
          "DIRECT accepted link delivers new payload");
    check(direct_read_net(output, sizeof(output)) == -2 &&
              direct_link_closed != 0u,
          "DIRECT accepted link closes after drained payload");

    set_socket_script(0, 0u, 0, 0u);
    check(spectrum_net_connect_host() == 1u && active_link == 0u &&
              direct_rx_count == 0u && direct_link_closed == 0u,
          "DIRECT reconnect starts with a fresh active link");

    set_socket_script(reconnected, (uint16_t)(sizeof(reconnected) - 1u),
                      reconnect_polls, 1u);
    check(direct_read_net(output, sizeof(output)) == 0 &&
              strcmp(output, "RECONNECTED") == 0 && direct_rx_count == 0u &&
              !direct_link_closed,
          "DIRECT reconnect has no stale payload or closure ghost");
}

/* CREATE + MATCH: the host's low-RAM bytes start as garbage (only
   0x5b00..0x5b4f is cleared at boot). */
static void match_reset(const char *name, uint8_t color,
                        const int16_t *statuses, uint8_t count)
{
    uint8_t i;

    reset_socket_script(0, 0u, 0, 0u);
    connect_calls = 0u;
    match_log_len = 0u;
    match_log[0] = '\0';
    match_open_result = 2;
    match_socket_index = 2;
    match_info_result = SPXN_OK;
    match_info_out = 0;
    match_info_only_id = 0xffu;
    match_info_name = 0;
    match_info_serial = "0123456789ABCDEF";
    match_send_count = 0u;
    match_send_result = SPXN_OK;
    match_status_calls = 0u;
    match_status_len = count;
    match_status_pos = 0u;
    for (i = 0u; i < count; ++i) {
        match_status_script[i] = statuses[i];
    }
    match_phase = 0xaau;
    match_ticks = 0xaau;
    match_socket = 0xaau;
    strcpy(match_saved_name, "STALE");
    netchesszx_link_match = 1u;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    strcpy(netchesszx_player_name, name);
    netchesszx_host_color = color;
}

static void test_match_host_registers(void)
{
    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, 0, 0u);
    check(direct_listen_ovl() == 1u && strcmp(match_log, "iicoM") == 0 &&
              connect_calls == 0u && listen_calls == 0u,
          "MATCH host registers without ROM CONNECT or LISTEN");
    check(match_phase == 1u && match_ticks == 0u && match_socket == 2u &&
              active_link == 0xffu,
          "MATCH host keeps the socket index and starts in phase 1");
    check(match_info_id == SPXINFO_STRING_SERIAL &&
              match_info_out == direct_serial_arg + SPXINFO_STRING_SIZE &&
              match_info_cap == SPXINFO_STRING_SIZE &&
              strcmp(match_saved_name, "CARTNAME") == 0,
          "MATCH host compares the serial in scratch and saves the name "
          "in low RAM");
    check(match_open_host_ok && match_open_host == direct_match_host_arg &&
              match_open_ip == direct_ip4_arg,
          "MATCH host stages the service outside the overlay page");
    check(match_send_count == 1u && match_send_socket[0] == 2u &&
              match_send_command[0] == SPXMATCH_CMD_MATCH &&
              match_send_action[0] == SPXMATCH_POLL && match_send_null[0],
          "MATCH host starts MATCH on its socket");

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, 0, 0u);
    match_open_result = SPXN_EROM;
    check(direct_listen_ovl() == 0u && strcmp(match_log, "iico") == 0 &&
              match_phase == 0u,
          "a failed MATCH open fails the listen like DIRECT");

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, 0, 0u);
    match_send_result = SPXN_EROM;
    check(direct_listen_ovl() == 0u && strcmp(match_log, "iicoMc") == 0 &&
              match_phase == 0u,
          "a refused MATCH start closes the socket and fails the listen");
    netchesszx_link_match = 0u;
}

static void test_match_host_names(void)
{
    static const int16_t start_reply[] = { 0 };
    static const struct {
        const char *name;
        uint8_t color;
        const char *lobby;
        const char *label;
    } cases[] = {
        { "IGNACIO", NETCHESSZX_COLOR_WHITE, "IGNACIO (W)",
          "MATCH host publishes NAME (W) as white" },
        { "IGNACIO", NETCHESSZX_COLOR_BLACK, "IGNACIO (B)",
          "MATCH host publishes NAME (B) as black" },
        { "", NETCHESSZX_COLOR_WHITE, "HOST (W)",
          "MATCH host without NAME publishes HOST (W)" },
        { "ABCDEFGHIJ", NETCHESSZX_COLOR_WHITE, "ABCDEFGHIJ (W)",
          "MATCH host fits a 10-character NAME, NUL-terminated" }
    };
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(cases) / sizeof(cases[0])); ++i) {
        match_reset(cases[i].name, cases[i].color, start_reply, 1u);
        (void)direct_listen_ovl();
        check(direct_wait_pc_connect_ovl() == 0u && match_phase == 2u &&
                  match_send_count == 2u && match_send_socket[1] == 2u &&
                  match_send_command[1] == SPXMATCH_CMD_SET_NAME &&
                  match_send_action[1] == 0u && !match_send_null[1] &&
                  strcmp(match_send_text[1], cases[i].lobby) == 0,
              cases[i].label);
    }
    netchesszx_link_match = 0u;
}

static void test_match_host_polls_then_connects(void)
{
    static const int16_t replies[] = {
        0, 1, SPXMATCH_BUSY, SPXMATCH_STAGE_CONNECTED
    };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 4u);
    (void)direct_listen_ovl();
    check(direct_wait_pc_connect_ovl() == 1u && match_phase == 5u &&
              active_link == 0u,
          "MATCH host continues as the connected host on stage 3");
    check(match_send_count == 3u &&
              match_send_command[2] == SPXMATCH_CMD_MATCH &&
              match_send_action[2] == SPXMATCH_POLL && match_send_null[2] &&
              match_send_frame[2] - match_status_frame[1] == 50u &&
              match_status_frame[2] == match_send_frame[2] + 1u,
          "MATCH host polls 50 frames after the name reply, not before");
    check(strcmp(match_log, "iicoMNM") == 0 && close_calls == 1u &&
              poll_calls == 0u && recv_calls == 0u && send_all_calls == 0u,
          "MATCH host never touches the stream before pairing");
    netchesszx_link_match = 0u;
}

/* The snapshot is defined only for MATCH replies: a SET_NAME reply, even a
   stale 3 or a rejection, only leads to the next poll. */
static void test_match_host_name_reply_is_not_a_stage(void)
{
    static const int16_t name_replies[] = {
        SPXMATCH_STAGE_CONNECTED, SPXN_EROM
    };
    int16_t replies[2] = { 0, 0 };
    uint8_t i;

    for (i = 0u; i < 2u; ++i) {
        replies[1] = name_replies[i];
        match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 2u);
        (void)direct_listen_ovl();
        check(direct_wait_pc_connect_ovl() == 0u && match_phase == 4u &&
                  active_link == 0xffu && strcmp(match_log, "iicoMNM") == 0,
              i == 0u ? "a SET_NAME reply of 3 does not connect the host"
                      : "a rejected SET_NAME does not re-register the host");
        check(match_send_count == 3u &&
                  match_send_command[2] == SPXMATCH_CMD_MATCH &&
                  match_send_action[2] == SPXMATCH_POLL &&
                  match_send_frame[2] - match_status_frame[1] == 50u,
              i == 0u ? "MATCH host polls at the normal pace after a "
                        "SET_NAME reply of 3"
                      : "MATCH host polls at the normal pace after a "
                        "rejected SET_NAME");
    }
    netchesszx_link_match = 0u;
}

static void test_match_host_reregisters(void)
{
    static const int16_t failures_seen[] = {
        SPXMATCH_STAGE_ERROR, SPXN_EROM, SPXMATCH_STAGE_CANCELLED
    };
    static const char *const labels[] = {
        "after stage 5", "after a controller failure", "after stage 4"
    };
    char label[96];
    int16_t replies[4] = { 0, 1, 0, SPXN_EROM };
    uint8_t i;

    for (i = 0u; i < 3u; ++i) {
        replies[2] = failures_seen[i];
        match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 4u);
        (void)direct_listen_ovl();
        close_calls = 0u;
        sprintf(label, "MATCH host cancels, closes and re-registers %s",
                labels[i]);
        check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_FAILED &&
                  match_phase == 0u && close_calls == 1u &&
                  strcmp(match_log, "iicoMNMXc") == 0,
              label);
        sprintf(label, "MATCH host registers again on the next wait %s",
                labels[i]);
        check(direct_wait_pc_connect_ovl() == 0u && match_phase == 1u &&
                  close_calls == 2u &&
                  strcmp(match_log, "iicoMNMXccoM") == 0,
              label);
        sprintf(label, "MATCH host sends MATCH cancel on its socket %s",
                labels[i]);
        check(match_send_count == 5u && match_send_socket[3] == 2u &&
                  match_send_command[3] == SPXMATCH_CMD_MATCH &&
                  match_send_action[3] == SPXMATCH_CANCEL &&
                  match_send_null[3] &&
                  match_send_frame[3] == match_status_frame[2],
              label);
        check(match_send_command[4] == SPXMATCH_CMD_MATCH &&
                  match_send_action[4] == SPXMATCH_POLL &&
                  strcmp(match_saved_name, "CARTNAME") == 0,
              "MATCH re-registration keeps the first saved cartridge name");
        /* The pause is the DIRECT re-arm pace: 50 idle frames, counted
           from the wait after the one that reported the failure. */
        sprintf(label, "MATCH host pauses before registering %s", labels[i]);
        check(match_open_frame - match_status_frame[2] ==
                  DIRECT_REARM_FRAMES &&
                  match_send_frame[4] == match_open_frame,
              label);
    }
    netchesszx_link_match = 0u;
}

/* Controller rule: a paired match is left through MATCH before the next
   registration closes and reopens the socket. */
static void test_match_host_closes_after_pair(void)
{
    static const int16_t replies[] = {
        0, 1, SPXMATCH_STAGE_CONNECTED, 0, SPXN_EROM
    };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 5u);
    (void)direct_listen_ovl();
    check(direct_wait_pc_connect_ovl() == 1u && match_phase == 5u,
          "MATCH host marks a paired session");
    match_log_len = 0u;
    match_log[0] = '\0';
    match_send_count = 0u;
    match_open_result = 1;
    check(direct_listen_ovl() == 1u && strcmp(match_log, "ciicoM") == 0,
          "the next MATCH listen closes before it registers again");
    check(match_send_count == 1u && match_send_socket[0] == 1u &&
              match_send_command[0] == SPXMATCH_CMD_MATCH &&
              match_send_action[0] == SPXMATCH_POLL && match_phase == 1u,
          "MATCH closes the paired socket, then starts anew without LEAVE");

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, 0, 0u);
    match_phase = 0u;
    check(direct_listen_ovl() == 1u && strcmp(match_log, "iicoM") == 0,
          "a MATCH listen in phase IDLE sends no leave");
    netchesszx_link_match = 0u;
}

static void test_match_host_cancel_busy(void)
{
    static const int16_t replies[] = {
        SPXMATCH_BUSY, SPXMATCH_BUSY, SPXMATCH_BUSY, 0, 0,
        SPXMATCH_STAGE_CANCELLED
    };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 6u);
    (void)direct_listen_ovl();
    key_cancel_at = 1u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              match_phase == 0u && match_status_pos == 6u,
          "MATCH host cancel returns to Setup in phase 0");
    check(strcmp(match_log, "iicoMNXc") == 0,
          "MATCH cancel restores the name, cancels, then closes");
    check(match_status_frame[3] == 3u && match_send_frame[1] == 3u,
          "MATCH cancel waits for the busy reply before SET_NAME");
    check(match_send_command[1] == SPXMATCH_CMD_SET_NAME &&
              match_send_socket[1] == 2u &&
              strcmp(match_send_text[1], "CARTNAME") == 0,
          "MATCH cancel restores the saved cartridge name");
    check(match_send_command[2] == SPXMATCH_CMD_MATCH &&
              match_send_action[2] == SPXMATCH_CANCEL && match_send_null[2],
          "MATCH cancel sends MATCH cancel after the name reply");
    netchesszx_link_match = 0u;
}

static void test_match_host_cancel_timeout(void)
{
    static const int16_t name_busy[] = { 0 };

    /* The MATCH start never answers: no further command, just close. */
    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, 0, 0u);
    (void)direct_listen_ovl();
    key_cancel_at = 1u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              strcmp(match_log, "iicoMc") == 0 && match_phase == 0u &&
              match_status_calls == MATCH_REPLY_FRAMES &&
              wait_calls == MATCH_REPLY_FRAMES - 1u,
          "MATCH cancel gives up on a busy mailbox after 250 frames");

    /* The SET_NAME restore never answers: MATCH cancel is skipped too. */
    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, name_busy, 1u);
    (void)direct_listen_ovl();
    key_cancel_at = 1u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              strcmp(match_log, "iicoMNc") == 0 && match_phase == 0u,
          "MATCH cancel skips MATCH cancel after a SET_NAME timeout");
    netchesszx_link_match = 0u;
}

static void test_match_host_cancel_no_saved_name(void)
{
    static const int16_t replies[] = { 0, SPXMATCH_STAGE_CANCELLED };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 2u);
    match_info_result = SPXN_EROM;
    check(direct_listen_ovl() == 1u && match_phase == 1u,
          "MATCH host still hosts without operation 21");
    key_cancel_at = 1u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              strcmp(match_log, "icoMXc") == 0 && match_phase == 0u,
          "MATCH cancel without a saved name skips SET_NAME");
    netchesszx_link_match = 0u;
}

static void test_match_host_serial_lookup_failure(void)
{
    static const int16_t replies[] = { 0, 0, SPXMATCH_STAGE_CANCELLED };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 3u);
    match_info_name = "0123456789ABCDEF";
    match_info_result = SPXN_EROM;
    match_info_only_id = SPXINFO_STRING_SERIAL;
    strcpy(direct_serial_arg + SPXINFO_STRING_SIZE, match_info_name);
    (void)direct_listen_ovl();
    key_cancel_at = 1u;
    check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
              strcmp(match_log, "iicoMNXc") == 0 &&
              match_send_command[1] == SPXMATCH_CMD_SET_NAME &&
              strcmp(match_send_text[1], match_info_name) == 0,
          "MATCH cancel preserves the name when SERIAL fails despite stale "
          "matching scratch");
    netchesszx_link_match = 0u;
}

/* A paired game leaves the cartridge named `NAME (W)`: the next host session
   must save and restore the unmarked name. An unnamed cartridge reads as its
   serial, restored by a blank SET_NAME. */
static void test_match_host_saves_unmarked_name(void)
{
    static const int16_t replies[] = { 0, SPXMATCH_STAGE_CANCELLED };
    static const struct {
        const char *read;
        const char *restored;
        const char *label;
    } cases[] = {
        { "IGNACIO (W)", "IGNACIO",
          "MATCH cancel restores the name without a white mark" },
        { "IGNACIO (B)", "IGNACIO",
          "MATCH cancel restores the name without a black mark" },
        { "IGNACIO", "IGNACIO",
          "MATCH cancel keeps an unmarked cartridge name" },
        { "0123456789ABCDEF", "",
          "MATCH cancel clears the canonical 16-character serial fallback" },
        { "FEDCBA9876543210", "FEDCBA9876543210",
          "MATCH cancel preserves a distinct 16-character cartridge name" },
        { "(W)", "(W)",
          "MATCH cancel keeps a name shorter than the mark" },
        { "IGNACIO (X)", "IGNACIO (X)",
          "MATCH cancel keeps any other bracketed suffix" },
        { " (W)", "",
          "MATCH cancel strips a bare 4-character mark to a blank name" },
        { "ABCDEFGHIJKLMNOPQRS (B)", "ABCDEFGHIJKLMNOPQRS",
          "MATCH cancel strips the mark from a 23-character name" },
        { "ABCDEFGHIJKLMNOPQRSTUVWX", "",
          "MATCH cancel treats a 24-character read as the serial fallback" },
        { "0123456789ABCDEF0123456789ABCDEF"
          "0123456789ABCDEF0123456789ABCDEF", "",
          "MATCH cancel restores an unnamed cartridge with a blank name" }
    };
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(cases) / sizeof(cases[0])); ++i) {
        match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 2u);
        match_info_name = cases[i].read;
        (void)direct_listen_ovl();
        key_cancel_at = 1u;
        check(direct_wait_pc_connect_ovl() == SPECTRUM_LINK_CANCELLED &&
                  match_send_command[1] == SPXMATCH_CMD_SET_NAME &&
                  strcmp(match_send_text[1], cases[i].restored) == 0,
              cases[i].label);
    }
    match_info_name = 0;
    netchesszx_link_match = 0u;
}

/* JOIN + MATCH: after ROM CONNECT the guest must be the initiator. */
static void match_guest_reset(const char *serial, const char *initiator)
{
    reset_socket_script(0, 0u, 0, 0u);
    connect_calls = 0u;
    connect_result = SPXN_OK;
    match_log_len = 0u;
    match_log[0] = '\0';
    match_info_result = SPXN_OK;
    match_info_only_id = 0xffu;
    match_info_serial = serial;
    match_info_initiator = initiator;
    match_send_count = 0u;
    match_send_result = SPXN_OK;
    match_socket_index = 3;
    match_status_calls = 0u;
    match_status_len = 2u;
    match_status_pos = 0u;
    match_status_script[0] = 0;
    match_status_script[1] = 0;
    netchesszx_link_match = 1u;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
}

static void test_match_guest_initiator_ok(void)
{
    match_guest_reset("SERIAL-A", "SERIAL-A");
    check(direct_connect_ovl(connect_ctx) == 1u && strcmp(match_log, "ii") == 0 &&
              active_link == 0u && close_calls == 0u,
          "MATCH guest that is the initiator keeps its connection");
    netchesszx_link_match = 0u;
}

static void test_match_guest_picked_by_guest(void)
{
    char serial[SPXINFO_STRING_SIZE];
    char initiator[SPXINFO_STRING_SIZE];

    match_guest_reset("SERIAL-A", "SERIAL-B");
    check(direct_connect_ovl(connect_ctx) == SPECTRUM_LINK_NOT_HOST &&
              strcmp(match_log, "iic") == 0 && close_calls == 1u &&
              connect_calls == 0u,
          "MATCH guest picked by another guest closes and says NOT_HOST");
    check(match_send_count == 0u && match_status_calls == 0u,
          "rejected paired guest closes its descriptor without lobby commands");

    memset(serial, 'S', SPXINFO_STRING_SIZE - 1u);
    serial[SPXINFO_STRING_SIZE - 1u] = '\0';
    memcpy(initiator, serial, sizeof(initiator));
    initiator[SPXINFO_STRING_SIZE - 2u] = 'T';
    match_guest_reset(serial, initiator);
    check(direct_connect_ovl(connect_ctx) == SPECTRUM_LINK_NOT_HOST,
          "64-character serials that differ at the end are compared whole");
    match_guest_reset(serial, serial);
    check(direct_connect_ovl(connect_ctx) == 1u,
          "equal 64-character serials stay paired");

    match_guest_reset("SERIAL-A", "SERIAL-B");
    netchesszx_link_match = 0u;
    check(direct_connect_ovl(connect_ctx) == 1u && strcmp(match_log, "c") == 0,
          "DIRECT guests never read the serials");
}

/* JOIN, before the firmware menu: a name a host session left marked is
   restored through a short session; an unmarked one opens nothing. */
static void test_match_guest_unmarks_name(void)
{
    static const int16_t replies[] = { 0, 0, 4 };

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 3u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    match_info_name = "IGNACIO (W)";
    connect_ctx[SPECTRUM_OVL_CTX_DIRECT_UNMARK] = 1u;
    check(direct_connect_ovl(connect_ctx) == 1u &&
              strcmp(match_log, "iicoMNXc") == 0 &&
              match_send_command[1] == SPXMATCH_CMD_SET_NAME &&
              strcmp(match_send_text[1], "IGNACIO") == 0 &&
              match_send_action[2] == SPXMATCH_CANCEL &&
              match_phase == 0u && connect_calls == 0u,
          "MATCH guest removes a host mark before the firmware menu");

    match_reset("IGNACIO", NETCHESSZX_COLOR_WHITE, replies, 3u);
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    match_info_name = "IGNACIO";
    check(direct_connect_ovl(connect_ctx) == 1u &&
              strcmp(match_log, "ii") == 0 && match_send_count == 0u,
          "MATCH guest with an unmarked name opens no session");
    connect_ctx[SPECTRUM_OVL_CTX_DIRECT_UNMARK] = 0u;
    match_info_name = 0;
    netchesszx_link_match = 0u;
}

static void test_match_guest_no_strings(void)
{
    match_guest_reset("SERIAL-A", "SERIAL-B");
    match_info_result = SPXN_EROM;
    check(direct_connect_ovl(connect_ctx) == 1u && strcmp(match_log, "i") == 0 &&
              close_calls == 0u && active_link == 0u,
          "MATCH guest skips the check without operation 21");

    match_guest_reset("SERIAL-A", "SERIAL-B");
    match_info_result = SPXN_EROM;
    match_info_only_id = SPXINFO_STRING_MATCH_INITIATOR;
    check(direct_connect_ovl(connect_ctx) == 1u && strcmp(match_log, "ii") == 0 &&
              close_calls == 0u,
          "MATCH guest skips the check without an initiator");
    netchesszx_link_match = 0u;
}

static void test_match_guest_remote_bye_retry(void)
{
    static const uint8_t payload[] = "BYE\n";
    static const int16_t polls[] = { SPXN_POLLIN, SPXN_POLLHUP };
    char output[16];

    match_guest_reset("SERIAL-A", "SERIAL-A");
    match_phase = MATCH_PHASE_PAIRED;
    set_socket_script(payload, sizeof(payload) - 1u, polls, 2u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "BYE") == 0 && close_calls == 0u,
          "remote BYE returns before the guest observes EOF");
    match_menu_trace = 1u;
    check(test_pick_and_join() == 1u &&
              strcmp(match_log, "ciicoMii") == 0 &&
              match_send_count == 1u &&
              match_send_action[0] == SPXMATCH_POLL,
          "guest retry closes its active MATCH before the next picker");

    match_guest_reset("SERIAL-A", "SERIAL-A");
    match_socket_index = -1;
    check(test_pick_and_join() == 1u &&
              strcmp(match_log, "ciicoMii") == 0 && match_send_count == 1u,
          "a menu opened with no stream sends no redundant leave");

    match_guest_reset("SERIAL-A", "SERIAL-A");
    picker_result = MATCH_PICK_CANCEL;
    check(test_pick_and_join() ==
                  SPECTRUM_LINK_CANCELLED &&
              close_calls == 3u && match_phase == MATCH_PHASE_IDLE,
          "menu cancellation still closes after closing the old MATCH");
    match_menu_trace = 0u;
    netchesszx_link_match = 0u;
    connect_result = SPXN_OK;
    picker_result = 3;
}

static void test_match_close(void)
{
    static const int16_t replies[] = { SPXMATCH_BUSY, 0, SPXMATCH_BUSY, 0 };
    static const int16_t ready[] = { 0, 0 };
    static const int16_t failed[] = { SPXN_EROM, SPXN_EROM };
    uint8_t role;
    int16_t socket;

    for (role = 0u; role != 2u; ++role) {
        match_reset("NAME", NETCHESSZX_COLOR_WHITE, replies, 4u);
        netchesszx_session_role = role;
        match_socket_index = 3;
        match_socket = 0u; /* Stale lobby state is never the stream mapping. */
        match_phase = MATCH_PHASE_PAIRED;
        spectrum_net_close_direct(0u);
        check(strcmp(match_log, "c") == 0 &&
                  match_send_count == 0u && match_status_calls == 0u &&
                  wait_calls == 0u && close_calls == 1u &&
                  match_phase == MATCH_PHASE_IDLE,
              "paired MATCH closes normally without releasing transport first");
        (void)direct_close_ovl();
        check(match_send_count == 0u,
              "a repeated close never leaves a socket already closed");
    }

    for (socket = -1; socket <= 4; socket += 5) {
        match_reset("NAME", NETCHESSZX_COLOR_WHITE, ready, 2u);
        match_socket_index = socket;
        (void)direct_close_ovl();
        check(match_send_count == 0u && match_status_calls == 0u &&
                  close_calls == 1u && match_phase == MATCH_PHASE_IDLE,
              "invalid or closed stream skips MATCH leave but still closes");
    }

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, 0, 0u);
    (void)direct_close_ovl();
    check(match_send_count == 0u && close_calls == 1u &&
              match_status_calls == 0u && wait_calls == 0u,
          "established close does not wait for or overwrite a busy mailbox");

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, ready, 1u);
    (void)direct_close_ovl();
    check(match_send_count == 0u && close_calls == 1u &&
              match_status_calls == 0u && wait_calls == 0u,
          "established close needs no controller reply");

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, ready, 2u);
    match_send_result = SPXN_EROM;
    (void)direct_close_ovl();
    check(match_send_count == 0u && match_status_calls == 0u && close_calls == 1u,
          "a failing mailbox send cannot prevent normal descriptor close");

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, failed, 2u);
    (void)direct_close_ovl();
    check(match_send_count == 0u && match_status_calls == 0u && close_calls == 1u,
          "a controller failure still closes the local stream");

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, ready, 2u);
    netchesszx_link_match = 0u;
    (void)direct_close_ovl();
    check(match_send_count == 0u && match_status_calls == 0u && close_calls == 1u,
          "plain DIRECT closes without MATCH mailbox traffic");
}

static void test_match_eof_closes_after_drain(void)
{
    static const uint8_t payload[] = "FINAL\n";
    static const int16_t polls[] = { SPXN_POLLIN | SPXN_POLLHUP, SPXN_POLLHUP };
    static const int16_t replies[] = { 0, 0 };
    char output[16];

    match_reset("NAME", NETCHESSZX_COLOR_WHITE, replies, 2u);
    match_socket_index = 1;
    set_socket_script(payload, sizeof(payload) - 1u, polls, 2u);
    check(direct_read_host(output, sizeof(output)) == 0u &&
              strcmp(output, "FINAL") == 0 && close_calls == 0u &&
              match_send_count == 0u,
          "MATCH delivers the final payload before closing");
    check(direct_read_host(output, sizeof(output)) == 0xfeu &&
              match_send_count == 0u && close_calls == 1u &&
              match_phase == MATCH_PHASE_IDLE,
          "drained MATCH EOF closes without releasing transport first");
    netchesszx_link_match = 0u;
}

static void test_clock_timezone(void)
{
    netchesszx_timezone = -1;
    netchesszx_timezone_last = -1;
    clock_calls = 0u;
    check(spectranext_time_sync_ovl() == 1u && clock_calls == 1u &&
              clock_hour == 23u && clock_minute == 30u &&
              clock_second == 40u &&
              fat_date == (uint16_t)(((2025u - 1980u) << 9) |
                                     (12u << 5) | 31u),
          "SNTP UTC converts to local time and previous date");

    netchesszx_timezone = NETCHESSZX_TIME_RTC;
    netchesszx_timezone_last = 2;
    check(spectranext_time_sync_ovl() == 1u && netchesszx_timezone == 2 &&
              clock_hour == 2u && clock_minute == 30u &&
              ((fat_date >> 5) & 0x0fu) == 1u &&
              (fat_date & 0x1fu) == 1u,
          "RTC setting falls back to last numeric timezone");
}

static void test_clock_retry_is_polled_without_waiting(void)
{
    uint16_t waits_before = wait_calls;
    uint8_t polls_after_success;

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    netchesszx_timezone = -1;
    clock_calls = 0u;
    udp_open_calls = 0u;
    udp_send_calls = 0u;
    udp_poll_calls = 0u;
    udp_close_calls = 0u;
    udp_ready = 0u;

    spectrum_net_clock_retry_start();
    check(udp_open_calls == 1u && udp_send_calls == 1u &&
              wait_calls == waits_before && clock_calls == 0u,
          "async SNTP start does not wait or set an unverified clock");
    spectrum_net_background_drain_clock();
    check(udp_poll_calls == 1u && wait_calls == waits_before &&
              clock_calls == 0u,
          "async SNTP idle poll performs one bounded step");

    udp_ready = 1u;
    spectrum_net_background_drain_clock();
    check(clock_calls == 1u && clock_hour == 23u &&
              clock_minute == 30u && clock_second == 40u &&
              udp_close_calls == 1u,
          "async SNTP applies timezone and closes after a valid reply");
    polls_after_success = udp_poll_calls;
    spectrum_net_background_drain_clock();
    check(udp_poll_calls == polls_after_success,
          "completed async SNTP is no longer polled");

    udp_ready = 0u;
    spectrum_net_clock_retry_start();
    spectrum_net_clock_retry_cancel();
    polls_after_success = udp_poll_calls;
    spectrum_net_background_drain_clock();
    check(udp_poll_calls == polls_after_success && udp_close_calls == 2u,
          "async SNTP cancellation closes without another poll");
}

static void test_direct_graceful_close(void)
{
    static const uint8_t pending[] = "ACK PING\n";
    static const int16_t graceful[] = { SPXN_POLLCON, SPXN_POLLIN, 0, SPXN_POLLHUP };
    static const int16_t terminal[] = { SPXN_POLLHUP | SPXN_POLLIN, SPXN_POLLNVAL, SPXN_EROM };
    uint8_t i;

    reset_socket_script(pending, sizeof(pending) - 1u, graceful, 4u);
    spectrum_net_close_direct(150u);
    check(close_calls == 1u && poll_calls == 4u && recv_calls == 1u && wait_calls == 3u,
          "graceful close yields and drains until peer shutdown");
    reset_socket_script(0, 0u, 0, 0u);
    spectrum_net_close_direct(150u);
    check(close_calls == 1u && poll_calls == 150u && wait_calls == 150u,
          "unresponsive peer cannot hold close beyond existing send budget");
    reset_socket_script(0, 0u, 0, 0u);
    spectrum_net_close_direct(0u);
    check(close_calls == 1u && poll_calls == 0u && wait_calls == 0u,
          "failed BYE and ordinary close do not wait");
    for (i = 0u; i < 3u; ++i) {
        reset_socket_script(pending, sizeof(pending) - 1u, terminal + i, 1u);
        spectrum_net_close_direct(150u);
        check(close_calls == 1u && poll_calls == 1u && recv_calls == 0u && wait_calls == 0u,
              "peer shutdown or poll error closes immediately");
    }
    reset_socket_script(0, 0u, (const int16_t[]){SPXN_POLLIN}, 1u);
    spectrum_net_close_direct(150u);
    check(close_calls == 1u && recv_calls == 1u && wait_calls == 0u,
          "zero-length receive completes graceful close");
}

static void test_match_picker(void)
{
    static const int16_t ready[] = { 0, 0 };
    uint8_t mode;

    for (mode = 1; mode <= 19; ++mode) {
        uint8_t expected = SPECTRUM_LINK_CANCELLED;
        match_reset("GUEST", NETCHESSZX_COLOR_WHITE, ready, 2);
        netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
        match_info_initiator = match_info_serial;
        picker_mode = mode;
        spectrum_input_pending_event = 0;
        picker_frames = picker_clears = picker_rows = picker_connects = 0;
        picker_key_frame = 0xffff;
        picker_move_frame = 0xffff;
        picker_move_count = 0;
        picker_raced = picker_suppressed_key = 0;
        picker_connecting = picker_notifications = picker_notice_clears = 0;
        if (mode == 2 || mode == 6 || mode == 9 || mode == 15 || mode == 16)
            expected = 0;
        else if (mode == 12 || mode == 17 || mode == 18)
            expected = SPECTRUM_LINK_NOT_HOST;
        else if (mode >= 7 && mode <= 11)
            expected = 1;
        check(test_pick_and_join() == expected, "picker joins selected host or cancels cleanly");
        check(picker_frames == 1, "picker frame is painted only once");
        if (mode == 19) {
            check(picker_move_frame == 5 && picker_move_count == 1 && picker_cursor[1] &&
                      picker_clears == 2 && picker_connects == 0,
                  "revision retry preserves the queued arrow until the whole list is ready");
        } else if (mode >= 15) {
            check(picker_connects == 0 && picker_clears == 1 && match_phase == MATCH_PHASE_IDLE,
                  "pending poll errors, timeout or unsolicited pairing close without CONNECT");
            if (mode == 15)
                check(wait_calls == 300, "asynchronous poll timeout stays bounded at 250 frames");
            if (mode == 18)
                check(picker_connecting && wait_calls == 55,
                      "activation shows CONNECTING before draining an outstanding poll");
        } else if (mode >= 13) {
            unsigned key_frame = mode == 13 ? 3 : 52;
            printf("picker latency mode %u: key=%u cursor=%u delta=%u frames\n",
                   mode, key_frame, picker_move_frame, picker_move_frame-key_frame);
            check(picker_move_frame <= key_frame + 1,
                  "cursor responds within one frame even while the controller is busy");
            check(picker_move_count == (mode == 13 ? 2 : 1),
                  "successive arrows remain responsive while the controller is busy");
            check(picker_clears == 1 && picker_connects == 0,
                  "navigation changes only the cursor, without refreshing or joining");
        } else if (mode == 1) {
            check(picker_rows == 9 && picker_clears == 1 && picker_connects == 0,
                  "nine hosts fit and unchanged revisions never repaint");
            check(strstr(picker_text[8], "WHITE") != 0 && picker_cursor[0],
                  "last host is visible and initial selection is first host");
        } else if (mode == 2) {
            check(picker_connects == 1 && picker_clears == 3 && picker_rows == 4,
                  "sparse peers and removal repaint only changed lists; stale CONNECT exits");
            check(picker_text[1][0] == 0 && picker_cursor[0],
                  "shrinking lists erase old rows");
            check(match_phase == MATCH_PHASE_IDLE,
                  "stale CONNECT closes the lobby instead of silently retrying");
        } else if (mode == 3) {
            check(picker_clears == 1 && picker_rows == 1 && picker_connects == 0 &&
                      strcmp(picker_text[0], "NO HOSTS AVAILABLE") == 0,
                  "empty list shows one message and never joins");
        } else if (mode == 4) {
            check(wait_calls < MATCH_REPLY_FRAMES && picker_clears == 0,
                  "BREAK interrupts a busy controller wait");
        } else if (mode == 5) {
            check(picker_clears == 2 && picker_rows == 1 && picker_connects == 0,
                  "revision change during enumeration refreshes inside the same picker");
        } else if (mode == 12) {
            check(picker_connects == 0 && picker_clears == 0 && close_calls == 3,
                  "being selected by another guest closes without joining a stale choice");
        } else if (mode >= 10) {
            check(picker_connects == 1 && picker_clears == 1,
                  "one queued ENTER/SPACE survives poll or enumeration wait");
            check(picker_suppressed_key == (mode == 10 ? 13 : 32),
                  "queued activation suppresses its own key");
        } else if (mode >= 7) {
            check(picker_connects == 1 && picker_cursor[0] &&
                      picker_clears == (mode == 8 ? 3 : 2),
                  "empty-to-populated list selects first host and joins on activation");
            check(picker_suppressed_key == (mode == 8 ? 32 : 13),
                  "ENTER and SPACE suppress their own held key");
            check(picker_connecting && wait_calls == (mode == 8 ? 115 : (mode == 9 ? 60 : 65)),
                  "persistent CONNECTING covers the controller wait");
            if (mode == 9)
                check(match_phase == MATCH_PHASE_IDLE,
                      "rejected CONNECT closes even without a new revision");
        } else {
            check(picker_clears == 0 && picker_connects == 0 && close_calls == 3 &&
                      match_phase == MATCH_PHASE_IDLE,
                  "failed controller closes its socket without retrying the picker");
        }
        picker_mode = 0;
    }
    memset(&match_snapshot, 0, sizeof match_snapshot);
    strcpy(match_snapshot.device, "DEVICE");
    strcpy(match_snapshot.name, "HOST (W)");
    match_snapshot.requires_code = 1;
    {
        char line[29];
        check(!match_picker_label(line, MATCH_SNAPSHOT), "protected peers are not public Shatranj hosts");
        match_snapshot.requires_code = 0;
        strcpy(match_snapshot.name, "HOST (W) tail");
        check(!match_picker_label(line, MATCH_SNAPSHOT), "host suffix must be exact");
        strcpy(match_snapshot.name, "ABCDEFGHIJKLMNOPQRS (B)");
        check(match_picker_label(line, MATCH_SNAPSHOT) &&
                  strcmp(line + 1, "ABCDEFGHIJKLMNOPQRS   BLACK") == 0,
              "maximum host name keeps exact padding and colour columns");
        strcpy(match_snapshot.name, "A\1B (W)");
        check(match_picker_label(line, MATCH_SNAPSHOT) &&
                  strcmp(line + 1, "A?B                   WHITE") == 0,
              "short names pad the row and sanitize control characters");
    }
    netchesszx_link_match = 0;
}

static void test_match_join_outcomes(void)
{
    static const int16_t ready[] = { 0, 0 };
    static const int16_t connecting[] = { 2, SPXMATCH_BUSY, 2, 3 };
    static const int16_t failed_pair[] = { 2, 5 };
    static const int16_t stalled_pair[] = { 2, SPXMATCH_BUSY };
    static const int16_t terminal[] = { 1, 4, 5, SPXN_EROM, SPXMATCH_BUSY };
    uint8_t i;

    for (i = 0; i < 11; ++i) {
        uint16_t frames = picker_frames, clears = picker_clears;
        match_reset("GUEST", NETCHESSZX_COLOR_WHITE, ready, 2);
        netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
        match_info_initiator = match_info_serial;
        picker_mode = 0;
        spectrum_input_pending_event = 0;
        join_reply_index = 0;
        join_replies = i == 0 || i >= 6 ? connecting : terminal + i - 1;
        join_reply_count = i == 0 || i >= 6 ? 4 : 1;
        join_cancel_frame = i == 6 || i == 7 ? 1 : 0;
        if (i == 7) {
            join_replies = terminal + 4;
            join_reply_count = 1;
        }
        join_select_result = i == 8 ? SPXN_EROM : SPXN_OK;
        if (i == 9) {
            join_replies = failed_pair;
            join_reply_count = 2;
        }
        if (i == 10) {
            join_replies = stalled_pair;
            join_reply_count = 2;
        }
        match_saved_name[0] = MATCH_NAME_KEEP;
        MATCH_PICK_PENDING = 0;
        MATCH_PICK_SLOT = 7;
        MATCH_PICK_REV_LO = (char)0xcd;
        MATCH_PICK_REV_HI = (char)0xab;
        check(spectrum_net_join_host() ==
                  (i == 0 ? 1 : (i == 9 ? SPECTRUM_LINK_FAILED :
                   (i == 6 || i == 7 ? SPECTRUM_LINK_CANCELLED : 0))),
              "join handles success, rejection, controller error, timeout and BREAK");
        check(picker_index == 7 && picker_revision == 0xabcd,
              "join uses the selected slot and full 16-bit revision");
        check(picker_frames == frames && picker_clears == clears,
              "join never repaints the picker over the game panels");
        check(i == 0 ? match_phase == MATCH_PHASE_PAIRED :
                       match_phase == MATCH_PHASE_IDLE && close_calls == 1,
              "failed or cancelled join closes once; success preserves the stream");
        if (i == 5)
            check(wait_calls == MATCH_REPLY_FRAMES, "busy join has a bounded timeout");
        if (i == 10)
            check(wait_calls == WAIT_LONG + MATCH_REPLY_FRAMES - 1u,
                  "a stalled pairing exhausts one wait budget and closes");
        check(spectrum_overlay_context[SPECTRUM_OVL_CTX_DIRECT_UNMARK] == 0,
              "join clears its overlay action before ordinary DIRECT use");
    }
    join_replies = 0;
    join_cancel_frame = 0;
    join_select_result = SPXN_OK;
    spectrum_input_pending_event = 0;
    netchesszx_link_match = 0;
}

int main(void)
{
    test_preflight_status();
    test_mqtt_oversized_discard_above_buffer_capacity();
    test_direct_drain_and_accept();
    test_direct_match_guest_connects();
    test_direct_wait_timeout_keeps_listener();
    test_direct_wait_recovers_stale_listener();
    test_direct_wait_retries_failed_relisten();
    test_direct_wait_cancel_during_refused_listen();
    test_direct_pollin_receive();
    test_direct_fragmented_line();
    test_direct_coalesced_lines_fifo();
    test_direct_idle_yields();
    test_direct_nul_and_send();
    test_direct_nul_recovers_at_next_line();
    test_direct_read_classification();
    test_direct_queue_boundary();
    test_direct_background_drain();
    test_direct_send_outcomes();
    test_direct_active_link_zero();
    test_direct_reconnect_resets_state();
    test_match_host_registers();
    test_match_host_names();
    test_match_host_polls_then_connects();
    test_match_host_name_reply_is_not_a_stage();
    test_match_host_reregisters();
    test_match_host_closes_after_pair();
    test_match_host_cancel_busy();
    test_match_host_cancel_timeout();
    test_match_host_cancel_no_saved_name();
    test_match_host_serial_lookup_failure();
    test_match_host_saves_unmarked_name();
    test_match_guest_initiator_ok();
    test_match_guest_picked_by_guest();
    test_match_guest_no_strings();
    test_match_guest_unmarks_name();
    test_match_guest_remote_bye_retry();
    test_match_close();
    test_match_eof_closes_after_drain();
    test_match_picker();
    test_match_join_outcomes();
    test_direct_graceful_close();
    test_clock_timezone();
    test_clock_retry_is_polled_without_waiting();
    if (failures != 0) {
        fprintf(stderr, "%d Spectranext seam checks failed\n", failures);
        return 1;
    }
    puts("Spectranext link checks passed");
    return 0;
}
