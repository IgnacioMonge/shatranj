#ifndef NETCHESSZX_COMMON_MQTT_SESSION_PROTOCOL_H
#define NETCHESSZX_COMMON_MQTT_SESSION_PROTOCOL_H

#include <stdint.h>

#define NETCHESS_MQTT_SESSION_COLOR_WHITE 0u
#define NETCHESS_MQTT_SESSION_COLOR_BLACK 1u

#ifdef __cplusplus
extern "C" {
#endif

char netchess_mqtt_session_color_char(uint8_t color);
uint8_t netchess_mqtt_session_color_value(char color, uint8_t *out);
const char *netchess_mqtt_session_parse_u16_token(const char *p,
                                                  uint16_t *out);

uint8_t netchess_mqtt_session_parse_host(const char *payload,
                                         uint8_t *host_color,
                                         uint16_t *session_id);
uint8_t netchess_mqtt_session_parse_join(const char *payload,
                                         uint16_t *session_id);
uint8_t netchess_mqtt_session_parse_side(const char *payload,
                                         char verb,
                                         char *side,
                                         uint16_t *session_id,
                                         uint8_t *has_session_id);

uint8_t netchess_mqtt_session_format_host(char *out,
                                          uint8_t out_cap,
                                          uint8_t host_color,
                                          uint16_t session_id);
uint8_t netchess_mqtt_session_format_join(char *out,
                                          uint8_t out_cap,
                                          uint16_t session_id);
uint8_t netchess_mqtt_session_format_side(char *out,
                                          uint8_t out_cap,
                                          char verb,
                                          char side,
                                          uint16_t session_id,
                                          uint8_t include_session_id);

#ifdef __cplusplus
}
#endif

#endif
