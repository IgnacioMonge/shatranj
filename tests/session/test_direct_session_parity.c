#define NETCHESSZX_HOST_SESSION_TEST 1

#include "direct_parity.h"
#include "spectrum/overlay/overlay.h"

volatile uint8_t
    netchesszx_host_overlay_context[SPECTRUM_OVERLAY_CONTEXT_SIZE];

#ifdef NETCHESSZX_SPECTRANEXT
/* The host parity fixture includes the real Next overlay without the SDK
   headers. These declarations keep the test seam source-compatible with the
   target socket API; the fixture provides inert implementations below. */
#define SPXN_OK 0
#define SPXN_POLLCON 1
#define SPXN_POLLHUP 2
#define SPXN_POLLIN 4
#define SPXN_POLLNVAL 128
int16_t spxn_poll(void);
int16_t spxn_recv(void *buffer, uint16_t maximum);
int16_t spxn_accept_single(void);
int16_t spxn_listen(uint16_t port);
int16_t spxn_resolve(const char *host, uint8_t *ip4be);
int16_t spxn_connect(const uint8_t *ip4be, uint16_t port);
void spxn_close(void);
int16_t spxn_send_all(const void *buffer, uint16_t length,
                     uint16_t zero_budget);
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include "../spectrum/editor_fixture.h"
#include "../../src/spectrum/app/app.c"
#include "../spectrum/setup_fixture.h"
#include "../../src/spectrum/overlay/direct_ovl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

typedef char direct_parity_nack_sync_capacity_check[
    sizeof("NACK 65535 SYNC") == 16u &&
    sizeof("NACK 65535 SYNC") <= SPECTRUM_LINK_PAYLOAD_MAX ? 1 : -1];

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

netchesszx_session_ping_t *netchesszx_host_session_ping;
static uint8_t host_chat_ping_reset_expected;
static uint8_t host_chat_ping_reset_forbidden;
static uint8_t host_chat_ping_reset_violation;

void netchesszx_host_session_observe_ping_reset(
    netchesszx_session_ping_t *ping)
{
    (void)ping;
    if (host_chat_ping_reset_forbidden) {
        host_chat_ping_reset_violation = 1u;
    }
    host_chat_ping_reset_expected = 0u;
}

static char host_payload[SPECTRUM_LINK_PAYLOAD_MAX];
static const DirectParityScenario *host_scenario;
static DirectParityTrace *host_trace;
static uint8_t host_step_pos;
static uint8_t host_link_id;
static uint8_t host_role;
static uint8_t host_side_initialized;
static uint8_t host_observed_color;
static uint8_t host_ready_seen;
static uint8_t host_started_seen;
static uint8_t host_failed;
static uint16_t host_now_ticks;
static uint16_t host_timeout_deadline;
static uint8_t host_timeout_active;
static const uint8_t *host_fail_payload;
static uint8_t host_fail_length;
static uint8_t host_failed_send_pending;
static uint8_t host_active_link_down;
static uint8_t host_control_draw_pending;
static uint8_t host_control_reset_pending;
static uint8_t host_local_confirm_pending;
static uint8_t host_local_draw_pending;
static uint8_t host_local_reset_pending;
static uint8_t host_local_takeback_pending;
static uint8_t host_takeback_result_seen;
static uint8_t host_resign_control_pending;
static uint8_t host_local_resign_pending;
static uint8_t host_connected_state;
uint8_t spectrum_gui_board_pieces_visible;
#define host_pieces_visible spectrum_gui_board_pieces_visible
static uint8_t host_board_flipped;
static uint8_t host_side_panels_visible;
static uint8_t host_about_visible;
static uint8_t host_fileui_visible;
static uint8_t host_menu_key_calls;
static uint8_t host_menu_visible;
static uint8_t host_status_phase;
static uint8_t host_status_phase_calls;
static uint8_t host_redraw_square_calls;
static char host_last_notice[96];
static uint8_t host_last_msg_id = 0xffu;
static const uint8_t *host_restore_file_payload;
static const uint8_t *host_recovery_payload;
static uint8_t host_recovery_available;
static uint8_t host_recovery_writes;
static unsigned host_cursor_marks;
static unsigned host_recovery_cursor_marks;
static uint8_t host_recovery_write_ok;
static uint8_t host_recovery_saved[NETCHESSZX_SAVE_WIRE_B64_SIZE];
static uint8_t host_manual_saves;
static char host_manual_saved[NETCHESSZX_SAVE_WIRE_B64_SIZE];
static uint8_t host_restore_apply_pending;
static uint8_t host_restore_marker_seen;
static uint8_t host_restore_domain_pending;
static uint8_t host_restore_result_seen;
static uint8_t host_restore_reject_seen;
static uint8_t host_restore_control_pending;
#ifdef NETCHESSZX_NEXT_BANKING
static uint8_t host_next_sprites_reset_seen;
#endif
#if defined(NETCHESSZX_NEXT) || defined(NETCHESSZX_SPECTRANEXT)
uint8_t net_uart_direct_idle_ticks =
    NETCHESSZX_SESSION_DIRECT_3S_POLLS_AT(NETCHESSZX_SESSION_FRAME_HZ);
#endif

/* Real DIRECT overlay state and UART seam.  Candidate transcripts enter the
   same parser/rejector used by the target; this harness only captures its AT
   wire and normalizes that physical wire to SEND/CLOSE observations. */
char line_buf[SPECTRUM_NET_LINE_MAX];
char direct_rx_payload[SPECTRUM_NET_PAYLOAD_MAX];
char direct_rx_payload2[SPECTRUM_NET_PAYLOAD_MAX];
uint8_t direct_rx_link;
uint8_t direct_rx_link2;
uint8_t active_link;
uint8_t line_pos;
uint8_t direct_rx_count;
uint8_t direct_rx_head;
uint8_t direct_rx_payload_len;
#ifdef NETCHESSZX_SPECTRANEXT
uint8_t direct_rx_discard;
uint8_t spxn_direct_pending_cursor;
uint8_t spxn_direct_pending_length;
uint8_t spxn_direct_pending_scratch[SPECTRUM_MQTT_PACKET_MAX];
#endif
uint16_t direct_ipd_remaining;
uint8_t direct_ipd_accept;
uint8_t direct_ipd_link;
uint8_t direct_link_closed;
uint8_t direct_peer_valid;
uint8_t direct_intruder_link;
static const uint8_t *host_uart_feed;
static size_t host_uart_feed_len;
static size_t host_uart_feed_pos;
static char host_uart_tx[96];
static size_t host_uart_tx_len;

void reset_line_buf(void)
{
    line_pos = 0u;
    line_buf[0] = '\0';
}

void net_wait_frame(void) {}
uint8_t spectrum_uart_ready(void)
{
    return (uint8_t)(host_uart_feed_pos < host_uart_feed_len);
}
uint8_t spectrum_uart_read(void) { return host_uart_feed[host_uart_feed_pos++]; }
static uint8_t host_uart_capture(const uint8_t *data, size_t length)
{
    if (host_uart_tx_len + length >= sizeof(host_uart_tx)) {
        host_failed = 1u;
        return 0u;
    }
    memcpy(host_uart_tx + host_uart_tx_len, data, length);
    host_uart_tx_len += length;
    host_uart_tx[host_uart_tx_len] = '\0';
    return 1u;
}
#ifdef NETCHESSZX_SPECTRANEXT
static uint16_t host_spxn_ipd_remaining;
static uint8_t host_spxn_ipd_link;
static char host_spxn_header[24];
static uint8_t host_spxn_header_len;

int16_t spxn_poll(void)
{
    return 0;
}

int16_t spxn_recv(void *buffer, uint16_t maximum)
{
    (void)buffer;
    (void)maximum;
    return 0;
}

int16_t spxn_accept_single(void)
{
    return SPXN_OK;
}

int16_t spxn_listen(uint16_t port)
{
    (void)port;
    return SPXN_OK;
}

int16_t spxn_resolve(const char *host, uint8_t *ip4be)
{
    (void)host;
    memset(ip4be, 0, 4u);
    return SPXN_OK;
}

int16_t spxn_connect(const uint8_t *ip4be, uint16_t port)
{
    (void)ip4be;
    (void)port;
    return SPXN_OK;
}

void spxn_close(void) {}

int16_t spxn_send_all(const void *buffer, uint16_t length,
                     uint16_t zero_budget)
{
    (void)zero_budget;
    return host_uart_capture((const uint8_t *)buffer, length)
               ? SPXN_OK : -1;
}

/* The Next socket has no ESP-AT +IPD framing. This small test-only shim keeps
   the existing intruder/link-down fixture inputs usable for both overlays. */
static uint8_t direct_feed_uart_byte_ovl(uint8_t c)
{
    if (host_spxn_ipd_remaining != 0u) {
        --host_spxn_ipd_remaining;
        return 0u;
    }
    if (c == ':') {
        unsigned link;
        unsigned length;

        host_spxn_header[host_spxn_header_len] = '\0';
        if (sscanf(host_spxn_header, "+IPD,%u,%u", &link, &length) != 2 ||
            link > 4u || length == 0u) {
            host_spxn_header_len = 0u;
            return 0u;
        }
        host_spxn_ipd_link = (uint8_t)link;
        host_spxn_ipd_remaining = (uint16_t)length;
        host_spxn_header_len = 0u;
        direct_intruder_link = host_spxn_ipd_link;
        return 0u;
    }
    if (c == '\n') {
        host_spxn_header[host_spxn_header_len] = '\0';
        if (host_spxn_header_len >= 8u &&
            host_spxn_header[1] == ',' &&
            strncmp(host_spxn_header + 2u, "CLOSED", 6u) == 0) {
            direct_link_closed = (uint8_t)(host_spxn_header[0] ==
                                           (char)('0' + active_link));
        }
        host_spxn_header_len = 0u;
        return 0u;
    }
    if (c != '\r' && host_spxn_header_len + 1u <
                         (uint8_t)sizeof(host_spxn_header)) {
        host_spxn_header[host_spxn_header_len++] = (char)c;
    }
    return 0u;
}
#endif
uint8_t spectrum_uart_send_string(const char *text)
{
    return host_uart_capture((const uint8_t *)text, strlen(text));
}
uint8_t spectrum_uart_send_bytes(const uint8_t *data, uint8_t length)
{
    return host_uart_capture(data, length);
}
uint8_t spectrum_uart_send_crlf(void)
{
    return host_uart_capture((const uint8_t *)"\r\n", 2u);
}
uint8_t spectrum_key_poll(void) { return 0u; }
void spectrum_net_guard_wait(uint16_t frames) { (void)frames; }
uint8_t spectrum_net_at_cmd(const char *command, uint16_t frames)
{
    (void)command;
    (void)frames;
    return 1u;
}
uint8_t spectrum_net_ensure_command_mode(void) { return 1u; }
void mqtt_abort_stream_mode(void) {}

static void check(uint8_t condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void check_restore_host_color_guard(void)
{
    netchesszx_save_meta_t meta = { 0 };

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(restore_host_color_ok(&meta),
          "matching DIRECT restore host color accepted");
    meta.host_color = NETCHESSZX_SAVE_HOST_BLACK;
    check(!restore_host_color_ok(&meta),
          "cross-color DIRECT restore rejected");

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_BLACK);
    meta.host_color = NETCHESSZX_SAVE_HOST_BLACK;
    check(restore_host_color_ok(&meta),
          "matching guest restore host color accepted");
    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(!restore_host_color_ok(&meta),
          "cross-color guest restore rejected");
}

#ifdef NETCHESSZX_NEXT
static void check_direct_recovery_budget_rearmed(void)
{
    direct_recovery_state = 0u;
    check(direct_prepare_link_ovl() && direct_recovery_state == 1u,
          "Next DIRECT entry rearms one hard-reset attempt");
}
#endif

#ifdef NETCHESSZX_NEXT_BANKING
static uint8_t host_next_find_slot(const uint8_t *slots, uint8_t square)
{
    uint8_t i;

    for (i = 0u; i < 32u; ++i) {
        if (slots[i] == square) {
            return i;
        }
    }
    return 0xffu;
}

static uint8_t host_next_restore_redraw(uint8_t reset_slots)
{
    static const char initial[65] =
        "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR";
    char target[64];
    uint8_t slots[32];
    uint8_t slot_count = 0u;
    uint8_t square;
    uint8_t drawn = 0u;

    memcpy(target, initial, sizeof(target));
    target[1] = '.';
    target[18] = 'n';
    target[36] = 'P';
    target[52] = '.';
    for (square = 0u; square < 64u; ++square) {
        if (initial[square] != '.') {
            slots[slot_count++] = square;
        }
    }
    check(slot_count == 32u, "Next sprite fixture starts with 32 pieces");
    if (reset_slots) {
        memset(slots, 0xff, sizeof(slots));
    }

    /* Faithful branch transcription of screen.asm next_find_slot,
       next_alloc_slot and next_hide_piece_slot while redraw scans 0..63. */
    for (square = 0u; square < 64u; ++square) {
        uint8_t slot = host_next_find_slot(slots, square);

        if (target[square] == '.') {
            if (slot != 0xffu) {
                slots[slot] = 0xffu;
            }
            continue;
        }
        if (slot == 0xffu) {
            slot = host_next_find_slot(slots, 0xffu);
            if (slot == 0xffu) {
                continue;
            }
            slots[slot] = square;
        }
        if (square == 36u) {
            drawn |= 1u;
        } else if (square == 18u) {
            drawn |= 2u;
        }
    }
    return drawn;
}

static void check_next_restore_sprite_allocator_contract(void)
{
    check(host_next_restore_redraw(0u) == 2u,
          "dirty Next slots drop e4 but draw c6");
    check(host_next_restore_redraw(1u) == 3u,
          "reset Next slots draw e4 and c6");
}
#endif

