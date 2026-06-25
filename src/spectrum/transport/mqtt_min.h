#ifndef NETCHESSZX_SPECTRUM_MQTT_MIN_H
#define NETCHESSZX_SPECTRUM_MQTT_MIN_H

#include <stdint.h>

#define SPECTRUM_MQTT_PACKET_MAX 160u
#define SPECTRUM_MQTT_TOPIC_MAX 48u
#define SPECTRUM_MQTT_PAYLOAD_MAX 96u
#define SPECTRUM_MQTT_SCRATCH_BASE 0x6531u
#define SPECTRUM_MQTT_PACKET_SCRATCH ((uint8_t *)SPECTRUM_MQTT_SCRATCH_BASE)

#define SPECTRUM_MQTT_CONNACK 2u
#define SPECTRUM_MQTT_PUBLISH 3u
#define SPECTRUM_MQTT_PUBACK 4u
#define SPECTRUM_MQTT_SUBACK 9u
#define SPECTRUM_MQTT_PINGRESP 13u

uint8_t spectrum_mqtt_subscribe(uint8_t *out,
                                 uint8_t cap,
                                 uint16_t packet_id,
                                 const char *topic);
uint8_t spectrum_mqtt_publish(uint8_t *out,
                               uint8_t cap,
                               uint16_t packet_id,
                               const char *topic,
                               const char *payload,
                               uint8_t retain);
uint8_t spectrum_mqtt_type(const uint8_t *packet, uint8_t len);
int16_t spectrum_mqtt_parse_publish(const uint8_t *packet,
                                    uint8_t len,
                                    char *payload,
                                    uint8_t payload_cap,
                                    uint16_t *packet_id,
                                    uint8_t *retained);

#endif
