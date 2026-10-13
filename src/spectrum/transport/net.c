#include "spectrum/transport/net.h"

#include "spectrum/session/identity.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/transport/mqtt_min.h"
#ifdef NETCHESSZX_SPECTRANEXT
#ifndef NETCHESSZX_HOST_TEST
#include "spxn.h"
#include "spxn_stream.h"
#include "spxtime.h"
#endif
#else
#include "spectrum/transport/esp_at.h"
#endif
#include "spectrum/lowram_map.h"
#include "spectrum/overlay/overlay.h"
#include "spectrum/platform/platform.h"
#include "spectrum/session/timing.h"
#ifdef NETCHESSZX_SPECTRANEXT
#include "spectrum/platform/text.h"
#endif

#ifdef NETCHESSZX_SDCC_IY
void netchesszx_asm_net_copy(uint8_t *dst, const uint8_t *src, uint16_t len);
void netchesszx_asm_net_move(uint8_t *dst, const uint8_t *src, uint16_t len);
#ifndef NETCHESSZX_SPECTRANEXT
uint8_t netchesszx_asm_direct_pop(void);
#endif
#define net_copy netchesszx_asm_net_copy
#define net_move netchesszx_asm_net_move
#else
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
#endif

#define WAIT_MED 500
#define WAIT_POLL 2
#define MQTT_STREAM_BACKGROUND_DRAIN_BUDGET 64u

#ifndef NETCHESSZX_SPECTRANEXT
#ifdef NETCHESSZX_HOST_TEST
static char direct_rx_spill[SPECTRUM_NET_PAYLOAD_MAX + 1u];
#define DIRECT_RX_SPILL direct_rx_spill
#else
#define DIRECT_RX_SPILL ((char *)SPECTRUM_MQTT_RUNTIME_ASSETS_END)
#endif
#if (SPECTRUM_MQTT_RUNTIME_ASSETS_END + SPECTRUM_NET_PAYLOAD_MAX + 1u) > \
    SPECTRUM_MQTT_SCRATCH_BASE
#error "DIRECT spill exceeds inactive MQTT stream storage"
#endif
#endif

#if defined(NETCHESSZX_SPECTRANEXT) && defined(NETCHESSZX_FIXED_LOW_RAM)
#define SPXN_NET_AT(address) __at(address)
#else
#define SPXN_NET_AT(address)
#endif

SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_ACTIVE_LINK_ADDR) uint8_t active_link;
#ifdef NETCHESSZX_HIGH_SCRATCH
/* Classic: the esxDOS loader owns both slots above the IM2 vector. */
extern char direct_rx_payload[SPECTRUM_NET_PAYLOAD_MAX];
extern char direct_rx_payload2[SPECTRUM_NET_PAYLOAD_MAX];
#else
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_RX_ADDR)
char direct_rx_payload[SPECTRUM_NET_PAYLOAD_MAX];
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_RX2_ADDR)
char direct_rx_payload2[SPECTRUM_NET_PAYLOAD_MAX];
#endif
#ifndef NETCHESSZX_SPECTRANEXT
uint8_t direct_rx_link;
uint8_t direct_rx_link2;
#endif
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_COUNT_ADDR) uint8_t direct_rx_count;
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_HEAD_ADDR) uint8_t direct_rx_head;
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_LEN_ADDR)
uint8_t direct_rx_payload_len;
#ifdef NETCHESSZX_SPECTRANEXT
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_DISCARD_ADDR)
uint8_t direct_rx_discard;
#else
uint16_t direct_ipd_remaining;
uint8_t direct_ipd_accept;
uint8_t direct_ipd_link;
#endif
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_CLOSED_ADDR)
uint8_t direct_link_closed;
#ifndef NETCHESSZX_SPECTRANEXT
uint8_t direct_peer_valid;
/* Link id of a third party that connected while a session is active; 0xff =
   none. The DIRECT overlay closes it without dropping the active session. */
