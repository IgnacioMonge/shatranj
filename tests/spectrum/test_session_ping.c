#include "spectrum/session/ping.h"

#include <stdio.h>
#include <stdlib.h>

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void tick(netchesszx_session_ping_t *ping, uint8_t is_mqtt,
                 uint8_t count, uint8_t expected)
{
    uint8_t rc = NETCHESSZX_SESSION_PING_NONE;

    while (count-- != 0u) {
        rc = netchesszx_session_ping_timeout(ping, is_mqtt);
    }
    check(rc == expected, "unexpected timeout event");
}

static void test_direct_ping_ack(void)
{
    netchesszx_session_ping_t ping;

    netchesszx_session_ping_reset(&ping);
    tick(&ping, 0u, 19u, NETCHESSZX_SESSION_PING_NONE);
    tick(&ping, 0u, 1u, NETCHESSZX_SESSION_PING_SEND);
    netchesszx_session_ping_sent(&ping, 0u);
    check(netchesszx_session_ping_ack(&ping, 0u), "accept direct ack");
    tick(&ping, 0u, 20u, NETCHESSZX_SESSION_PING_SEND);
}

static void test_direct_loss_after_misses(void)
{
    netchesszx_session_ping_t ping;

    netchesszx_session_ping_reset(&ping);
    tick(&ping, 0u, 20u, NETCHESSZX_SESSION_PING_SEND);
    netchesszx_session_ping_sent(&ping, 0u);
    tick(&ping, 0u, 20u, NETCHESSZX_SESSION_PING_SEND);
    netchesszx_session_ping_sent(&ping, 0u);
    tick(&ping, 0u, 20u, NETCHESSZX_SESSION_PING_LOST);
}

static void test_mqtt_pingresp_resets_misses(void)
{
    netchesszx_session_ping_t ping;
    uint8_t i;

    netchesszx_session_ping_reset(&ping);
    tick(&ping, 1u, 120u, NETCHESSZX_SESSION_PING_SEND);
    netchesszx_session_ping_sent(&ping, 1u);
    netchesszx_session_ping_rx_mqtt_pingresp(&ping);
    for (i = 0u; i != 4u; ++i) {
        tick(&ping, 1u, 120u, NETCHESSZX_SESSION_PING_SEND);
        netchesszx_session_ping_sent(&ping, 1u);
    }
    tick(&ping, 1u, 120u, NETCHESSZX_SESSION_PING_LOST);
}

int main(void)
{
    test_direct_ping_ack();
    test_direct_loss_after_misses();
    test_mqtt_pingresp_resets_misses();
    puts("session ping tests ok");
    return 0;
}
