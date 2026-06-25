#include "spectrum/config/session.h"
#include "spectrum/session/poll.h"
#include "spectrum/transport/link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int16_t stub_read_result;
static const char *stub_payload;
static uint8_t stub_payload_flags;
static uint8_t stub_background_drain_count;
static uint8_t stub_link_activity;
static uint8_t stub_send_ping_count;
static uint8_t stub_send_ping_ok = 1u;
static uint8_t stub_send_ack_ping_count;
static uint8_t stub_send_ack_ping_ok = 1u;
static char stub_last_ack_ping[SPECTRUM_LINK_PAYLOAD_MAX];

static void reset_stubs(void)
{
    stub_read_result = SPECTRUM_LINK_READ_TIMEOUT;
    stub_payload = "";
    stub_payload_flags = 0u;
    stub_background_drain_count = 0u;
    stub_link_activity = 0u;
    stub_send_ping_count = 0u;
    stub_send_ping_ok = 1u;
    stub_send_ack_ping_count = 0u;
    stub_send_ack_ping_ok = 1u;
    stub_last_ack_ping[0] = '\0';
}

int16_t spectrum_net_read_payload(char *payload, uint8_t payload_cap)
{
    if (stub_read_result >= 0 && payload_cap != 0u) {
        strncpy(payload, stub_payload, (size_t)payload_cap - 1u);
        payload[payload_cap - 1u] = '\0';
    }
    return stub_read_result;
}

void spectrum_net_background_drain(void)
{
    ++stub_background_drain_count;
}

uint8_t spectrum_net_link_activity(void)
{
    uint8_t activity = stub_link_activity;

    stub_link_activity = 0u;
    return activity;
}

uint8_t spectrum_net_payload_flags(void)
{
    return stub_payload_flags;
}

uint8_t netchesszx_session_send_ping(void)
{
    ++stub_send_ping_count;
    return stub_send_ping_ok;
}

uint8_t netchesszx_session_send_ack_ping(const char *rx)
{
    ++stub_send_ack_ping_count;
    strncpy(stub_last_ack_ping, rx, sizeof(stub_last_ack_ping) - 1u);
    stub_last_ack_ping[sizeof(stub_last_ack_ping) - 1u] = '\0';
    return stub_send_ack_ping_ok;
}

static void require_u8(const char *label, uint8_t got, uint8_t want)
{
    if (got != want) {
        fprintf(stderr, "%s: got %u want %u\n", label, got, want);
        exit(1);
    }
}

static void test_payload_event(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    stub_read_result = 0;
    stub_payload = "MOVE 1 e2e4";

    require_u8("poll event",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_EVENT);
    require_u8("event tag", (uint8_t)out.event, NETCHESSZX_SESSION_EVENT_MOVE);
    require_u8("retained", out.retained, 0u);
    require_u8("background drain", stub_background_drain_count, 1u);
}

static void test_retained_side_effect_event_ignored(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    stub_read_result = 0;
    stub_payload = "MOVE 1 e2e4";
    stub_payload_flags = SPECTRUM_LINK_PAYLOAD_RETAINED;

    require_u8("retained poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_NONE);
    require_u8("retained ignored event",
               (uint8_t)out.event,
               NETCHESSZX_SESSION_EVENT_UNKNOWN);
}

static void test_direct_timeout_sends_ping(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;
    uint8_t i;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    stub_read_result = SPECTRUM_LINK_READ_TIMEOUT;

    /* DIRECT_IDLE_PING_TICKS idle polls trigger exactly one keepalive ping. */
    for (i = 0u; i < 20u; ++i) {
        require_u8("timeout poll",
                   netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
                   NETCHESSZX_SESSION_POLL_NONE);
    }
    require_u8("send ping count", stub_send_ping_count, 1u);
}

static void test_mqtt_activity_resets_ping_state(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    ping.idle_ticks = 119u;
    ping.misses = 4u;
    stub_read_result = SPECTRUM_LINK_READ_TIMEOUT;
    stub_link_activity = 1u;

    require_u8("mqtt activity poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_NONE);
    require_u8("mqtt activity idle reset", ping.idle_ticks, 0u);
    require_u8("mqtt activity misses reset", ping.misses, 0u);
    require_u8("mqtt activity consumed", stub_link_activity, 0u);
}

static void test_direct_ping_is_consumed_and_acked_each_time(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    stub_read_result = 0;
    stub_payload = "PING";

    require_u8("direct ping poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_NONE);
    require_u8("direct ping ack count", stub_send_ack_ping_count, 1u);
    if (strcmp(stub_last_ack_ping, "PING") != 0) {
        fprintf(stderr, "direct ping ack payload: %s\n", stub_last_ack_ping);
        exit(1);
    }

    require_u8("direct ping duplicate poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_NONE);
    require_u8("direct ping duplicate ack count", stub_send_ack_ping_count, 2u);
}

static void test_direct_ack_ping_is_consumed(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_ping_reset(&ping);
    ping.direct_pending = 1u;
    ping.misses = 2u;
    stub_read_result = 0;
    stub_payload = "ACK PING";

    require_u8("direct ack ping poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_NONE);
    require_u8("direct ack pending", ping.direct_pending, 0u);
    require_u8("direct ack misses", ping.misses, 0u);
}

static void test_read_disconnect(void)
{
    char payload[SPECTRUM_LINK_PAYLOAD_MAX];
    netchesszx_session_ping_t ping;
    netchesszx_session_poll_result_t out;

    reset_stubs();
    netchesszx_session_ping_reset(&ping);
    stub_read_result = -2;

    require_u8("disconnect poll",
               netchesszx_session_poll(&ping, payload, sizeof(payload), &out),
               NETCHESSZX_SESSION_POLL_DISCONNECTED);
}

int main(void)
{
    test_payload_event();
    test_retained_side_effect_event_ignored();
    test_direct_timeout_sends_ping();
    test_mqtt_activity_resets_ping_state();
    test_direct_ping_is_consumed_and_acked_each_time();
    test_direct_ack_ping_is_consumed();
    test_read_disconnect();
    puts("session poll tests ok");
    return 0;
}
