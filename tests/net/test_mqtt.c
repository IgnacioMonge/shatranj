#include "common/mqtt/mqtt.h"
#include "spectrum/transport/mqtt_min.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *label)
{
    if (!ok) {
        printf("FAIL: %s\n", label);
        ++failures;
    }
}

static void check_u16(uint16_t got, uint16_t expected, const char *label)
{
    if (got != expected) {
        printf("FAIL: %s got=%u expected=%u\n",
               label,
               (unsigned)got,
               (unsigned)expected);
        ++failures;
    }
}

static int feed_packet(const uint8_t *data,
                       size_t len,
                       netchess_mqtt_packet_t *packet)
{
    netchess_mqtt_parser_t parser;
    int rc = NETCHESSZX_MQTT_NEED_MORE;
    size_t i;

    netchess_mqtt_parser_init(&parser);
    for (i = 0u; i < len; ++i) {
        rc = netchess_mqtt_parser_feed(&parser, data[i], packet);
        if (i + 1u < len && rc != NETCHESSZX_MQTT_NEED_MORE) {
            return NETCHESSZX_MQTT_ERROR;
        }
    }
    return rc;
}

static void test_remaining_length(void)
{
    uint8_t out[4];
    size_t n;

    n = netchess_mqtt_encode_remaining(0u, out, sizeof(out));
    check(n == 1u && out[0] == 0x00u, "vli 0");

    n = netchess_mqtt_encode_remaining(127u, out, sizeof(out));
    check(n == 1u && out[0] == 0x7fu, "vli 127");

    n = netchess_mqtt_encode_remaining(128u, out, sizeof(out));
    check(n == 2u && out[0] == 0x80u && out[1] == 0x01u, "vli 128");

    n = netchess_mqtt_encode_remaining(16384u, out, sizeof(out));
    check(n == 3u && out[0] == 0x80u && out[1] == 0x80u && out[2] == 0x01u,
          "vli 16384");

    n = netchess_mqtt_encode_remaining(2097152u, out, sizeof(out));
    check(n == 4u && out[0] == 0x80u && out[1] == 0x80u &&
              out[2] == 0x80u && out[3] == 0x01u,
          "vli 2097152");
}

static void test_connect_encoder(void)
{
    uint8_t out[128];
    size_t n;

    n = netchess_mqtt_encode_connect(out,
                                     sizeof(out),
                                     "NXZXA",
                                     60u,
                                     "netchesszx/v1/CODE/pres_w",
                                     "OFFLINE W DISCONNECT",
                                     1u);
    check(n != 0u, "connect encode");
    check(out[0] == 0x10u, "connect type");
    check(memcmp(out + 4u, "MQTT", 4u) == 0, "connect protocol");
    check(out[9] == 0x26u, "connect flags clean will qos0 retain");
    check(out[10] == 0x00u && out[11] == 0x3cu, "connect keepalive");
}

static void test_publish_fragmentation(void)
{
    static const char topic[] = "netchesszx/v1/KATEMUNO7/w2b";
    static const uint8_t payload[] = "M 1 e2e4 2987";
    uint8_t out[256];
    netchess_mqtt_packet_t packet;
    size_t n;
    size_t cut;

    n = netchess_mqtt_encode_publish(out,
                                     sizeof(out),
                                     7u,
                                     topic,
                                     payload,
                                     (uint16_t)(sizeof(payload) - 1u),
                                     1u,
                                     0u);
    check(n != 0u, "publish encode");

    for (cut = 1u; cut <= n; ++cut) {
        netchess_mqtt_parser_t parser;
        int rc = NETCHESSZX_MQTT_NEED_MORE;
        size_t i;

        netchess_mqtt_parser_init(&parser);
        for (i = 0u; i < cut; ++i) {
            rc = netchess_mqtt_parser_feed(&parser, out[i], &packet);
            if (i + 1u < n) {
                check(rc == NETCHESSZX_MQTT_NEED_MORE, "publish prefix need more");
            }
        }
        for (i = cut; i < n; ++i) {
            rc = netchess_mqtt_parser_feed(&parser, out[i], &packet);
            if (i + 1u < n) {
                check(rc == NETCHESSZX_MQTT_NEED_MORE, "publish suffix need more");
            }
        }
        check(rc == NETCHESSZX_MQTT_PACKET_READY, "publish packet ready");
        check(packet.type == NETCHESS_MQTT_PUBLISH, "publish type");
        check(packet.qos == 1u, "publish qos");
        check_u16(packet.packet_id, 7u, "publish packet id");
        check(strcmp(packet.topic, topic) == 0, "publish topic");
        check(packet.payload_len == sizeof(payload) - 1u, "publish payload len");
        check(memcmp(packet.payload, payload, sizeof(payload) - 1u) == 0,
              "publish payload");
    }
}