uint8_t direct_intruder_link = 0xffu;
#endif
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_FLAGS_ADDR)
uint8_t net_payload_flags;
static uint8_t clock_retry_pending;
#ifdef NETCHESSZX_SPECTRANEXT
#define SPECTRUM_OVL_TIME 17u
#define SPECTRUM_OVL_TIME_SYNC 0u
#define SPECTRUM_OVL_TIME_CLOCK_RETRY_START 1u
#define SPECTRUM_OVL_TIME_CLOCK_RETRY_POLL 2u
#define SPECTRUM_OVL_TIME_CLOCK_RETRY_CANCEL 3u
#endif
#ifdef NETCHESSZX_SPECTRANEXT
SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_DIRECT_3S_POLLS_ADDR)
uint8_t net_uart_direct_idle_ticks;
#ifdef NETCHESSZX_HOST_TEST
uint8_t spxn_direct_pending_cursor;
uint8_t spxn_direct_pending_length;
uint8_t spxn_direct_pending_scratch[SPECTRUM_MQTT_PACKET_MAX];
#define NET_DIRECT_PENDING_CURSOR spxn_direct_pending_cursor
#define NET_DIRECT_PENDING_LENGTH spxn_direct_pending_length
#define NET_DIRECT_PENDING_SCRATCH spxn_direct_pending_scratch
#else
#define NET_DIRECT_PENDING_CURSOR \
    (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_CURSOR_ADDR)
#define NET_DIRECT_PENDING_LENGTH \
    (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_LENGTH_ADDR)
#define NET_DIRECT_PENDING_SCRATCH \
    ((void *)NETCHESSZX_LOWRAM_SPXN_DIRECT_PENDING_DATA_ADDR)
#endif
#endif

static void direct_reset_rx_state(void);
static uint8_t mqtt_drain_uart_budget(uint8_t budget) __z88dk_fastcall;
void mqtt_abort_stream_mode(void);

#ifdef NETCHESSZX_SPECTRANEXT
/* Link-only compatibility for the resident shrink object: its AT parser is
   unreachable in this target, but z80asm still resolves _line_buf.  Alias the
   symbol to the already-reserved bss_user block; no native path reads/writes
   it. */
#ifdef NETCHESSZX_FIXED_LOW_RAM
__at(NETCHESSZX_LOWRAM_SPXN_BSS_USER_ADDR)
#endif
char line_buf[SPECTRUM_NET_LINE_MAX];
#ifdef NETCHESSZX_FIXED_LOW_RAM
__at(NETCHESSZX_LOWRAM_LAST_IP_ADDR)
#endif
char last_ip[NETCHESSZX_LOWRAM_LAST_IP_SIZE];

void net_wait_frame(void)
{
    spectrum_net_runtime_wait_frame_plain();
}

void spxn_send_idle(void)
{
    spectrum_net_runtime_wait_frame_plain();
}

void spxtime_idle(void)
{
    spectrum_net_runtime_wait_frame_plain();
}

const char *spectrum_net_last_ip(void)
{
    return last_ip;
}

#endif

static void clock_retry_set_context(void)
{
    uint16_t state_addr = (uint16_t)spectrum_net_payload_scratch();

    spectrum_overlay_context[0] = (uint8_t)state_addr;
    spectrum_overlay_context[1] = (uint8_t)(state_addr >> 8);
}

void spectrum_net_clock_retry_start(void)
{
    clock_retry_set_context();
#ifdef NETCHESSZX_SPECTRANEXT
    if (clock_retry_pending) {
        (void)spectrum_overlay_exec_cached(
            SPECTRUM_OVL_TIME, SPECTRUM_OVL_TIME_CLOCK_RETRY_CANCEL);
    }
    clock_retry_pending = spectrum_overlay_exec_cached(
        SPECTRUM_OVL_TIME, SPECTRUM_OVL_TIME_CLOCK_RETRY_START);
#else
    clock_retry_pending = spectrum_overlay_exec_cached(
        SPECTRUM_OVL_MQTT_TX, SPECTRUM_OVL_MQTT_TX_CLOCK_RETRY_START);
#endif
}

void spectrum_net_clock_retry_cancel(void)
{
    if (!clock_retry_pending) {
        return;
    }
    clock_retry_set_context();
#ifdef NETCHESSZX_SPECTRANEXT
    (void)spectrum_overlay_exec_cached(
        SPECTRUM_OVL_TIME, SPECTRUM_OVL_TIME_CLOCK_RETRY_CANCEL);
#endif
    clock_retry_pending = 0u;
}

