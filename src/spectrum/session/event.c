#include "spectrum/session/event.h"

#include "common/protocol/direct_session_protocol.h"
#include "common/protocol/game_protocol.h"
#include "spectrum/config/session.h"
#include "spectrum/session/mqtt.h"
#include "spectrum/transport/keepalive_protocol.h"

static uint8_t session_peer_ready;

static const uint8_t retained_ignore_map[] = {
    0u, /* UNKNOWN */
    0u, /* DIRECT_HELLO */
    0u, /* PING */
    0u, /* ACK_PING */
    1u, /* ACK */
    1u, /* NACK */
    1u, /* BYE */
    1u, /* RESET */
    1u, /* GAME_START */
    1u, /* MOVE */
    1u, /* CHAT */
    0u, /* MQTT_EMPTY */
    1u, /* MQTT_PEER_OFFLINE */
    1u, /* MQTT_FOREIGN_HOST */
    0u, /* MQTT_HOST */
    0u, /* MQTT_PEER_READY */
    1u  /* MQTT_TEXT */
};

static netchesszx_session_event_t netchesszx_session_classify_game_payload(
    const char *payload)
{
    if (netchess_after_prefix(payload, "MOVE ") != 0) {
        return NETCHESSZX_SESSION_EVENT_MOVE;
    }
    if (netchess_after_prefix(payload, "CHAT ") != 0) {
        return NETCHESSZX_SESSION_EVENT_CHAT;
    }
    if (netchess_proto_is_ack(payload)) {
        return NETCHESSZX_SESSION_EVENT_ACK;
    }
    if (netchess_proto_is_nack(payload)) {
        return NETCHESSZX_SESSION_EVENT_NACK;
    }
    if (netchess_proto_is_bye(payload)) {
        return NETCHESSZX_SESSION_EVENT_BYE;
    }
    if (netchess_proto_is_reset(payload)) {
        return NETCHESSZX_SESSION_EVENT_RESET;
    }
    if (netchess_after_prefix(payload, "GAME START") != 0) {
        return NETCHESSZX_SESSION_EVENT_GAME_START;
    }
    return NETCHESSZX_SESSION_EVENT_UNKNOWN;
}

void netchesszx_session_peer_reset(void)
{
    session_peer_ready = 0u;
}

void netchesszx_session_peer_mark_ready(void)
{
    session_peer_ready = 1u;
}

uint8_t netchesszx_session_peer_ready(void)
{
    return session_peer_ready;
}

uint8_t netchesszx_session_mqtt_can_accept_game_start(void)
{
    return (uint8_t)(session_peer_ready &&
                     netchesszx_host_color_ready &&
                     netchesszx_mqtt_session_id != 0u);
}

uint8_t netchesszx_session_event_ignores_retained(netchesszx_session_event_t event,
                                                  uint8_t retained)
{
    if (!retained) {
        return 0u;
    }
    if ((uint8_t)event >= sizeof(retained_ignore_map)) {
        return 0u;
    }
    return retained_ignore_map[(uint8_t)event];
}

uint8_t netchesszx_session_mqtt_host_flags(const char *payload,
                                           uint8_t game_active,
                                           uint8_t retained,
                                           uint8_t *bad_color)
{
    uint8_t host_color;
    uint8_t color_changed;
    uint8_t new_live_session;
    uint16_t retained_session_id;
    uint8_t flags = 0u;

    *bad_color = 0u;
    if (game_active) {
        return 0u;
    }
    if (retained) {
        if (!netchesszx_session_mqtt_parse_host_payload(payload,
                                                        &host_color,
                                                        &retained_session_id)) {
            *bad_color = 1u;
            return 0u;
        }
        return NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT;
    }
    host_color = netchesszx_session_mqtt_apply_host_color(payload,
                                                          game_active,
                                                          &color_changed,
                                                          &new_live_session);
    if (host_color == 0u) {
        *bad_color = 1u;
        return 0u;
    }
    if (new_live_session) {
        session_peer_ready = 0u;
    }
    if (color_changed) {
        flags |= NETCHESSZX_SESSION_MQTT_HOST_COLOR_CHANGED;
    }
    if (host_color == 2u) {
        flags |= NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE;
    }
    if ((host_color == 1u || host_color == 2u) && !session_peer_ready) {
        if (host_color == 1u) {
            flags |= NETCHESSZX_SESSION_MQTT_HOST_PUBLISH_SETUP;
        }
        session_peer_ready = 1u;
        flags |= NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT;
    }
    return flags;
}

netchesszx_session_event_t netchesszx_session_classify_event(
    const char *payload,
    uint8_t is_mqtt,
    uint8_t retained,
    uint8_t is_host)
{
    if (!is_mqtt && netchess_direct_is_hello(payload)) {
        return NETCHESSZX_SESSION_EVENT_DIRECT_HELLO;
    }
    if (spectrum_keepalive_is_ack_ping(payload)) {
        return NETCHESSZX_SESSION_EVENT_ACK_PING;
    }
    if (spectrum_keepalive_is_ping(payload)) {
        return NETCHESSZX_SESSION_EVENT_PING;
    }

    {
        netchesszx_session_event_t event =
            netchesszx_session_classify_game_payload(payload);

        if (event != NETCHESSZX_SESSION_EVENT_UNKNOWN) {
            return event;
        }
    }

    if (!is_mqtt) {
        return NETCHESSZX_SESSION_EVENT_UNKNOWN;
    }
    if (payload[0] == '\0') {
        return NETCHESSZX_SESSION_EVENT_MQTT_EMPTY;
    }
    switch (payload[0]) {
    case 'F':
        if (netchesszx_session_mqtt_offline_matches_peer(payload)) {
            return NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE;
        }
        break;
    case 'H':
        if (is_host &&
            netchesszx_session_mqtt_payload_is_foreign_host(payload)) {
            return NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST;
        }
        if (!is_host && payload[1] == ' ') {
            return NETCHESSZX_SESSION_EVENT_MQTT_HOST;
        }
        break;
    case 'J':
        if (!retained &&
            netchesszx_session_mqtt_payload_marks_peer_ready(payload,
                                                             is_host)) {
            return NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY;
        }
        break;
    case 'M':
    case 'O':
        break;
    default:
        return NETCHESSZX_SESSION_EVENT_MQTT_TEXT;
    }
    return NETCHESSZX_SESSION_EVENT_UNKNOWN;
}
