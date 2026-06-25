#include "spectrum/transport/net.h"

#include "spectrum/config/session.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/esp_at.h"
#include "spectrum/lowram_map.h"
#include "spectrum/overlay/overlay.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/text.h"

static void net_copy(uint8_t *dst, const uint8_t *src, uint16_t len)
{
    while (len-- != 0u) { *dst++ = *src++; }
}

static void net_move(uint8_t *dst, const uint8_t *src, uint16_t len)
{
    if (dst < src) {
        while (len-- != 0u) { *dst++ = *src++; }
    } else {
        dst += len; src += len;
        while (len-- != 0u) { *--dst = *--src; }
    }
}

#define WAIT_MED 500
#define WAIT_POLL 2
#define MQTT_STREAM_BACKGROUND_DRAIN_BUDGET 16u
#define SPECTRUM_NET_MQTT_READ_TIMEOUT (-4)

#define mqtt_presence_payload ((char *)NETCHESSZX_LOWRAM_MQTT_PRESENCE_ADDR)
uint8_t active_link;
char direct_rx_payload[SPECTRUM_NET_PAYLOAD_MAX];
char direct_rx_payload2[SPECTRUM_NET_PAYLOAD_MAX];
uint8_t direct_rx_link;
uint8_t direct_rx_link2;
uint8_t direct_rx_count;
uint8_t direct_rx_head;
uint8_t direct_rx_payload_len;
uint16_t direct_ipd_remaining;
uint8_t direct_ipd_accept;
uint8_t direct_ipd_link;
uint8_t direct_link_closed;
static uint8_t net_link_activity;
static uint8_t net_payload_flags;

static void direct_reset_rx_state(void);
static uint8_t mqtt_drain_uart_budget(uint8_t budget) __z88dk_fastcall;

void spectrum_net_background_drain(void)
{
    if (!netchesszx_transport_is_mqtt()) {
        return;
    }
    (void)mqtt_drain_uart_budget(MQTT_STREAM_BACKGROUND_DRAIN_BUDGET);
}

uint8_t spectrum_net_link_activity(void)
{
    uint8_t activity = net_link_activity;

    net_link_activity = 0u;
    return activity;
}

uint8_t spectrum_net_payload_flags(void)
{
    return net_payload_flags;
}

char *spectrum_net_payload_scratch(void)
{
    return direct_rx_payload;
}

static void net_wait_frame_plain(void)
{
    spectrum_net_runtime_wait_frame_plain();
}

void spectrum_net_start_uart(void)
{
    spectrum_uart_init();
    spectrum_uart_flush(25u);
    direct_reset_rx_state();
    reset_line_buf();
}

static void direct_reset_rx_state(void)
{
    direct_rx_count = 0u;
    direct_rx_head = 0u;
    direct_rx_payload_len = 0u;
    direct_ipd_remaining = 0u;
    direct_ipd_accept = 0u;
    direct_ipd_link = 0u;
    direct_link_closed = 0u;
    reset_line_buf();
}


uint8_t spectrum_net_listen(void)
{
    direct_reset_rx_state();
    return spectrum_overlay_exec(SPECTRUM_OVL_DIRECT,
                                 SPECTRUM_OVL_DIRECT_LISTEN);
}

uint8_t spectrum_net_connect_host(void)
{
    direct_reset_rx_state();
    return spectrum_overlay_exec(SPECTRUM_OVL_DIRECT,
                                 SPECTRUM_OVL_DIRECT_CONNECT);
}

uint8_t spectrum_net_wait_pc_connect(void)
{
    /* Flush stale RX state from any previous session so a reconnect is accepted
       cleanly. Without this, leftover direct_rx_count / direct_link_closed /
       active_link from the dropped peer make the accept return instantly on the
       dead link, then bail, alternating DISCONNECTED/WAITING forever. */
    direct_reset_rx_state();
    return spectrum_overlay_exec(SPECTRUM_OVL_DIRECT,
                                 SPECTRUM_OVL_DIRECT_WAIT_CONNECT);
}

