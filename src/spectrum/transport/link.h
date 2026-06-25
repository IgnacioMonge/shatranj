#ifndef NETCHESSZX_SPECTRUM_LINK_H
#define NETCHESSZX_SPECTRUM_LINK_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

#define SPECTRUM_LINK_PAYLOAD_MAX 48u
#define SPECTRUM_LINK_READ_TIMEOUT (-3)
#define SPECTRUM_LINK_CANCELLED 2u
#define SPECTRUM_LINK_PAYLOAD_RETAINED 0x01u

void spectrum_net_start_uart(void);
uint8_t spectrum_net_listen(void);
uint8_t spectrum_net_connect_host(void);
uint8_t spectrum_net_wait_pc_connect(void);
int16_t spectrum_net_read_payload(char *payload, uint8_t payload_cap);
uint8_t spectrum_net_send_text(const char *text);
uint8_t spectrum_net_send_ping(void);
char *spectrum_net_payload_scratch(void);
uint8_t spectrum_net_link_activity(void);
uint8_t spectrum_net_payload_flags(void);
void spectrum_net_background_drain(void);

#define spectrum_link_start_uart spectrum_net_start_uart
#define spectrum_link_listen spectrum_net_listen
#define spectrum_link_connect_host spectrum_net_connect_host
#define spectrum_link_wait_pc_connect spectrum_net_wait_pc_connect
#define spectrum_link_read_payload spectrum_net_read_payload
#define spectrum_link_send_text spectrum_net_send_text
#define spectrum_link_send_ping spectrum_net_send_ping
#define spectrum_link_payload_scratch spectrum_net_payload_scratch
#define spectrum_link_activity spectrum_net_link_activity
#define spectrum_link_payload_flags spectrum_net_payload_flags
#define spectrum_link_background_drain spectrum_net_background_drain

uint8_t spectrum_net_mqtt_start(void);
uint8_t spectrum_net_mqtt_activate_side(void);
uint8_t spectrum_net_mqtt_publish_setup(uint8_t retain) NETCHESSZX_FASTCALL;

#define spectrum_link_mqtt_start spectrum_net_mqtt_start
#define spectrum_link_mqtt_activate_side spectrum_net_mqtt_activate_side
#define spectrum_link_mqtt_publish_setup spectrum_net_mqtt_publish_setup

#endif
