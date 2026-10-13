#ifndef NETCHESSZX_SPECTRUM_NET_H
#define NETCHESSZX_SPECTRUM_NET_H

#include <stdint.h>
#include "spectrum/transport/link.h"

#define SPECTRUM_NET_PAYLOAD_MAX SPECTRUM_LINK_PAYLOAD_MAX
#define SPECTRUM_NET_LINE_MAX 48u
#define SPECTRUM_NET_DIRECT_TX_PAYLOAD_CAP 64u
#define SPECTRUM_NET_DIRECT_RX_QUEUE_COUNT 3u
#define SPECTRUM_NET_READ_TIMEOUT SPECTRUM_LINK_READ_TIMEOUT
#define SPECTRUM_NET_PAYLOAD_RETAINED SPECTRUM_LINK_PAYLOAD_RETAINED
/* Shared by the resident MQTT reader and the MQTT_CONNECT packet wait. */
#define SPECTRUM_NET_MQTT_READ_TIMEOUT (-4)
#define MQTT_PAYLOAD_QUEUE_COUNT 3u

uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames);
void spectrum_net_guard_wait(uint16_t frames) NETCHESSZX_FASTCALL;
uint8_t spectrum_net_ensure_command_mode(void);
int16_t spectrum_net_mqtt_read_payload(char *payload, uint8_t payload_cap);

#endif