static int16_t direct_read_payload(char *payload, uint8_t payload_cap)
{
    uint16_t payload_addr = (uint16_t)payload;
    uint8_t link;

    /* payload_scratch() aliases direct_rx_payload; clearing here would wipe a
       payload queued during a blocking DIRECT send before the overlay reads it. */
    if (payload_cap != 0u && payload != direct_rx_payload) {
        payload[0] = '\0';
    }
    spectrum_overlay_context[0] = (uint8_t)payload_addr;
    spectrum_overlay_context[1] = (uint8_t)(payload_addr >> 8);
    spectrum_overlay_context[2] = payload_cap;
    link = spectrum_overlay_exec_cached(SPECTRUM_OVL_DIRECT,
                                         SPECTRUM_OVL_DIRECT_READ);
    if (link == 0u && payload_cap != 0u && payload[0] == '\0') {
        return SPECTRUM_NET_READ_TIMEOUT;
    }
    return (int16_t)(int8_t)link;
}

static uint8_t direct_send_text(const char *text) __z88dk_fastcall
{
    uint16_t text_addr = (uint16_t)text;

    spectrum_overlay_context[0] = (uint8_t)text_addr;
    spectrum_overlay_context[1] = (uint8_t)(text_addr >> 8);
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_DIRECT,
                                        SPECTRUM_OVL_DIRECT_SEND);
}

static uint16_t mqtt_next_id = 1u;
static uint8_t mqtt_stream_len;
static uint8_t mqtt_stream_active;
static uint8_t mqtt_pending_len;
#define MQTT_STREAM_MAX 223u
#define MQTT_STREAM_BASE 0x6432u
#define MQTT_STREAM_DRAIN_BUDGET 64u
#if MQTT_STREAM_MAX < (SPECTRUM_MQTT_PACKET_MAX + MQTT_STREAM_DRAIN_BUDGET - 1u)
#error "MQTT_STREAM_MAX must hold one near-complete MQTT packet plus one UART drain"
#endif
#if SPECTRUM_MQTT_SCRATCH_BASE < (MQTT_STREAM_BASE + MQTT_STREAM_MAX)
#error "MQTT scratch overlaps mqtt_stream"
#endif
#if (SPECTRUM_MQTT_SCRATCH_BASE + SPECTRUM_MQTT_PACKET_MAX) > 0x6800u
#error "MQTT scratch overlaps overlay slot"
#endif
static __at(MQTT_STREAM_BASE) uint8_t mqtt_stream[MQTT_STREAM_MAX];
#define MQTT_PACKET SPECTRUM_MQTT_PACKET_SCRATCH
#define MQTT_STREAM mqtt_stream

#define MQTT_SUFFIX_OUT 0u
#define MQTT_SUFFIX_IN 2u
#define MQTT_SUFFIX_OUT_ACK 4u
#define MQTT_SUFFIX_IN_ACK 6u
#define MQTT_SUFFIX_PRESENCE 8u
#define MQTT_SUFFIX_PEER_PRESENCE 10u
#define MQTT_SUFFIX_CLIENT_SIDE 12u

static const char mqtt_suffix_w2b[] = "w2b";
static const char mqtt_suffix_b2w[] = "b2w";
static const char mqtt_suffix_ack_b[] = "ack_b";
static const char mqtt_suffix_ack_w[] = "ack_w";
static const char mqtt_suffix_pres_w[] = "pres_w";
static const char mqtt_suffix_pres_b[] = "pres_b";
static const char mqtt_suffix_client_w[] = "-W";
static const char mqtt_suffix_client_b[] = "-B";

static const char *const mqtt_side_suffixes[] = {
    mqtt_suffix_w2b, mqtt_suffix_b2w,
    mqtt_suffix_b2w, mqtt_suffix_w2b,
    mqtt_suffix_ack_b, mqtt_suffix_ack_w,
    mqtt_suffix_ack_w, mqtt_suffix_ack_b,
    mqtt_suffix_pres_w, mqtt_suffix_pres_b,
    mqtt_suffix_pres_b, mqtt_suffix_pres_w,
    mqtt_suffix_client_w, mqtt_suffix_client_b
};

static void mqtt_reset_session_state(void);

static const char *mqtt_side_suffix(uint8_t pair_index) __z88dk_fastcall
{
    return mqtt_side_suffixes[(uint8_t)(pair_index +
        (netchesszx_local_is_white() ? 0u : 1u))];
}

const char *spectrum_net_mqtt_out_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_OUT);
}

const char *spectrum_net_mqtt_in_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_IN);
}

