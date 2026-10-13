#include "spectrum/session/identity.h"

uint8_t netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
uint8_t netchesszx_transport = NETCHESSZX_TRANSPORT_MQTT;
uint8_t netchesszx_local_color = NETCHESSZX_COLOR_WHITE;
uint8_t netchesszx_host_color = NETCHESSZX_COLOR_WHITE;
uint8_t netchesszx_host_color_ready = 1u;
uint16_t netchesszx_mqtt_session_id = 0u;

void netchesszx_session_configure(uint8_t role,
                                  uint8_t transport,
                                  uint8_t host_color)
{
    if (role != NETCHESSZX_SESSION_ROLE_JOIN) {
        role = NETCHESSZX_SESSION_ROLE_HOST;
    }
    if (host_color != NETCHESSZX_COLOR_BLACK) {
        host_color = NETCHESSZX_COLOR_WHITE;
    }

    if (transport != NETCHESSZX_TRANSPORT_DIRECT) {
        transport = NETCHESSZX_TRANSPORT_MQTT;
    }

    netchesszx_session_role = role;
    netchesszx_transport = transport;
    netchesszx_host_color = host_color;
    netchesszx_local_color = (role == NETCHESSZX_SESSION_ROLE_HOST)
                                  ? host_color
                                  : (uint8_t)(host_color ^ 1u);
    netchesszx_host_color_ready = 1u;
}

const char *netchesszx_session_start_text(void)
{
    if (netchesszx_transport_is_mqtt()) {
        return netchesszx_text_game_start;
    }
    return netchesszx_local_is_white()
               ? "GAME START WHITE=HOST"
               : "GAME START WHITE=GUEST";
}
