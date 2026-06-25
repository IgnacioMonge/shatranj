#include "spectrum/transport/esp_at.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clock_ready;
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static const uint8_t *uart_feed;
static uint16_t uart_feed_len;
static uint16_t uart_feed_pos;

extern char line_buf[];
extern char last_ip[];

void spectrum_net_runtime_wait_frame(void)
{
}

void spectrum_net_runtime_wait_frame_plain(void)
{
}

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    clock_ready = 1u;
    clock_hour = hour;
    clock_minute = minute;
    clock_second = second;
}

uint8_t spectrum_net_runtime_clock_ready(void)
{
    return clock_ready;
}

void spectrum_net_runtime_publish_ip_status(const char *ip)
{
    (void)ip;
}

void spectrum_uart_init(void)
{
}

void spectrum_uart_flush(uint16_t frames)
{
    (void)frames;
}

void spectrum_uart_send_string(const char *s)
{
    (void)s;
}

void spectrum_uart_send_bytes(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

void spectrum_uart_send_crlf(void)
{
}

uint8_t spectrum_uart_ready(void)
{
    return (uint8_t)(uart_feed_pos < uart_feed_len);
}

uint8_t spectrum_uart_read(void)
{
    if (uart_feed_pos >= uart_feed_len) {
        return 0u;
    }
    return uart_feed[uart_feed_pos++];
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void reset_clock(void)
{
    clock_ready = 0u;
    clock_hour = 0u;
    clock_minute = 0u;
    clock_second = 0u;
}

static void feed_uart(const char *text)
{
    uart_feed = (const uint8_t *)text;
    uart_feed_len = (uint16_t)strlen(text);
    uart_feed_pos = 0u;
    reset_line_buf();
}

static void test_read_line(void)
{
    feed_uart("\r\nOK\r\n");
    check(read_line(1u), "reads CRLF line");
    check(strcmp(line_buf, "OK") == 0, "CR stripped and empty line ignored");

    feed_uart("12345678901234567890123456789012345678901234567\n");
    check(read_line(1u), "reads max line");
    check(strlen(line_buf) == 47u, "max line length");

    feed_uart("123456789012345678901234567890123456789012345678\nOK\n");
    check(read_line(1u), "overflow resets and skips overflowing line");
    check(strcmp(line_buf, "OK") == 0, "next line after overflow");

    feed_uart("\n\nA\n");
    check(read_line(1u), "empty lines skipped before payload");
    check(strcmp(line_buf, "A") == 0, "payload after empty lines");
}

static void test_capture_ip(void)
{
    last_ip[0] = '\0';
    netchesszx_esp_at_test_capture_ip("+CIFSR:STAIP,\"192.168.1.44\"");
    check(strcmp(spectrum_net_last_ip(), "192.168.1.44") == 0,
          "captures STAIP");

    netchesszx_esp_at_test_capture_ip("+CIFSR:APIP,\"10.0.0.1\"");
    check(strcmp(spectrum_net_last_ip(), "192.168.1.44") == 0,
          "ignores non-STAIP lines");

    netchesszx_esp_at_test_capture_ip("+CIFSR:STAIP,\"0.0.0.0\"");
    check(spectrum_net_last_ip()[0] == '\0', "zero IP clears last_ip");
}

static void test_capture_time(void)
{
    reset_clock();
    netchesszx_esp_at_test_capture_time(
        "+CIPSNTPTIME:Fri Jun  5 12:34:56 2026");
    check(clock_ready, "captures SNTP time");
    check(clock_hour == 12u && clock_minute == 34u && clock_second == 56u,
          "SNTP time fields");

    reset_clock();
    netchesszx_esp_at_test_capture_time(
        "+CIPSNTPTIME:Thu Jan  1 00:00:01 1970");
    check(!clock_ready, "ignores 1970 default time");

    reset_clock();
    netchesszx_esp_at_test_capture_time(
        "+CIPSNTPTIME:Fri Jun  5 29:01:02 2026");
    check(!clock_ready, "rejects invalid hour");
}

int main(void)
{
    test_read_line();
    test_capture_ip();
    test_capture_time();
    puts("esp_at tests ok");
    return 0;
}