const char *spectrum_net_mqtt_out_ack_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_OUT_ACK);
}

const char *spectrum_net_mqtt_in_ack_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_IN_ACK);
}

const char *spectrum_net_mqtt_presence_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_PRESENCE);
}

const char *spectrum_net_mqtt_peer_presence_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_PEER_PRESENCE);
}

const char *spectrum_net_mqtt_client_side_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_CLIENT_SIDE);
}

const char *spectrum_net_mqtt_presence_payload(void)
{
    char *p = spectrum_append_text(mqtt_presence_payload,
                                   netchesszx_local_is_white() ?
                                       "O W " : "O B ");
    (void)spectrum_append_u16(p, netchesszx_mqtt_session_id);
    return mqtt_presence_payload;
}

void spectrum_net_mqtt_setup_payload(char *out) NETCHESSZX_FASTCALL
{
    char *p;

    if (netchesszx_session_is_host()) {
        p = spectrum_append_text(out, "H ");
        *p++ = netchesszx_local_side_char();
        *p++ = ' ';
        (void)spectrum_append_u16(p, netchesszx_mqtt_session_id);
        return;
    }
    p = spectrum_append_text(out, "J ");
    (void)spectrum_append_u16(p, netchesszx_mqtt_session_id);
}

uint8_t spectrum_net_mqtt_publish_session(void)
{
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_PUBLISH_SESSION);
}

uint8_t spectrum_net_mqtt_publish_setup(uint8_t retain) NETCHESSZX_FASTCALL
{
    spectrum_overlay_context[0] = retain;
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_PUBLISH_SETUP);
}

uint8_t spectrum_net_mqtt_activate_side(void)
{
    return spectrum_overlay_exec(SPECTRUM_OVL_MQTT_CONNECT,
                                 SPECTRUM_OVL_MQTT_CONNECT_ACTIVATE);
}

uint8_t mqtt_enter_stream_mode(void)
{
    mqtt_stream_active = 0u;
    mqtt_stream_len = 0u;

    if (!spectrum_net_at_cmd("AT+CIPMODE=1", WAIT_MED)) {
        return 0u;
    }

    reset_line_buf();
    spectrum_uart_send_string("AT+CIPSEND");
    spectrum_uart_send_crlf();
    if (!wait_for_prompt(WAIT_MED)) {
        return 0u;
    }

    reset_line_buf();
    mqtt_stream_active = 1u;
    return 1u;
}

void mqtt_abort_stream_mode(void)
{
    mqtt_stream_active = 0u;
    mqtt_stream_len = 0u;
}

static uint8_t mqtt_stream_put(uint8_t c) __z88dk_fastcall
{
    if (mqtt_stream_len < MQTT_STREAM_MAX) {
        MQTT_STREAM[mqtt_stream_len++] = c;
        return 1u;
    }
    return 0u;
}

static uint8_t mqtt_drain_uart_budget(uint8_t budget) __z88dk_fastcall
{
    uint8_t any = 0u;

    if (!mqtt_stream_active) {
        return 0u;
    }

    while (budget-- != 0u && spectrum_uart_ready()) {
        any = 1u;
        if (!mqtt_stream_put(spectrum_uart_read())) {
            return 2u;
        }
    }

    return any;
}

static uint8_t mqtt_fill_stream(uint16_t frames) __z88dk_fastcall
{
    if (!mqtt_stream_active) {
        return 0u;
    }

    while (frames-- != 0u) {
        uint8_t drained = mqtt_drain_uart_budget(MQTT_STREAM_DRAIN_BUDGET);

        if (drained != 0u) {
            return drained == 2u ? 0u : 1u;
        }
        net_wait_frame_plain();
    }
    return 0u;
}