static void test_ack_packets(void)
{
    uint8_t puback[4];
    uint8_t connack[] = {0x20u, 0x02u, 0x00u, 0x00u};
    uint8_t suback[] = {0x90u, 0x03u, 0x00u, 0x2au, 0x01u};
    uint8_t pingresp[] = {0xd0u, 0x00u};
    netchess_mqtt_packet_t packet;
    size_t n;

    n = netchess_mqtt_encode_puback(puback, sizeof(puback), 17u);
    check(n == 4u, "puback encode length");
    check(feed_packet(puback, n, &packet) == NETCHESSZX_MQTT_PACKET_READY,
          "puback parse");
    check(packet.type == NETCHESS_MQTT_PUBACK, "puback type");
    check_u16(packet.packet_id, 17u, "puback id");

    check(feed_packet(connack, sizeof(connack), &packet) == NETCHESSZX_MQTT_PACKET_READY,
          "connack parse");
    check(packet.type == NETCHESS_MQTT_CONNACK && packet.return_code == 0u,
          "connack fields");

    check(feed_packet(suback, sizeof(suback), &packet) == NETCHESSZX_MQTT_PACKET_READY,
          "suback parse");
    check(packet.type == NETCHESS_MQTT_SUBACK && packet.packet_id == 42u &&
              packet.return_code == 1u,
          "suback fields");

    check(feed_packet(pingresp, sizeof(pingresp), &packet) == NETCHESSZX_MQTT_PACKET_READY,
          "pingresp parse");
    check(packet.type == NETCHESS_MQTT_PINGRESP, "pingresp type");
}

static void test_rejections(void)
{
    uint8_t malformed_vli[] = {0x30u, 0x80u, 0x80u, 0x80u, 0x80u};
    uint8_t over_cap[] = {0x30u, 0x81u, 0x04u};
    uint8_t qos2[] = {0x34u, 0x00u};
    uint8_t long_topic[48];
    netchess_mqtt_packet_t packet;
    size_t i;
    size_t n;

    check(feed_packet(malformed_vli, sizeof(malformed_vli), &packet) ==
              NETCHESSZX_MQTT_ERROR,
          "malformed vli rejected");
    check(feed_packet(over_cap, sizeof(over_cap), &packet) == NETCHESSZX_MQTT_ERROR,
          "over cap rejected");
    check(feed_packet(qos2, sizeof(qos2), &packet) == NETCHESSZX_MQTT_ERROR,
          "qos2 rejected");

    for (i = 0u; i < sizeof(long_topic); ++i) {
        long_topic[i] = 'A';
    }
    n = netchess_mqtt_encode_publish(long_topic,
                                     sizeof(long_topic),
                                     1u,
                                     "netchesszx/v1/TOO_LONG_TOPIC_NAME_123456789",
                                     (const uint8_t *)"X",
                                     1u,
                                     1u,
                                     0u);
    check(n == 0u, "encoder cap protects long packet buffer");
}

static void test_subscribe_and_control_encoders(void)
{
    uint8_t out[96];
    size_t n;

    n = netchess_mqtt_encode_subscribe(out,
                                       sizeof(out),
                                       9u,
                                       "netchesszx/v1/KATEMUNO7/state",
                                       1u);
    check(n != 0u && out[0] == 0x82u, "subscribe encode");
    check(netchess_mqtt_encode_pingreq(out, sizeof(out)) == 2u &&
              out[0] == 0xc0u && out[1] == 0x00u,
          "pingreq encode");
    check(netchess_mqtt_encode_disconnect(out, sizeof(out)) == 2u &&
              out[0] == 0xe0u && out[1] == 0x00u,
          "disconnect encode");
}

static void test_spectrum_publish_parser_bounds(void)
{
    uint8_t wrapped_topic_len[] = {0x30u, 0x04u, 0xffu, 0xfdu, 'X', 'Y'};
    uint8_t packet[SPECTRUM_MQTT_PACKET_MAX];
    char payload[8];
    uint16_t packet_id;
    uint8_t retained;
    uint16_t n;
    int16_t got;

    n = spectrum_mqtt_publish(packet,
                              sizeof(packet),
                              5u,
                              "netchesszx/v1/DEV/b2w",
                              "1234567",
                              0u);
    check(n != 0u, "spectrum publish small encode");
    got = spectrum_mqtt_parse_publish(packet, n, payload, sizeof(payload), &packet_id, &retained);
    check(got == 7 && strcmp(payload, "1234567") == 0 && packet_id == 5u &&
              retained == 0u,
          "spectrum publish small parse");
    packet[0] |= 1u;
    got = spectrum_mqtt_parse_publish(packet, n, payload, sizeof(payload), &packet_id, &retained);
    check(got == 7 && retained == 1u, "spectrum retained flag parse");

    n = spectrum_mqtt_publish(packet,
                              sizeof(packet),
                              6u,
                              "netchesszx/v1/DEV/b2w",
                              "12345678",
                              0u);
    check(n != 0u, "spectrum publish oversized encode");
    got = spectrum_mqtt_parse_publish(packet, n, payload, sizeof(payload), &packet_id, &retained);
    check(got < 0, "spectrum publish oversized rejected");

    got = spectrum_mqtt_parse_publish(wrapped_topic_len,
                                      sizeof(wrapped_topic_len),
                                      payload,
                                      sizeof(payload),
                                      &packet_id,
                                      &retained);
    check(got < 0, "spectrum publish wrapped topic length rejected");
}

int main(void)
{
    test_remaining_length();
    test_connect_encoder();
    test_publish_fragmentation();
    test_ack_packets();
    test_rejections();
    test_subscribe_and_control_encoders();
    test_spectrum_publish_parser_bounds();

    if (failures != 0) {
        printf("mqtt tests failed: %d\n", failures);
        return 1;
    }
    printf("mqtt tests ok\n");
    return 0;
}
