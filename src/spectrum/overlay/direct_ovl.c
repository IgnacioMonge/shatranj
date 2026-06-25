#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/net.h"
#include <stddef.h>

#define WAIT_MED 500
#define WAIT_LONG 2000
#define WAIT_POLL 2
#define WAIT_DIRECT_PROMPT 50
#define WAIT_DIRECT_SEND_OK 150
#define DIRECT_SEND_GUARD_FRAMES 4u
#define DIRECT_KEY_CANCEL 0x8au

#define DIRECT_CIPSTART_MAX_TEXT \
    (19u + NETCHESSZX_DIRECT_HOST_MAX + 2u + 5u)
#if SPECTRUM_NET_LINE_MAX <= DIRECT_CIPSTART_MAX_TEXT
#error "SPECTRUM_NET_LINE_MAX too small for AT+CIPSTART"
#endif

extern char line_buf[];
extern char direct_rx_payload[];
extern char direct_rx_payload2[];
extern uint8_t direct_rx_link;
extern uint8_t direct_rx_link2;
extern uint8_t active_link;
extern uint8_t line_pos;
extern uint8_t direct_rx_count;
extern uint8_t direct_rx_head;
extern uint8_t direct_rx_payload_len;
extern uint16_t direct_ipd_remaining;
extern uint8_t direct_ipd_accept;
extern uint8_t direct_ipd_link;
extern uint8_t direct_link_closed;
extern void reset_line_buf(void);
extern void net_wait_frame(void);

#define direct_digit_value(c) ((uint8_t)((uint8_t)(c) - (uint8_t)'0'))
#define direct_is_digit(c) (direct_digit_value(c) <= 9u)
#define direct_is_link_digit(c) (direct_digit_value(c) <= 4u)

struct direct_read_ctx {
    char *payload;
    uint8_t payload_cap;
};

struct direct_send_ctx {
    const char *text;
};

#define DIRECT_STATIC_ASSERT(name, cond) \
    typedef char direct_static_assert_##name[(cond) ? 1 : -1]

DIRECT_STATIC_ASSERT(read_payload_offset,
                     offsetof(struct direct_read_ctx, payload) == 0u);
DIRECT_STATIC_ASSERT(read_payload_cap_offset,
                     offsetof(struct direct_read_ctx, payload_cap) == 2u);
DIRECT_STATIC_ASSERT(read_ctx_size, sizeof(struct direct_read_ctx) == 3u);
DIRECT_STATIC_ASSERT(send_text_offset,
                     offsetof(struct direct_send_ctx, text) == 0u);
DIRECT_STATIC_ASSERT(send_ctx_size, sizeof(struct direct_send_ctx) == 2u);

static uint8_t direct_queue_payload_ovl(void)
{
    char *slot_payload;
    uint8_t slot;

    if (direct_rx_payload_len == 0u) {
        return 1u;
    }
    if (direct_rx_count >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
        /* Queue is full; drop the extra burst rather than declaring the link
           dead. The sender will recover through duplicate MOVE/START ACKs. */
        direct_rx_payload_len = 0u;
        return 1u;
    }

    slot = (uint8_t)((direct_rx_head + direct_rx_count) & 1u);
    slot_payload = slot ? direct_rx_payload2 : direct_rx_payload;
    slot_payload[direct_rx_payload_len] = '\0';
    direct_rx_payload_len = 0u;
    if (slot) {
        direct_rx_link2 = direct_ipd_link;
    } else {
        direct_rx_link = direct_ipd_link;
    }
    ++direct_rx_count;
    return 1u;
}

static uint8_t direct_parse_ipd_header_ovl(void)
{
    char *p = line_buf + 5;
    uint16_t first = 0u;
    uint16_t len = 0u;
    uint8_t link = 0u;
    uint8_t seen = 0u;

    while (direct_is_digit((uint8_t)*p)) {
        first = (uint16_t)((first << 3) + (first << 1) +
                           direct_digit_value((uint8_t)*p));
        seen = 1u;
        ++p;
    }
    if (!seen) {
        return 0u;
    }

    if (*p == ',') {
        link = (uint8_t)first;
        ++p;
        seen = 0u;
        while (direct_is_digit((uint8_t)*p)) {
            len = (uint16_t)((len << 3) + (len << 1) +
                             direct_digit_value((uint8_t)*p));
            seen = 1u;
            ++p;
        }
        if (!seen) {
            return 0u;
        }
    } else {
        len = first;
    }

    if (*p != ':') {
        return 0u;
    }

    direct_ipd_accept = 1u;
    if (active_link != 0xffu) {
        if (link != active_link) {
            /* Data on a different link id means the previous peer is gone and a
               new one has connected. Drop this burst and flag the link closed so
               the game loop exits and re-accepts the fresh connection instead of
               staying stuck on the stale session. */
            direct_ipd_accept = 0u;
            direct_link_closed = 1u;
        }
    }

    direct_ipd_link = link;
    direct_ipd_remaining = len;
    reset_line_buf();
    return 1u;
}

