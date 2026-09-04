#ifndef NETCHESSZX_SPECTRUM_NET_H
#define NETCHESSZX_SPECTRUM_NET_H

#include <stdint.h>
#include "spectrum/config/session.h"
#include "spectrum/transport/link.h"

#define SPECTRUM_NET_PAYLOAD_MAX SPECTRUM_LINK_PAYLOAD_MAX
#define SPECTRUM_NET_LINE_MAX 48u
#define SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP 64u
#define SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT 3u
#define SPECTRUM_NET_READ_TIMEOUT SPECTRUM_LINK_READ_TIMEOUT
#define SPECTRUM_NET_PAYLOAD_RETAINED SPECTRUM_LINK_PAYLOAD_RETAINED

uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames);
void spectrum_net_guard_wait(uint16_t frames) NETCHESSZX_FASTCALL;
uint8_t spectrum_net_ensure_command_mode(void);
const char *spectrum_net_mqtt_out_suffix(void);
const char *spectrum_net_mqtt_in_suffix(void);
const char *spectrum_net_mqtt_out_ack_suffix(void);
const char *spectrum_net_mqtt_in_ack_suffix(void);
const char *spectrum_net_mqtt_presence_suffix(void);
const char *spectrum_net_mqtt_peer_presence_suffix(void);
const char *spectrum_net_mqtt_presence_payload(void);
void spectrum_net_mqtt_setup_payload(char *out) NETCHESSZX_FASTCALL;
int16_t spectrum_net_mqtt_read_payload(char *payload, uint8_t payload_cap);
uint8_t spectrum_net_mqtt_send_text(const char *text) NETCHESSZX_FASTCALL;

#endif
