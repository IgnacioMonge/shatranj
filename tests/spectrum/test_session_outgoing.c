#include "spectrum/session/outgoing.h"

#include "common/session/session.h"
#include "spectrum/config/session.h"
#include "spectrum/session/route.h"
#include "spectrum/overlay/overlay_context.h"

typedef char mqtt_request_flags_match_overlay_abi[
    NETCHESSZX_SESSION_ROUTE_RETAIN == NETCHESSZX_MQTT_TX_RETAIN &&
    NETCHESSZX_SESSION_ROUTE_OFFLINE == NETCHESSZX_MQTT_TX_OFFLINE &&
    NETCHESSZX_SESSION_ROUTE_CLEAR_META == NETCHESSZX_MQTT_TX_CLEAR_META &&
    NETCHESSZX_SESSION_MQTT_TX_ROUTE_MASK == NETCHESSZX_MQTT_TX_ROUTE_MASK &&
    NETCHESSZX_SESSION_MQTT_TX_CTX_FLAGS == NETCHESSZX_MQTT_TX_CTX_FLAGS ? 1 : -1];

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sent_text;
static uint8_t sent_route;

void spectrum_net_mqtt_set_publish_flags(uint8_t flags)
{
    sent_route = flags;
}

uint8_t spectrum_net_send_text(const char *text)
{
    sent_text = text;
    return 1u;
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void expect_sent(const char *expected, const char *message)
{
    check(sent_text != 0 && strcmp(sent_text, expected) == 0, message);
}

static void test_ack_nack(void)
{
    sent_text = 0;
    check(netchesszx_session_send_ack_move("0"), "zero ack move sent");
    expect_sent("ACK 0", "zero ack move text");
    check(netchesszx_session_send_nack_move("0"), "zero nack move sent");
    expect_sent("NACK 0", "zero nack move text");
    check(netchesszx_session_send_ack_move("12"), "ack move sent");
    expect_sent("ACK 12", "ack move text");
    check(netchesszx_session_send_nack_move("13"), "nack move sent");
    expect_sent("NACK 13", "nack move text");
    check(netchesszx_session_send_ack_move("65535"), "max ack move sent");
    expect_sent("ACK 65535", "max ack move text");
    check(netchesszx_session_send_nack_move("65535"), "max nack move sent");
    expect_sent("NACK 65535", "max nack move text");
}

static void test_ping_response(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    check(netchesszx_session_send_ping(), "direct ping sent");
    expect_sent("PING", "direct ping text");
    check(sent_route == SESSION_ROUTE_GAME, "ping uses game route");
    check(netchesszx_session_send_ack_ping(), "ack ping sent");
    expect_sent("ACK PING", "ack ping text");
    check(sent_route == SESSION_ROUTE_ACK, "ping ack uses ack route");
}

static void test_reset_start(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    check(netchesszx_session_send_start_game(), "mqtt start sent");
    expect_sent("GAME START", "mqtt start text");
    check(sent_route == SESSION_ROUTE_CONTROL, "start uses control route");
}

int main(void)
{
    check(NETCHESSZX_SESSION_ROUTE_DEFAULT == SESSION_ROUTE_DEFAULT &&
          NETCHESSZX_SESSION_ROUTE_CONTROL == SESSION_ROUTE_CONTROL &&
          NETCHESSZX_SESSION_ROUTE_PRESENCE == SESSION_ROUTE_PRESENCE &&
          NETCHESSZX_SESSION_ROUTE_GAME == SESSION_ROUTE_GAME &&
          NETCHESSZX_SESSION_ROUTE_ACK == SESSION_ROUTE_ACK &&
          NETCHESSZX_SESSION_ROUTE_PRESENCE_PEER ==
              SESSION_ROUTE_PRESENCE_PEER &&
          NETCHESSZX_SESSION_ROUTE_META == SESSION_ROUTE_META,
          "compact route ABI matches common session");
    test_ack_nack();
    test_ping_response();
    test_reset_start();
    puts("session outgoing tests ok");
    return 0;
}