static uint8_t direct_feed_payload_byte_ovl(uint8_t c)
{
    uint8_t queued = 0u;

    --direct_ipd_remaining;
    if (direct_ipd_accept) {
        if (c == '\r') {
            /* ignore */
        } else if (c == '\n') {
            queued = direct_queue_payload_ovl();
        } else if (direct_rx_count >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
            /* Drop a burst that arrives before queued payloads are consumed. */
        } else if (direct_rx_payload_len < (SPECTRUM_NET_PAYLOAD_MAX - 1u)) {
            char *slot_payload =
                ((direct_rx_head + direct_rx_count) & 1u) ? direct_rx_payload2
                                                          : direct_rx_payload;
            slot_payload[direct_rx_payload_len++] = (char)c;
        } else {
            direct_link_closed = 1u;
        }

    }

    if (direct_ipd_remaining == 0u) {
        if (direct_ipd_accept && !direct_link_closed &&
            direct_rx_payload_len != 0u) {
            queued = direct_queue_payload_ovl();
        }
        direct_ipd_accept = 0u;
        reset_line_buf();
    }
    if (direct_link_closed) {
        return 3u;
    }
    return queued ? 2u : 0u;
}

static uint8_t direct_feed_uart_byte_ovl(uint8_t c)
{
    if (direct_ipd_remaining != 0u) {
        return direct_feed_payload_byte_ovl(c);
    }

    if (c == '>') {
        return 4u;
    }

    if (c == '\r') {
        return 0u;
    }

    if (c == '\n') {
        if (line_pos == SPECTRUM_NET_LINE_MAX) {
            reset_line_buf();
            return 0u;
        }
        if (line_pos != 0u) {
            line_buf[line_pos] = '\0';
            line_pos = 0u;
            if ((line_buf[0] == 'C' && line_buf[1] == 'L') ||
                (line_buf[2] == 'C' && line_buf[3] == 'L') ||
                (line_buf[0] == 'E' && line_buf[1] == 'R') ||
                (line_buf[0] == 'U' && line_buf[1] == 'N')) {
                direct_link_closed = 1u;
                return 3u;
            }
            return 1u;
        }
        return 0u;
    }

    if (line_pos < (SPECTRUM_NET_LINE_MAX - 1u)) {
        line_buf[line_pos++] = (char)c;
        line_buf[line_pos] = '\0';
    } else {
        line_pos = SPECTRUM_NET_LINE_MAX;
    }

    if (c == ':' &&
        line_buf[0] == '+' && line_buf[1] == 'I' &&
        line_buf[2] == 'P' && line_buf[3] == 'D' &&
        line_buf[4] == ',') {
        if (direct_parse_ipd_header_ovl()) {
            return 0u;
        }
        reset_line_buf();
    }

    return 0u;
}

static uint8_t direct_drain_uart_ovl(uint16_t frames)
{
    while (frames-- != 0u) {
        while (spectrum_uart_ready()) {
            uint8_t rc = direct_feed_uart_byte_ovl(spectrum_uart_read());

            if (rc != 0u) {
                return rc;
            }
        }
        net_wait_frame();
    }
    return 0u;
}

static uint8_t direct_wait_for_ok_ovl(uint16_t frames, uint8_t cancellable)
{
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl(1u);

        if (cancellable && spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            return SPECTRUM_LINK_CANCELLED;
        }
        if (rc == 3u) {
            return 0u;
        }
        if (rc != 1u) {
            continue;
        }
        if (line_buf[0] == 'O' && line_buf[1] == 'K') {
            return 1u;
        }
        if (line_buf[0] == 'S' && line_buf[1] == 'E' &&
            line_buf[5] == 'O') {
            return 1u;
        }
        if (line_buf[0] == 'E' || line_buf[0] == 'F') {
            return 0u;
        }
        net_wait_frame();
    }
    return 0u;
}

static uint8_t direct_wait_for_prompt_ovl(uint16_t frames)
{
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl(1u);

        if (rc == 4u) {
            return 1u;
        }
        if (rc == 3u) {
            return 0u;
        }
    }
    return 0u;
}

static void direct_send_linebuf_ovl(void)
{
    spectrum_uart_send_string(line_buf);
    reset_line_buf();
    spectrum_uart_send_crlf();
}

static uint8_t direct_tcp_connect_ovl(void)
{
    (void)spectrum_append_u16(
        spectrum_append_text(
            spectrum_append_text(
                spectrum_append_text(line_buf, "AT+CIPSTART=\"TCP\",\""),
                netchesszx_direct_host),
            "\","),
        netchesszx_direct_port);
    direct_send_linebuf_ovl();
    return direct_wait_for_ok_ovl(WAIT_LONG, 1u);
}

