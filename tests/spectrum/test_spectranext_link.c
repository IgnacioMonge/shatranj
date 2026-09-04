#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_SPECTRANEXT 1
#define __z88dk_fastcall
#define __z88dk_callee
#define __at(address)

#define SPXN_OK          0
#define SPXN_EROM       -1
#define SPXN_EINVAL     -6
#define SPXN_ESENDSTALL -8
#define SPXN_POLLCON     1
#define SPXN_POLLHUP     2
#define SPXN_POLLIN      4
#define SPXN_POLLNVAL  128

#define SCRIPT_MAX 16u

struct spxn_status {
    uint8_t controller;
    uint8_t wifi;
    uint8_t ip4host[4];
};

struct spxtime_result {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t fat_date;
    uint16_t fat_time;
};

struct spxtime_request {
    const char *host;
    uint8_t *scratch;
    uint16_t scratch_size;
    uint16_t timeout_ticks;
    struct spxtime_result *out;
};

struct spxudp_endpoint {
    uint8_t ip4be[4];
    uint16_t port;
    uint16_t local_port;
};

struct spxudp_socket {
    uint8_t fd;
    uint8_t open;
    uint8_t poll_in;
};

#define SPXTIME_SNTP_PACKET_SIZE 48u
#define SPXTIME_SNTP_PORT 123u

int16_t spxn_status(struct spxn_status *out);
int16_t spxn_detect(void);
int16_t spxn_poll(void);
int16_t spxn_recv(void *buffer, uint16_t maximum);
int16_t spxn_accept(void);
int16_t spxn_listen(uint16_t port);
int16_t spxn_resolve(const char *host, uint8_t *ip4be);
int16_t spxn_connect(const uint8_t *ip4be, uint16_t port);
void spxn_close(void);
int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget);
int16_t spxtime_sntp(const struct spxtime_request *request);
int16_t spxtime_parse_sntp(const uint8_t *packet, uint16_t length,
                           struct spxtime_result *out);
int16_t spxudp_open(struct spxudp_socket *socket);
int16_t spxudp_sendto(struct spxudp_socket *socket, const void *buffer,
                      uint16_t length,
                      const struct spxudp_endpoint *destination);
int16_t spxudp_poll(struct spxudp_socket *socket);
int16_t spxudp_recvfrom(struct spxudp_socket *socket, void *buffer,
                        uint16_t maximum,
                        struct spxudp_endpoint *source);
void spxudp_close(struct spxudp_socket *socket);

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
int8_t netchesszx_timezone = 0;
int8_t netchesszx_timezone_last = 0;
uint8_t netchesszx_rtc_available;

static int failures;
static uint16_t wait_calls;
static uint8_t close_calls;
static uint8_t listen_calls;
static int16_t listen_result;
static uint8_t accept_calls;
static uint8_t send_all_calls;
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

static void check(int condition, const char *label)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", label);
    }
}

void spectrum_net_runtime_wait_frame_plain(void)
{
    ++wait_calls;
}

void spectrum_net_runtime_wait_frame(void)
{
    ++wait_calls;
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
    return 0u;
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
        entry_id == SPECTRUM_OVL_DIRECT_LISTEN) {
        return direct_listen_ovl();
    }
    if (overlay_id == SPECTRUM_OVL_DIRECT &&
        entry_id == SPECTRUM_OVL_DIRECT_CONNECT) {
        return direct_connect_ovl();
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

int16_t spxn_accept(void)
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

int16_t spxn_resolve(const char *host, uint8_t *ip4be)
{
    (void)host;
    ip4be[0] = 127u;
    ip4be[1] = 0u;
    ip4be[2] = 0u;
    ip4be[3] = 1u;
    return SPXN_OK;
}

int16_t spxn_connect(const uint8_t *ip4be, uint16_t port)
{
    (void)ip4be;
    (void)port;
    return SPXN_OK;
}

void spxn_close(void)
{
    ++close_calls;
}

int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget)
{
    const char *text = (const char *)buffer;
    int16_t result = SPXN_OK;

    (void)zero_budget;
    ++send_all_calls;
    if (send_all_calls == 1u) {
        check(length == 5u && memcmp(text, "HELLO", 5u) == 0,
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
    direct_link_closed = 0u;
    active_link = 0u;
    close_calls = 0u;
    listen_calls = 0u;
    listen_result = SPXN_OK;
    accept_calls = 0u;
    wait_calls = 0u;
    set_socket_script(data, length, polls, poll_count);
    send_all_calls = 0u;
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
    check(direct_wait_pc_connect_ovl() == 0u && close_calls == 1u &&
              listen_calls == 1u && accept_calls == 0u,
          "DIRECT reports an unsuccessful listener recreation as idle");

    listen_result = SPXN_OK;
    set_socket_script(0, 0u, late_connect, 1u);
    check(direct_wait_pc_connect_ovl() == 1u && close_calls == 2u &&
              listen_calls == 2u && accept_calls == 1u,
          "DIRECT retries a failed listener recreation on the next batch");
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
    check(direct_read_host(output, sizeof(output)) ==
              (uint8_t)SPECTRUM_NET_READ_TIMEOUT &&
              direct_rx_payload_len == 0u && direct_rx_discard == 0u &&
              wait_calls == 1u && poll_calls == 2u && recv_calls == 1u,
          "DIRECT full queue drops overflow without a ghost payload");
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

    spectrum_net_start_uart();
    check(direct_rx_count == 0u && direct_rx_head == 0u &&
              direct_rx_payload_len == 0u && direct_rx_discard == 0u &&
              direct_link_closed == 0u && close_calls == 1u,
          "DIRECT close/reset clears queued and closure state");

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

int main(void)
{
    test_preflight_status();
    test_mqtt_oversized_discard_above_buffer_capacity();
    test_direct_drain_and_accept();
    test_direct_wait_timeout_keeps_listener();
    test_direct_wait_recovers_stale_listener();
    test_direct_wait_retries_failed_relisten();
    test_direct_pollin_receive();
    test_direct_fragmented_line();
    test_direct_coalesced_lines_fifo();
    test_direct_idle_yields();
    test_direct_nul_and_send();
    test_direct_nul_recovers_at_next_line();
    test_direct_read_classification();
    test_direct_queue_boundary();
    test_direct_send_outcomes();
    test_direct_active_link_zero();
    test_direct_reconnect_resets_state();
    test_clock_timezone();
    test_clock_retry_is_polled_without_waiting();
    if (failures != 0) {
        fprintf(stderr, "%d Spectranext seam checks failed\n", failures);
        return 1;
    }
    puts("Spectranext link checks passed");
    return 0;
}