static void check_restore_codec_contract(void)
{
    static const char expected_b64[] =
        "uazam4iIiIgAAAAAAAAAAAAAAAAAAAAAEREREUI1YyR8_wAAAQEAAAAAOwGm";
    static const char expected_chunk_0[] =
        "uazam4iIiIgAAAAAAAAAAAAAAAAAAA";
    static const char expected_chunk_1[] =
        "AAEREREUI1YyR8_wAAAQEAAAAAOwGm";
    spectrum_board_snapshot_t source;
    spectrum_board_snapshot_t decoded;
    netchesszx_save_meta_t source_meta = { 0 };
    netchesszx_save_meta_t decoded_meta = { 0 };
    char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];

    spectrum_board_reset();
    spectrum_board_snapshot_save(&source);
    source_meta.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
    source_meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    source_meta.view_flags = NETCHESSZX_SAVE_VIEW_FLIPPED;
    source_meta.timers[0] = 1u;
    source_meta.timers[5] = 59u;
    check(spectrum_restore_build_b64(&source, &source_meta, b64),
          "real restore overlay host encoder");
    check(sizeof(expected_b64) - 1u == NETCHESSZX_SAVE_WIRE_B64_SIZE &&
              sizeof(expected_chunk_0) - 1u == NETCHESSZX_SAVE_WIRE_CHUNK_SIZE &&
              sizeof(expected_chunk_1) - 1u == NETCHESSZX_SAVE_WIRE_CHUNK_SIZE &&
              memcmp(b64, expected_b64, NETCHESSZX_SAVE_WIRE_B64_SIZE) == 0 &&
              memcmp(b64, expected_chunk_0,
                     NETCHESSZX_SAVE_WIRE_CHUNK_SIZE) == 0 &&
              memcmp(b64 + NETCHESSZX_SAVE_WIRE_CHUNK_SIZE,
                     expected_chunk_1, NETCHESSZX_SAVE_WIRE_CHUNK_SIZE) == 0,
          "real restore overlay exact 60-byte two-chunk vector");
    check(spectrum_restore_decode(b64, &decoded, &decoded_meta),
          "real restore overlay host decoder");
    check(memcmp(source.cells, decoded.cells, sizeof(source.cells)) == 0 &&
              source.side == decoded.side &&
              source.castle == decoded.castle && source.ep == decoded.ep &&
              source_meta.ply == decoded_meta.ply &&
              source_meta.flags == decoded_meta.flags &&
              source_meta.host_color == decoded_meta.host_color &&
              source_meta.view_flags == decoded_meta.view_flags &&
              memcmp(source_meta.timers, decoded_meta.timers,
                     sizeof(source_meta.timers)) == 0,
          "real restore overlay roundtrip");
    b64[0] = '!';
    check(!spectrum_restore_decode(b64, &decoded, &decoded_meta),
          "real restore overlay rejects invalid base64");
}

static uint8_t host_trace_push(uint8_t type,
                               uint8_t link_id,
                               uint8_t code,
                               uint16_t value,
                               const char *payload)
{
    DirectParityObservation *observation;
    size_t length = payload == 0 ? 0u : strlen(payload);

    if (host_trace == 0 || host_trace->count >= DIRECT_PARITY_TRACE_CAPACITY ||
        length >= DIRECT_PARITY_PAYLOAD_CAPACITY) {
        host_failed = 1u;
        return 0u;
    }
    observation = &host_trace->observations[host_trace->count++];
    observation->type = type;
    observation->link_id = link_id;
    observation->code = code;
    observation->value = value;
    observation->length = (uint8_t)length;
    if (length != 0u) {
        memcpy(observation->payload, payload, length);
    }
    observation->payload[length] = '\0';
    return 1u;
}

static uint8_t host_restore_phase(void)
{
    if (netchesszx_session_state.game_over) {
        return DIRECT_PARITY_PHASE_OVER;
    }
    return netchesszx_session_state.game_status_active ? DIRECT_PARITY_PHASE_ACTIVE
                              : DIRECT_PARITY_PHASE_READY;
}

static void host_observe_restore_apply(void)
{
    static const char ready[] = "R";
    static const char active[] = "A";
    static const char over[] = "O";
    uint8_t phase = host_restore_phase();
    const char *text = phase == DIRECT_PARITY_PHASE_READY
        ? ready : (phase == DIRECT_PARITY_PHASE_ACTIVE ? active : over);

    if (!host_restore_apply_pending || host_restore_control_pending == 0u ||
        host_restore_marker_seen != 1u) {
        host_failed = 1u;
        return;
    }
    host_restore_apply_pending = 0u;
    host_restore_marker_seen = 0u;
    host_restore_control_pending = 0u;
    if (host_restore_domain_pending) {
        host_restore_result_seen = 1u;
    }
    (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_GAME_RESTORE, netchesszx_session_state.game_ply, text);
}

static void host_observe_restore_rejected(uint8_t domain_result)
{
    if (!host_restore_control_pending || host_restore_apply_pending) {
        host_failed = 1u;
        return;
    }
    host_restore_control_pending = 0u;
    host_restore_domain_pending = 0u;
    host_restore_reject_seen = domain_result;
    (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_REQUEST_RESTORE,
                          DIRECT_PARITY_RESULT_REJECTED, "RN");
}

static void host_observe_side(void)
{
    if (host_role == DIRECT_PARITY_ROLE_GUEST &&
        netchesszx_session_peer_ready() &&
        (!host_side_initialized ||
         host_observed_color != netchesszx_local_color)) {
        host_side_initialized = 1u;
        host_observed_color = netchesszx_local_color;
        (void)host_trace_push(DIRECT_PARITY_OBS_SIDE,
                              DIRECT_PARITY_LINK_NONE,
                              netchesszx_local_color, 0u, 0);
    }
}

static void host_transport_feed(const uint8_t *bytes, size_t length)
{
    size_t i;

    for (i = 0u; i < length; ++i) {
        (void)direct_feed_uart_byte_ovl(bytes[i]);
    }
}

/* The target overlays spend one frame on an empty SpectraNext socket poll;
   the ESP/UART path spends two. Keep the host transcript clock aligned with
   the real overlay selected by the test define. */
static void host_empty_transport_poll(void)
{
    spectrum_frame_wait();
#ifndef NETCHESSZX_SPECTRANEXT
    spectrum_frame_wait();
#endif
}

static uint16_t host_timeout_frames(uint16_t frames_at_50_hz)
{
    if (frames_at_50_hz == 126u) {
        return (uint16_t)(NETCHESSZX_SESSION_DIRECT_REPLY_POLLS *
                          NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES);
    }
    return (uint16_t)((frames_at_50_hz * NETCHESSZX_SESSION_FRAME_HZ +
                       49u) / 50u);
}

static uint8_t host_transport_reject_intruder(const DirectParityStep *step)
{
#ifndef NETCHESSZX_SPECTRANEXT
    static const uint8_t validated_replies[] = ">\r\nSEND OK\r\nOK\r\n";
    static const uint8_t handshake_replies[] = "OK\r\n";
    static const uint8_t active_close_replies[] = ">\r\n1,CLOSED\r\nOK\r\n";
    char header[24];
    char expected[64];
    char payload[SPECTRUM_NET_PAYLOAD_MAX];
    uint8_t ctx[3] = { 0u, 0u, (uint8_t)sizeof(payload) };
    int header_length;
    int expected_length;
    uint8_t read_result;
#endif
    uint8_t active_closed;

    if (step == 0 || step->payload == 0 || step->length == 0u ||
        step->link_id == host_link_id || step->link_id > 4u ||
        direct_ipd_remaining != 0u || direct_intruder_link != 0xffu) {
        return 0u;
    }
#ifdef NETCHESSZX_SPECTRANEXT
    /* Next has one real socket and therefore no ESP-AT candidate-link
       framing. Preserve the target-neutral observation contract in this
       host-only fixture without pretending the overlay has extra links. */
    active_closed = (uint8_t)(host_fail_length == 4u &&
        host_fail_payload != 0 && memcmp(host_fail_payload, "BUSY", 4u) == 0);
    if (host_fail_length != 0u && !active_closed) {
        return 0u;
    }
    if (active_closed) {
        host_fail_payload = 0;
        host_fail_length = 0u;
    }
    if (direct_peer_valid &&
        !host_trace_push(DIRECT_PARITY_OBS_SEND, step->link_id,
                         0u, 0u, "BUSY")) {
        return 0u;
    }
    return host_trace_push(DIRECT_PARITY_OBS_CLOSE, step->link_id,
                           0u, 0u, 0);
#else
    header_length = snprintf(header, sizeof(header), "+IPD,%u,%u:",
                             (unsigned)step->link_id,
                             (unsigned)(step->length + 1u));
    if (header_length <= 0 || (size_t)header_length >= sizeof(header)) {
        return 0u;
    }
    host_transport_feed((const uint8_t *)header, (size_t)header_length);
    host_transport_feed(step->payload, step->length);
    host_transport_feed((const uint8_t *)"\n", 1u);
    if (direct_intruder_link != step->link_id || direct_ipd_remaining != 0u) {
        return 0u;
    }

    active_closed = (uint8_t)(host_fail_length == 4u &&
        host_fail_payload != 0 && memcmp(host_fail_payload, "BUSY", 4u) == 0);
    if (host_fail_length != 0u && !active_closed) {
        return 0u;
    }

    host_uart_tx_len = 0u;
    host_uart_tx[0] = '\0';
    host_uart_feed = active_closed ? active_close_replies
        : (direct_peer_valid ? validated_replies : handshake_replies);
    host_uart_feed_len = active_closed ? sizeof(active_close_replies) - 1u
        : (direct_peer_valid ? sizeof(validated_replies) - 1u
                             : sizeof(handshake_replies) - 1u);
    host_uart_feed_pos = 0u;
    direct_host_test_ptr = payload;
    read_result = direct_read_payload_ovl(ctx);
    if (read_result != (active_closed ? 0xfeu
                                      : (uint8_t)SPECTRUM_NET_READ_TIMEOUT) ||
        host_uart_feed_pos != host_uart_feed_len ||
        direct_link_closed != active_closed ||
        active_link != host_link_id || direct_intruder_link != 0xffu) {
        return 0u;
    }
    if (active_closed) {
        host_fail_payload = 0;
        host_fail_length = 0u;
    }

    if (direct_peer_valid) {
        expected_length = snprintf(expected, sizeof(expected),
            "AT+CIPSEND=%u,5\r\nBUSY\nAT+CIPCLOSE=%u\r\n",
            (unsigned)step->link_id, (unsigned)step->link_id);
    } else {
        expected_length = snprintf(expected, sizeof(expected),
            "AT+CIPCLOSE=%u\r\n", (unsigned)step->link_id);
    }
    if (expected_length <= 0 || (size_t)expected_length >= sizeof(expected) ||
        strcmp(host_uart_tx, expected) != 0) {
        return 0u;
    }
    if (direct_peer_valid &&
        !host_trace_push(DIRECT_PARITY_OBS_SEND, step->link_id,
                         0u, 0u, "BUSY")) {
        return 0u;
    }
    return host_trace_push(DIRECT_PARITY_OBS_CLOSE, step->link_id,
                           0u, 0u, 0);
#endif
}

static uint8_t host_transport_link_down(uint8_t link_id)
{
    char line[12];
    int length;

    if (link_id > 4u || direct_ipd_remaining != 0u) {
        return 0u;
    }
    length = snprintf(line, sizeof(line), "%u,CLOSED\r\n",
                      (unsigned)link_id);
    if (length <= 0 || (size_t)length >= sizeof(line)) {
        return 0u;
    }
    host_transport_feed((const uint8_t *)line, (size_t)length);
    return (uint8_t)(direct_link_closed == (link_id == host_link_id));
}

static void host_transport_begin_link(uint8_t link_id)
{
    direct_rx_count = 0u;
    direct_rx_head = 0u;
    direct_rx_payload_len = 0u;
    direct_ipd_remaining = 0u;
    direct_ipd_accept = 0u;
    direct_ipd_link = 0u;
    direct_link_closed = 0u;
    direct_peer_valid = 0u;
    direct_intruder_link = 0xffu;
    active_link = link_id;
    host_uart_feed = 0;
    host_uart_feed_len = 0u;
    host_uart_feed_pos = 0u;
    host_uart_tx_len = 0u;
    host_uart_tx[0] = '\0';
    reset_line_buf();
}

char *spectrum_net_payload_scratch(void)
{
    return host_payload;
}

int16_t spectrum_net_read_payload(char *payload, uint8_t payload_cap)
{
    const DirectParityStep *step;

    if (host_chat_ping_reset_expected) {
        host_failed = 1u;
        return -2;
    }
    host_chat_ping_reset_forbidden = 0u;

read_next_step:
    if (host_local_confirm_pending) {
        host_empty_transport_poll();
        return SPECTRUM_LINK_READ_TIMEOUT;
    }
    if (host_scenario == 0 || host_step_pos >= host_scenario->step_count) {
        host_failed = 1u;
        return -2;
    }
    step = &host_scenario->steps[host_step_pos];
    if (step->type == DIRECT_PARITY_IN_LOCAL ||
        step->type == DIRECT_PARITY_IN_DOMAIN ||
        step->type == DIRECT_PARITY_IN_SEND_FAIL ||
        step->type == DIRECT_PARITY_IN_SEND_PENDING ||
        step->type == DIRECT_PARITY_IN_TX_RESULT ||
        step->type == DIRECT_PARITY_IN_TX_GUARD_TIMEOUT ||
        step->type == DIRECT_PARITY_IN_DECISION) {
        host_empty_transport_poll();
        return SPECTRUM_LINK_READ_TIMEOUT;
    }
    /* A failed send that reaches another transport poll did not trigger
       fail-hard teardown (ACK PING is the intentional compact equivalent). */
    host_failed_send_pending = 0u;
    ++host_step_pos;
    if (step->type == DIRECT_PARITY_IN_RX &&
        step->link_id != host_link_id) {
        if (!host_transport_reject_intruder(step)) {
            host_failed = 1u;
            return -2;
        }
        goto read_next_step;
    }
    if (step->type == DIRECT_PARITY_IN_LINK_DOWN) {
        if (!host_transport_link_down(step->link_id)) {
            host_failed = 1u;
            return -2;
        }
        host_active_link_down = (uint8_t)(step->link_id == host_link_id);
        if (step->link_id == host_link_id) {
            return -2;
        }
        goto read_next_step;
    }
    if (step->type == DIRECT_PARITY_IN_RX && step->payload != 0 &&
        step->length == 6u && memcmp(step->payload, "RESIGN", 6u) == 0 &&
        netchesszx_session_state.game_status_active) {
        host_resign_control_pending = 1u;
    }
    if (step->type == DIRECT_PARITY_IN_RX && step->payload != 0 &&
        step->length == 2u && memcmp(step->payload, "RQ", 2u) == 0 &&
        host_role == DIRECT_PARITY_ROLE_GUEST) {
        host_restore_control_pending = 1u;
        host_restore_domain_pending = 1u;
    }
    if (step->type == DIRECT_PARITY_IN_TIMEOUT) {
        uint16_t frames = host_timeout_frames(step->value);

        --host_step_pos;
        if (!host_timeout_active) {
            if (frames == 0u ||
                (frames % NETCHESSZX_SESSION_DIRECT_EMPTY_POLL_FRAMES) != 0u) {
                host_failed = 1u;
                return -2;
            }
            host_timeout_deadline = (uint16_t)(host_now_ticks + frames);
            host_timeout_active = 1u;
        }
        /* Match the selected overlay's empty-read cadence. */
        host_empty_transport_poll();
        if (host_now_ticks == host_timeout_deadline) {
            host_timeout_active = 0u;
            ++host_step_pos;
        }
        return SPECTRUM_LINK_READ_TIMEOUT;
    }
    if (step->type != DIRECT_PARITY_IN_RX || step->payload == 0 ||
        step->length == 0u || step->length >= payload_cap) {
        host_failed = 1u;
        return -2;
    }
    memcpy(payload, step->payload, step->length);
    payload[step->length] = '\0';
    return 0;
}

