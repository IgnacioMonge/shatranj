#include <stdio.h>
#include <string.h>

#ifndef __at
#define __at(address)
#endif

#include "spectrum/transport/mqtt_min.h"

static uint8_t host_mqtt_packet[SPECTRUM_MQTT_PACKET_MAX];
#undef SPECTRUM_MQTT_PACKET_SCRATCH
#define SPECTRUM_MQTT_PACKET_SCRATCH host_mqtt_packet

#include "spectrum/transport/mqtt_min.c"
#include "spectrum/transport/net.c"

static int failures;
static uint8_t at_ok;
static uint8_t send_ok;
static uint8_t prompt_ok;
static unsigned ensure_calls;
static const uint8_t *uart_data;
static uint8_t uart_len;
static uint8_t uart_pos;

void net_wait_frame(void) {}
void spectrum_net_runtime_wait_frame_plain(void) {}
uint8_t spectrum_overlay_exec_cached(uint8_t overlay_id, uint8_t entry_id)
{
    (void)overlay_id;
    (void)entry_id;
    return 0u;
}
uint8_t spectrum_uart_ready(void) { return (uint8_t)(uart_pos < uart_len); }
uint8_t spectrum_uart_read(void) { return uart_data[uart_pos++]; }

uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames)
{
    (void)cmd;
    (void)frames;
    return at_ok;
}

uint8_t spectrum_net_ensure_command_mode(void)
{
    ++ensure_calls;
    return 1u;
}

uint8_t spectrum_uart_send_string(const char *text)
{
    (void)text;
    return send_ok;
}

uint8_t spectrum_uart_send_crlf(void)
{
    return send_ok;
}

uint8_t spectrum_uart_send_bytes(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return 1u;
}

uint8_t wait_for_prompt(uint16_t frames)
{
    (void)frames;
    return prompt_ok;
}

void reset_line_buf(void)
{
}

static void check(int ok, const char *label)
{
    if (!ok) {
        printf("FAIL: %s\n", label);
        ++failures;
    }
}

static void set_stream(const uint8_t *data, uint8_t len)
{
    memcpy(MQTT_STREAM, data, len);
    mqtt_stream_len = len;
}

static void set_uart(const uint8_t *data, uint8_t len)
{
    uart_data = data;
    uart_len = len;
    uart_pos = 0u;
}

static void test_garbage_compacts_once_to_packet(void)
{
    static const uint8_t stream[] = { 0x01u, 0xffu, 0x30u, 0x02u, 0u, 1u };

    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 4, "garbage: finds MQTT packet");
    check(mqtt_stream_len == 4u, "garbage: discarded prefix once");
    check(MQTT_STREAM[0] == 0x30u && MQTT_STREAM[3] == 1u,
          "garbage: packet stays intact");
}

static void test_garbage_keeps_fragment_start(void)
{
    static const uint8_t stream[] = { 0x01u, 0xffu, 0xd0u };

    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 0, "fragment: waits for final byte");
    check(mqtt_stream_len == 1u && MQTT_STREAM[0] == 0xd0u,
          "fragment: keeps possible packet start");
    MQTT_STREAM[mqtt_stream_len++] = 0u;
    check(mqtt_take_stream_packet() == 2, "fragment: completes PINGRESP");
}

static void test_oversized_packet_does_not_disconnect(void)
{
    uint8_t stream[MQTT_STREAM_MAX];
    uint16_t pos = 0u;

    mqtt_reset_session_state();
    stream[pos++] = 0x30u;
    stream[pos++] = 0xa1u;
    stream[pos++] = 0x01u; /* 161-byte body: above the 160-byte cap. */
    memset(stream + pos, 'X', 161u);
    pos = (uint16_t)(pos + 161u);
    stream[pos++] = 0xd0u;
    stream[pos++] = 0u;
    set_stream(stream, (uint8_t)pos);

    check(mqtt_take_stream_packet() == 0,
          "oversized packet: discards without disconnecting");
    check(mqtt_stream_len == 2u,
          "oversized packet: preserves following packet");
    check(mqtt_take_stream_packet() == 2,
          "oversized packet: accepts the next broker packet");
}

static void test_fragmented_oversized_packet_does_not_desync(void)
{
    uint8_t stream[103u];

    mqtt_reset_session_state();
    stream[0u] = 0x30u;
    stream[1u] = 0xa1u;
    stream[2u] = 0x01u;
    memset(stream + 3u, 'X', 100u);
    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 0 && mqtt_discard_remaining == 61u,
          "fragmented oversized packet: remembers remaining body");

    memset(stream, 'X', 40u);
    set_uart(stream, 40u);
    check(mqtt_drain_uart_budget(40u) == 1u &&
              mqtt_discard_remaining == 21u,
          "fragmented oversized packet: drains middle fragment");

    memset(stream, 'X', 21u);
    stream[21u] = 0xd0u;
    stream[22u] = 0u;
    set_uart(stream, 23u);
    check(mqtt_drain_uart_budget(23u) == 1u && mqtt_stream_len == 2u,
          "fragmented oversized packet: keeps bytes after discarded body");
    check(mqtt_take_stream_packet() == 2 && mqtt_discard_remaining == 0u,
          "fragmented oversized packet: resumes at exact packet boundary");
}

