#include "spectrum/session/event.h"

#include "spectrum/config/session.h"

#include <stdio.h>
#include <stdlib.h>

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void expect_event(const char *payload,
                         uint8_t is_mqtt,
                         uint8_t retained,
                         netchesszx_session_event_t expected,
                         const char *message)
{
    netchesszx_session_event_t ev;

    ev = netchesszx_session_classify_event(payload,
                                           is_mqtt,
                                           retained,
                                           netchesszx_session_is_host());
    check(ev == expected, message);
}

static void test_common_protocol_events(void)
{
    expect_event("HELLO DIRECT HOST WHITE=HOST",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_DIRECT_HELLO,
                 "direct hello");
    expect_event("PING",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_PING,
                 "ping");
    expect_event("ACK PING",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_ACK_PING,
                 "ack ping");
    expect_event("ACK 12",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_ACK,
                 "ack");
    expect_event("NACK 12",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_NACK,
                 "nack");
    expect_event("BYE",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_BYE,
                 "bye");
    expect_event("RESET",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_RESET,
                 "reset");
    expect_event("GAME START WHITE=GUEST",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_GAME_START,
                 "game start");
    expect_event("MOVE 1 e2e4",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MOVE,
                 "move");
    expect_event("CHAT hello",
                 0u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_CHAT,
                 "chat");
}

static void test_mqtt_session_events(void)
{
    netchesszx_session_event_t ev;

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_mqtt_session_id = 7u;
    expect_event("",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MQTT_EMPTY,
                 "mqtt empty");
    expect_event("F B 7",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
                 "mqtt peer offline");
    expect_event("OFFLINE",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_UNKNOWN,
                 "unknown offline ignored");
    expect_event("H W 8",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST,
                 "mqtt foreign host");
    expect_event("H W 7",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_UNKNOWN,
                 "same session host echo ignored by classifier");
    expect_event("GAME START WHITE=GUEST",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_GAME_START,
                 "mqtt game start stays game start");
    ev = netchesszx_session_classify_event("J 7",
                                           1u,
                                           0u,
                                           netchesszx_session_is_host());
    check(ev == NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY,
          "mqtt peer ready");
    expect_event("J 7",
                 1u,
                 1u,
                 NETCHESSZX_SESSION_EVENT_UNKNOWN,
                 "retained join ignored");
    expect_event("J 8",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_UNKNOWN,
                 "stale join ignored");
    expect_event("J 7 DEVICE",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_UNKNOWN,
                 "legacy join token ignored");
    expect_event("TEXT",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MQTT_TEXT,
                 "mqtt text");

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    expect_event("H W 7",
                 1u,
                 0u,
                 NETCHESSZX_SESSION_EVENT_MQTT_HOST,
                 "mqtt host");
}

static void test_retained_policy(void)
{
    check(netchesszx_session_event_ignores_retained(NETCHESSZX_SESSION_EVENT_ACK,
                                                    1u),
          "retained ack ignored");
    check(netchesszx_session_event_ignores_retained(
              NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
              1u),
          "retained offline ignored");
    check(netchesszx_session_event_ignores_retained(
              NETCHESSZX_SESSION_EVENT_GAME_START,
              1u),
          "retained game start ignored");
    check(netchesszx_session_event_ignores_retained(
              NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST,
              1u),
          "retained foreign host ignored");
    check(!netchesszx_session_event_ignores_retained(
              NETCHESSZX_SESSION_EVENT_MQTT_HOST,
              1u),
          "retained host handled");
    check(!netchesszx_session_event_ignores_retained(
              NETCHESSZX_SESSION_EVENT_ACK_PING,
              1u),
          "retained ack ping not suppressed");
}

static void test_peer_state_actions(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_session_peer_reset();
    check(!netchesszx_session_peer_ready(), "peer starts clear");
    netchesszx_session_peer_mark_ready();
    check(netchesszx_session_peer_ready(), "peer ready state");
    netchesszx_session_peer_clear();
    check(!netchesszx_session_peer_ready(), "peer clear state");
}

static void test_mqtt_host_flags(void)
{
    uint8_t bad;
    uint8_t flags;
    uint8_t old_local_color;

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_mqtt_session_id = 0u;
    netchesszx_session_peer_reset();
    old_local_color = netchesszx_local_color;
    check(!netchesszx_session_mqtt_can_accept_game_start(),
          "mqtt start gate starts closed");
    netchesszx_session_peer_mark_ready();
    check(!netchesszx_session_mqtt_can_accept_game_start(),
          "mqtt start gate needs session id");
    netchesszx_session_peer_reset();

    flags = netchesszx_session_mqtt_host_flags("H W 9", 0u, 1u, &bad);
    check(!bad, "retained host valid");
    check(flags & NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT,
          "retained host waits");
    check(!netchesszx_session_peer_ready(), "retained host no peer ready");
    check(!netchesszx_session_mqtt_can_accept_game_start(),
          "retained host cannot start game");
    check(netchesszx_mqtt_session_id == 0u, "retained host keeps session clear");
    check(netchesszx_local_color == old_local_color,
          "retained host keeps local color");
    check((flags & (NETCHESSZX_SESSION_MQTT_HOST_COLOR_CHANGED |
                    NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE)) == 0u,
          "retained host has no side effects");

    flags = netchesszx_session_mqtt_host_flags("H W 9", 0u, 0u, &bad);
    check(!bad, "live host valid");
    check(flags & NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT,
          "live host ready wait");
    check(netchesszx_session_peer_ready(), "live host marks ready");
    check(netchesszx_session_mqtt_can_accept_game_start(),
          "live host enables game start");

    flags = netchesszx_session_mqtt_host_flags("H X 9", 0u, 0u, &bad);
    check(flags == 0u && bad, "bad host color");
}

int main(void)
{
    test_common_protocol_events();
    test_mqtt_session_events();
    test_retained_policy();
    test_peer_state_actions();
    test_mqtt_host_flags();
    puts("session event tests ok");
    return 0;
}