static void spectrum_net_transport_drain(void)
{
#ifdef NETCHESSZX_SPECTRANEXT
    if (netchesszx_transport_is_mqtt()) {
        (void)mqtt_drain_uart_budget(MQTT_STREAM_BACKGROUND_DRAIN_BUDGET);
    } else if (NET_DIRECT_PENDING_LENGTH == 0u) {
        uint8_t events = (uint8_t)spxn_poll();

        if ((events & SPXN_POLLNVAL) != 0u) {
            direct_link_closed = 1u;
        } else if ((events & SPXN_POLLIN) != 0u) {
            uint8_t got = (uint8_t)spxn_recv(
                NET_DIRECT_PENDING_SCRATCH, SPECTRUM_MQTT_PACKET_MAX);

            if (got == 0u || got > SPECTRUM_MQTT_PACKET_MAX) {
                direct_link_closed = 1u;
            } else {
                NET_DIRECT_PENDING_CURSOR = 0u;
                NET_DIRECT_PENDING_LENGTH = got;
            }
        } else if ((events & SPXN_POLLHUP) != 0u) {
            direct_link_closed = 1u;
        }
    }
    return;
#else
    if (!netchesszx_transport_is_mqtt()) {
        /* DIRECT: stockpile raw UART bytes in the platform ring while the app
           is blocked; the DIRECT overlay parses them on its next read. */
        spectrum_uart_background_pump();
        return;
    }
    (void)mqtt_drain_uart_budget(MQTT_STREAM_BACKGROUND_DRAIN_BUDGET);
#endif
}

/* Overlay ABI: keep this a raw transport pump. Dispatching another overlay
   here would overwrite the caller's code and return into the wrong bank. */
void spectrum_net_background_drain(void)
{
    spectrum_net_transport_drain();
}

void spectrum_net_background_drain_clock(void)
{
    if (clock_retry_pending) {
        clock_retry_set_context();
#ifdef NETCHESSZX_SPECTRANEXT
        clock_retry_pending = spectrum_overlay_exec_cached(
            SPECTRUM_OVL_TIME, SPECTRUM_OVL_TIME_CLOCK_RETRY_POLL);
#else
        clock_retry_pending = spectrum_overlay_exec_cached(
            SPECTRUM_OVL_MQTT_TX, SPECTRUM_OVL_MQTT_TX_CLOCK_RETRY_POLL);
#endif
        if (clock_retry_pending) {
            return;
        }
    }
    spectrum_net_transport_drain();
}

uint8_t spectrum_net_payload_flags(void)
{
    return net_payload_flags;
}

char *spectrum_net_payload_scratch(void)
{
    /* Each transport lends storage owned only by the inactive backend:
       MQTT uses the first DIRECT queue slot (the second stores a queued
       publish); DIRECT uses the MQTT packet scratch below the overlay slot. */
    if (netchesszx_transport_is_mqtt()) {
        return direct_rx_payload;
    }
#ifdef NETCHESSZX_HOST_TEST
    {
        static char host_scratch[SPECTRUM_MQTT_PACKET_MAX];
        return host_scratch;
    }
#else
    return (char *)SPECTRUM_MQTT_PACKET_SCRATCH;
#endif
}

void spectrum_net_start_uart(void)
{
#ifdef NETCHESSZX_SPECTRANEXT
    spxn_close();
#else
    spectrum_uart_init();
    spectrum_uart_flush(25u);
#endif
    direct_reset_rx_state();
#ifndef NETCHESSZX_SPECTRANEXT
    reset_line_buf();
#endif
}

static void direct_reset_rx_state(void)
{
    direct_rx_count = 0u;
    direct_rx_head = 0u;
    direct_rx_payload_len = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    direct_rx_discard = 0u;
    NET_DIRECT_PENDING_CURSOR = 0u;
    NET_DIRECT_PENDING_LENGTH = 0u;
    direct_link_closed = 0u;
#else
    direct_ipd_remaining = 0u;
    direct_ipd_accept = 0u;
    direct_ipd_link = 0u;
    direct_link_closed = 0u;
    direct_peer_valid = 0u;
    direct_intruder_link = 0xffu;
    reset_line_buf();
#endif
}

