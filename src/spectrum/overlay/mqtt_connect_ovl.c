#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/net.h"

#define WAIT_FAST 50
#define WAIT_SHORT 150
#define WAIT_LONG 2000

extern char last_ip[];
extern uint16_t mqtt_next_id;
extern uint8_t mqtt_stream_len;
extern uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len);
extern uint8_t mqtt_wait_packet_into(uint8_t *packet, uint8_t wanted, uint16_t frames);
extern uint8_t mqtt_enter_stream_mode(void);
extern void mqtt_abort_stream_mode(void);
extern uint16_t mqtt_connect_packet_ovl(void);
extern const char mqtt_will_topic_prefix[];

uint8_t mqtt_packet_ovl[SPECTRUM_MQTT_PACKET_MAX];

static uint16_t mqtt_alloc_id_ovl(void)
{
    uint16_t id = mqtt_next_id++;

    if (mqtt_next_id == 0u) {
        mqtt_next_id = 1u;
    }
    return id;
}

static void mqtt_topic_ovl(char *out, const char *suffix)
{
    char *p;

    p = spectrum_append_text(out, mqtt_will_topic_prefix);
    p = spectrum_append_text(p, netchesszx_mqtt_code);
    p = spectrum_append_text(p, "/");
    (void)spectrum_append_text(p, suffix);
}

static uint8_t mqtt_subscribe_suffix_ovl(const char *suffix)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint16_t len;
    uint16_t id;

    mqtt_topic_ovl(topic, suffix);
    id = mqtt_alloc_id_ovl();
    len = spectrum_mqtt_subscribe(mqtt_packet_ovl,
                                  SPECTRUM_MQTT_PACKET_MAX,
                                  id,
                                  topic);
    if (len == 0u || !mqtt_send_raw_packet(mqtt_packet_ovl, len) ||
        !mqtt_wait_packet_into(mqtt_packet_ovl, SPECTRUM_MQTT_SUBACK, WAIT_LONG)) {
        return 0u;
    }
    return (uint8_t)(mqtt_packet_ovl[1u] == 3u &&
                     mqtt_packet_ovl[2u] == (uint8_t)(id >> 8) &&
                     mqtt_packet_ovl[3u] == (uint8_t)id &&
                     mqtt_packet_ovl[4u] <= 2u);
}

static uint8_t mqtt_publish_suffix_ovl(const char *suffix,
                                       const char *payload,
                                       uint8_t retain)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint16_t len;
    uint16_t id;

    mqtt_topic_ovl(topic, suffix);
    id = mqtt_alloc_id_ovl();
    len = spectrum_mqtt_publish(mqtt_packet_ovl,
                                SPECTRUM_MQTT_PACKET_MAX,
                                id,
                                topic,
                                payload,
                                retain);
    if (len == 0u || !mqtt_send_raw_packet(mqtt_packet_ovl, len)) {
        return 0u;
    }
    return 1u;
}

static uint8_t mqtt_subscribe_all_ovl(void)
{
    return mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_ack_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_peer_presence_suffix());
}

static uint8_t mqtt_publish_setup_ovl(void)
{
    char setup[32];

    spectrum_net_mqtt_setup_payload(setup);
    return mqtt_publish_suffix_ovl("meta",
                                   setup,
                                   netchesszx_session_is_host() ? 1u : 0u);
}

static uint8_t mqtt_activate_side_internal(void)
{
    return mqtt_subscribe_all_ovl() &&
           mqtt_publish_suffix_ovl(spectrum_net_mqtt_presence_suffix(),
                                   spectrum_net_mqtt_presence_payload(),
                                   1u);
}

static uint8_t at_cmd_fast(const char *cmd)
{
    return spectrum_net_at_cmd(cmd, WAIT_FAST);
}

static void mqtt_prepare_single_link_ovl(void)
{
    (void)at_cmd_fast("AT+CIPCLOSE");
    (void)at_cmd_fast("AT+CIPMUX=0");
    (void)at_cmd_fast("AT+CIPMODE=0");
}

static uint8_t mqtt_tcp_connect_single_ovl(const char *host, uint16_t port)
{
    (void)spectrum_append_u16(
        spectrum_append_text(
            spectrum_append_text(spectrum_append_text((char *)mqtt_packet_ovl, "AT+CIPSTART=\"TCP\",\""), host),
            "\","),
        port);
    return spectrum_net_at_cmd((char *)mqtt_packet_ovl, WAIT_LONG);
}

static uint8_t radio_warm_up_ovl(void)
{
    if (!spectrum_net_ensure_command_mode()) {
        return 0u;
    }
    if (!spectrum_net_at_cmd("AT+CWMODE=1", WAIT_SHORT)) {
        (void)spectrum_net_at_cmd("AT+CWMODE_DEF=1", WAIT_SHORT);
    }
    (void)at_cmd_fast("AT+CWAUTOCONN=1");
    if (last_ip[0] == '\0' && !spectrum_net_query_ip_with_retry(3u)) {
        return 0u;
    }
    spectrum_net_publish_ip_status();
    return 1u;
}

static uint8_t mqtt_open_session_ovl(void)
{
    uint16_t len;

    if (!mqtt_tcp_connect_single_ovl(netchesszx_mqtt_host, netchesszx_mqtt_port)) {
        return 0u;
    }
    if (!mqtt_enter_stream_mode()) {
        return 0u;
    }
    len = mqtt_connect_packet_ovl();
    if (len == 0u || !mqtt_send_raw_packet(mqtt_packet_ovl, len) ||
        !mqtt_wait_packet_into(mqtt_packet_ovl, SPECTRUM_MQTT_CONNACK, WAIT_LONG) ||
        mqtt_packet_ovl[1u] != 2u ||
        mqtt_packet_ovl[3u] != 0u) {
        mqtt_abort_stream_mode();
        return 0u;
    }
    return 1u;
}

uint8_t mqtt_connect_start_ovl(void)
{
    mqtt_stream_len = 0u;
    mqtt_next_id = 1u;

    if (!radio_warm_up_ovl()) {
        return 0u;
    }

    if (!mqtt_open_session_ovl()) {
        spectrum_net_guard_wait(30u);
        if (!spectrum_net_ensure_command_mode()) {
            return 0u;
        }
        mqtt_prepare_single_link_ovl();
        if (!mqtt_open_session_ovl()) {
            return 0u;
        }
    }

    if (!mqtt_subscribe_suffix_ovl("meta")) {
        return 0u;
    }
    if (netchesszx_session_is_host() || netchesszx_host_color_ready) {
        if (!mqtt_activate_side_internal()) {
            return 0u;
        }
        (void)mqtt_publish_setup_ovl();
    }
    return 1u;
}

uint8_t mqtt_activate_side_ovl(void)
{
    return mqtt_activate_side_internal() && mqtt_publish_setup_ovl();
}