uint8_t spectrum_net_send_text(const char *text)
{
    uint8_t traced;

    /* Advisory MACH is outside session-phase parity observations. */
    if (strncmp(text, "MACH ", 5) == 0) {
        return 1u;
    }

    host_observe_side();
    traced = host_trace_push(DIRECT_PARITY_OBS_SEND, host_link_id,
                             0u, 0u, text);
    if (traced && strcmp(text, "ACK RESIGN") == 0 &&
        host_local_resign_pending) {
        host_local_resign_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESIGN,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    }
    if (!traced || host_fail_length == 0u) {
        return traced;
    }
    if (host_fail_payload == 0 || strlen(text) != host_fail_length ||
        memcmp(text, host_fail_payload, host_fail_length) != 0) {
        return traced;
    }
    host_fail_payload = 0;
    host_fail_length = 0u;
    host_failed_send_pending = 1u;
    return 0u;
}

void spectrum_net_mqtt_set_publish_flags(uint8_t flags)
{
    (void)flags;
}

void spectrum_net_direct_peer_mark_valid(void) { direct_peer_valid = 1u; }
void spectrum_net_background_drain(void) {}
void spectrum_net_background_drain_clock(void) {}
/* DIRECT fixture: transcript RX is live/non-retained and has no unframed
   activity outside explicit steps. */