void spectrum_net_direct_peer_mark_valid(void)
{
#ifndef NETCHESSZX_SPECTRANEXT
    direct_peer_valid = 1u;
#endif
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

    if (payload_cap != 0u) {
        payload[0] = '\0';
    }
    spectrum_overlay_context[0] = (uint8_t)payload_addr;
    spectrum_overlay_context[1] = (uint8_t)(payload_addr >> 8);
    spectrum_overlay_context[2] = payload_cap;
#ifndef NETCHESSZX_SPECTRANEXT
    if (direct_rx_count != 0u &&
        (direct_intruder_link == 0xffu || direct_ipd_remaining != 0u)) {
#ifdef NETCHESSZX_SDCC_IY
        link = netchesszx_asm_direct_pop();
#else
        const char *source = direct_rx_head == 0u ? direct_rx_payload
                             : (direct_rx_head == 1u ? direct_rx_payload2
                                                    : DIRECT_RX_SPILL);
        uint8_t n = 0u;

        if (payload_cap != 0u) {
            while (source[n] != '\0' &&
                   n < (uint8_t)(payload_cap - 1u)) {
                payload[n] = source[n];
                ++n;
            }
            payload[n] = '\0';
        }
        link = direct_rx_head == 0u ? direct_rx_link
               : (direct_rx_head == 1u ? direct_rx_link2
                                       : (uint8_t)DIRECT_RX_SPILL[
                                             SPECTRUM_NET_PAYLOAD_MAX]);
        --direct_rx_count;
        if (++direct_rx_head >= SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT) {
            direct_rx_head = 0u;
        }
#endif
        goto read_done;
    }
#endif
    link = spectrum_overlay_exec_cached(SPECTRUM_OVL_DIRECT,
                                         SPECTRUM_OVL_DIRECT_READ);
    goto read_done; /* keep the label used when the direct path is compiled out */
read_done:
#ifdef NETCHESSZX_NEXT
    if (spectrum_uart_error()) {
        if (payload_cap != 0u) {
            payload[0] = '\0';
        }
        return -2;
    }
#endif
    if (link == 0u && payload_cap != 0u && payload[0] == '\0') {
        return SPECTRUM_NET_READ_TIMEOUT;
    }
    /* The overlay returns status codes truncated to a byte: 0xfd is
       READ_TIMEOUT (-3), 0xfe is link-closed (-2). Sign-extend so the
       poll layer sees the negative values again; real link ids stay in
       0..4. Without this a timeout came back as +253, was taken for a
       payload, and stale caller data was re-classified on every poll tick:
       PING/RESET ghost-event storms on the DIRECT link. */
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

static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_ID_ADDR) uint16_t mqtt_next_id;
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_STREAM_LEN_ADDR)
uint8_t mqtt_stream_len;
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_STREAM_ACTIVE_ADDR)
uint8_t mqtt_stream_active;
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_QUEUE_COUNT_ADDR)
uint8_t mqtt_payload_count;
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_QUEUE_HEAD_ADDR)
uint8_t mqtt_payload_head;
static SPXN_NET_AT(NETCHESSZX_LOWRAM_SPXN_MQTT_KEEPALIVE_ADDR)
spectrum_mqtt_broker_keepalive_t mqtt_broker_keepalive;
#define MQTT_STREAM_ACTIVE 0x01u
#define MQTT_STREAM_DISCARDING 0x80u
#define MQTT_STREAM_MAX 223u
#define MQTT_STREAM_BASE SPECTRUM_MQTT_RUNTIME_ASSETS_END
#define MQTT_STREAM_DRAIN_BUDGET 64u
#if MQTT_STREAM_BASE < SPECTRUM_MQTT_RUNTIME_ASSETS_END
#error "MQTT stream overlaps runtime assets"
#endif
#if MQTT_STREAM_MAX < (SPECTRUM_MQTT_PACKET_MAX + MQTT_STREAM_DRAIN_BUDGET - 1u)
#error "MQTT_STREAM_MAX must hold one near-complete MQTT packet plus one UART drain"
#endif
#if SPECTRUM_MQTT_SCRATCH_BASE < (MQTT_STREAM_BASE + MQTT_STREAM_MAX)
#error "MQTT scratch overlaps mqtt_stream"
#endif
#ifndef NETCHESSZX_NEXT_BANKING
/* Classic/Spectranext player identity lives between the stream and scratch. */
#if NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR < (MQTT_STREAM_BASE + MQTT_STREAM_MAX)
#error "player identity overlaps mqtt_stream"
#endif
#if NETCHESSZX_LOWRAM_PEER_NAME_END > SPECTRUM_MQTT_SCRATCH_BASE
#error "player identity overlaps MQTT packet scratch"
#endif
#endif
#ifdef NETCHESSZX_NEXT_BANKING
#define SPECTRUM_MQTT_SCRATCH_LIMIT 0x3b78u
#else
#define SPECTRUM_MQTT_SCRATCH_LIMIT 0x6800u
#endif
#if (SPECTRUM_MQTT_SCRATCH_BASE + SPECTRUM_MQTT_PACKET_MAX) > \
    SPECTRUM_MQTT_SCRATCH_LIMIT
