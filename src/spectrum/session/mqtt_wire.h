#ifndef NETCHESSZX_SPECTRUM_SESSION_MQTT_WIRE_H
#define NETCHESSZX_SPECTRUM_SESSION_MQTT_WIRE_H

#include <stdint.h>

#include "spectrum/session/route.h"

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define NETCHESSZX_SESSION_MQTT_SETUP_LIVE 0u
#define NETCHESSZX_SESSION_MQTT_SETUP_RETAINED 1u
#define NETCHESSZX_SESSION_MQTT_SETUP_CLEAR 2u

const char *spectrum_net_mqtt_out_suffix(void);
const char *spectrum_net_mqtt_in_suffix(void);
const char *spectrum_net_mqtt_out_ack_suffix(void);
const char *spectrum_net_mqtt_in_ack_suffix(void);
const char *spectrum_net_mqtt_presence_suffix(void);
const char *spectrum_net_mqtt_peer_presence_suffix(void);
const char *spectrum_net_mqtt_presence_payload(uint8_t route)
    NETCHESSZX_FASTCALL;
void spectrum_net_mqtt_setup_payload(char *out) NETCHESSZX_FASTCALL;

uint8_t netchesszx_session_mqtt_start(void);
uint8_t netchesszx_session_mqtt_activate_side(void);
uint8_t netchesszx_session_mqtt_probe_seat(void);
uint8_t netchesszx_session_mqtt_publish_presence(void);
uint8_t netchesszx_session_mqtt_publish_offline(uint8_t route)
    NETCHESSZX_FASTCALL;
#define netchesszx_session_mqtt_publish_setup spectrum_net_mqtt_publish_setup

#endif
