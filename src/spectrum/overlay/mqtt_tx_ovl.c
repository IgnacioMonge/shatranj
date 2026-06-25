#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/net.h"

extern uint16_t mqtt_next_id;
extern char line_buf[];
extern uint8_t read_line(uint16_t frames) __z88dk_fastcall;
extern void reset_line_buf(void);
extern void net_wait_frame(void);
extern uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len);

#define WAIT_SHORT 150

static uint16_t mqtt_tx_alloc_id(void)
{
    uint16_t id = mqtt_next_id++;

    if (mqtt_next_id == 0u) {
        mqtt_next_id = 1u;
    }
    return id;
}

static uint8_t mqtt_tx_prefix(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0u;
        }
    }
    return 1u;
}

static void mqtt_tx_topic(char *out, const char *suffix)
{
    char *p;

    p = spectrum_append_text(out, "netchesszx/v1/");
    p = spectrum_append_text(p, netchesszx_mqtt_code);
    p = spectrum_append_text(p, "/");
    (void)spectrum_append_text(p, suffix);
}

static uint8_t mqtt_tx_publish_suffix(const char *suffix,
                                      const char *payload,
                                      uint8_t retain)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint16_t len;

    mqtt_tx_topic(topic, suffix);
    len = spectrum_mqtt_publish(SPECTRUM_MQTT_PACKET_SCRATCH,
                                SPECTRUM_MQTT_PACKET_MAX,
                                mqtt_tx_alloc_id(),
                                topic,
                                payload,
                                retain);
    if (len == 0u) {
        return 0u;
    }
    return mqtt_send_raw_packet(SPECTRUM_MQTT_PACKET_SCRATCH, len);
}

static uint8_t mqtt_parse_2digits_ovl(const char *p)
{
    uint8_t tens = (uint8_t)(p[0] - '0');

    return (uint8_t)((tens << 3) + (tens << 1) + (uint8_t)(p[1] - '0'));
}

static uint8_t mqtt_has_chars_ovl(const char *p, uint8_t count)
{
    while (count-- != 0u) {
        if (*p++ == '\0') {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t mqtt_time_payload_ovl(const char *p)
{
    while (p[0] != '\0') {
        if (mqtt_has_chars_ovl(p, 8u) &&
            p[0] >= '0' && p[0] <= '2' &&
            p[1] >= '0' && p[1] <= '9' &&
            p[2] == ':' &&
            p[3] >= '0' && p[3] <= '5' &&
            p[4] >= '0' && p[4] <= '9' &&
            p[5] == ':' &&
            p[6] >= '0' && p[6] <= '5' &&
            p[7] >= '0' && p[7] <= '9') {
            uint8_t hour;
            uint8_t minute;
            uint8_t second;

            if (mqtt_has_chars_ovl(p, 13u) &&
                p[8] == ' ' &&
                p[9] == '1' && p[10] == '9' &&
                p[11] == '7' && p[12] == '0') {
                return 0u;
            }
            hour = mqtt_parse_2digits_ovl(p);
            minute = mqtt_parse_2digits_ovl(p + 3);
            second = mqtt_parse_2digits_ovl(p + 6);
            if (hour < 24u) {
                spectrum_net_runtime_set_clock(hour, minute, second);
                return 1u;
            }
            return 0u;
        }
        ++p;
    }
    return 0u;
}

static uint8_t mqtt_capture_time_ovl(void)
{
    char *p = line_buf;

    while (p[0] != '\0') {
        if (mqtt_has_chars_ovl(p, 13u) &&
            p[0] == '+' && p[1] == 'C' && p[2] == 'I' &&
            p[3] == 'P' && p[4] == 'S' && p[5] == 'N' &&
            p[6] == 'T' && p[7] == 'P' && p[8] == 'T' &&
            p[9] == 'I' && p[10] == 'M' && p[11] == 'E' &&
            p[12] == ':') {
            return mqtt_time_payload_ovl(p + 13);
        }
        ++p;
    }
    return 0u;
}

static void mqtt_send_at_ovl(const char *cmd)
{
    reset_line_buf();
    spectrum_uart_send_string(cmd);
    spectrum_uart_send_crlf();
}

static uint8_t mqtt_wait_time_ovl(uint16_t frames)
{
    while (frames-- != 0u) {
        if (read_line(1u) && mqtt_capture_time_ovl()) {
            return 1u;
        }
        net_wait_frame();
    }
    return spectrum_net_runtime_clock_ready();
}

uint8_t mqtt_tx_sync_time_ovl(void)
{
    uint8_t i;

    if (!spectrum_net_at_cmd("AT+CIPSNTPCFG=1," NETCHESSZX_STRINGIFY(NETCHESSZX_TZ) ",\"pool.ntp.org\",\"time.google.com\"", WAIT_SHORT)) {
        (void)spectrum_net_at_cmd("AT+CIPSNTPCFG=1," NETCHESSZX_STRINGIFY(NETCHESSZX_TZ), WAIT_SHORT);
    }

    spectrum_net_guard_wait(100u);
    for (i = 0u; i < 3u; ++i) {
        mqtt_send_at_ovl("AT+CIPSNTPTIME?");
        if (mqtt_wait_time_ovl(WAIT_SHORT)) {
            return 1u;
        }
        spectrum_net_guard_wait(50u);
    }
    return 0u;
}

uint8_t mqtt_tx_send_text_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *text = (const char *)((uint16_t)ctx[0] | ((uint16_t)ctx[1] << 8));

    if (mqtt_tx_prefix(text, "ACK GAME START")) {
        return mqtt_tx_publish_suffix("meta", text, 0u);
    }
    if (mqtt_tx_prefix(text, "ACK ")) {
        return mqtt_tx_publish_suffix(spectrum_net_mqtt_out_ack_suffix(),
                                      text,
                                      0u);
    }
    if (mqtt_tx_prefix(text, netchesszx_text_game_start)) {
        return mqtt_tx_publish_suffix("meta", text, 0u);
    }
    return mqtt_tx_publish_suffix(spectrum_net_mqtt_out_suffix(), text, 0u);
}

uint8_t mqtt_tx_publish_setup_ovl(uint8_t *ctx) __z88dk_fastcall
{
    char setup[32];

    spectrum_net_mqtt_setup_payload(setup);
    return mqtt_tx_publish_suffix("meta", setup, ctx[0]);
}

uint8_t mqtt_tx_publish_session_ovl(void)
{
    if (!mqtt_tx_publish_suffix(spectrum_net_mqtt_presence_suffix(),
                                spectrum_net_mqtt_presence_payload(),
                                1u)) {
        return 0u;
    }
    spectrum_overlay_context[0] = netchesszx_session_is_host() ? 1u : 0u;
    return mqtt_tx_publish_setup_ovl(spectrum_overlay_context);
}