#error "MQTT scratch overlaps overlay slot"
#endif
#ifdef NETCHESSZX_HOST_TEST
static uint8_t mqtt_stream[MQTT_STREAM_MAX];
#else
static __at(MQTT_STREAM_BASE) uint8_t mqtt_stream[MQTT_STREAM_MAX];
#endif
#define MQTT_PACKET SPECTRUM_MQTT_PACKET_SCRATCH
#define MQTT_STREAM mqtt_stream
#define mqtt_discard_remaining \
    (((mqtt_stream_active & MQTT_STREAM_DISCARDING) != 0u) \
         ? mqtt_stream_len : 0u)
#define MQTT_PAYLOAD_SLOT_SIZE (SPECTRUM_NET_PAYLOAD_MAX + 2u)
#if (MQTT_PAYLOAD_QUEUE_COUNT * MQTT_PAYLOAD_SLOT_SIZE) > SPECTRUM_MQTT_PACKET_MAX
#error "MQTT payload queue exceeds inactive packet scratch"
#endif

static void mqtt_reset_session_state(void);

uint8_t spectrum_net_mqtt_subscribe_side(void)
{
    return spectrum_overlay_exec(SPECTRUM_OVL_MQTT_CONNECT,
                                 SPECTRUM_OVL_MQTT_CONNECT_ACTIVATE);
}

uint8_t spectrum_net_mqtt_subscribe_seat(void)
{
    return spectrum_overlay_exec(SPECTRUM_OVL_MQTT_CONNECT,
                                 SPECTRUM_OVL_MQTT_CONNECT_PROBE_SEAT);
}

uint8_t mqtt_enter_stream_mode(void)
{
    mqtt_stream_active = 0u;
    mqtt_stream_len = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    return 1u;
#else
    if (!spectrum_net_at_cmd("AT+CIPMODE=1", WAIT_MED)) {
        goto fail;
    }

    reset_line_buf();
    if (!spectrum_uart_send_string("AT+CIPSEND") ||
        !spectrum_uart_send_crlf()) {
        goto fail;
    }
    if (!wait_for_prompt(WAIT_MED)) {
        goto fail;
    }

    reset_line_buf();
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    return 1u;

fail:
    mqtt_abort_stream_mode();
    return 0u;
#endif
}

void mqtt_abort_stream_mode(void)
{
    mqtt_stream_active = 0u;
    mqtt_stream_len = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    spxn_close();
#else
    (void)spectrum_net_ensure_command_mode();
#endif
}