static int16_t mqtt_take_stream_packet(uint8_t *packet, uint16_t cap)
{
    uint16_t remaining;
    uint16_t total;
    uint8_t header_len;
    uint8_t b;
    uint8_t type;

retry:
    if (mqtt_stream_len < 2u) {
        return 0;
    }

    type = (uint8_t)(MQTT_STREAM[0u] >> 4);
    if (!((MQTT_STREAM[0u] == 0x20u && type == SPECTRUM_MQTT_CONNACK) ||
          ((MQTT_STREAM[0u] & 0xf0u) == 0x30u && type == SPECTRUM_MQTT_PUBLISH) ||
          (MQTT_STREAM[0u] == 0x40u && type == SPECTRUM_MQTT_PUBACK) ||
          (MQTT_STREAM[0u] == 0x90u && type == SPECTRUM_MQTT_SUBACK) ||
          (MQTT_STREAM[0u] == 0xd0u && type == SPECTRUM_MQTT_PINGRESP))) {
        --mqtt_stream_len;
        if (mqtt_stream_len != 0u) {
            net_move(MQTT_STREAM, MQTT_STREAM + 1u, mqtt_stream_len);
        }
        goto retry;
    }

    b = (uint8_t)MQTT_STREAM[1u];
    remaining = (uint16_t)(b & 0x7fu);
    header_len = 2u;
    if ((b & 0x80u) != 0u) {
        if (mqtt_stream_len < 3u) {
            return 0;
        }
        b = (uint8_t)MQTT_STREAM[2u];
        if ((b & 0x80u) != 0u) {
            mqtt_stream_len = 0u;
            return -1;
        }
        remaining = (uint16_t)(remaining + ((uint16_t)(b & 0x7fu) << 7));
        header_len = 3u;
    }
    if (remaining > cap || (uint16_t)(header_len + remaining) > cap) {
        mqtt_stream_len = 0u;
        return -1;
    }
    total = (uint16_t)(header_len + remaining);
    if (mqtt_stream_len < total) {
        return 0;
    }

    if (total > cap) {
        mqtt_stream_len = 0u;
        return -1;
    }
    net_copy(packet, MQTT_STREAM, total);
    mqtt_stream_len = (uint8_t)(mqtt_stream_len - total);
    if (mqtt_stream_len != 0u) {
        net_move(MQTT_STREAM, MQTT_STREAM + total, mqtt_stream_len);
    }
    return total;
}

static int16_t mqtt_read_stream_packet(uint8_t *packet, uint16_t cap, uint16_t frames)
{
    int16_t got = mqtt_take_stream_packet(packet, cap);

    if (got != 0) {
        return got;
    }
    if (!mqtt_fill_stream(frames)) {
        return SPECTRUM_NET_MQTT_READ_TIMEOUT;
    }
    got = mqtt_take_stream_packet(packet, cap);
    return got == 0 ? SPECTRUM_NET_MQTT_READ_TIMEOUT : got;
}

static int16_t mqtt_read_live_packet(uint8_t *packet, uint16_t cap, uint16_t frames)
{
    uint8_t pending = mqtt_pending_len;

    if (pending != 0u) {
        mqtt_pending_len = 0u;
        if (pending > cap) {
            return -1;
        }
        net_copy(packet, MQTT_PACKET, pending);
        return pending;
    }
    return mqtt_read_stream_packet(packet, cap, frames);
}

static void mqtt_stash_packet(const uint8_t *packet, uint8_t len)
{
    if (mqtt_pending_len == 0u) {
        net_copy(MQTT_PACKET, packet, len);
        mqtt_pending_len = len;
    }
}

uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len)
{
    if (mqtt_stream_active) {
        /* Transparent (CIPMODE=1) writes give no synchronous TCP status, so a
           send cannot observe a dropped link. Broker/link loss is detected on
           the read side: the app loop pings and declares the peer gone after
           MQTT_PING_MISSES_MAX missing PINGRESPs. */
        spectrum_uart_send_bytes(packet, len);
        return 1u;
    }
    return 0u;
}

static uint8_t mqtt_send_packet(uint8_t len) __z88dk_fastcall
{
    return mqtt_send_raw_packet(MQTT_PACKET, len);
}

static void mqtt_puback_id(uint16_t packet_id) __z88dk_fastcall
{
    if (packet_id == 0u) {
        return;
    }
    MQTT_PACKET[0u] = 0x40u;
    MQTT_PACKET[1u] = 0x02u;
    MQTT_PACKET[2u] = (uint8_t)(packet_id >> 8);
    MQTT_PACKET[3u] = (uint8_t)packet_id;
    (void)mqtt_send_packet(4u);
}

