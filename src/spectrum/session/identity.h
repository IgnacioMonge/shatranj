#ifndef NETCHESSZX_SPECTRUM_SESSION_IDENTITY_H
#define NETCHESSZX_SPECTRUM_SESSION_IDENTITY_H

#include <stdint.h>
#include "common/protocol/game_protocol.h"

#define NETCHESSZX_SESSION_ROLE_HOST 0u
#define NETCHESSZX_SESSION_ROLE_JOIN 1u

#define NETCHESSZX_TRANSPORT_DIRECT 0u
#define NETCHESSZX_TRANSPORT_MQTT 1u

#define NETCHESSZX_COLOR_WHITE 0u
#define NETCHESSZX_COLOR_BLACK 1u

/* Session-owned resident identity. Symbols are retained for the overlay ABI.
   Configuration supplies choices; it does not own negotiated state. */
extern uint8_t netchesszx_session_role;
extern uint8_t netchesszx_transport;
extern uint8_t netchesszx_local_color;
extern uint8_t netchesszx_host_color;
extern uint8_t netchesszx_host_color_ready;
extern uint16_t netchesszx_mqtt_session_id;
#define netchesszx_session_identity_session_role() ((uint8_t)netchesszx_session_role)
#define netchesszx_session_identity_transport() ((uint8_t)netchesszx_transport)
#define netchesszx_session_identity_local_color() ((uint8_t)netchesszx_local_color)
#define netchesszx_session_identity_host_color() ((uint8_t)netchesszx_host_color)
#define netchesszx_session_identity_host_color_ready() ((uint8_t)netchesszx_host_color_ready)
#define netchesszx_session_identity_mqtt_session_id() ((uint16_t)netchesszx_mqtt_session_id)
#define netchesszx_text_game_start NETCHESS_PROTO_GAME_START

void netchesszx_session_configure(uint8_t role,
                                  uint8_t transport,
                                  uint8_t host_color);
const char *netchesszx_session_start_text(void);

#define netchesszx_session_is_host() \
    (netchesszx_session_role == NETCHESSZX_SESSION_ROLE_HOST)
#define netchesszx_session_can_start_game() \
    netchesszx_session_is_host()
#define netchesszx_transport_is_mqtt() \
    (netchesszx_transport == NETCHESSZX_TRANSPORT_MQTT)
#define netchesszx_local_is_white() \
    (netchesszx_local_color == NETCHESSZX_COLOR_WHITE)
#define netchesszx_session_has_local_turn(white_turn) \
    ((uint8_t)((white_turn) == netchesszx_local_is_white()))
#define netchesszx_local_side_char() \
    (netchesszx_local_is_white() ? 'W' : 'B')
#define netchesszx_remote_side_char() \
    (netchesszx_local_is_white() ? 'B' : 'W')
#define netchesszx_local_side_name() \
    (netchesszx_local_is_white() ? "WHITE" : "BLACK")
/* These operations own identity writes; callers still decide transitions until
   phase 4. They expand in place to preserve the resident/overlay cost. */
#define netchesszx_session_forget_color() (netchesszx_host_color_ready = 0u)
#define netchesszx_session_set_id(id) (netchesszx_mqtt_session_id = (id))
#define netchesszx_session_probe_identity(color, id) do { \
    netchesszx_local_color = (uint8_t)((color) ^ 1u); \
    netchesszx_mqtt_session_id = (id); \
} while (0)

#endif
