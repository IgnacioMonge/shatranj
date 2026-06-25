#include "spectrum/transport/esp_at.h"

#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/platform.h"
#include "spectrum/lowram_map.h"
#ifndef NETCHESSZX_HOST_TEST
#include "spectrum/overlay/overlay.h"
#endif

#include <string.h>
#include "common/protocol/game_protocol.h"

#ifndef SPECTRUM_NET_LINE_MAX
#define SPECTRUM_NET_LINE_MAX 48u
#endif

#ifndef NETCHESSZX_TZ
#define NETCHESSZX_TZ 2
#endif
#define NETCHESSZX_STR2(x) #x
#define NETCHESSZX_STR(x) NETCHESSZX_STR2(x)

#define LINE_MAX SPECTRUM_NET_LINE_MAX
#define WAIT_SHORT 150
#define WAIT_MED 500
#define WAIT_INIT_CMD 30
#define LAST_IP_SIZE 18u
#define LAST_IP_TEXT_MAX 15u

/* Resident ABI for DIRECT/MQTT overlays. Keep out of esp_at.h; app/session use
   accessors only. */
char line_buf[LINE_MAX];
#ifdef NETCHESSZX_FIXED_LOW_RAM
__at(NETCHESSZX_LOWRAM_LAST_IP_ADDR) char last_ip[LAST_IP_SIZE];
#else
char last_ip[LAST_IP_SIZE];
#endif
uint8_t line_pos;
static const char at_ate0[] = "ATE0";
static const char at_cipclose[] = "AT+CIPCLOSE";
static const char at_cipclose_5[] = "AT+CIPCLOSE=5";
static const char at_cipmode_0[] = "AT+CIPMODE=0";
static const char at_cipmux_0[] = "AT+CIPMUX=0";
static const char at_cipserver_0[] = "AT+CIPSERVER=0";
static const char at_cwautoconn_1[] = "AT+CWAUTOCONN=1";
static const char at_cwmode_1[] = "AT+CWMODE=1";
static const char at_cwmode_def_1[] = "AT+CWMODE_DEF=1";

void net_wait_frame(void)
{
    spectrum_net_runtime_wait_frame();
}

void reset_line_buf(void)
{
    line_pos = 0u;
    line_buf[0] = '\0';
    line_buf[1] = '\0';
    line_buf[2] = '\0';
    line_buf[3] = '\0';
    line_buf[4] = '\0';
    line_buf[5] = '\0';
}

const char *spectrum_net_last_ip(void)
{
    return last_ip;
}

uint8_t read_line(uint16_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        while (spectrum_uart_ready()) {
            uint8_t c = spectrum_uart_read();

            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (line_pos == LINE_MAX) {
                    reset_line_buf();
                    continue;
                }
                if (line_pos != 0u) {
                    line_buf[line_pos] = '\0';
                    line_pos = 0u;
                    return 1u;
                }
                continue;
            }
            if (line_pos < (LINE_MAX - 1u)) {
                line_buf[line_pos++] = (char)c;
            } else {
                line_pos = LINE_MAX;
            }
        }
        net_wait_frame();
    }
    return 0u;
}

static uint8_t line_has(const char *needle) NETCHESSZX_FASTCALL
{
    return netchess_after_prefix(line_buf, needle) != 0;
}

static void capture_ip_from_line(void)
{
    const char *q = netchess_after_prefix(line_buf, "+CIFSR:STAIP,\"");
    uint8_t n;

    if (q == 0) {
        return;
    }

    n = 0u;
    while (*q != '\0' && *q != '"' && n < LAST_IP_TEXT_MAX) {
        last_ip[n++] = *q++;
    }
    last_ip[n] = '\0';
    if (netchess_after_prefix(last_ip, "0.0.0.0") != 0) {
        last_ip[0] = '\0';
    }
}

#ifdef NETCHESSZX_HOST_TEST
static uint8_t parse_2digits(const char *p)
{
    uint8_t tens = (uint8_t)(p[0] - '0');

    return (uint8_t)((tens << 3) + (tens << 1) + (uint8_t)(p[1] - '0'));
}

static void capture_time_from_line(void)
{
    const char *p = netchess_after_prefix(line_buf, "+CIPSNTPTIME:");
    if (p == 0) {
        return;
    }
    while (p[0] != '\0') {
        if (p[0] >= '0' && p[0] <= '2' &&
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

            if (p[8] == ' ' &&
                p[9] == '1' && p[10] == '9' &&
                p[11] == '7' && p[12] == '0') {
                return;
            }
            hour = parse_2digits(p);
            minute = parse_2digits(p + 3);
            second = parse_2digits(p + 6);
            if (hour < 24u) {
                spectrum_net_runtime_set_clock(hour, minute, second);
            }
            return;
        }
        ++p;
    }
}
#endif

