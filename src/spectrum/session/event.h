#ifndef NETCHESSZX_SPECTRUM_SESSION_EVENT_H
#define NETCHESSZX_SPECTRUM_SESSION_EVENT_H

#include <stdint.h>

typedef enum netchesszx_session_event {
    NETCHESSZX_SESSION_EVENT_UNKNOWN = 0,
    NETCHESSZX_SESSION_EVENT_DIRECT_HELLO,
    NETCHESSZX_SESSION_EVENT_PING,
    NETCHESSZX_SESSION_EVENT_ACK_PING,
    NETCHESSZX_SESSION_EVENT_ACK,
    NETCHESSZX_SESSION_EVENT_NACK,
    NETCHESSZX_SESSION_EVENT_BYE,
    NETCHESSZX_SESSION_EVENT_RESET,
    NETCHESSZX_SESSION_EVENT_GAME_START,
    NETCHESSZX_SESSION_EVENT_MOVE,
    NETCHESSZX_SESSION_EVENT_CHAT,
    NETCHESSZX_SESSION_EVENT_MQTT_EMPTY,
    NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE,
    NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST,
    NETCHESSZX_SESSION_EVENT_MQTT_HOST,
    NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY,
    NETCHESSZX_SESSION_EVENT_MQTT_TEXT
} netchesszx_session_event_t;

#define NETCHESSZX_SESSION_MQTT_HOST_COLOR_CHANGED 0x01u
#define NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE 0x02u
#define NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT 0x04u
#define NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT 0x08u
#define NETCHESSZX_SESSION_MQTT_HOST_PUBLISH_SETUP 0x10u

/* Retained boundary: classifier suppresses retained MQTT peer-ready events
   because they would falsely prove liveness. The event retained guard below
   suppresses side-effecting retained events before app dispatch.

   This module owns peer liveness state. MQTT classification also reads the
   configured session id and remote side from session config. If host flag
   handling later aborts on network I/O, the app must reset peer state before
   re-entering the message loop. */
netchesszx_session_event_t netchesszx_session_classify_event(
    const char *payload,
    uint8_t is_mqtt,
    uint8_t retained,
    uint8_t is_host);

void netchesszx_session_peer_reset(void);
#define netchesszx_session_peer_clear netchesszx_session_peer_reset
void netchesszx_session_peer_mark_ready(void);
uint8_t netchesszx_session_peer_ready(void);
uint8_t netchesszx_session_mqtt_can_accept_game_start(void);
uint8_t netchesszx_session_event_ignores_retained(netchesszx_session_event_t event,
                                                  uint8_t retained);
uint8_t netchesszx_session_mqtt_host_flags(const char *payload,
                                           uint8_t game_active,
                                           uint8_t retained,
                                           uint8_t *bad_color);

#endif