static uint8_t mqtt_drain_uart_budget(uint8_t budget) __z88dk_fastcall
{
#ifdef NETCHESSZX_SPECTRANEXT
    int16_t flags;
    int16_t got;

    if (!mqtt_stream_active) {
        return 0u;
    }
    flags = spxn_poll();
    if (flags < 0) {
        mqtt_abort_stream_mode();
        return 2u;
    }
    if ((flags & SPXN_POLLIN) != 0) {
        /* One POLLIN grant authorizes exactly this RECV. A HUP flag is
           handled only after the final readable bytes are copied. */
        if (mqtt_discard_remaining != 0u) {
            if (budget > mqtt_discard_remaining) {
                budget = mqtt_discard_remaining;
            }
            got = spxn_recv(MQTT_STREAM, budget);
            if (got <= 0) {
                mqtt_abort_stream_mode();
                return 2u;
            }
            mqtt_stream_len =
                (uint8_t)(mqtt_stream_len - (uint8_t)got);
            if (mqtt_stream_len == 0u) {
                mqtt_stream_active = MQTT_STREAM_ACTIVE;
            }
            return 1u;
        }
        if (mqtt_stream_len >= MQTT_STREAM_MAX) {
            return 2u;
        }
        if (budget > (uint8_t)(MQTT_STREAM_MAX - mqtt_stream_len)) {
            budget = (uint8_t)(MQTT_STREAM_MAX - mqtt_stream_len);
        }
        got = spxn_recv(MQTT_STREAM + mqtt_stream_len, budget);
        if (got <= 0) {
            mqtt_abort_stream_mode();
            return 2u;
        }
        mqtt_stream_len = (uint8_t)(mqtt_stream_len + (uint8_t)got);
        return 1u;
    }
    if ((flags & (SPXN_POLLHUP | SPXN_POLLNVAL)) != 0) {
        mqtt_abort_stream_mode();
        return 2u;
    }
    return 0u;
#else
    uint8_t any = 0u;

    if (!mqtt_stream_active) {
        return 0u;
    }

    while (budget-- != 0u && spectrum_uart_ready()) {
        if (mqtt_discard_remaining != 0u) {
            (void)spectrum_uart_read();
            --mqtt_stream_len;
            if (mqtt_stream_len == 0u) {
                mqtt_stream_active = MQTT_STREAM_ACTIVE;
            }
            any = 1u;
            continue;
        }
        if (mqtt_stream_len >= MQTT_STREAM_MAX) {
            /* Full: leave the byte in the UART/ring. Reading it before this
               check dropped it and desynced the MQTT parser. */
            return 2u;
        }
        MQTT_STREAM[mqtt_stream_len++] = spectrum_uart_read();
        any = 1u;
    }

    return any;
#endif
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
        spectrum_net_runtime_wait_frame_plain();
    }
    return 0u;
}

static void mqtt_consume_stream_packet(uint8_t total) __z88dk_fastcall
{
    mqtt_stream_len = (uint8_t)(mqtt_stream_len - total);
    if (mqtt_stream_len != 0u) {
        net_move(MQTT_STREAM, MQTT_STREAM + total, mqtt_stream_len);
    }
}

/* Return a view length; the caller consumes the stream after copying/parsing. */
static int16_t mqtt_take_stream_packet(void)
{
    uint8_t discard = 0u;
    uint8_t remaining;
    uint8_t total;
    uint8_t header_len;
    uint8_t b;

    /* In discard mode, stream_len counts unread body bytes, not buffered data. */
    if ((mqtt_stream_active & MQTT_STREAM_DISCARDING) != 0u) {
        return 0;
    }

    /* Keep the last byte when no header survives: it may be the first byte of
       the next fragmented packet. Compact only once after discarded garbage. */
    while (mqtt_stream_len >= 2u) {
        b = MQTT_STREAM[discard];
        if ((b & 0xf0u) == 0x30u || b == 0x20u || b == 0x40u ||
            b == 0x90u || b == 0xd0u) {
            break;
        }
        ++discard;
        --mqtt_stream_len;
    }
    if (discard != 0u) {
        net_move(MQTT_STREAM, MQTT_STREAM + discard, mqtt_stream_len);
    }
    if (mqtt_stream_len < 2u) {
        return 0;
    }

    b = (uint8_t)MQTT_STREAM[1u];
    remaining = (uint8_t)(b & 0x7fu);
    header_len = 2u;
    if ((b & 0x80u) != 0u) {
        if (mqtt_stream_len < 3u) {
            return 0;
        }
        b = (uint8_t)MQTT_STREAM[2u];
        if (b > 1u) {
            /* The 8-bit discard counter cannot preserve this packet boundary. */
            mqtt_abort_stream_mode();
            return -2;
        }
        remaining = (uint8_t)(remaining + (uint8_t)(b << 7));
        header_len = 3u;
    }
    if (remaining > (uint8_t)(SPECTRUM_MQTT_PACKET_MAX - header_len)) {
        discard = (uint8_t)(mqtt_stream_len - header_len);
        if (discard < remaining) {
            mqtt_stream_len = (uint8_t)(remaining - discard);
            mqtt_stream_active =
                (uint8_t)(MQTT_STREAM_ACTIVE | MQTT_STREAM_DISCARDING);
        } else {
            mqtt_consume_stream_packet((uint8_t)(header_len + remaining));
        }
        return 0;
    }
    total = (uint8_t)(header_len + remaining);
    if (mqtt_stream_len < total) {
        return 0;
    }

    spectrum_mqtt_broker_keepalive_reset(&mqtt_broker_keepalive);
    return total;
}

