#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"

#define NETCHESSZX_DEFAULT_COLOR NETCHESSZX_COLOR_WHITE

uint8_t netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
uint8_t netchesszx_transport = NETCHESSZX_TRANSPORT_MQTT;
uint8_t netchesszx_local_color = NETCHESSZX_DEFAULT_COLOR;
uint8_t netchesszx_host_color = NETCHESSZX_DEFAULT_COLOR;
uint8_t netchesszx_host_color_ready = 1u;
uint8_t netchesszx_notation = NETCHESSZX_NOTATION_COORD;
uint8_t netchesszx_movement_hints = 0u;
uint8_t netchesszx_board_theme_index = 0u;
uint8_t netchesszx_piece_set_index = NETCHESSZX_PIECE_SET_STD;
uint8_t netchesszx_board_light_attr = 0x38u;
uint8_t netchesszx_board_dark_attr = 0x07u;
uint16_t netchesszx_mqtt_session_id = 0u;
#if defined(NETCHESSZX_FIXED_LOW_RAM)
__at(NETCHESSZX_LOWRAM_HINTED_ROWS_ADDR) uint8_t netchesszx_hinted_rows[8];
#else
uint8_t netchesszx_hinted_rows[8];
#endif
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

const char netchesszx_mqtt_host[] = NETCHESSZX_MQTT_HOST;
/* AT+CIPSTART prefix/tail, a five-digit uint16_t port, and the NUL. */
typedef char netchesszx_mqtt_host_size_check[
    (sizeof(netchesszx_mqtt_host) - 1u + 27u <=
     NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE)
        ? 1
        : -1];
char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u];
const uint16_t netchesszx_mqtt_port = NETCHESSZX_MQTT_PORT;
char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u] = "";
uint16_t netchesszx_direct_port = NETCHESSZX_PORT;
int8_t netchesszx_timezone = NETCHESSZX_TIME_RTC;
int8_t netchesszx_timezone_last = NETCHESSZX_TZ;
uint8_t netchesszx_rtc_available = 0u;