uint8_t mqtt_wait_packet_into(uint8_t *packet, uint8_t wanted, uint16_t frames)
{
    while (frames-- != 0u) {
        int16_t got = mqtt_read_stream_packet(packet,
                                              SPECTRUM_MQTT_PACKET_MAX,
                                              WAIT_POLL);

        if (got == SPECTRUM_NET_MQTT_READ_TIMEOUT) {
            net_wait_frame();
            continue;
        }
        if (got <= 0) {
            return 0u;
        }
        {
            uint8_t type = spectrum_mqtt_type(packet, (uint16_t)got);

            if (wanted == 0u || type == wanted) {
                return 1u;
            }
            if (mqtt_pending_len != 0u ||
                (uint16_t)got > SPECTRUM_MQTT_PACKET_MAX) {
                return 0u;
            }
            mqtt_stash_packet(packet, (uint8_t)got);
        }
    }
    return 0u;
}

static void mqtt_reset_session_state(void)
{
    mqtt_stream_len = 0u;
    mqtt_stream_active = 0u;
    mqtt_pending_len = 0u;
    net_payload_flags = 0u;
    mqtt_next_id = 1u;
}

uint8_t spectrum_net_mqtt_start(void)
{
    mqtt_reset_session_state();
    return spectrum_overlay_exec(SPECTRUM_OVL_MQTT_CONNECT,
                                 SPECTRUM_OVL_MQTT_CONNECT_START);
}

int16_t spectrum_net_mqtt_read_payload(char *payload, uint8_t payload_cap)
{
    uint16_t packet_id = 0u;
    int16_t got;
    uint8_t retained;

    net_payload_flags = 0u;

    got = mqtt_read_live_packet(MQTT_PACKET, SPECTRUM_MQTT_PACKET_MAX, WAIT_POLL);
    if (got == SPECTRUM_NET_MQTT_READ_TIMEOUT) {
        return SPECTRUM_NET_READ_TIMEOUT;
    }
    if (got <= 0) {
        return -2;
    }

    if (spectrum_mqtt_type(MQTT_PACKET, (uint16_t)got) == SPECTRUM_MQTT_PINGRESP &&
        (uint16_t)got <= 2u) {
        net_link_activity = 1u;
        return SPECTRUM_NET_READ_TIMEOUT;
    }

    if (spectrum_mqtt_type(MQTT_PACKET, (uint16_t)got) !=
        SPECTRUM_MQTT_PUBLISH) {
        return SPECTRUM_NET_READ_TIMEOUT;
    }

    got = spectrum_mqtt_parse_publish(MQTT_PACKET,
                                       (uint16_t)got,
                                       payload,
                                       payload_cap,
                                       &packet_id,
                                       &retained);
    if (got < 0) {
        mqtt_puback_id(packet_id);
        return SPECTRUM_NET_READ_TIMEOUT;
    }
    mqtt_puback_id(packet_id);
    if (retained) {
        net_payload_flags |= SPECTRUM_NET_PAYLOAD_RETAINED;
    }
    return 0;
}

uint8_t spectrum_net_mqtt_send_text(const char *text) NETCHESSZX_FASTCALL
{
    uint16_t text_addr = (uint16_t)text;

    spectrum_overlay_context[0] = (uint8_t)text_addr;
    spectrum_overlay_context[1] = (uint8_t)(text_addr >> 8);
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_SEND_TEXT);
}

static uint8_t spectrum_net_mqtt_send_ping(void)
{
    MQTT_PACKET[0u] = 0xc0u;
    MQTT_PACKET[1u] = 0u;
    return mqtt_send_raw_packet(MQTT_PACKET, 2u);
}

int16_t spectrum_net_read_payload(char *payload, uint8_t payload_cap)
{
    net_payload_flags = 0u;
    if (netchesszx_transport_is_mqtt()) {
        return spectrum_net_mqtt_read_payload(payload, payload_cap);
    }
    return direct_read_payload(payload, payload_cap);
}

uint8_t spectrum_net_send_text(const char *text)
{
    spectrum_net_background_drain();
    if (netchesszx_transport_is_mqtt()) {
        return spectrum_net_mqtt_send_text(text);
    }
    return direct_send_text(text);
}

uint8_t spectrum_net_send_ping(void)
{
    spectrum_net_background_drain();
    if (netchesszx_transport_is_mqtt()) {
        return spectrum_net_mqtt_send_ping();
    }
    return direct_send_text("PING");
}
