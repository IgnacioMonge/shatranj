#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"

uint8_t netchesszx_notation = NETCHESSZX_NOTATION_COORD;
uint8_t netchesszx_movement_hints = 0u;
uint8_t netchesszx_board_theme_index = 0u;
uint8_t netchesszx_piece_set_index = NETCHESSZX_PIECE_SET_STD;
uint8_t netchesszx_board_light_attr = 0x38u;
uint8_t netchesszx_board_dark_attr = 0x07u;
#if defined(NETCHESSZX_FIXED_LOW_RAM)
__at(NETCHESSZX_LOWRAM_HINTED_ROWS_ADDR) uint8_t netchesszx_hinted_rows[8];
#else
uint8_t netchesszx_hinted_rows[8];
#endif
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
#if !defined(NETCHESSZX_FIXED_LOW_RAM)
char netchesszx_player_name_line[NETCHESS_PROTO_NAME_LINE_SIZE] =
    NETCHESS_PROTO_NAME_PREFIX;
char netchesszx_peer_name[NETCHESS_PLAYER_NAME_SIZE];
#endif
/* The fixed low-RAM identity holds exactly the MNAME line and one name. */
#if NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_SIZE != NETCHESS_PROTO_NAME_LINE_SIZE || \
    NETCHESSZX_LOWRAM_PEER_NAME_SIZE != NETCHESS_PLAYER_NAME_SIZE
#error "low-RAM player identity sizes differ from the MNAME protocol"
#endif
int8_t netchesszx_timezone = NETCHESSZX_TIME_RTC;
int8_t netchesszx_timezone_last = NETCHESSZX_TZ;
uint8_t netchesszx_rtc_available = 0u;