uint8_t spectrum_net_payload_flags(void) { return 0u; }
uint8_t netchesszx_session_mqtt_activate_side(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t netchesszx_session_mqtt_probe_seat(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t netchesszx_session_mqtt_publish_presence(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t netchesszx_session_mqtt_publish_offline(uint8_t route)
{
    (void)route;
    host_failed = 1u;
    return 0u;
}
uint8_t netchesszx_session_mqtt_publish_setup(uint8_t mode)
{
    (void)mode;
    host_failed = 1u;
    return 0u;
}
uint8_t spectrum_net_sync_time(void)
{
    host_failed = 1u;
    return 0u;
}
void spectrum_net_clock_retry_start(void) {}
void spectrum_net_clock_retry_cancel(void) {}
void spectrum_net_runtime_wait_frame(void) {}
uint8_t spectrum_net_preflight_run(uint8_t quiet) NETCHESSZX_FASTCALL
{
    (void)quiet;
    host_failed = 1u;
    return 0u;
}
const char *spectrum_net_last_ip(void)
{
    host_failed = 1u;
    return "";
}
uint8_t spectrum_net_runtime_clock_ready(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t netchesszx_asm_mqtt_strlen8(const char *text)
{
    uint8_t length = 0u;

    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

uint8_t netchesszx_asm_restore_chunk_step(uint8_t *mask,
                                          char *cache,
                                          const char *frame)
{
    uint8_t chunk = (uint8_t)(frame[3] - '0');
    uint8_t off = (uint8_t)(chunk * NETCHESSZX_SAVE_WIRE_CHUNK_SIZE);

    if ((*mask & RESTORE_RX_APPLIED) != 0u) {
        if (memcmp(cache + off, frame + 5u,
                   NETCHESSZX_SAVE_WIRE_CHUNK_SIZE) != 0) {
            *mask = 0u;
            return RESTORE_CHUNK_REJECT;
        }
        return RESTORE_CHUNK_REACK;
    }
    if ((*mask & RESTORE_RX_RECEIVE) == 0u) {
        return RESTORE_CHUNK_REJECT;
    }
    memcpy(cache + off, frame + 5u, NETCHESSZX_SAVE_WIRE_CHUNK_SIZE);
    *mask |= (uint8_t)(1u << chunk);
    return (*mask & RESTORE_RX_ALL) == RESTORE_RX_ALL
        ? RESTORE_CHUNK_COMPLETE : RESTORE_CHUNK_PARTIAL;
}

uint8_t spectrum_input_poll_event(void)
{
    const DirectParityStep *step;

    if (host_local_confirm_pending) {
        host_local_confirm_pending = 0u;
        if (confirm_action == CONFIRM_DRAW_SEND) {
            host_local_draw_pending = 1u;
        } else if (confirm_action == CONFIRM_RESET_SEND ||
                   confirm_action == CONFIRM_RESTART_GAME) {
            host_local_reset_pending = 1u;
        }
        if (confirm_action != CONFIRM_NONE) {
            return 'y';
        }
    }
    if (host_scenario != 0 && host_step_pos < host_scenario->step_count) {
process_next_step:
        step = &host_scenario->steps[host_step_pos];
        if (step->type == DIRECT_PARITY_IN_LOCAL) {
            ++host_step_pos;
            if (step->request == DIRECT_PARITY_REQUEST_START) {
                return 32u;
            }
            if (step->request == DIRECT_PARITY_REQUEST_BYE &&
                step->length == 0u) {
                confirm_action = CONFIRM_DISCONNECT;
                return 'y';
            }
            if ((step->request == DIRECT_PARITY_REQUEST_MOVE ||
                 step->request == DIRECT_PARITY_REQUEST_CHAT) &&
                step->payload != 0) {
                if (step->length >= NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE) {
                    host_failed = 1u;
                    return 0u;
                }
                memcpy(local_input, step->payload, step->length);
                local_input[step->length] = '\0';
                local_input_len = step->length;
                local_input_cursor = step->length;
                local_input_mode = 1u;
                if (step->request == DIRECT_PARITY_REQUEST_CHAT) {
                    if (host_fail_length != 0u) {
                        host_chat_ping_reset_forbidden = 1u;
                    } else {
                        host_chat_ping_reset_expected = 1u;
                    }
                }
                return 13u;
            }
            if (step->request == DIRECT_PARITY_REQUEST_DRAW &&
                step->length == 0u) {
                host_local_confirm_pending = 1u;
                return SPECTRUM_GUI_KEY_MENU_DRAW;
            }
            if (step->request == DIRECT_PARITY_REQUEST_RESET &&
                step->length == 0u) {
                host_local_confirm_pending = 1u;
                return SPECTRUM_GUI_KEY_MENU_REST;
            }
            if (step->request == DIRECT_PARITY_REQUEST_RESIGN &&
                step->length == 0u) {
                host_local_confirm_pending = 1u;
                host_resign_control_pending = 1u;
                host_local_resign_pending = 1u;
                return SPECTRUM_GUI_KEY_MENU_RESIGN;
            }
            if (step->request == DIRECT_PARITY_REQUEST_TAKEBACK &&
                step->length == 0u) {
                host_local_confirm_pending = 1u;
                host_local_takeback_pending = 1u;
                return SPECTRUM_GUI_KEY_MENU_TAKEBACK;
            }
            if (step->request == DIRECT_PARITY_REQUEST_RESTORE) {
                if (step->length == 0u) {
                    return KEY_CANCEL;
                }
                if (step->payload == 0 ||
                    step->length != NETCHESSZX_SAVE_WIRE_B64_SIZE) {
                    host_failed = 1u;
                    return 0u;
                }
                host_restore_file_payload = step->payload;
                host_restore_control_pending = 1u;
                host_restore_domain_pending = 0u;
                local_load_game("PARITY");
                host_restore_file_payload = 0;
                return 0u;
            }
            host_failed = 1u;
        } else if (step->type == DIRECT_PARITY_IN_DOMAIN) {
            ++host_step_pos;
            if (step->phase != 0u) {
                if (host_step_pos < host_scenario->step_count) {
                    goto process_next_step;
                }
            } else if (host_restore_result_seen) {
                if (step->request != DIRECT_PARITY_RESULT_ACCEPTED ||
                    step->payload == 0 || step->length != 1u ||
                    step->value != netchesszx_session_state.game_ply ||
                    step->payload[0] != host_restore_phase()) {
                    host_failed = 1u;
                }
                host_restore_result_seen = 0u;
                host_restore_domain_pending = 0u;
            } else if (host_restore_reject_seen) {
                if (step->request != DIRECT_PARITY_RESULT_REJECTED) {
                    host_failed = 1u;
                }
                host_restore_reject_seen = 0u;
            } else if (step->request != DIRECT_PARITY_RESULT_ACCEPTED) {
                host_failed = 1u;
            }
            if (host_takeback_result_seen) {
                if (step->value == 0u || netchesszx_session_state.game_ply != step->value - 1u) {
                    host_failed = 1u;
                }
                host_takeback_result_seen = 0u;
            }
        } else if (step->type == DIRECT_PARITY_IN_SEND_FAIL ||
                   step->type == DIRECT_PARITY_IN_SEND_PENDING) {
            ++host_step_pos;
            if (host_fail_length != 0u || step->payload == 0 ||
                step->length == 0u) {
                host_failed = 1u;
            } else {
                host_fail_payload = step->payload;
                host_fail_length = step->length;
                if (host_step_pos < host_scenario->step_count) {
                    goto process_next_step;
                }
            }
        } else if (step->type == DIRECT_PARITY_IN_TX_GUARD_TIMEOUT) {
            ++host_step_pos;
            if (!host_failed_send_pending) {
                host_failed = 1u;
            }
            host_failed_send_pending = 0u;
            if (host_step_pos < host_scenario->step_count) {
                goto process_next_step;
            }
        } else if (step->type == DIRECT_PARITY_IN_TX_RESULT) {
            ++host_step_pos;
            if (step->request != DIRECT_PARITY_TX_OK &&
                step->request != DIRECT_PARITY_TX_FAILED) {
                host_failed = 1u;
            } else if (step->phase == 0u) {
                if (!host_failed_send_pending) {
                    host_failed = 1u;
                }
                host_failed_send_pending = 0u;
            }
            if (host_step_pos < host_scenario->step_count) {
                goto process_next_step;
            }
        } else if (step->type == DIRECT_PARITY_IN_DECISION) {
            ++host_step_pos;
            if (step->request != DIRECT_PARITY_DECISION_ACCEPT &&
                step->request != DIRECT_PARITY_DECISION_REJECT) {
                host_failed = 1u;
            } else if (step->phase != 0u) {
                if (host_step_pos < host_scenario->step_count) {
                    goto process_next_step;
                }
            } else if (confirm_action == CONFIRM_TAKEBACK_ACCEPT &&
                       netchesszx_session_state.takeback_pending_ply != 0u) {
                return step->request == DIRECT_PARITY_DECISION_ACCEPT
                           ? 'y' : 'n';
            } else if (confirm_action == CONFIRM_RESTORE_ACCEPT) {
                return step->request == DIRECT_PARITY_DECISION_ACCEPT
                           ? 'y' : 'n';
            } else if (confirm_action == CONFIRM_RESET_ACCEPT &&
                       netchesszx_session_state.control_pending == 0u) {
                if (step->request == DIRECT_PARITY_DECISION_ACCEPT) {
                    host_control_reset_pending = 1u;
                    host_started_seen = 0u;
                    return 'y';
                }
                return 'n';
            } else if (confirm_action != CONFIRM_RESET_ACCEPT ||
                       netchesszx_session_state.control_pending != CONTROL_PENDING_DRAW_INCOMING) {
                host_failed = 1u;
            } else if (step->request == DIRECT_PARITY_DECISION_ACCEPT) {
                host_control_draw_pending = 1u;
                return 'y';
            } else {
                return 'n';
            }
        }
    }
    return 0u;
}
void spectrum_input_flush_until_release(void) {}
void spectrum_input_suppress_until_release(uint8_t key) { (void)key; }
static char host_move_lower(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return (char)(value + ('a' - 'A'));
    }
    return value;
}

static uint8_t host_move_square(const char **text, char **move)
{
    char value = host_move_lower(**text);

    if (value < 'a' || value > 'h') {
        return 0u;
    }
    *(*move)++ = value;
    ++*text;
    value = **text;
    if (value < '1' || value > '8') {
        return 0u;
    }
    *(*move)++ = value;
    ++*text;
    return 1u;
}

uint8_t spectrum_input_parse_move(const char *text, char *move)
{
    const char *source = text;
    char *destination = move;
    char promotion;

    /* Host transcription of asm/overlay/input_edit/entry_input_edit.asm's
       input_edit_parse_move_ovl_entry; this symbol is otherwise outside the
       judge because the target implementation is Z80-only. */
    while (*source == ' ') {
        ++source;
    }
    if (!host_move_square(&source, &destination)) {
        return 0u;
    }
    if (*source == '-' || *source == ' ') {
        ++source;
    }
    if (!host_move_square(&source, &destination)) {
        return 0u;
    }
    *destination = '\0';
    promotion = host_move_lower(*source);
    if (promotion == 'q' || promotion == 'r' || promotion == 'b' ||
        promotion == 'n') {
        *destination++ = promotion;
        *destination = '\0';
        ++source;
    }
    while (*source == ' ') {
        ++source;
    }
    return (uint8_t)(*source == '\0');
}

static void check_move_parser_contract(void)
{
    typedef struct HostMoveParserVector {
        const char *input;
        const char *output;
    } HostMoveParserVector;
    static const HostMoveParserVector accepted[] = {
        { "  e2e4", "e2e4" },
        { "a1h8", "a1h8" },
        { "h8a1", "h8a1" },
        { "E2-E4", "e2e4" },
        { "e2-e4", "e2e4" },
        { "e2 e4", "e2e4" },
        { "e2e4", "e2e4" },
        { "e7e8q", "e7e8q" },
        { "a2a1R", "a2a1r" },
        { "b7b8b", "b7b8b" },
        { "g2g1N", "g2g1n" },
        { "e2e4   ", "e2e4" },
        { "e7e8q ", "e7e8q" }
    };
    static const char *const rejected[] = {
        "\te2e4", "/draw", "", "i2e4", "e0e4", "e9e4",
        "e2  e4", "e2 - e4", "e2xe4", "e7e8k", "e7e8 q",
        "e7e8qq", "e2e4!", "e2e4 x"
    };
    struct HostMoveBuffer {
        char move[6];
        uint8_t guard[4];
    } output;
    uint8_t i;

    /* Golden branch map for entry_input_edit.asm's move parser:
       Leading-space loop: "  e2e4" / tab reject.
       File/rank bounds and ASCII fold: limits, i/0/9,
       command/empty rejects, and uppercase vectors.
       Exactly one '-', one space, or no separator; doubled/invalid
       separators reject.
       q/r/b/n immediate promotion and fold; invalid, separated, or
       doubled promotions reject.
       Trailing-space loop and NUL end check: trailing spaces accept;
       other suffixes reject.  Failed parses may leave partial output, so only
       the return value is contractual on rejected vectors. */
    for (i = 0u; i < (uint8_t)(sizeof(accepted) / sizeof(accepted[0])); ++i) {
        memset(&output, 0x5au, sizeof(output));
        if (!spectrum_input_parse_move(accepted[i].input, output.move) ||
            strcmp(output.move, accepted[i].output) != 0 ||
            output.guard[0] != 0x5au || output.guard[1] != 0x5au ||
            output.guard[2] != 0x5au || output.guard[3] != 0x5au) {
            fprintf(stderr, "FAIL: ASM move parser accepted vector %u\n",
                    (unsigned)i);
            exit(1);
        }
    }
    for (i = 0u; i < (uint8_t)(sizeof(rejected) / sizeof(rejected[0])); ++i) {
        memset(&output, 0x5au, sizeof(output));
        if (spectrum_input_parse_move(rejected[i], output.move) ||
            output.guard[0] != 0x5au || output.guard[1] != 0x5au ||
            output.guard[2] != 0x5au || output.guard[3] != 0x5au) {
            fprintf(stderr, "FAIL: ASM move parser rejected vector %u\n",
                    (unsigned)i);
            exit(1);
        }
    }
}

static void check_text_helper_contract(void)
{
    typedef struct HostU16Vector {
        uint16_t value;
        const char *text;
    } HostU16Vector;
    static const HostU16Vector vectors[] = {
        { 0u, "0" }, { 9u, "9" }, { 10u, "10" }, { 99u, "99" },
        { 100u, "100" }, { 999u, "999" }, { 1000u, "1000" },
        { 9999u, "9999" }, { 10000u, "10000" }, { 65535u, "65535" }
    };
    char buffer[49];
    char length_48[49];
    char *end;
    uint8_t i;

    /* Golden contracts for shrink_kernels.asm:472-483 and
       asm/spectrum/text.asm:8-27,31-95: length, exact text, terminating NUL,
       and returned pointer. */
    memset(length_48, 'x', 48u);
    length_48[48] = '\0';
    check(netchesszx_asm_mqtt_strlen8("") == 0u &&
              netchesszx_asm_mqtt_strlen8("x") == 1u &&
              netchesszx_asm_mqtt_strlen8(length_48) == 48u,
          "ASM strlen8 golden vectors");

    memset(buffer, 0x5au, sizeof(buffer));
    end = spectrum_append_text(buffer, "");
    check(end == buffer && buffer[0] == '\0' && buffer[1] == 0x5a,
          "ASM append text empty vector");
    memset(buffer, 0x5au, sizeof(buffer));
    end = spectrum_append_text(buffer, "ABC");
    check(end == buffer + 3 && strcmp(buffer, "ABC") == 0 &&
              buffer[4] == 0x5a,
          "ASM append text nonempty vector");

    for (i = 0u; i < (uint8_t)(sizeof(vectors) / sizeof(vectors[0])); ++i) {
        size_t length = strlen(vectors[i].text);

        memset(buffer, 0x5au, sizeof(buffer));
        end = spectrum_append_u16(buffer, vectors[i].value);
        if (end != buffer + length || strcmp(buffer, vectors[i].text) != 0 ||
            buffer[length + 1u] != 0x5a) {
            fprintf(stderr, "FAIL: ASM append u16 vector %u\n", (unsigned)i);
            exit(1);
        }
    }
}

void netchesszx_input_edit_render_overlay(void) {}
void netchesszx_input_edit_begin_empty_overlay(void)
{
    local_input[0] = '\0';
    local_input_len = 0u;
    local_input_cursor = 0u;
    local_input_mode = 1u;
}
void netchesszx_input_edit_stop_clear_overlay(void)
{
    local_input[0] = '\0';
    local_input_len = 0u;
    local_input_cursor = 0u;
    local_input_mode = 0u;
}
static void check_input_editor_driver(void)
{
    local_input_mode = 0u;
    local_input_len = 3u;
    local_input_cursor = 2u;
    local_input[0] = 'x';
    netchesszx_input_edit_begin_empty_overlay();
    check(local_input_mode == 1u && local_input_len == 0u &&
              local_input_cursor == 0u && local_input[0] == '\0',
          "host input editor begin invariants");
    local_input_len = 2u;
    local_input_cursor = 1u;
    local_input[0] = 'y';
    netchesszx_input_edit_stop_clear_overlay();
    check(local_input_mode == 0u && local_input_len == 0u &&
              local_input_cursor == 0u && local_input[0] == '\0',
          "host input editor stop invariants");

    local_input_mode = 1u;
    local_input_len = 2u;
    local_input_cursor = 2u;
    input_history_pos = 0u;
    memcpy(local_input, "ab", 3u);
    check(spectrum_editor_append_hot('c') && strcmp(local_input, "abc") == 0 &&
              local_input_len == 3u && local_input_cursor == 3u &&
              input_history_pos == INPUT_HISTORY_NONE,
          "resident input append hot path");
    local_input_cursor = 1u;
    check(!spectrum_editor_append_hot('d') && strcmp(local_input, "abc") == 0,
          "mid-line input remains in cold editor");
}
void netchesszx_input_edit_key_overlay(uint8_t key) { (void)key; }
void netchesszx_input_edit_history_add_overlay(const char *text) { (void)text; }

uint8_t spectrum_gui_status_phase(uint8_t phase)
{
    uint8_t platform = SPECTRUM_STATUS_UNPACK_PLATFORM(phase);
    uint8_t old_platform = SPECTRUM_STATUS_UNPACK_PLATFORM(host_status_phase);

    host_status_phase = phase;
    ++host_status_phase_calls;
    /* Compact MACH is a status event; normalize only the post-ready,
       known-platform observation to the common typed action. */
    if (host_trace != 0 && netchesszx_session_peer_ready_state &&
        platform != NETCHESS_PLAT_UNKNOWN && platform != old_platform) {
        (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_GAME_PLATFORM,
                              platform, 0);
    }
    return 1u;
}
static char host_peer_name_seen[NETCHESS_PLAYER_NAME_SIZE];

void spectrum_gui_moves_header(void)
{
    /* Compact MNAME is a header repaint; normalize a newly accepted name to
       the common typed action, as spectrum_gui_status_phase does for MACH. */
    if (host_trace != 0 && netchesszx_peer_name[0] != '\0' &&
        strcmp(netchesszx_peer_name, host_peer_name_seen) != 0) {
        (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_GAME_PEER_NAME, 0u,
                              netchesszx_peer_name);
    }
    strcpy(host_peer_name_seen, netchesszx_peer_name);
}
void spectrum_gui_set_status_error(const char *text) { (void)text; }
void spectrum_app_shift_clock(int8_t hour_delta) { (void)hour_delta; }
void spectrum_app_game_timer_start(void)
{
    spectrum_time_game_timer_start();
    if (host_restore_apply_pending) {
        host_observe_restore_apply();
        return;
    }
    if (host_local_reset_pending) {
        host_local_reset_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESET,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    } else if (host_control_reset_pending) {
        host_control_reset_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESET,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    }
    if (!host_started_seen) {
        host_started_seen = 1u;
        (void)host_trace_push(DIRECT_PARITY_OBS_STARTED,
                              DIRECT_PARITY_LINK_NONE, 0u, 0u, 0);
    }
}
void spectrum_app_game_timer_stop(void)
{
    spectrum_time_game_timer_stop();
    if (host_restore_apply_pending) {
        host_observe_restore_apply();
        return;
    }
    if (host_resign_control_pending) {
        host_resign_control_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESIGN,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    } else if (host_local_draw_pending) {
        host_local_draw_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_DRAW,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    } else if (host_control_draw_pending) {
        host_control_draw_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_DRAW,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    }
    host_started_seen = 0u;
}
void spectrum_app_game_timer_restore(const uint8_t *timers)
{
    spectrum_time_game_timer_restore(timers);
}
void spectrum_app_move_timer_reset(void) { spectrum_time_move_timer_reset(); }
void spectrum_gui_set_turn_label(uint8_t mode) { (void)mode; }
void spectrum_gui_set_connected(uint8_t connected)
{
    if (connected == 0u && !host_active_link_down) {
        (void)host_trace_push(DIRECT_PARITY_OBS_CLOSE, host_link_id,
                              0u, 0u, 0);
    }
    if (connected == 0u) {
        host_chat_ping_reset_forbidden = 0u;
        if (confirm_action != CONFIRM_NONE || netchesszx_session_state.restore_rx_mask != 0u) {
            host_failed = 1u;
        }
        host_restore_control_pending = 0u;
        host_restore_domain_pending = 0u;
        host_active_link_down = 0u;
        host_failed_send_pending = 0u;
        host_local_reset_pending = 0u;
    }
    host_connected_state = connected;
}
void spectrum_gui_notify(const char *text, uint8_t is_error)
{
    snprintf(host_last_notice, sizeof host_last_notice, "%s", text);
    (void)is_error;
    if (strcmp(text, "Load cancelled") == 0) {
        host_observe_restore_rejected(0u);
    } else if (strcmp(text, "RESET cancelled: no response") == 0 ||
               strcmp(text, "DRAW cancelled: no response") == 0 ||
               strcmp(text, "TAKEBACK cancelled: no response") == 0) {
        (void)host_trace_push(
            DIRECT_PARITY_OBS_CONTROL_RESULT,
            DIRECT_PARITY_LINK_NONE,
            text[0] == 'T' ? DIRECT_PARITY_REQUEST_TAKEBACK :
            text[0] == 'R' ? DIRECT_PARITY_REQUEST_RESET
                           : DIRECT_PARITY_REQUEST_DRAW,
            DIRECT_PARITY_RESULT_CANCELLED,
            host_timeout_active ? 0 :
            text[0] == 'R' ? "NACK RESET" : "NACK DRAW");
    } else if (strcmp(text, "RESET request withdrawn") == 0 ||
               strcmp(text, "DRAW request withdrawn") == 0 ||
               strcmp(text, "TAKEBACK request withdrawn") == 0) {
        (void)host_trace_push(
            DIRECT_PARITY_OBS_CONTROL_RESULT,
            DIRECT_PARITY_LINK_NONE,
            text[0] == 'T' ? DIRECT_PARITY_REQUEST_TAKEBACK :
            text[0] == 'R' ? DIRECT_PARITY_REQUEST_RESET
                           : DIRECT_PARITY_REQUEST_DRAW,
            DIRECT_PARITY_RESULT_EXPIRED, 0);
    }
}
void spectrum_gui_notify_persistent(const char *text)
{
    if (host_local_resign_pending && strcmp(text, msg_resignation_lost) == 0 &&
        !netchesszx_session_state.resign_pending) {
        host_local_resign_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESIGN,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    }
}
void spectrum_gui_notify_success(const char *text) { (void)text; }
void spectrum_gui_notify_msg(uint16_t packed)
{
    uint8_t msg_id = SPECTRUM_GUI_MSG_ID(packed);
    uint8_t kind = SPECTRUM_GUI_MSG_KIND(packed);

    host_last_msg_id = msg_id;

    if (kind > SPECTRUM_GUI_MSG_KIND_SUCCESS) {
        host_failed = 1u;
    }
    if (!host_ready_seen && netchesszx_session_peer_ready() &&
        (msg_id == SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT ||
         msg_id == SPECTRUM_GUI_MSG_OPPONENT_READY_GO)) {
        host_ready_seen = 1u;
        (void)host_trace_push(DIRECT_PARITY_OBS_READY,
                              DIRECT_PARITY_LINK_NONE, 0u, 0u, 0);
    }
    if (msg_id == SPECTRUM_GUI_MSG_OPPONENT_DRAW_REQUEST) {
        (void)host_trace_push(DIRECT_PARITY_OBS_DECISION,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_DRAW, 0u, 0);
    }
    if (msg_id == SPECTRUM_GUI_MSG_TAKEBACK_REQUEST) {
        (void)host_trace_push(DIRECT_PARITY_OBS_DECISION,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_TAKEBACK, 0u, 0);
    }
    if (msg_id == SPECTRUM_GUI_MSG_RESET_REQUEST ||
        msg_id == SPECTRUM_GUI_MSG_RESTART_REQUEST) {
        (void)host_trace_push(DIRECT_PARITY_OBS_DECISION,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESET, 0u, 0);
    }
    if (msg_id == SPECTRUM_GUI_MSG_DRAW_REJECTED && host_local_draw_pending) {
        host_local_draw_pending = 0u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE, DIRECT_PARITY_REQUEST_DRAW,
                              DIRECT_PARITY_RESULT_REJECTED, "NACK DRAW");
    }
    if (msg_id == SPECTRUM_GUI_MSG_TAKEBACK_DONE &&
        host_local_takeback_pending) {
        host_local_takeback_pending = 0u;
        host_takeback_result_seen = 1u;
        (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_TAKEBACK,
                              DIRECT_PARITY_RESULT_ACCEPTED, 0);
    }
    if (msg_id == SPECTRUM_GUI_MSG_RESTORE_REQUEST) {
        (void)host_trace_push(DIRECT_PARITY_OBS_DECISION,
                              DIRECT_PARITY_LINK_NONE,
                              DIRECT_PARITY_REQUEST_RESTORE, 0u, 0);
    }
    if ((msg_id == SPECTRUM_GUI_MSG_LOAD_DECLINED ||
         msg_id == SPECTRUM_GUI_MSG_LOAD_FAIL) &&
        host_restore_control_pending) {
        host_observe_restore_rejected(
            (uint8_t)(msg_id == SPECTRUM_GUI_MSG_LOAD_FAIL));
    }
}
void spectrum_app_tick(void) {}
void spectrum_gui_set_board_view(uint8_t local_black)
{
    host_board_flipped = local_black;
}
uint8_t spectrum_gui_is_board_flipped(void) { return host_board_flipped; }
uint8_t spectrum_gui_are_board_pieces_visible(void) { return host_pieces_visible; }
void spectrum_gui_toggle_board_view(void) { host_board_flipped ^= 1u; }
void spectrum_gui_set_board_snapshot(const char *cells)
{
    (void)cells;
    if (!host_restore_control_pending) {
        return;
    }
    if (host_restore_apply_pending) {
        host_failed = 1u;
        return;
    }
#ifdef NETCHESSZX_NEXT_BANKING
    host_next_sprites_reset_seen = 0u;
#endif
    host_restore_marker_seen = 0u;
    host_restore_apply_pending = 1u;
}
void spectrum_gui_set_board_pieces_visible(uint8_t visible)
{
    host_pieces_visible = visible;
}
void spectrum_gui_hide_board_pieces(void) { host_pieces_visible = 0u; }
void spectrum_gui_draw_board(void) {}
void spectrum_gui_redraw_board_view(void) {}
void spectrum_gui_restore_board_area(void) { host_about_visible = 0u; }
void spectrum_gui_restore_game_center(void)
{
    host_about_visible = 0u;
    host_side_panels_visible = 1u;
}
static uint8_t host_animate_calls;
void spectrum_gui_animate_board_pieces(void)
{
    host_pieces_visible = 1u;
    ++host_animate_calls;
}
void spectrum_gui_draw_status(void) {}
void spectrum_gui_restore_side_panels(void) { host_side_panels_visible = 1u; }
uint8_t spectrum_gui_side_panels_visible(void)
{
    return host_side_panels_visible;
}
void spectrum_gui_redraw_board_squares(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    if (host_restore_apply_pending && !host_next_sprites_reset_seen) {
        host_failed = 1u;
    }
#endif
}
void spectrum_render_about_off(void) {}
#ifdef NETCHESSZX_NEXT_BANKING
void spectrum_next_sprites_hide_all(void)
{
    host_next_sprites_reset_seen = 1u;
}
#endif
void spectrum_gui_reset_logs(void) {}
void spectrum_gui_reset_moves(void) {}
void spectrum_gui_add_move(const char *ply, const char *move)
{
    if (host_restore_apply_pending) {
        const char *expected = (netchesszx_session_state.game_ply & 1u) != 0u ? "1" : "2";

        ++host_restore_marker_seen;
        if (move != 0 || strcmp(ply, expected) != 0) {
            host_failed = 1u;
        }
    }
}
void spectrum_gui_prepare_move_row(void) {}
void spectrum_gui_remove_last_move(uint16_t ply)
{
    if (ply == 0u || netchesszx_session_state.game_ply != ply - 1u) {
        host_failed = 1u;
        return;
    }
    (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_GAME_TAKEBACK, ply, 0);
}
void spectrum_gui_add_chat(char who, const char *text)
{
    if (((uint8_t)who & NETCHESSZX_CHAT_EVENT_SIDE_FLAG) != 0u) {
        char side = (char)((uint8_t)who &
                           (uint8_t)~NETCHESSZX_CHAT_EVENT_SIDE_FLAG);

        if ((side != netchesszx_local_side_char() &&
             side != netchesszx_remote_side_char()) ||
            (strcmp(text, NETCHESS_PROTO_DRAW) != 0 &&
             strcmp(text, NETCHESS_PROTO_RESIGN) != 0 &&
             strcmp(text, NETCHESS_PROTO_TAKEBACK) != 0 &&
             strcmp(text, NETCHESSZX_UI_EVENT_DRAW_AGREED) != 0 &&
             strcmp(text, NETCHESSZX_UI_EVENT_RESIGNATION_LOST) != 0 &&
             strcmp(text, NETCHESSZX_UI_EVENT_OPPONENT_RESIGN) != 0)) {
            host_failed = 1u;
        }
        return;
    }
    (void)host_trace_push(
        DIRECT_PARITY_OBS_CHAT,
        DIRECT_PARITY_LINK_NONE,
        who == netchesszx_local_side_char() ? DIRECT_PARITY_CHAT_LOCAL
                                           : DIRECT_PARITY_CHAT_REMOTE,
        0u,
        text);
}
void spectrum_gui_prepare_move(const char *move) { (void)move; }
void spectrum_gui_apply_move(const char *move)
{
    /* A remote move was delivered at its board commit, before the ACK. */
    if (netchesszx_session_state.pending_local_ply == 0u) {
        return;
    }
    (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_GAME_LOCAL_MOVE,
                          netchesszx_session_state.game_ply, move);
}
void netchesszx_host_session_observe_remote_move(const char *move)
{
    if (netchesszx_session_view()->takeback_pending_ply == 0u) {
        host_local_takeback_pending = 0u;
    }

    (void)host_trace_push(DIRECT_PARITY_OBS_GAME,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_GAME_REMOTE_MOVE,
                          netchesszx_session_state.game_ply, move);
}
void netchesszx_host_session_observe_move_result(const char *notation)
{
    (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_REQUEST_MOVE,
                          DIRECT_PARITY_RESULT_ACCEPTED, notation);
}
void netchesszx_host_session_observe_move_rejection(const char *reason)
{
    (void)host_trace_push(DIRECT_PARITY_OBS_CONTROL_RESULT,
                          DIRECT_PARITY_LINK_NONE,
                          DIRECT_PARITY_REQUEST_MOVE,
                          DIRECT_PARITY_RESULT_REJECTED, reason);
}
void spectrum_gui_set_input(const char *text) { (void)text; }
void spectrum_gui_redraw_square(uint8_t row, uint8_t col)
{
    (void)row;
    (void)col;
    ++host_redraw_square_calls;
}
void spectrum_gui_mark_cursor(uint8_t row, uint8_t col, uint8_t selected)
{
    ++host_cursor_marks;
    (void)row;
    (void)col;
    (void)selected;
}
void spectrum_gui_clear_cursor_coords(void) {}
uint8_t spectrum_gui_show_about(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t spectrum_gui_about_visible(void) { return host_about_visible; }
uint8_t spectrum_gui_show_board_page(uint8_t page)
{
    host_about_visible = page;
    return 1u;
}
uint8_t spectrum_render_game_stats(void)
{
    return 1u;
}
uint8_t spectrum_render_chlog(void) { return 0u; }
uint8_t spectrum_gui_fileui_visible(void) { return host_fileui_visible; }
uint8_t spectrum_gui_handle_menu_key(uint8_t key)
{
    ++host_menu_key_calls;
    if (key == SPECTRUM_GUI_KEY_MENU) {
        host_menu_visible ^= 1u;
        return 0u;
    }
    return key;
}
void spectrum_gui_hide_menu(void) { host_menu_visible = 0u; }

void spectrum_info_show_setup(void) {}
void spectrum_info_clear_tail(uint8_t row) { (void)row; }
void spectrum_info_line(const char *line) { (void)line; }

void netchesszx_setup_render_edit_line(uint8_t row)
{
    (void)row;
}
uint16_t netchesszx_setup_compute_visible(uint16_t defined_mask)
{
    (void)defined_mask;
    host_failed = 1u;
    return 0u;
}
void netchesszx_setup_paint_attrs(void) {}
void netchesszx_setup_render_rows(uint8_t values,
                                  uint16_t visible_mask,
                                  uint16_t dirty_mask)
{
    (void)values;
    (void)visible_mask;
    (void)dirty_mask;
}
void netchesszx_setup_render_overlay(uint16_t force_dirty,
                                    uint16_t render_mode)
{
    (void)force_dirty;
    (void)render_mode;
}
uint8_t netchesszx_setup_step_overlay(uint8_t key)
{
    (void)key;
    host_failed = 1u;
    return 0u;
}
void netchesszx_board_theme_apply(uint8_t theme) { (void)theme; }
static uint8_t host_piece_set_loads;
static uint8_t host_piece_set_loaded;
uint8_t netchesszx_piece_set_load(uint8_t set)
{
    ++host_piece_set_loads;
    host_piece_set_loaded = set;
    return 1u;
}

void spectrum_board_clear_legal_hints(void) {}
void spectrum_board_show_legal_hints(uint8_t from_row, uint8_t from_col)
{
    (void)from_row;
    (void)from_col;
}

uint8_t spectrum_fileui_open_render(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t spectrum_fileui_rerender(void)
{
    host_failed = 1u;
    return 0u;
}
uint8_t spectrum_fileui_send_key(uint8_t key)
{
    (void)key;
    host_failed = 1u;
    return SPECTRUM_FILEUI_ACT_NONE;
}
const char *spectrum_fileui_selected_name(void)
{
    host_failed = 1u;
    return "";
}
uint8_t spectrum_recovery_capture(uint8_t flags, char *output)
{
    spectrum_board_snapshot_t snapshot;
    netchesszx_save_meta_t meta;

    /* RECOVERY encodes the live position. */
    spectrum_board_snapshot_save(&snapshot);
    host_recovery_cursor_marks = host_cursor_marks;
    meta.ply = netchesszx_session_view()->game_ply;
    meta.flags = flags;
    meta.host_color = netchesszx_session_identity_host_color();
    meta.view_flags = spectrum_gui_is_board_flipped()
        ? NETCHESSZX_SAVE_VIEW_FLIPPED
        : 0u;
    memcpy(meta.timers, spectrum_time_view()->game_timers,
           sizeof meta.timers);
    if ((!output && !host_recovery_write_ok) ||
        !spectrum_restore_build_b64(&snapshot, &meta,
                                    output ? output : (char *)host_recovery_saved)) {
        return 0u;
    }
    if (!output) {
        ++host_recovery_writes;
    }
    return 1u;
}
#ifdef NETCHESSZX_RECOVERY
static uint8_t host_recovery_retires;
void spectrum_recovery_retire(void)
{
    ++host_recovery_retires;
}
#endif
uint8_t spectrum_saveload_run(uint8_t entry, const char *name, const char *buf)
{
    if (entry & SPECTRUM_RECOVERY_DISPATCH) {
        entry &= 0x1fu;
    }
    if (name == 0) {
        if (entry == SPECTRUM_RECOVERY_RESUME) {
            if (!host_recovery_available) {
                return 0u;
            }
            if (buf != 0) {
                memcpy((char *)buf, host_recovery_payload,
                       NETCHESSZX_SAVE_WIRE_B64_SIZE);
            }
            return 1u;
        }
        if (entry == SPECTRUM_RECOVERY_WRITE && buf != 0) {
            memcpy(host_recovery_saved, buf, sizeof host_recovery_saved);
            ++host_recovery_writes;
            return 1u;
        }
        host_failed = 1u;
        return 0u;
    }
    if (entry == SPECTRUM_SAVELOAD_SAVE && buf != 0) {
        memcpy(host_manual_saved, buf, sizeof host_manual_saved);
        ++host_manual_saves;
        return 1u;
    }
    if (entry != SPECTRUM_OVL_SAVELOAD_LOAD_NCZS ||
        host_restore_file_payload == 0 || buf == 0) {
        host_failed = 1u;
        return 0u;
    }
    memcpy((char *)buf, host_restore_file_payload,
           NETCHESSZX_SAVE_WIRE_B64_SIZE);
    return 1u;
}
void spectrum_frame_wait(void)
{
    ++host_now_ticks;
}
uint16_t spectrum_frame_count(void) { return host_now_ticks; }

uint8_t direct_spectrum_run(const DirectParityScenario *scenario,
                            DirectParityTrace *trace)
{
    DirectParityObservation *observation;
    uint8_t role;
    uint8_t color;

    if (scenario == 0 || trace == 0 || scenario->step_count == 0u ||
        scenario->steps[0].type != DIRECT_PARITY_IN_LINK_UP) {
        return 0u;
    }
    if (scenario->role == DIRECT_PARITY_ROLE_HOST) {
        role = NETCHESSZX_SESSION_ROLE_HOST;
    } else if (scenario->role == DIRECT_PARITY_ROLE_GUEST) {
        role = NETCHESSZX_SESSION_ROLE_JOIN;
    } else {
        return 0u;
    }
    if (scenario->host_color == DIRECT_PARITY_COLOR_WHITE) {
        color = NETCHESSZX_COLOR_WHITE;
    } else if (scenario->host_color == DIRECT_PARITY_COLOR_BLACK) {
        color = NETCHESSZX_COLOR_BLACK;
    } else {
        return 0u;
    }

    memset(trace, 0, sizeof(*trace));
    host_scenario = scenario;
    host_trace = trace;
    host_step_pos = 1u;
    host_link_id = scenario->steps[0].link_id;
    host_role = scenario->role;
    host_side_initialized = 0u;
    host_observed_color = DIRECT_PARITY_COLOR_UNKNOWN;
    host_ready_seen = 0u;
    host_started_seen = 0u;
    host_failed = 0u;
    host_now_ticks = 0u;
    host_timeout_deadline = 0u;
    host_timeout_active = 0u;
    host_chat_ping_reset_expected = 0u;
    host_chat_ping_reset_forbidden = 0u;
    host_chat_ping_reset_violation = 0u;
    host_fail_payload = 0;
    host_fail_length = 0u;
    host_failed_send_pending = 0u;
    host_active_link_down = 0u;
    host_control_draw_pending = 0u;
    host_control_reset_pending = 0u;
    host_local_confirm_pending = 0u;
    host_local_draw_pending = 0u;
    host_local_reset_pending = 0u;
    host_local_takeback_pending = 0u;
    host_takeback_result_seen = 0u;
    host_resign_control_pending = 0u;
    host_local_resign_pending = 0u;
    host_connected_state = 0xffu;
    host_status_phase = 0u;
    host_status_phase_calls = 0u;
    status_phase_current = SPECTRUM_STATUS_PACK(STATUS_PHASE_CONNECTED,
                                                NETCHESS_PLAT_UNKNOWN);
    /* Production enters the link loop with the setup piece preview visible. */
    host_pieces_visible = 1u;
    host_board_flipped = 0u;
    host_side_panels_visible = 0u;
    host_about_visible = 0u;
    host_fileui_visible = 0u;
    host_restore_file_payload = 0;
    host_recovery_payload = 0;
    host_recovery_available = 0u;
    host_recovery_writes = 0u;
    host_recovery_write_ok = 1u;
    host_manual_saves = 0u;
    host_restore_apply_pending = 0u;
    host_restore_marker_seen = 0u;
    host_restore_domain_pending = 0u;
    host_restore_result_seen = 0u;
    host_restore_reject_seen = 0u;
    host_restore_control_pending = 0u;
#ifdef NETCHESSZX_NEXT_BANKING
    host_next_sprites_reset_seen = 0u;
#endif
    setup_restart_requested = 0u;
    host_transport_begin_link(host_link_id);
    netchesszx_session_configure(role, NETCHESSZX_TRANSPORT_DIRECT, color);
    while (1) {
        const DirectParityStep *next;

        game_message_loop();
        if (!host_trace_push(DIRECT_PARITY_OBS_ENDED,
                             DIRECT_PARITY_LINK_NONE, 0u, 0u, 0)) {
            return 0u;
        }
        if (host_step_pos >= scenario->step_count) {
            break;
        }
        next = &scenario->steps[host_step_pos];
        if (next->type != DIRECT_PARITY_IN_LINK_UP) {
            host_failed = 1u;
            break;
        }
        ++host_step_pos;
        host_link_id = next->link_id;
        host_side_initialized = 0u;
        host_observed_color = DIRECT_PARITY_COLOR_UNKNOWN;
        host_ready_seen = 0u;
        host_started_seen = 0u;
        host_active_link_down = 0u;
        setup_restart_requested = 0u;
        host_transport_begin_link(host_link_id);
    }
    if (host_failed || host_timeout_active || host_fail_length != 0u ||
        host_chat_ping_reset_expected || host_chat_ping_reset_forbidden ||
        host_chat_ping_reset_violation ||
        host_failed_send_pending ||
        host_control_draw_pending || host_control_reset_pending ||
        host_local_confirm_pending || host_local_draw_pending ||
        host_local_reset_pending || host_local_takeback_pending ||
        host_takeback_result_seen || host_resign_control_pending ||
        host_restore_file_payload != 0 || host_restore_apply_pending ||
        host_restore_domain_pending || host_restore_result_seen ||
        host_restore_reject_seen ||
        host_restore_control_pending ||
        host_connected_state == 0xffu || host_pieces_visible == 0xffu ||
        host_step_pos != scenario->step_count ||
        netchesszx_session_peer_ready() ||
        trace->count >= DIRECT_PARITY_TRACE_CAPACITY) {
        fprintf(stderr,
                "Spectrum runner state: failed=%u timeout=%u fail_len=%u "
                "draw=%u reset=%u confirm=%u local_draw=%u local_reset=%u "
                "step=%u/%u ready=%u trace=%u\n",
                (unsigned)host_failed, (unsigned)host_timeout_active,
                (unsigned)host_fail_length,
                (unsigned)host_control_draw_pending,
                (unsigned)host_control_reset_pending,
                (unsigned)host_local_confirm_pending,
                (unsigned)host_local_draw_pending,
                (unsigned)host_local_reset_pending,
                (unsigned)host_step_pos, (unsigned)scenario->step_count,
                (unsigned)netchesszx_session_peer_ready(),
                (unsigned)trace->count);
        for (observation = trace->observations;
             observation < &trace->observations[trace->count];
             ++observation) {
            fprintf(stderr, "  trace %u: %u/%u/%u/%u/%.*s\n",
                    (unsigned)(observation - trace->observations),
                    (unsigned)observation->type,
                    (unsigned)observation->link_id,
                    (unsigned)observation->code,
                    (unsigned)observation->value,
                    (int)observation->length, observation->payload);
        }
        return 0u;
    }
    return 1u;
}

static void check_trace(const DirectParityScenario *scenario,
                        const DirectParityTrace *trace,
                        const char *runner)
{
    uint8_t i;

    if (trace->count != scenario->expected_count) {
        fprintf(stderr, "FAIL: %s %s observation count %u != %u\n",
                scenario->id, runner, (unsigned)trace->count,
                (unsigned)scenario->expected_count);
        for (i = 0u; i < trace->count; ++i) {
            fprintf(stderr, "  trace %u: %u/%u/%u/%u/%.*s\n",
                    (unsigned)i,
                    (unsigned)trace->observations[i].type,
                    (unsigned)trace->observations[i].link_id,
                    (unsigned)trace->observations[i].code,
                    (unsigned)trace->observations[i].value,
                    (int)trace->observations[i].length,
                    trace->observations[i].payload);
        }
        exit(1);
    }
    for (i = 0u; i < trace->count; ++i) {
        if (trace->observations[i].type != scenario->expected[i].type ||
            trace->observations[i].link_id != scenario->expected[i].link_id ||
            trace->observations[i].code != scenario->expected[i].code ||
            trace->observations[i].length != scenario->expected[i].length ||
            trace->observations[i].value != scenario->expected[i].value ||
            memcmp(trace->observations[i].payload,
                   scenario->expected[i].payload,
                   trace->observations[i].length) != 0) {
            fprintf(stderr,
                    "FAIL: %s %s observation %u got=%u/%u/%u/%u/%.*s "
                    "expected=%u/%u/%u/%u/%.*s\n",
                    scenario->id, runner, (unsigned)i,
                    (unsigned)trace->observations[i].type,
                    (unsigned)trace->observations[i].link_id,
                    (unsigned)trace->observations[i].code,
                    (unsigned)trace->observations[i].value,
                    (int)trace->observations[i].length,
                    trace->observations[i].payload,
                    (unsigned)scenario->expected[i].type,
                    (unsigned)scenario->expected[i].link_id,
                    (unsigned)scenario->expected[i].code,
                    (unsigned)scenario->expected[i].value,
                    (int)scenario->expected[i].length,
                    scenario->expected[i].payload);
            exit(1);
        }
    }
}

static void check_mqtt_peer_loss_blocks_taboption(void)
{
    char payload[] = "";

    host_menu_key_calls = 0u;
    host_menu_visible = 0u;
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    check(process_local_key(SPECTRUM_GUI_KEY_MENU),
          "TABOPTION key consumed while peer present");
    check(host_menu_visible, "TABOPTION opens while peer present");
    check(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
                                        payload, 0u) == SESSION_DISPATCH_HANDLED,
          "MQTT peer-offline handled");
    check(!netchesszx_session_peer_ready(), "MQTT peer cleared after offline");
    check(!host_menu_visible, "TABOPTION closed on MQTT peer loss");
    check(process_local_key(SPECTRUM_GUI_KEY_MENU),
          "TABOPTION key consumed while peer absent");
    check(host_menu_key_calls == 1u,
          "TABOPTION handler not reached while peer absent");

    setup_restart_requested = 0u;
    netchesszx_mqtt_session_id = 0u;
    check(process_local_key(KEY_CANCEL),
          "BREAK consumed while waiting without peer");
    check(setup_restart_requested,
          "BREAK returns to setup while waiting without peer");

    setup_restart_requested = 0u;
    netchesszx_session_peer_mark_ready();
    check(process_local_key(SPECTRUM_GUI_KEY_MENU),
          "TABOPTION key consumed after new peer");
    check(host_menu_key_calls == 2u && host_menu_visible,
          "TABOPTION available again after new peer");
    spectrum_gui_hide_menu();
    netchesszx_session_state.game_over = 1u;
    check(process_local_key(SPECTRUM_GUI_KEY_MENU),
          "TABOPTION key consumed at rematch boundary");
    check(host_menu_key_calls == 3u && host_menu_visible,
          "TABOPTION remains available at rematch boundary with peer");
    spectrum_gui_hide_menu();
    netchesszx_session_state.game_ply = 42u;
    host_pieces_visible = 1u;
    check(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
                                        payload, 0u) == SESSION_DISPATCH_HANDLED,
          "MQTT peer-offline handled from game over");
    check(!netchesszx_session_state.game_over && netchesszx_session_state.game_ply == 0u && !host_pieces_visible,
          "MQTT peer loss discards finished board and history");
}

static void check_mqtt_peer_identity_reset(void)
{
    char payload[] = "";

    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.game_over = 0u;
    local_turn = 0u;
    netchesszx_session_state.start_pending = 0u;
    confirm_action = CONFIRM_NONE;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    status_phase_current = SPECTRUM_STATUS_PACK(STATUS_PHASE_CONNECTED,
                                                NETCHESS_PLAT_MAC);
    host_status_phase_calls = 0u;

    check(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
                                        payload, 0u) == SESSION_DISPATCH_HANDLED,
          "MQTT peer replacement/offline handled");
    check(host_status_phase_calls != 0u &&
              SPECTRUM_STATUS_UNPACK_PHASE(host_status_phase) ==
                  STATUS_PHASE_CONNECTED &&
              SPECTRUM_STATUS_UNPACK_PLATFORM(host_status_phase) ==
                  NETCHESS_PLAT_UNKNOWN,
          "MQTT peer reset renders unknown platform");

    netchesszx_session_peer_mark_ready();
    check(session_presence_handle_event(NETCHESSZX_SESSION_EVENT_MACH_PC,
                                        payload, 0u) == SESSION_DISPATCH_HANDLED &&
              SPECTRUM_STATUS_UNPACK_PHASE(host_status_phase) ==
                  STATUS_PHASE_CONNECTED &&
              SPECTRUM_STATUS_UNPACK_PLATFORM(host_status_phase) ==
                  NETCHESS_PLAT_PC,
          "MACH PC updates ready replacement peer platform");
}

static void check_direct_guest_disconnect_forgets_side(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_host_color_ready = 1u;
    clear_disconnected_session_state();
    check(!netchesszx_host_color_ready,
          "DIRECT guest disconnect forgets learned side");

    /* The listen/reconnect wait after a DIRECT loss never restores the side
       panels, so the disconnect itself must repaint the MOVES header. */
    netchesszx_session_peer_mark_ready();
    strcpy(netchesszx_peer_name, "PEPE");
    spectrum_gui_moves_header();
    check(strcmp(host_peer_name_seen, "PEPE") == 0,
          "MOVES header shows the DIRECT peer name");
    handle_opponent_disconnected();
    check(netchesszx_peer_name[0] == '\0' && host_peer_name_seen[0] == '\0',
          "DIRECT disconnect repaints the MOVES header without the peer name");
}

static void check_cursor_reselects_own_piece(void)
{
    uint8_t saved_hints = netchesszx_movement_hints;

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.control_pending = 0u;
    local_turn = 1u;
    netchesszx_session_state.pending_local_ply = 0u;
    netchesszx_movement_hints = 0u;
    host_board_flipped = 0u;
    cursor_home(0x0406u);
    cursor_move(KEY_DOWN);
    cursor_move(KEY_LEFT);
    cursor_move(KEY_LEFT);
    cursor_move(KEY_LEFT);
    spectrum_ux_board_activate();
    cursor_move(KEY_RIGHT);
    cursor_move(KEY_RIGHT);
    cursor_move(KEY_RIGHT);
    cursor_move(KEY_RIGHT);
    cursor_move(KEY_RIGHT);

    check(cursor_select_or_move('\r'), "own-piece reselection consumed");
    check(spectrum_ux_selected_position() == spectrum_ux_cursor_position(),
          "own-piece reselection changes move origin");

    netchesszx_session_state.game_status_active = 0u;
    local_turn = 0u;
    netchesszx_movement_hints = saved_hints;
    cursor_hide();
}

static void check_turn_grant_clears_stale_cursor(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    netchesszx_session_state.game_status_active = 1u;
    local_turn = 1u;
    netchesszx_session_state.pending_local_ply = 0u;
    host_board_flipped = 0u;
    cursor_home(0x0406u);
    cursor_move(KEY_DOWN);
    cursor_move(KEY_RIGHT);
    cursor_move(KEY_RIGHT);
    spectrum_ux_board_activate();
    local_turn = 0u;
    host_redraw_square_calls = 0u;

    turn_set_notice(1u, 0u);
    check(host_redraw_square_calls != 0u && spectrum_ux_selected_position() == 0xffffu,
          "turn grant clears stale board cursor before repositioning");

    netchesszx_session_state.game_status_active = 0u;
    local_turn = 0u;
    netchesszx_session_peer_clear();
}

static void check_setup_preview_skips_first_start_redraw(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.game_over = 0u;
    netchesszx_session_state.start_pending = 0u;
    netchesszx_session_state.resign_pending = 0u;
    pending_local_clear();
    host_pieces_visible = 1u;
    host_animate_calls = 0u;

    host_status_phase_calls = 0u;
    game_start_state();
    check(host_status_phase_calls == 1u,
          "GAME START paints the status and turn once");
    check(host_animate_calls == 0u && host_pieces_visible,
          "first start keeps setup pieces without redraw");

    host_animate_calls = 0u;
    netchesszx_session_state.game_over = 1u;
    host_status_phase_calls = 0u;
    game_start_state();
    check(host_status_phase_calls == 1u,
          "GAME START paints the status and turn once");
    check(host_animate_calls == 1u && host_pieces_visible,
          "rematch still animates pieces onto the board");

    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.game_over = 0u;
    host_pieces_visible = 1u;
    host_board_flipped = 0u;
    host_animate_calls = 0u;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    host_status_phase_calls = 0u;
    game_start_state();
    check(host_status_phase_calls == 1u,
          "GAME START paints the status and turn once");
    check(host_animate_calls == 1u && host_pieces_visible && host_board_flipped,
          "guest start animates when setup preview faces the other way");

    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.game_over = 0u;
    local_turn = 0u;
    netchesszx_session_peer_clear();
}

static void check_local_move_ply_overflow(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.control_pending = 0u;
    pending_local_clear();
    takeback_clear();
    netchesszx_session_state.game_ply = 65535u;
    host_uart_feed_len = 0u;
    host_uart_feed_pos = 0u;
    host_uart_tx_len = 0u;

    check(send_local_move("e2e4") == LOCAL_MOVE_REJECTED &&
              host_uart_tx_len == 0u &&
              netchesszx_session_state.pending_local_ply == 0u &&
              netchesszx_session_state.game_ply == 65535u,
          "local move beyond ply 65535 rejected before transmission");

    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.game_ply = 0u;
    netchesszx_session_peer_clear();
}

static void check_chat_editor_during_pending_control(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    host_menu_visible = 0u;
    confirm_action = CONFIRM_NONE;
    netchesszx_session_state.pending_local_ply = 0u;
    local_input_mode = 0u;
    netchesszx_session_state.game_over = 0u;
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.control_pending = CONTROL_PENDING_DRAW_SENT;

    check(process_local_key(13u), "chat editor opens during pending draw");
    check(local_input_mode, "pending draw does not block chat editor");
    host_board_flipped = 0u;
    check(process_local_key(SPECTRUM_GUI_KEY_MENU_FLIP) && !host_board_flipped,
          "active editor consumes flip key before board presentation");
    check(process_local_key(SPECTRUM_GUI_KEY_MENU),
          "TABOPTION opens from chat editor");
    check(!local_input_mode && host_menu_visible,
          "TABOPTION closes chat editor before opening");
    spectrum_gui_hide_menu();

    check(process_local_key(SPECTRUM_GUI_KEY_MENU_FLIP) && host_board_flipped,
          "inactive editor allows board flip");

    netchesszx_session_state.game_over = 1u;
    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_state.control_pending = CONTROL_PENDING_RESET;
    netchesszx_session_state.last_control_accept = CONTROL_ACCEPT_NONE;
    check(process_local_key(13u), "Enter during a pending restart");
    check(local_input_mode && confirm_action == CONFIRM_NONE,
          "pending restart keeps chat open and arms no prompt");
    edit_stop_clear();

    netchesszx_session_state.control_pending = 0u;
    netchesszx_session_state.last_control_accept = CONTROL_ACCEPT_NONE;
    netchesszx_session_state.game_over = 0u;
}

static void check_non_chat_input_liveness_status(void)
{
    uint8_t old_piece_set;

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    host_menu_visible = 0u;
    confirm_action = CONFIRM_NONE;
    netchesszx_session_state.pending_local_ply = 0u;
    netchesszx_session_state.takeback_pending_ply = 0u;
    netchesszx_session_state.restore_rx_mask = 0u;
    netchesszx_session_state.game_over = 0u;
    netchesszx_session_state.game_status_active = 1u;

    local_input_mode = 1u;
    check(process_local_key('x') == 1u,
          "typing without submit has normal key status");

    local_input[0] = '\0';
    local_input_len = 0u;
    local_input_cursor = 0u;
    check(process_local_key(13u) == 1u && !local_input_mode,
          "empty submit has normal key status");

    check(process_local_key(13u) == 1u && local_input_mode,
          "opening editor has normal key status");
    check(process_local_key(KEY_CANCEL) == 1u && !local_input_mode,
          "closing editor has normal key status");

    old_piece_set = netchesszx_piece_set_index;
    host_piece_set_loads = 0u;
    check(process_local_key(SPECTRUM_GUI_KEY_MENU_ASSET) == 1u &&
              host_piece_set_loads == 1u &&
              host_piece_set_loaded == (uint8_t)((old_piece_set + 1u) %
                  NETCHESSZX_PIECE_SET_COUNT) &&
              netchesszx_piece_set_index == host_piece_set_loaded,
          "menu ASSET cycles and loads the next piece set");
    netchesszx_piece_set_index = old_piece_set;

    check(process_local_key(SPECTRUM_GUI_KEY_MENU_DRAW) == 1u &&
              confirm_action == CONFIRM_DRAW_SEND,
          "menu DRAW reuses the local command policy");
    confirm_action = CONFIRM_NONE;
    edit_stop_clear();
    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_peer_clear();
}

static void check_about_dismisses_before_confirmation(void)
{
    host_about_visible = 1u;
    confirm_action = CONFIRM_TAKEBACK_ACCEPT;
    netchesszx_session_state.takeback_pending_ply = 2u;

    check(process_local_key('x') == 1u && !host_about_visible,
          "About dismisses before hidden confirmation input");
    check(confirm_action == CONFIRM_TAKEBACK_ACCEPT && netchesszx_session_state.takeback_pending_ply == 2u,
          "About dismissal preserves pending confirmation");

    confirm_action = CONFIRM_NONE;
    netchesszx_session_state.takeback_pending_ply = 0u;
}

static void check_game_stats_keys(void)
{
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_over = 1u;
    netchesszx_session_state.resign_pending = 0u;
    netchesszx_session_state.control_pending = 0u;
    local_turn = 0u;
    local_input_mode = 0u;
    confirm_action = CONFIRM_NONE;
    check(process_local_key(13u) && local_input_mode &&
              confirm_action == CONFIRM_NONE,
          "Enter after OVER opens chat, not the restart prompt");
    check(process_local_key(KEY_CANCEL) && !local_input_mode,
          "chat after OVER closes normally");
    check(process_local_key('x') && !local_input_mode &&
              confirm_action == CONFIRM_RESTART_GAME,
          "a key after OVER shows the statistics/restart prompt");
    check(process_local_key(13u) && local_input_mode &&
              confirm_action == CONFIRM_NONE,
          "Enter on the restart prompt opens chat and drops the offer");
    check(process_local_key(KEY_CANCEL) && !local_input_mode,
          "chat from the restart prompt closes normally");
    check(process_local_key('x') && confirm_action == CONFIRM_RESTART_GAME,
          "the next key re-arms the restart prompt");
    check(process_local_key('s') && host_about_visible == 3u,
          "S opens statistics from the restart prompt");
    check(process_local_key('y') && !host_about_visible &&
              confirm_action == CONFIRM_RESTART_GAME &&
              !netchesszx_session_state.control_pending,
          "any key closes statistics without requesting RESET");
    check(process_local_key('s') && host_about_visible == 3u,
          "statistics can be reopened without changing OVER");
    check(process_local_key('n') && !host_about_visible &&
              confirm_action == CONFIRM_RESTART_GAME,
          "closing statistics preserves the restart prompt");
    check(process_local_key('s') && host_about_visible == 3u,
          "statistics reopen before a peer request");
    confirm_action = CONFIRM_RESET_ACCEPT; /* peer RESET arrived meanwhile */
    check(process_local_key('y') && !host_about_visible &&
              confirm_action == CONFIRM_RESET_ACCEPT &&
              !netchesszx_session_state.control_pending,
          "closing statistics never answers the peer's restart request");
    check(process_local_key('s') && host_about_visible == 3u &&
              confirm_action == CONFIRM_RESET_ACCEPT,
          "S opens statistics from the opponent's restart request in OVER");
    check(process_local_key('x') && !host_about_visible &&
              confirm_action == CONFIRM_RESET_ACCEPT,
          "closing statistics keeps the opponent's restart request");
    /* Host waiting for the answer: WAITING ACK only; no statistics, no
       withdrawal; BREAK leaves to Setup. */
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_over = 1u;
    confirm_action = CONFIRM_NONE;
    netchesszx_session_state.control_pending = CONTROL_PENDING_RESET;
    check(process_local_key('x') && confirm_action == CONFIRM_NONE &&
              netchesszx_session_state.control_pending == CONTROL_PENDING_RESET,
          "other keys keep a pending restart request");
    check(process_local_key('s') && !host_about_visible &&
              netchesszx_session_state.control_pending == CONTROL_PENDING_RESET,
          "S does not open statistics while the restart request is pending");
    check(process_local_key('n') && confirm_action == CONFIRM_NONE &&
              netchesszx_session_state.control_pending == CONTROL_PENDING_RESET,
          "N does not withdraw the restart request");
    setup_restart_requested = 0u;
    (void)process_local_key(KEY_CANCEL);
    check(setup_restart_requested, "BREAK while waiting leaves to Setup");
    setup_restart_requested = 0u;

    /* Guest: no offer of its own; it waits for the host's with S. */
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_over = 1u;
    netchesszx_session_state.control_pending = 0u;
    confirm_action = CONFIRM_NONE;
    check(process_local_key('x') && confirm_action == CONFIRM_NONE &&
              !netchesszx_session_state.control_pending,
          "a guest key after OVER arms no restart request");
    check(process_local_key('y') && confirm_action == CONFIRM_NONE &&
              !netchesszx_session_state.control_pending,
          "Y on the guest's wait sends nothing");
    check(process_local_key('s') && host_about_visible == 3u,
          "S opens statistics from the guest's wait");
    check(process_local_key('x') && !host_about_visible &&
              confirm_action == CONFIRM_NONE,
          "closing statistics returns the guest to its wait");
    (void)process_local_key(KEY_CANCEL);
    check(setup_restart_requested, "BREAK on the guest's wait leaves to Setup");
    setup_restart_requested = 0u;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_state.control_pending = 0u;
    confirm_action = CONFIRM_NONE;
    netchesszx_session_state.game_over = 0u;
    netchesszx_session_peer_clear();
}

static void check_restore_transfer_blocks_file_and_editor(void)
{
    char partial[NETCHESSZX_SAVE_WIRE_CHUNK_SIZE];

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    host_failed = 0u;
    host_fileui_visible = 0u;
    host_menu_visible = 0u;
    confirm_action = CONFIRM_NONE;
    local_input_mode = 0u;
    netchesszx_session_state.game_over = 0u;
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.restore_rx_mask = RESTORE_TX_PENDING;

    check(process_local_key(13u),
          "restore TX consumes editor key");
    check(!local_input_mode,
          "restore TX blocks editor workspace reuse");
    check(process_local_key(SPECTRUM_GUI_KEY_MENU_FILE),
          "restore TX consumes FILE key");
    check(!host_failed && !host_fileui_visible,
          "restore TX blocks FILE workspace replacement");

    memset(restore_b64_pending, 'R', sizeof(restore_b64_pending));
    memcpy(partial, restore_b64_pending, sizeof(partial));
    netchesszx_session_state.restore_rx_mask = (uint8_t)(RESTORE_RX_RECEIVE | 1u);

    check(process_local_key(13u),
          "restore transfer consumes editor key");
    check(!local_input_mode,
          "restore transfer blocks editor workspace reuse");
    check(process_local_key(SPECTRUM_GUI_KEY_MENU_FILE),
          "restore transfer consumes FILE key");
    check(!host_failed && !host_fileui_visible,
          "restore transfer blocks FILE workspace replacement");

    host_fileui_visible = 1u;
    check(process_local_key(KEY_DOWN),
          "restore transfer outranks visible FILE browser");
    check(!host_failed,
          "restore transfer blocks FILE browser actions");

    host_fileui_visible = 0u;
    host_board_flipped = 0u;
    local_turn = 1u;
    netchesszx_session_state.pending_local_ply = 0u;
    cursor_home(0x0406u);
    cursor_move(KEY_UP);
    cursor_move(KEY_UP);
    check(process_local_key(KEY_DOWN),
          "restore receive consumes cursor key");
    check(netchesszx_session_state.pending_local_ply == 0u && spectrum_ux_cursor_position() == 36u &&
              memcmp(partial, restore_b64_pending, sizeof(partial)) == 0,
          "restore receive preserves partial chunk and sends no move");

    netchesszx_session_state.restore_rx_mask = RESTORE_RX_APPLIED;
    check(process_local_key(KEY_DOWN),
          "applied restore cache permits cursor key");
    check(spectrum_ux_cursor_position() == 44u,
          "applied restore cache does not freeze interaction");

    host_fileui_visible = 0u;
    netchesszx_session_state.restore_rx_mask = 0u;
    netchesszx_session_state.game_status_active = 0u;
    netchesszx_session_peer_clear();
}

static void check_loaded_timers_survive_application(void)
{
    const uint8_t flags[] = { NETCHESSZX_SAVE_FLAG_ACTIVE, 0u,
        NETCHESSZX_SAVE_FLAG_ACTIVE | NETCHESSZX_SAVE_FLAG_GAME_OVER };
    const uint8_t timers[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    netchesszx_save_meta_t meta = { 0 };
    DirectParityTrace trace = { 0 };
    uint8_t i;

    host_trace = &trace;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    spectrum_board_reset();
    spectrum_board_snapshot_save(&restore_snapshot);
    memcpy(meta.timers, timers, sizeof timers);
    for (i = 0u; i < sizeof flags; ++i) {
        meta.flags = flags[i];
        saveload_apply_snapshot(&meta, 0u);
        check(memcmp(spectrum_time_view()->game_timers, timers, sizeof timers) == 0,
              "snapshot application preserves all six saved timer bytes");
        check(spectrum_time_view()->game_timer_active ==
                  (flags[i] == NETCHESSZX_SAVE_FLAG_ACTIVE),
              "only an active non-ended snapshot resumes its timers");
    }
    host_trace = 0;
    host_failed = 0u;
}

static void check_restore_orientation(void)
{
    netchesszx_save_meta_t meta = { 0 };
    uint8_t host_color;
    uint8_t flipped;

    spectrum_board_reset();
    spectrum_board_snapshot_save(&restore_snapshot);
    for (host_color = NETCHESSZX_COLOR_WHITE;
         host_color <= NETCHESSZX_COLOR_BLACK; ++host_color) {
        netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                     NETCHESSZX_TRANSPORT_DIRECT,
                                     host_color);
        meta.host_color = netchesszx_session_identity_host_color();
        for (flipped = 0u; flipped <= 1u; ++flipped) {
            meta.view_flags = flipped ? NETCHESSZX_SAVE_VIEW_FLIPPED : 0u;
            host_board_flipped = (uint8_t)!flipped;
            SESSION_APP_APPLY_LOADED(&meta);
            check(host_board_flipped == flipped,
                  "offline load restores saved board orientation");

            host_board_flipped = (uint8_t)!flipped;
            check(spectrum_restore_build_b64(&restore_snapshot, &meta,
                                             restore_b64_pending),
                  "local restore orientation fixture encodes");
            netchesszx_session_state.restore_rx_mask = RESTORE_TX_AWAIT_ACK;
            (void)session_control_handle_event(
                NETCHESSZX_SESSION_EVENT_RESTORE_RA,
                (char *)NETCHESS_PROTO_RESTORE_RA);
            check(host_board_flipped == flipped,
                  "agreed local restore restores saved board orientation");
        }
        {
            netchesszx_save_meta_t session_app_meta = meta;
            uint8_t local_view = (uint8_t)!netchesszx_local_is_white();

            session_app_meta.view_flags = local_view
                ? 0u : NETCHESSZX_SAVE_VIEW_FLIPPED;
            host_board_flipped = local_view;
            SESSION_APP_RESTORE_APPLY();
            check(host_board_flipped == local_view,
                  "remote restore preserves local player board orientation");
        }
    }
}

static void check_load_preserves_pending_wire(void)
{
    uint8_t before[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    unsigned kind;

    for (kind = 0u; kind < 3u; ++kind) {
        memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);
        if (kind == 0u) netchesszx_session_state.pending_local_ply = 1u;
        if (kind == 1u) netchesszx_session_state.control_pending = CONTROL_PENDING_RESET;
        if (kind == 2u) netchesszx_session_state.restore_rx_mask = RESTORE_TX_PENDING;
        memset(restore_b64_pending, 'X', sizeof before);
        memcpy(before, restore_b64_pending, sizeof before);
        host_restore_file_payload = 0; /* Any attempted file read fails the stub. */
        host_failed = 0u;
        local_load_game("pending.NCZS");
        check(!host_failed && memcmp(before, restore_b64_pending, sizeof before) == 0,
              "LOAD rejects pending operations before reading shared restore buffer");
    }
    memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);
}

#ifdef NETCHESSZX_RECOVERY
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
/* Classic only: a takeback whose BOARD cannot load changes nothing and ends
   the session naming the overlay: accepting the peer's (key path) and the
   peer's ACK of ours (dispatch path). */
static void check_takeback_board_lost(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.game_ply = 2u;
    takeback_snapshot_save(2u, 0u);
    netchesszx_session_state.takeback_pending_ply = 2u;
    confirm_action = CONFIRM_TAKEBACK_ACCEPT;
    spectrum_board_host_fail = 1u;
    check(process_local_key('y') == 0u &&
              netchesszx_session_view()->game_ply == 2u,
          "accepting a takeback whose BOARD cannot load fails the key unchanged");
    handle_opponent_disconnected();     /* game_message_loop's next step */
    check(strcmp(host_last_notice, spectrum_app_overlay_load_error) == 0,
          "the failed takeback accept is named as a BOARD loss");

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_status_active = 1u;
    netchesszx_session_state.game_ply = 2u;
    takeback_snapshot_save(2u, 1u);
    netchesszx_session_state.takeback_pending_ply = 2u;
    check(session_control_handle_event(NETCHESSZX_SESSION_EVENT_ACK_MOVE,
                                       (char *)"ACK 2") ==
              SESSION_DISPATCH_EXIT &&
              netchesszx_session_view()->game_ply == 2u &&
              strcmp(host_last_notice, spectrum_app_overlay_load_error) == 0,
          "the peer's ACK of a takeback whose BOARD cannot load ends the session");
    spectrum_board_host_fail = 0u;
    takeback_clear();
}

/* The peer accepted our RESTORE but BOARD cannot load: end the session like
   a lost link, naming the overlay, with no apply and no checkpoint. */
static void check_restore_board_lost(void)
{
    spectrum_board_snapshot_t snapshot;
    netchesszx_save_meta_t meta = { 0 };
    uint8_t writes;

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    spectrum_board_snapshot_save(&snapshot);
    meta.ply = 8u;                      /* even: white to move, as reset */
    meta.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(spectrum_restore_build_b64(&snapshot, &meta, restore_b64_pending),
          "board-lost RESTORE fixture encodes");
    netchesszx_session_state.game_ply = 3u;
    netchesszx_session_state.restore_rx_mask = RESTORE_TX_AWAIT_ACK;
    writes = host_recovery_writes;
    spectrum_board_host_fail = 1u;
    check(session_control_handle_event(NETCHESSZX_SESSION_EVENT_RESTORE_RA,
                                       (char *)NETCHESS_PROTO_RESTORE_RA) ==
              SESSION_DISPATCH_EXIT,
          "an accepted RESTORE whose BOARD cannot load ends the session");
    spectrum_board_host_fail = 0u;
    check(strcmp(host_last_notice, spectrum_app_overlay_load_error) == 0 &&
              host_recovery_writes == writes &&
              netchesszx_session_view()->restore_rx_mask == 0u,
          "the BOARD loss is named, unapplied and not checkpointed");
    SESSION_APP_DISCONNECT();
    check(strcmp(host_last_notice, msg_connection_lost) == 0,
          "the next lost link is named as a lost link again");
}
#endif

static void check_recovery_app_contract(void)
{
    DirectParityTrace trace = { 0 };
    spectrum_board_snapshot_t snapshot;
    netchesszx_save_meta_t meta = { 0 };
    char recovery_b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    char cached_restore[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    uint8_t applied;

    host_trace = &trace;
    host_failed = 0u;
    host_recovery_available = 0u;
    host_recovery_writes = 0u;
    host_recovery_write_ok = 1u;
    recovery_state = 0u;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_mark_ready();
    spectrum_board_reset();
    host_cursor_marks = 0u;
    game_start_state();
    check(host_recovery_cursor_marks == 0u && host_cursor_marks != 0u,
          "GAME START finishes checkpoint I/O before showing the playable cursor");
    check(host_recovery_writes == 1u &&
              spectrum_restore_decode((const char *)host_recovery_saved,
                                      &snapshot, &meta) &&
              meta.ply == 0u,
          "GAME START checkpoints ply zero, retiring the previous offer");

    netchesszx_session_state.pending_local_ply = 1u;
    netchesszx_session_store_pending_move("e2e4");
    check(host_recovery_writes == 1u,
          "unacknowledged local move is not checkpointed");
    apply_pending_local_move(0);
    check(host_recovery_writes == 2u &&
              spectrum_restore_decode((const char *)host_recovery_saved,
                                      &snapshot, &meta) &&
              meta.ply == 1u &&
              (meta.flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u,
          "local ACK checkpoints the committed ply");

    memset(restore_b64_pending, 'R', sizeof cached_restore);
    memcpy(cached_restore, restore_b64_pending, sizeof cached_restore);
    netchesszx_session_state.restore_rx_mask = RESTORE_RX_APPLIED;
    {
        SESSION_APP_REMOTE_MOVE_WORKSPACE();
        SESSION_APP_APPLY_REMOTE_MOVE(2u, "e7e5", "", applied);
        if (applied) {
            SESSION_APP_FINISH_REMOTE_MOVE("2", "e7e5");
        }
    }
    check(applied && host_recovery_writes == 3u &&
              spectrum_restore_decode((const char *)host_recovery_saved,
                                      &snapshot, &meta) &&
              meta.ply == 2u,
          "remote committed move checkpoints the new ply");
    check(memcmp(cached_restore, restore_b64_pending,
                 sizeof cached_restore) == 0,
          "checkpoint preserves the accepted RESTORE cache");

#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
    spectrum_board_host_fail = 1u;
    check(!apply_takeback_snapshot() && host_recovery_writes == 3u &&
              netchesszx_session_view()->game_ply == 2u,
          "a takeback whose BOARD cannot load changes and checkpoints nothing");
    spectrum_board_host_fail = 0u;
#endif
    apply_takeback_snapshot();
    check(host_recovery_writes == 4u &&
              spectrum_restore_decode((const char *)host_recovery_saved,
                                      &snapshot, &meta) &&
              meta.ply == 1u,
          "accepted takeback checkpoints the restored ply");

    spectrum_board_snapshot_save(&snapshot);
    memset(&meta, 0, sizeof meta);
    meta.ply = 7u;
    meta.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(spectrum_restore_build_b64(&snapshot, &meta, recovery_b64),
          "recovery fixture encodes");
    host_recovery_payload = (const uint8_t *)recovery_b64;
    host_recovery_available = 1u;
    netchesszx_session_peer_clear();
    recovery_state = 0u;
    confirm_action = CONFIRM_NONE;
    ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT);
    check(confirm_action == CONFIRM_NONE &&
              host_last_msg_id == SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT,
          "guest readiness never offers a checkpoint");
    ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    check(confirm_action == CONFIRM_RESUME &&
              host_last_msg_id == SPECTRUM_GUI_MSG_RESUME_OFFER &&
              host_recovery_writes == 4u,
          "host readiness offers the newest checkpoint before any start");
    check(process_local_key('n') == 1u && confirm_action == CONFIRM_NONE &&
              host_last_msg_id == SPECTRUM_GUI_MSG_OPPONENT_READY_GO &&
              netchesszx_session_view()->game_ply == 1u &&
              host_recovery_writes == 4u,
          "declining the offer keeps the game state and every checkpoint");
    ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    check(process_local_key('x') == 1u && confirm_action == CONFIRM_RESUME,
          "the offer waits for an explicit Y or N");
    check(process_local_key('y') == 1u && confirm_action == CONFIRM_NONE,
          "accepting the offer consumes the confirmation");
    check(netchesszx_session_view()->game_ply == 7u &&
              host_recovery_writes == 5u &&
              spectrum_restore_decode((const char *)host_recovery_saved,
                                      &snapshot, &meta) &&
              meta.ply == 7u,
          "accepted resume applies and checkpoints through LOAD policy");
    host_recovery_available = 0u;
    ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    check(confirm_action == CONFIRM_NONE &&
              host_last_msg_id == SPECTRUM_GUI_MSG_OPPONENT_READY_GO,
          "without a candidate the host keeps the plain READY notice");
    host_recovery_available = 1u;

    /* With a live peer, Y only asks for consent: nothing is applied or
       checkpointed until the guest accepts, and its refusal keeps both. */
    meta.ply = 9u;
    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(spectrum_restore_build_b64(&snapshot, &meta, recovery_b64),
          "peer-consent recovery fixture encodes");
    netchesszx_session_peer_mark_ready();
    recovery_state = 0u;
    confirm_action = CONFIRM_NONE;
    ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    check(confirm_action == CONFIRM_RESUME,
          "a ready peer is offered the checkpoint");
    check(process_local_key('y') == 1u && confirm_action == CONFIRM_NONE &&
              netchesszx_session_view()->restore_rx_mask == RESTORE_TX_PENDING &&
              netchesszx_session_view()->game_ply == 7u &&
              host_recovery_writes == 5u,
          "accepted resume waits for the guest before applying");
    check(session_control_handle_event(NETCHESSZX_SESSION_EVENT_RESTORE_RN,
                                       (char *)NETCHESS_PROTO_RESTORE_RN) ==
              SESSION_DISPATCH_HANDLED,
          "guest refusal of the resume is handled");
    recovery_notify();
    check(netchesszx_session_view()->restore_rx_mask == 0u &&
              netchesszx_session_view()->game_ply == 7u &&
              host_recovery_writes == 5u,
          "guest refusal keeps the position and every checkpoint");
    netchesszx_session_peer_clear();

    meta.ply = 9u;
    meta.host_color = NETCHESSZX_SAVE_HOST_BLACK;
    check(spectrum_restore_build_b64(&snapshot, &meta, recovery_b64),
          "cross-colour recovery fixture encodes");
    netchesszx_session_state.game_ply = 7u;
    local_load_game(0);
    check(netchesszx_session_view()->game_ply == 7u &&
              host_recovery_writes == 5u,
          "cross-colour recovery is rejected without checkpointing");

    netchesszx_session_state.restore_rx_mask = RESTORE_TX_PENDING;
    check(session_control_handle_event(NETCHESSZX_SESSION_EVENT_RESTORE_RN,
                                       (char *)NETCHESS_PROTO_RESTORE_RN) ==
              SESSION_DISPATCH_HANDLED,
          "peer recovery refusal completes the RESTORE exchange");
    recovery_notify();
    check(netchesszx_session_view()->restore_rx_mask == 0u &&
              host_recovery_writes == 5u,
          "peer recovery refusal clears the exchange without checkpointing");

    meta.host_color = NETCHESSZX_SAVE_HOST_WHITE;
    check(spectrum_restore_build_b64(&snapshot, &meta, recovery_b64),
          "manual recovery fixture encodes");
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    recovery_state = 0u;
    {
        netchesszx_save_meta_t session_app_meta = { 0 };

        spectrum_board_reset();
        spectrum_board_snapshot_save(&restore_snapshot);
        session_app_meta.ply = 8u;
        session_app_meta.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
        session_app_meta.host_color = netchesszx_session_identity_host_color();
        netchesszx_session_state.restore_rx_mask = RESTORE_RX_RECEIVE;
        SESSION_APP_RESTORE_APPLY();
        check(SESSION_APP_RESTORE_CACHE(),
              "guest accepted RESTORE rebuilds its exact duplicate cache");
        check(netchesszx_session_view()->game_ply == 8u &&
                  host_recovery_writes == 6u &&
                  spectrum_restore_decode(
                      (const char *)host_recovery_saved, &snapshot, &meta) &&
                  meta.ply == 8u &&
                  meta.host_color == netchesszx_session_identity_host_color(),
              "guest accepted RESTORE checkpoints before the next move");
    }
    recovery_state &= (uint8_t)~RECOVERY_WARNING;
    host_recovery_write_ok = 0u;
    recovery_checkpoint();
    recovery_notify();
    check(strcmp(host_last_notice, msg_recovery_failed) == 0,
          "failed checkpoint reports through the resident notice path");
    host_recovery_write_ok = 1u;
    {
        char expected[NETCHESSZX_SAVE_WIRE_B64_SIZE];

        spectrum_board_snapshot_save(&snapshot);
        meta.ply = netchesszx_session_view()->game_ply;
        meta.flags = saveload_flags();
        meta.host_color = netchesszx_session_identity_host_color();
        meta.view_flags = spectrum_gui_is_board_flipped()
            ? NETCHESSZX_SAVE_VIEW_FLIPPED : 0u;
        spectrum_time_game_timer_save(meta.timers);
        check(spectrum_restore_build_b64(&snapshot, &meta, expected),
              "manual save reference encodes");
        local_save_game("MANUAL.NCZS");
        check(host_manual_saves == 1u &&
                  memcmp(host_manual_saved, expected, sizeof expected) == 0 &&
                  host_recovery_writes == 6u,
              "manual save keeps the exact wire payload without checkpointing");
    }

    /* Chosen exits retire the offer through RECOVERY (which checks for a
       live game); a lost link keeps it. */
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    host_recovery_retires = 0u;
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_status_active = 1u;
    handle_opponent_disconnected();
    check(host_recovery_retires == 0u && host_recovery_writes == 6u,
          "a lost connection leaves the resume offer untouched");
    netchesszx_session_peer_mark_ready();
    netchesszx_session_state.game_status_active = 1u;
    confirm_action = CONFIRM_DISCONNECT;
    check(process_local_key('y') == 1u && confirm_action == CONFIRM_NONE &&
              host_recovery_retires == 1u && host_recovery_writes == 6u,
          "menu disconnect retires the resume offer");
    {
        char bye[] = "BYE";

        netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                     NETCHESSZX_TRANSPORT_DIRECT,
                                     NETCHESSZX_COLOR_WHITE);
        netchesszx_session_peer_mark_ready();
        netchesszx_session_state.game_status_active = 1u;
        (void)session_presence_handle_event(NETCHESSZX_SESSION_EVENT_BYE,
                                            bye, 0u);
        check(host_recovery_retires == 2u,
              "the peer's BYE retires the resume offer");
    }
    host_trace = 0;
    host_failed = 0u;
    host_recovery_available = 0u;
    netchesszx_session_peer_clear();
    memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);
}
#endif