static uint8_t *mqtt_payload_slot(uint8_t slot) __z88dk_fastcall
{
    if (slot == 1u) {
        return MQTT_PACKET + MQTT_PAYLOAD_SLOT_SIZE;
    }
    if (slot == 2u) {
        return MQTT_PACKET + (2u * MQTT_PAYLOAD_SLOT_SIZE);
    }
    return MQTT_PACKET;
}

static int16_t mqtt_read_stream_packet(void)
{
    int16_t got;
    uint8_t filled;

#ifdef NETCHESSZX_NEXT
    if (spectrum_uart_error()) {
        return -2;
    }
#endif
    got = mqtt_take_stream_packet();
    if (got != 0) {
        return got;
    }
    filled = mqtt_fill_stream(WAIT_POLL);
#ifdef NETCHESSZX_NEXT
    if (spectrum_uart_error()) {
        return -2;
    }
#endif
    if (!filled) {
        return SPECTRUM_NET_MQTT_READ_TIMEOUT;
    }
    got = mqtt_take_stream_packet();
    return got == 0 ? SPECTRUM_NET_MQTT_READ_TIMEOUT : got;
}

uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len)
{
    if (mqtt_stream_active) {
#ifdef NETCHESSZX_SPECTRANEXT
        if (spxn_send_all(packet, len, 150u) == SPXN_OK) {
            return 1u;
        }
#else
        /* Transparent (CIPMODE=1) writes give no synchronous TCP status.
           The read path sends broker PINGREQs and closes the link after
           missing PINGRESPs, independently of peer liveness. */
        if (spectrum_uart_send_bytes(packet, len)) {
            return 1u;
        }
#endif
        mqtt_abort_stream_mode();
    }
    return 0u;
}

static int16_t mqtt_broker_keepalive_timeout(void)
{
    uint8_t ping[2u];
    uint8_t event = spectrum_mqtt_broker_keepalive_timeout(
        &mqtt_broker_keepalive);

    if (event == SPECTRUM_MQTT_KEEPALIVE_NONE) {
        return SPECTRUM_NET_MQTT_READ_TIMEOUT;
    }
    if (event == SPECTRUM_MQTT_KEEPALIVE_LOST) {
        return -2;
    }
    ping[0u] = SPECTRUM_MQTT_PINGREQ_HEADER;
    ping[1u] = 0u;
    return mqtt_send_raw_packet(ping, 2u)
        ? SPECTRUM_NET_MQTT_READ_TIMEOUT : -2;
}

static void mqtt_puback_id(uint16_t packet_id) __z88dk_fastcall
{
    uint8_t ack[4];

    if (packet_id == 0u) {
        return;
    }
    ack[0u] = 0x40u;
    ack[1u] = 0x02u;
    ack[2u] = (uint8_t)(packet_id >> 8);
    ack[3u] = (uint8_t)packet_id;
    (void)mqtt_send_raw_packet(ack, 4u);
}

static void mqtt_reset_session_state(void)
{
    mqtt_stream_len = 0u;
    mqtt_stream_active = 0u;
    mqtt_payload_count = 0u;
    mqtt_payload_head = 0u;
    net_payload_flags = 0u;
    spectrum_mqtt_broker_keepalive_reset(&mqtt_broker_keepalive);
    mqtt_next_id = 1u;
}

uint8_t spectrum_net_preflight_run(uint8_t quiet) NETCHESSZX_FASTCALL
{
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_QUIET] = quiet;
#ifdef NETCHESSZX_SPECTRANEXT
#ifdef NETCHESSZX_HOST_TEST
    extern uint8_t netchesszx_host_nextreg_peripheral_1;

    net_uart_direct_idle_ticks =
        NETCHESSZX_SESSION_DIRECT_3S_POLLS_FROM_NEXTREG(
            netchesszx_host_nextreg_peripheral_1);
#else
#asm
    di
    ld bc, 0x243b
    ld a, 0x05
    out (c), a
    inc b
    in a, (c)
    bit 2, a
    ld a, 150
    jr z, net_session_timing_store
    ld a, 180
net_session_timing_store:
    ld (_net_uart_direct_idle_ticks), a
    ei
