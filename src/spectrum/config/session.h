#ifndef NETCHESSZX_SPECTRUM_CONFIG_SESSION_H
#define NETCHESSZX_SPECTRUM_CONFIG_SESSION_H

#include <stdint.h>

#include "spectrum/session/identity.h"
#include "spectrum/lowram_map.h"
#include "common/protocol/platform_protocol.h"

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

#define NETCHESSZX_NOTATION_COORD 0u
#define NETCHESSZX_NOTATION_SAN 1u


#define NETCHESSZX_PIECE_SET_STD 0u
#define NETCHESSZX_PIECE_SET_COUNT 3u

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

void netchesszx_board_theme_apply(uint8_t theme);
uint8_t netchesszx_piece_set_load(uint8_t set) NETCHESSZX_FASTCALL;

#define netchesszx_notation_is_san() \
    (netchesszx_notation == NETCHESSZX_NOTATION_SAN)

extern const char netchesszx_mqtt_host[];
extern char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u];
extern const uint16_t netchesszx_mqtt_port;
extern char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u];
extern uint16_t netchesszx_direct_port;
#ifdef NETCHESSZX_SPECTRANEXT
/* MATCH: the DIRECT wire protocol over a peer chosen in the firmware
   matchmaking menu. Both roles connect to the service; neither listens. */
#define NETCHESSZX_MATCH_HOST "shatranj@match.spx"
#if defined(NETCHESSZX_FIXED_LOW_RAM)
#define netchesszx_link_match \
    (*(uint8_t *)NETCHESSZX_LOWRAM_SPXN_LINK_MATCH_ADDR)
#else
extern uint8_t netchesszx_link_match;
#endif
#define netchesszx_transport_is_match() \
    (!netchesszx_transport_is_mqtt() && netchesszx_link_match != 0u)
#endif
/* "MNAME " followed by the configured player name (empty when unset): the
   advisory line is sent as-is and the name is read in place. The peer name
   is the last valid MNAME accepted from the current peer, or empty. */
#if defined(NETCHESSZX_FIXED_LOW_RAM)
/* Fixed low-RAM addresses, so resident and overlay code share no symbol. */
#define netchesszx_player_name_line \
    ((char *)NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR)
#define netchesszx_peer_name ((char *)NETCHESSZX_LOWRAM_PEER_NAME_ADDR)
#else
extern char netchesszx_player_name_line[NETCHESS_PROTO_NAME_LINE_SIZE];
extern char netchesszx_peer_name[NETCHESS_PLAYER_NAME_SIZE];
#endif
#define netchesszx_player_name \
    (netchesszx_player_name_line + NETCHESS_PROTO_NAME_PREFIX_LEN)
extern int8_t netchesszx_timezone;
extern int8_t netchesszx_timezone_last;
extern uint8_t netchesszx_rtc_available;

#endif