static void test_malformed_varint_does_not_disconnect(void)
{
    static const uint8_t stream[] = {
        0x30u, 0x80u, 0x80u, 0xd0u, 0u
    };

    mqtt_reset_session_state();
    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 0,
          "malformed packet: does not disconnect");
    check(mqtt_stream_len == 4u,
          "malformed packet: drops only the invalid header byte");
    check(mqtt_take_stream_packet() == 2,
          "malformed packet: preserves and resumes at the next packet");
}

static void test_stream_entry_failure_restores_command_mode(void)
{
    at_ok = 0u;
    send_ok = 1u;
    prompt_ok = 1u;
    ensure_calls = 0u;
    mqtt_stream_active = 1u;
    mqtt_stream_len = 7u;

    check(!mqtt_enter_stream_mode(), "entry failure: reports failure");
    check(ensure_calls == 1u, "entry failure: restores command mode");
    check(!mqtt_stream_active && mqtt_stream_len == 0u,
          "entry failure: clears software stream state");

    at_ok = 1u;
    send_ok = 0u;
    ensure_calls = 0u;
    check(!mqtt_enter_stream_mode(), "send failure: reports failure");
    check(ensure_calls == 1u, "send failure: restores command mode");

    send_ok = 1u;
    prompt_ok = 0u;
    ensure_calls = 0u;
    check(!mqtt_enter_stream_mode(), "prompt failure: reports failure");
    check(ensure_calls == 1u, "prompt failure: restores command mode");

    prompt_ok = 1u;
    ensure_calls = 0u;
    check(mqtt_enter_stream_mode(), "entry success: enters stream mode");
    check(ensure_calls == 0u && mqtt_stream_active,
          "entry success: leaves command recovery idle");
}

static void test_three_publish_packets_survive_suback_wait(void)
{
    static const char *const topics[] = {
        "netchesszx/v1/NC0000/w2b",
        "netchesszx/v1/NC0000/ack_w",
        "netchesszx/v1/NC0000/b2w"
    };
    static const char *const payloads[] = { "ONE", "TWO", "THREE" };
    uint8_t ack[5u];
    char payload[SPECTRUM_NET_PAYLOAD_MAX];
    uint8_t expected_flags;
    uint8_t i;
    uint8_t len;
    uint8_t pos = 0u;

    mqtt_reset_session_state();
    for (i = 0u; i < 3u; ++i) {
        len = spectrum_mqtt_publish(MQTT_STREAM + pos,
                                    (uint8_t)(MQTT_STREAM_MAX - pos),
                                    (uint16_t)(i + 1u),
                                    topics[i], payloads[i],
                                    (uint8_t)(i == 0u));
        check(len != 0u, "publish queue: fixture packet built");
        pos = (uint8_t)(pos + len);
    }
    MQTT_STREAM[pos++] = 0x90u;
    MQTT_STREAM[pos++] = 0x03u;
    MQTT_STREAM[pos++] = 0u;
    MQTT_STREAM[pos++] = 4u;
    MQTT_STREAM[pos++] = 0u;
    mqtt_stream_len = pos;
    mqtt_stream_active = 1u;

    check(mqtt_wait_packet_into(ack, SPECTRUM_MQTT_SUBACK, 4u),
          "publish queue: third publish does not abort SUBACK wait");
    check(ack[2u] == 0u && ack[3u] == 4u && mqtt_payload_count == 3u,
          "publish queue: all three payloads retained");

    for (i = 0u; i < 3u; ++i) {
        check(spectrum_net_mqtt_read_payload(payload, sizeof(payload)) == 0 &&
                  strcmp(payload, payloads[i]) == 0,
              "publish queue: payload order preserved");
        expected_flags = (uint8_t)(
            (i == 0u ? SPECTRUM_LINK_PAYLOAD_RETAINED : 0u) |
            (i != 1u ? SPECTRUM_LINK_PAYLOAD_GAME_ROUTE : 0u));
        check(spectrum_net_payload_flags() == expected_flags,
              "publish queue: route and retained flags preserved");
    }
}

int main(void)
{
    test_garbage_compacts_once_to_packet();
    test_garbage_keeps_fragment_start();
    test_oversized_packet_does_not_disconnect();
    test_fragmented_oversized_packet_does_not_desync();
    test_malformed_varint_does_not_disconnect();
    test_stream_entry_failure_restores_command_mode();
    test_three_publish_packets_survive_suback_wait();

    if (failures != 0) {
        printf("%d MQTT stream tests failed\n", failures);
        return 1;
    }
    puts("MQTT stream tests passed");
    return 0;
}