#ifdef NETCHESSZX_HOST_TEST
static void test_set_line(const char *line)
{
    uint8_t n = 0u;

    reset_line_buf();
    while (line[n] != '\0' && n + 1u < LINE_MAX) {
        line_buf[n] = line[n];
        ++n;
    }
    line_buf[n] = '\0';
}

void netchesszx_esp_at_test_capture_ip(const char *line)
{
    test_set_line(line);
    capture_ip_from_line();
}

void netchesszx_esp_at_test_capture_time(const char *line)
{
    test_set_line(line);
    capture_time_from_line();
}
#endif

static uint8_t wait_for_ok(uint16_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        if (!read_line(1u)) {
            net_wait_frame();
            continue;
        }

        capture_ip_from_line();
#ifdef NETCHESSZX_HOST_TEST
        capture_time_from_line();
#endif
        if (line_has("OK") || line_has("ready")) {
            return 1u;
        }
        if (line_has("ERROR") || line_has("FAIL") ||
            line_has("CLOSED")) {
            return 0u;
        }
        net_wait_frame();
    }
    return 0u;
}

uint8_t wait_for_prompt(uint16_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        while (spectrum_uart_ready()) {
            uint8_t c = spectrum_uart_read();

            if (c == '>') {
                return 1u;
            }
        }
        net_wait_frame();
    }
    return 0u;
}

uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames)
{
    reset_line_buf();
    spectrum_uart_send_string(cmd);
    spectrum_uart_send_crlf();
    return wait_for_ok(frames);
}

void spectrum_net_guard_wait(uint16_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        net_wait_frame();
    }
}

static void drop_rx_available(uint16_t cap) NETCHESSZX_FASTCALL
{
    while (cap-- != 0u && spectrum_uart_ready()) {
        (void)spectrum_uart_read();
    }
    reset_line_buf();
}

static void flush_all_rx_buffers(void)
{
    drop_rx_available(255u);
    drop_rx_available(255u);
}

static void wait_drain(uint8_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        net_wait_frame();
        drop_rx_available(255u);
    }
}

static void escape_transparent_mode(void)
{
    wait_drain(55u);
    spectrum_uart_send_string("+++");
    wait_drain(55u);
    flush_all_rx_buffers();
}

static void hard_at_cmd(const char *cmd) NETCHESSZX_FASTCALL
{
    (void)spectrum_net_at_cmd(cmd, WAIT_INIT_CMD);
    reset_line_buf();
}

uint8_t spectrum_net_ensure_command_mode(void)
{
    spectrum_net_guard_wait(10u);
    flush_all_rx_buffers();

    escape_transparent_mode();
    hard_at_cmd(at_cipmode_0);
    hard_at_cmd(at_cipclose);
    hard_at_cmd(at_ate0);
    hard_at_cmd(at_cipserver_0);
    hard_at_cmd(at_cipmux_0);

    reset_line_buf();
    spectrum_uart_send_string("AT");
    spectrum_uart_send_crlf();
    return wait_for_ok(150u);
}

void spectrum_net_publish_ip_status(void)
{
    spectrum_net_runtime_publish_ip_status(last_ip);
}

uint8_t spectrum_net_query_ip_with_retry(uint8_t attempts) NETCHESSZX_FASTCALL
{
    while (attempts-- != 0u) {
        last_ip[0] = '\0';
        (void)spectrum_net_at_cmd("AT+CIFSR", WAIT_MED);
        if (last_ip[0] != '\0') {
            return 1u;
        }
        spectrum_net_guard_wait(50u);
    }
    return 0u;
}

uint8_t spectrum_net_prepare_radio(void)
{
    spectrum_uart_flush(30u);
    (void)spectrum_net_at_cmd(at_ate0, WAIT_SHORT);
    if (!spectrum_net_at_cmd(at_cwmode_1, WAIT_MED)) {
        (void)spectrum_net_at_cmd(at_cwmode_def_1, WAIT_MED);
    }
    (void)spectrum_net_at_cmd(at_cwautoconn_1, WAIT_SHORT);
    (void)spectrum_net_at_cmd(at_cipserver_0, WAIT_MED);
    (void)spectrum_net_at_cmd(at_cipclose, WAIT_MED);
    (void)spectrum_net_at_cmd(at_cipclose_5, WAIT_MED);
    (void)spectrum_net_at_cmd(at_cipmux_0, WAIT_MED);
    (void)spectrum_net_at_cmd(at_cipmode_0, WAIT_MED);
    (void)spectrum_net_at_cmd(at_cipserver_0, WAIT_MED);
    return 1u;
}

uint8_t spectrum_net_sync_time(void)
{
#ifdef NETCHESSZX_HOST_TEST
    return 0u;
#else
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_SYNC_TIME);
#endif
}
