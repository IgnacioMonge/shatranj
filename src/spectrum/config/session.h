#ifndef NETCHESSZX_SPECTRUM_CONFIG_SESSION_H
#define NETCHESSZX_SPECTRUM_CONFIG_SESSION_H

#include <stdint.h>

#include "common/protocol/game_protocol.h"
#include "spectrum/lowram_map.h"

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#ifndef NETCHESSZX_PORT
#define NETCHESSZX_PORT 5000
#endif

#ifndef NETCHESSZX_TZ
#define NETCHESSZX_TZ 2
#endif

#define NETCHESSZX_TIMEZONE_MIN (-11)
#define NETCHESSZX_TIMEZONE_MAX 13
#define NETCHESSZX_TIME_RTC 127

#define NETCHESSZX_STRINGIFY_2(x) #x
#define NETCHESSZX_STRINGIFY(x) NETCHESSZX_STRINGIFY_2(x)

#ifndef NETCHESSZX_MQTT_HOST
#ifdef NETCHESSZX_MQTT_HOST_TOKEN
#define NETCHESSZX_MQTT_HOST NETCHESSZX_STRINGIFY(NETCHESSZX_MQTT_HOST_TOKEN)
#else
#define NETCHESSZX_MQTT_HOST "test.mosquitto.org"
#endif
#endif

#ifndef NETCHESSZX_MQTT_PORT
#define NETCHESSZX_MQTT_PORT 1883
#endif

#ifndef NETCHESSZX_MQTT_CODE
#ifdef NETCHESSZX_MQTT_CODE_TOKEN
#define NETCHESSZX_MQTT_CODE NETCHESSZX_STRINGIFY(NETCHESSZX_MQTT_CODE_TOKEN)
#else
#define NETCHESSZX_MQTT_CODE "NC0000"
#endif
#endif

#define NETCHESSZX_MQTT_CODE_MAX 16u
#define NETCHESSZX_DIRECT_HOST_MAX 15u

#define NETCHESSZX_SESSION_ROLE_HOST 0u
#define NETCHESSZX_SESSION_ROLE_JOIN 1u

#define NETCHESSZX_TRANSPORT_DIRECT 0u
#define NETCHESSZX_TRANSPORT_MQTT 1u

#define NETCHESSZX_COLOR_WHITE 0u
#define NETCHESSZX_COLOR_BLACK 1u

#define NETCHESSZX_NOTATION_COORD 0u
#define NETCHESSZX_NOTATION_SAN 1u


#define NETCHESSZX_PIECE_SET_STD 0u

extern uint8_t netchesszx_session_role;
extern uint8_t netchesszx_transport;
extern uint8_t netchesszx_local_color;
extern uint8_t netchesszx_host_color;
extern uint8_t netchesszx_host_color_ready;
extern uint8_t netchesszx_notation;
extern uint8_t netchesszx_movement_hints;
extern uint8_t netchesszx_board_theme_index;
extern uint8_t netchesszx_piece_set_index;
extern uint8_t netchesszx_board_light_attr;
extern uint8_t netchesszx_board_dark_attr;
#if defined(NETCHESSZX_FIXED_LOW_RAM)
extern __at(NETCHESSZX_LOWRAM_HINTED_ROWS_ADDR) uint8_t netchesszx_hinted_rows[8];
#else
extern uint8_t netchesszx_hinted_rows[8];
#endif
extern uint16_t netchesszx_mqtt_session_id;
#define netchesszx_text_game_start NETCHESS_PROTO_GAME_START

void netchesszx_session_configure(uint8_t role,
                                  uint8_t transport,
                                  uint8_t host_color);
void netchesszx_board_theme_apply(uint8_t theme);
uint8_t netchesszx_piece_set_load(uint8_t set) NETCHESSZX_FASTCALL;
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
#define netchesszx_notation_is_san() \
    (netchesszx_notation == NETCHESSZX_NOTATION_SAN)

extern const char netchesszx_mqtt_host[];
extern char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u];
extern const uint16_t netchesszx_mqtt_port;
extern char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u];
extern uint16_t netchesszx_direct_port;
extern int8_t netchesszx_timezone;
extern int8_t netchesszx_timezone_last;
extern uint8_t netchesszx_rtc_available;

#endif