static void check_board_input_adapter(void)
{
    DirectParityTrace trace;
    char cells[64];
    unsigned side;
    unsigned variant;
    uint8_t saved_hints = netchesszx_movement_hints;

    netchesszx_movement_hints = 0u;
    for (side = 0u; side < 2u; ++side) {
        netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                    NETCHESSZX_TRANSPORT_DIRECT,
                                    side ? NETCHESSZX_COLOR_BLACK : NETCHESSZX_COLOR_WHITE);
        for (variant = 0u; variant < 4u; ++variant) {
            uint8_t index = variant == 0u ? (side ? 12u : 52u) :
                            variant == 1u ? (side ? 28u : 36u) : 0u;
            memset(cells, '.', sizeof cells);
            if (variant != 3u) cells[index] = side ? 'p' : 'P';
            spectrum_board_test_set(cells, (uint8_t)side, 0u, -1);
            cursor_reset_default_square();
            check(spectrum_ux_cursor_position() == (variant == 3u ? 52u : index),
                  "application supplies preferred, scanned or fallback cursor square");
            check(spectrum_ux_selected_position() == 0xffffu,
                  "application cursor reset clears selection");
        }
        spectrum_board_reset();
        check(spectrum_ux_selectable(side ? 1u : 6u, 4u) &&
              !spectrum_ux_selectable(side ? 6u : 1u, 4u) &&
              !spectrum_ux_selectable(4u, 4u), "application owns cell selection policy");
        spectrum_ux_selection_notice(0x0404u);
        check(strcmp(host_last_notice, "Empty e4") == 0, "empty square notice preserved");
        spectrum_ux_selection_notice(side ? 0x0401u : 0x0406u);
        check(strcmp(host_last_notice, side ? "Sel e7" : "Sel e2") == 0,
              "selected square notation preserved");
        spectrum_ux_selection_notice(side ? 0x0406u : 0x0401u);
        check(strcmp(host_last_notice, side ? "You play BLACK" : "You play WHITE") == 0,
              "opponent square notice preserved");

        memset(cells, '.', sizeof cells);
        cells[4] = 'k';
        cells[60] = 'K';
        cells[side ? 48u : 8u] = side ? 'p' : 'P';
        spectrum_board_test_set(cells, (uint8_t)side, 0u, -1);
        memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);
        netchesszx_session_peer_mark_ready();
        netchesszx_session_state.game_status_active = 1u;
        local_turn = 1u;
        memset(&trace, 0, sizeof trace);
        host_trace = &trace;
        host_fail_length = 0u;
        check(spectrum_ux_move_intent(side ? 6u : 1u, side ? 7u : 0u) == UX_MOVE_ACCEPTED,
              "coordinate intent enters productive session path");
        check(strcmp(netchesszx_session_state.pending_local_move,
                     side ? "a2a1q" : "a7a8q") == 0, "automatic queen promotion preserved");
        check(memcmp(spectrum_board_cells(), cells, sizeof cells) == 0,
              "coordinate intent waits for peer ACK before domain mutation");
        host_trace = 0;
    }
    memset(&netchesszx_session_state, 0, sizeof netchesszx_session_state);
    netchesszx_session_peer_clear();
    netchesszx_movement_hints = saved_hints;
    local_turn = 0u;
}