static uint8_t direct_prepare_link_ovl(void)
{
    if (!spectrum_net_ensure_command_mode()) {
        return 0u;
    }
    direct_link_closed = 0u;
    reset_line_buf();
    return 1u;
}

uint8_t direct_listen_ovl(void)
{
    active_link = 0u;

    if (!direct_prepare_link_ovl()) {
        return 0u;
    }

    if (!spectrum_net_at_cmd("AT+CIPMUX=1", WAIT_MED)) {
        return 0u;
    }

    (void)spectrum_append_u16(spectrum_append_text(line_buf, "AT+CIPSERVER=1,"),
                              netchesszx_direct_port);
    direct_send_linebuf_ovl();
    if (!direct_wait_for_ok_ovl(WAIT_MED, 0u)) {
        return 0u;
    }

    spectrum_net_publish_ip_status();
    return 1u;
}

uint8_t direct_connect_ovl(void)
{
    active_link = 0xffu;
    if (!direct_prepare_link_ovl()) {
        return 0u;
    }
    return direct_tcp_connect_ovl();
}

uint8_t direct_wait_pc_connect_ovl(void)
{
    uint16_t frames = WAIT_LONG;

    if (direct_rx_count != 0u) {
        active_link = direct_rx_head ? direct_rx_link2 : direct_rx_link;
        return 1u;
    }
    active_link = 0xffu;
    while (frames-- != 0u) {
        uint8_t rc = direct_drain_uart_ovl(1u);

        if (spectrum_key_poll() == DIRECT_KEY_CANCEL) {
            return SPECTRUM_LINK_CANCELLED;
        }
        if (rc == 2u) {
            active_link = direct_rx_head ? direct_rx_link2 : direct_rx_link;
            return 1u;
        }
        if (rc == 3u) {
            active_link = 0u;
            return 0u;
        }
        if (rc == 1u && direct_is_link_digit(line_buf[0]) &&
            line_buf[1] == ',' && line_buf[2] == 'C' &&
            line_buf[3] == 'O') {
            active_link = direct_digit_value(line_buf[0]);
            return 1u;
        }
        net_wait_frame();
    }

    return 0u;
}

uint8_t direct_read_payload_ovl(struct direct_read_ctx *ctx) __z88dk_fastcall
{
    char *payload = ctx->payload;
    uint8_t payload_cap = ctx->payload_cap;
    uint8_t link;
    uint8_t n;

    if (direct_link_closed) {
        direct_link_closed = 0u;
        return 0xfeu;
    }
    if (direct_rx_count == 0u) {
        (void)direct_drain_uart_ovl(WAIT_POLL);
        if (direct_link_closed) {
            direct_link_closed = 0u;
            return 0xfeu;
        }
    }
    if (direct_rx_count != 0u) {
        char *slot_payload = direct_rx_head ? direct_rx_payload2 : direct_rx_payload;

        n = 0u;
        if (payload_cap != 0u) {
            while (slot_payload[n] != '\0' && n + 1u < payload_cap) {
                payload[n] = slot_payload[n];
                ++n;
            }
            payload[n] = '\0';
        }
        link = direct_rx_head ? direct_rx_link2 : direct_rx_link;
        --direct_rx_count;
        direct_rx_head ^= 1u;
        if (direct_rx_count == 0u) {
            direct_rx_head = 0u;
        }
        direct_rx_payload_len = 0u;
        return link;
    }
    return (uint8_t)SPECTRUM_NET_READ_TIMEOUT;
}

uint8_t direct_send_text_ovl(struct direct_send_ctx *ctx) __z88dk_fastcall
{
    const char *text = ctx->text;
    char *cmd;
    uint8_t payload_len;
    uint8_t len;

    payload_len = 0u;
    while (text[payload_len] != '\0' &&
           payload_len < (SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP - 2u)) {
        ++payload_len;
    }
    len = (uint8_t)(payload_len + 1u);

    spectrum_net_guard_wait(DIRECT_SEND_GUARD_FRAMES);

    cmd = spectrum_append_text(line_buf, "AT+CIPSEND=");
    if (active_link != 0xffu) {
        cmd = spectrum_append_text(spectrum_append_u16(cmd, active_link), ",");
    }
    (void)spectrum_append_u16(cmd, len);
    direct_send_linebuf_ovl();

    if (!direct_wait_for_prompt_ovl(WAIT_DIRECT_PROMPT)) {
        return 0u;
    }

    reset_line_buf();
    spectrum_uart_send_bytes((const uint8_t *)text, payload_len);
    spectrum_uart_send_bytes((const uint8_t *)"\n", 1u);
    return direct_wait_for_ok_ovl(WAIT_DIRECT_SEND_OK, 0u);
}
