#include "spectrum/transport/mqtt_min.h"

static uint8_t mqtt_strlen8(const char *s)
{
    uint8_t n = 0u;
    while (s[n] != '\0') { ++n; }
    return n;
}

static void mqtt_copy(uint8_t *dst, const uint8_t *src, uint8_t len)
{
    while (len-- != 0u) { *dst++ = *src++; }
}

#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif

static uint8_t *mqtt_out;
static uint8_t mqtt_cap;
static uint8_t mqtt_pos;

static uint8_t put_u8(uint8_t value) NETCHESSZX_FASTCALL
{
    if (mqtt_pos >= mqtt_cap) {
        return 0u;
    }
    mqtt_out[mqtt_pos] = value;
    ++mqtt_pos;
    return 1u;
}

static uint8_t put_u16(uint16_t value) NETCHESSZX_FASTCALL
{
    if (!put_u8((uint8_t)(value >> 8))) {
        return 0u;
    }
    return put_u8((uint8_t)value);
}

static uint8_t put_bytes(const uint8_t *src, uint8_t len)
{
    if (len > mqtt_cap || mqtt_pos > (uint8_t)(mqtt_cap - len)) {
        return 0u;
    }
    mqtt_copy(mqtt_out + mqtt_pos, src, len);
    mqtt_pos = (uint8_t)(mqtt_pos + len);
    return 1u;
}

static uint8_t put_string(const char *text) NETCHESSZX_FASTCALL
{
    uint8_t len = mqtt_strlen8(text);

    return (uint8_t)(put_u16(len) &&
                     put_bytes((const uint8_t *)text, len));
}

static uint8_t encode_remaining(uint8_t value, uint8_t *out)
{
    if (value < 128u) {
        out[0] = value;
        return 1u;
    }
    out[0] = (uint8_t)((value & 0x7fu) | 0x80u);
    out[1] = 1u;
    return 2u;
}

static uint8_t start_packet(uint8_t header, uint8_t remaining)
{
    uint8_t enc[3];
    uint8_t enc_len = encode_remaining(remaining, enc);

    if (enc_len == 0u || mqtt_cap < (uint8_t)(1u + enc_len + remaining)) {
        return 0u;
    }
    mqtt_pos = 0u;
    return (uint8_t)(put_u8(header) &&
                     put_bytes(enc, enc_len));
}

uint8_t spectrum_mqtt_subscribe(uint8_t *out,
                                 uint8_t cap,
                                 uint16_t packet_id,
                                 const char *topic)
{
    uint8_t topic_len = mqtt_strlen8(topic);
    uint8_t remaining = (uint8_t)(2u + 2u + topic_len + 1u);

    mqtt_out = out;
    mqtt_cap = cap;

    if (packet_id == 0u || topic_len > SPECTRUM_MQTT_TOPIC_MAX ||
        !start_packet(0x82u, remaining)) {
        return 0u;
    }
    if (!put_u16(packet_id) ||
        !put_string(topic) ||
        !put_u8(1u)) {
        return 0u;
    }
    return mqtt_pos;
}

uint8_t spectrum_mqtt_publish(uint8_t *out,
                               uint8_t cap,
                               uint16_t packet_id,
                               const char *topic,
                               const char *payload,
                               uint8_t retain)
{
    uint8_t topic_len = mqtt_strlen8(topic);
    uint8_t payload_len = mqtt_strlen8(payload);
    uint8_t remaining = (uint8_t)(2u + topic_len + 2u + payload_len);

    mqtt_out = out;
    mqtt_cap = cap;

    if (packet_id == 0u || topic_len > SPECTRUM_MQTT_TOPIC_MAX ||
        payload_len > SPECTRUM_MQTT_PAYLOAD_MAX ||
        !start_packet((uint8_t)(0x32u | (retain ? 1u : 0u)), remaining)) {
        return 0u;
    }
    if (!put_string(topic) ||
        !put_u16(packet_id) ||
        !put_bytes((const uint8_t *)payload, payload_len)) {
        return 0u;
    }
    return mqtt_pos;
}

uint8_t spectrum_mqtt_type(const uint8_t *packet, uint8_t len)
{
    if (len == 0u) {
        return 0u;
    }
    return (uint8_t)(packet[0] >> 4);
}

int16_t spectrum_mqtt_parse_publish(const uint8_t *packet,
                                    uint8_t len,
                                    char *payload,
                                    uint8_t payload_cap,
                                    uint16_t *packet_id,
                                    uint8_t *retained)
{
    uint8_t remaining;
    uint8_t pos;
    uint8_t end;
    uint8_t topic_len;
    uint8_t payload_len;
    uint8_t qos;
    uint8_t b;

    *packet_id = 0u;
    *retained = 0u;
    if (len < 4u || (packet[0] >> 4) != SPECTRUM_MQTT_PUBLISH) {
        return -1;
    }
    *retained = (uint8_t)(packet[0] & 1u);
    b = packet[1u];
    remaining = (uint8_t)(b & 0x7fu);
    pos = 2u;
    if ((b & 0x80u) != 0u) {
        if (len < 5u || (packet[2u] & 0x80u) != 0u) {
            return -1;
        }
        remaining = (uint8_t)(remaining + ((uint8_t)(packet[2u] & 0x7fu) << 7));
        pos = 3u;
    }
    end = (uint8_t)(pos + remaining);
    if (len < end) {
        return -1;
    }
    qos = (uint8_t)((packet[0] >> 1) & 3u);
    if (qos > 1u) {
        return -1;
    }
    if ((uint8_t)(pos + 2u) > end) {
        return -1;
    }
    if (packet[pos] != 0u) {
        return -1;
    }
    topic_len = packet[pos + 1u];
    pos = (uint8_t)(pos + 2u);
    if (topic_len > (uint8_t)(end - pos)) {
        return -1;
    }
    pos = (uint8_t)(pos + topic_len);
    if (qos != 0u) {
        if ((uint8_t)(pos + 2u) > end) {
            return -1;
        }
        *packet_id = (uint16_t)(((uint16_t)packet[pos] << 8) | packet[pos + 1u]);
        pos = (uint8_t)(pos + 2u);
    }
    payload_len = (uint8_t)(end - pos);
    if (payload_cap == 0u || payload_len >= payload_cap) {
        return -1;
    }
    mqtt_copy((uint8_t *)payload, packet + pos, payload_len);
    payload[payload_len] = '\0';
    return (int16_t)payload_len;
}