int main(int argc, char **argv)
{
    DirectParityTrace reference;
    DirectParityTrace spectrum;
    uint8_t i;
    uint8_t ran = 0u;

    check_restore_host_color_guard();
#ifdef NETCHESSZX_NEXT
    check_direct_recovery_budget_rearmed();
#endif
    check_restore_codec_contract();
    check_loaded_timers_survive_application();
    check_restore_orientation();
    check_load_preserves_pending_wire();
#ifdef NETCHESSZX_RECOVERY
    check_recovery_app_contract();
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
    check_restore_board_lost();
    check_takeback_board_lost();
#endif
#endif
#ifdef NETCHESSZX_NEXT_BANKING
    check_next_restore_sprite_allocator_contract();
#endif
    check_move_parser_contract();
    check_text_helper_contract();
    check_input_editor_driver();
    check_cursor_reselects_own_piece();
    check_board_input_adapter();
    check_turn_grant_clears_stale_cursor();
    check_setup_preview_skips_first_start_redraw();
    check_local_move_ply_overflow();
    check_chat_editor_during_pending_control();
    check_non_chat_input_liveness_status();
    check_about_dismisses_before_confirmation();
    check_game_stats_keys();
    check_restore_transfer_blocks_file_and_editor();
    check_mqtt_peer_loss_blocks_taboption();
    check_mqtt_peer_identity_reset();
    check_direct_guest_disconnect_forgets_side();
    for (i = 0u; i < direct_parity_scenario_count; ++i) {
        const DirectParityScenario *scenario = &direct_parity_scenarios[i];

        if (argc == 2 && strcmp(argv[1], scenario->id) != 0) {
            continue;
        }
        ran = 1u;
        if (!direct_reference_run(scenario, &reference)) {
            fprintf(stderr, "FAIL: %s reference scenario ran\n", scenario->id);
            return 1;
        }
        check_trace(scenario, &reference, "reference");
        if (!direct_spectrum_run(scenario, &spectrum)) {
            fprintf(stderr, "FAIL: %s Spectrum scenario ran\n", scenario->id);
            return 1;
        }
        check_trace(scenario, &spectrum, "Spectrum");
    }
    check(ran, "requested scenario exists");
    puts("DIRECT semantic parity scenarios ok");
    return 0;
}