#endasm
#endif
#endif
    if (!spectrum_overlay_exec_cached(SPECTRUM_OVL_NET_CONNECT,
                                      SPECTRUM_OVL_NET_PREFLIGHT)) {
        return SPECTRUM_LINK_PREFLIGHT_OVL_FAIL;
    }
    if (spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_OK]) {
        return SPECTRUM_LINK_PREFLIGHT_OK;
    }
    if (spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_RETRY] == SPECTRUM_OVL_PREFLIGHT_RETRY_AGAIN) {
        return SPECTRUM_LINK_PREFLIGHT_RETRYING;
    }
    return SPECTRUM_LINK_PREFLIGHT_FAILED;
}

#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spectrum_net_sync_time(void)
{
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_TIME,
                                        SPECTRUM_OVL_TIME_SYNC);
}
#endif

uint8_t spectrum_net_mqtt_connect(uint8_t flags) NETCHESSZX_FASTCALL
{
    mqtt_reset_session_state();
    spectrum_overlay_context[NETCHESSZX_MQTT_CONNECT_CTX_FLAGS] = flags;
    return spectrum_overlay_exec(SPECTRUM_OVL_MQTT_CONNECT,
                                 SPECTRUM_OVL_MQTT_CONNECT_START);
}

int16_t spectrum_net_mqtt_read_payload(char *payload, uint8_t payload_cap)
{
    const uint8_t *packet;
    uint8_t *slot;
    uint16_t packet_id;
    int16_t got;
    uint8_t packet_len;
    uint8_t flags;

    net_payload_flags = 0u;
#ifdef NETCHESSZX_NEXT
    if (spectrum_uart_error()) {
        return -2;
    }
#endif
    if (mqtt_payload_count != 0u) {
        slot = mqtt_payload_slot(mqtt_payload_head);
        mqtt_payload_head = (uint8_t)(mqtt_payload_head + 1u);
        if (mqtt_payload_head == MQTT_PAYLOAD_QUEUE_COUNT) {
            mqtt_payload_head = 0u;
        }
        --mqtt_payload_count;
        if (payload_cap == 0u || slot[0u] >= payload_cap) {
            return SPECTRUM_NET_READ_TIMEOUT;
        }
        net_copy((uint8_t *)payload,
                 slot + 2u,
                 (uint16_t)(slot[0u] + 1u));
        net_payload_flags = slot[1u];
        return 0;
    }

    got = mqtt_read_stream_packet();
    packet = MQTT_STREAM;
    if (got == SPECTRUM_NET_MQTT_READ_TIMEOUT) {
        got = mqtt_broker_keepalive_timeout();
        if (got == SPECTRUM_NET_MQTT_READ_TIMEOUT) {
            return SPECTRUM_NET_READ_TIMEOUT;
        }
    }
    if (got <= 0) {
        return -2;
    }

    if (spectrum_mqtt_type(packet, (uint16_t)got) == SPECTRUM_MQTT_PINGRESP &&
        (uint16_t)got <= 2u) {
        mqtt_consume_stream_packet((uint8_t)got);
        return SPECTRUM_NET_READ_TIMEOUT;
    }

    if (spectrum_mqtt_type(packet, (uint16_t)got) !=
        SPECTRUM_MQTT_PUBLISH) {
        mqtt_consume_stream_packet((uint8_t)got);
        return SPECTRUM_NET_READ_TIMEOUT;
    }

    packet_len = (uint8_t)got;
    got = spectrum_mqtt_parse_publish(packet,
                                       packet_len,
                                       payload,
                                       payload_cap,
                                       &packet_id,
                                       &flags);
    mqtt_consume_stream_packet(packet_len);
    if (got < 0) {
        mqtt_puback_id(packet_id);
        return SPECTRUM_NET_READ_TIMEOUT;
    }
    mqtt_puback_id(packet_id);
    net_payload_flags = flags;
    return 0;
}

int16_t spectrum_net_read_payload(char *payload, uint8_t payload_cap)
{
    net_payload_flags = 0u;
    if (netchesszx_transport_is_mqtt()) {
        return spectrum_net_mqtt_read_payload(payload, payload_cap);
    }
    return direct_read_payload(payload, payload_cap);
}

uint8_t spectrum_net_send_text(const char *text) NETCHESSZX_FASTCALL
{
    spectrum_net_background_drain_clock();
    if (netchesszx_transport_is_mqtt()) {
        return spectrum_net_mqtt_publish(text);
    }
    return direct_send_text(text);
}
