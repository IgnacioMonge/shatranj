#include "spectrum/transport/esp_at.h"
#include "spectrum/transport/next_baud_recovery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clock_ready;
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static uint16_t fat_date;
static uint16_t fat_time;
static const uint8_t *uart_feed;
static uint16_t uart_feed_len;
static uint16_t uart_feed_pos;
static uint8_t uart_string_ok = 1u;
static uint8_t uart_crlf_ok = 1u;
#ifdef NETCHESSZX_NEXT
#define TEST_BAUD_115200 115200ul
#define TEST_BAUD_230400 230400ul

static uint8_t hard_reset_count;
static uint8_t hard_reset_replies;
static uint8_t soft_recovery_replies;
static uint8_t soft_escape_seen;
static uint16_t uart_string_calls;
static uint32_t uart_baud = TEST_BAUD_115200;
static uint32_t esp_baud = TEST_BAUD_115200;
static uint32_t at_bauds[4];
static uint8_t at_baud_count;
static uint8_t baud_115200_count;
static uint8_t baud_230400_count;
static uint8_t bridgezx_baud_emulation;
static uint8_t pending_at;
static uint8_t pending_uart_115200;
static uint8_t uart_115200_commands;
static void feed_uart(const char *text);
#endif

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

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    fat_date = date;
    fat_time = time;
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return fat_date;
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return fat_time;
}

void spectrum_net_runtime_publish_ip_status(const char *ip)
{
    (void)ip;
}

void spectrum_uart_init(void)
{
#ifdef NETCHESSZX_NEXT
    uart_baud = TEST_BAUD_115200;
    ++baud_115200_count;
#endif
}

#ifdef NETCHESSZX_NEXT
void spectrum_uart_set_baud_230400(void)
{
    uart_baud = TEST_BAUD_230400;
    ++baud_230400_count;
}

void spectrum_uart_hard_reset(void)
{
    ++hard_reset_count;
    esp_baud = TEST_BAUD_115200;
}

#endif

void spectrum_uart_flush(uint16_t frames)
{
    (void)frames;
}

uint8_t spectrum_uart_send_string(const char *s)
{
#ifdef NETCHESSZX_NEXT
    ++uart_string_calls;
    pending_at = (uint8_t)(strcmp(s, "AT") == 0);
    pending_uart_115200 =
        (uint8_t)(strcmp(s, "AT+UART_CUR=115200,8,1,0,0") == 0);
    if (pending_at && at_baud_count < 4u) {
        at_bauds[at_baud_count++] = uart_baud;
    }
    if (strcmp(s, "+++") == 0) {
        soft_escape_seen = 1u;
    }
#else
    (void)s;
#endif
    return uart_string_ok;
}

uint8_t spectrum_uart_send_bytes(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return 1u;
}

uint8_t spectrum_uart_send_crlf(void)
{
#ifdef NETCHESSZX_NEXT
    if (bridgezx_baud_emulation && pending_at && uart_baud == esp_baud) {
        feed_uart("OK\n");
    } else if (bridgezx_baud_emulation && pending_uart_115200 &&
               uart_baud == esp_baud && esp_baud == TEST_BAUD_230400) {
        esp_baud = TEST_BAUD_115200;
        ++uart_115200_commands;
    } else if (hard_reset_count && hard_reset_replies) {
        hard_reset_replies = 0u;
        feed_uart("OK\n");
    } else if (soft_escape_seen && soft_recovery_replies) {
        feed_uart("OK\n");
    }
    pending_at = 0u;
    pending_uart_115200 = 0u;
#endif
    return uart_crlf_ok;
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
    fat_date = 0u;
    fat_time = 0u;
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
    feed_uart("OK\n");
    check(read_line(0u), "zero-frame read consumes only available UART data");
    feed_uart("");
    check(!read_line(0u), "zero-frame read returns immediately when UART is idle");

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

    netchesszx_esp_at_test_capture_ip("+CIFSR:STAIP,\"255.255.255.255\"");
    check(strcmp(spectrum_net_last_ip(), "255.255.255.255") == 0,
          "maximum IPv4 remains NUL terminated and does not overwrite state");

    netchesszx_esp_at_test_capture_ip("+CIFSR:STAIP,\"1234567890123456\"");
    check(strcmp(spectrum_net_last_ip(), "123456789012345") == 0,
          "truncates malformed STAIP to the IPv4 width");

    netchesszx_esp_at_test_capture_ip("+CIFSR:APIP,\"10.0.0.1\"");
    check(strcmp(spectrum_net_last_ip(), "123456789012345") == 0,
          "ignores non-STAIP lines");

    netchesszx_esp_at_test_capture_ip("+CIFSR:STAIP,\"0.0.0.0\"");
    check(spectrum_net_last_ip()[0] == '\0', "zero IP clears last_ip");
}

static void test_capture_msdos_time(void)
{
    uint16_t date;
    uint16_t time;

    reset_clock();
    date = (uint16_t)(((uint16_t)46u << 9) | ((uint16_t)7u << 5) | 7u);
    time = (uint16_t)(((uint16_t)14u << 11) | ((uint16_t)5u << 5) | 29u);
    check(netchesszx_esp_at_test_capture_msdos_time(date, time),
          "captures RTC MS-DOS time");
    check(clock_hour == 14u && clock_minute == 5u && clock_second == 58u,
          "RTC MS-DOS time fields");
    check(fat_date == date && fat_time == time, "RTC FAT stamp");

    reset_clock();
    check(netchesszx_esp_at_test_validate_msdos_time(date, time, 0u),
          "detects RTC MS-DOS time without applying it");
    check(!clock_ready && fat_date == 0u && fat_time == 0u,
          "RTC capability probe preserves runtime clock and FAT stamp");

    reset_clock();
    date = (uint16_t)(((uint16_t)43u << 9) | ((uint16_t)7u << 5) | 7u);
    check(!netchesszx_esp_at_test_capture_msdos_time(date, time),
          "rejects old RTC date");
    check(!clock_ready, "old RTC date does not set clock");

    reset_clock();
    date = (uint16_t)(((uint16_t)46u << 9) | ((uint16_t)7u << 5) | 7u);
    time = (uint16_t)(((uint16_t)14u << 11) | ((uint16_t)5u << 5) | 30u);
    check(!netchesszx_esp_at_test_capture_msdos_time(date, time),
          "rejects invalid RTC seconds");
}

static void test_capture_rtc_year_bounds(void)
{
    const uint16_t time = (uint16_t)(((uint16_t)14u << 11) |
                                     ((uint16_t)5u << 5) | 29u);
    uint16_t date;

    reset_clock();
    date = (uint16_t)(((uint16_t)44u << 9) | ((uint16_t)1u << 5) | 1u);
    check(netchesszx_esp_at_test_capture_msdos_time(date, time),
          "accepts 2024-01-01 RTC date");

    reset_clock();
    date = (uint16_t)(((uint16_t)55u << 9) | ((uint16_t)12u << 5) | 31u);
    check(netchesszx_esp_at_test_capture_msdos_time(date, time),
          "accepts 2035-12-31 RTC date");

    reset_clock();
    date = (uint16_t)(((uint16_t)56u << 9) | ((uint16_t)1u << 5) | 1u);
    check(!netchesszx_esp_at_test_capture_msdos_time(date, time),
          "rejects 2036-01-01 RTC date");
    check(!clock_ready, "rejected 2036-01-01 does not set clock");
}

static void test_at_tx_failure(void)
{
    feed_uart("OK\n");
    uart_string_ok = 0u;
    check(!spectrum_net_at_cmd("AT", 1u), "AT string TX failure propagates");
    check(uart_feed_pos == 0u, "AT string failure does not wait for response");

    uart_string_ok = 1u;
    uart_crlf_ok = 0u;
    check(!spectrum_net_at_cmd("AT", 1u), "AT CRLF TX failure propagates");
    check(uart_feed_pos == 0u, "AT CRLF failure does not wait for response");

    uart_crlf_ok = 1u;
    check(spectrum_net_at_cmd("AT", 1u), "AT succeeds after complete TX");
}

#ifdef NETCHESSZX_NEXT
static void test_next_bridgezx_baud_recovery(void)
{
    feed_uart("");
    uart_baud = TEST_BAUD_115200;
    esp_baud = TEST_BAUD_230400;
    at_baud_count = 0u;
    baud_115200_count = 0u;
    baud_230400_count = 0u;
    uart_115200_commands = 0u;
    hard_reset_count = 0u;
    soft_escape_seen = 0u;
    bridgezx_baud_emulation = 1u;
    netchesszx_esp_at_test_recovery_begin();

    check(spectrum_next_preflight_command_mode(),
          "Next restores the baud inherited from BridgeZX");
    check(at_baud_count == 3u &&
              at_bauds[0] == TEST_BAUD_115200 &&
              at_bauds[1] == TEST_BAUD_230400 &&
              at_bauds[2] == TEST_BAUD_115200,
          "Next tries normal baud before inherited 230400 and restored 115200");
    check(baud_230400_count == 1u && baud_115200_count == 1u,
          "BridgeZX recovery switches each local baud exactly once");
    check(uart_115200_commands == 1u,
          "BridgeZX recovery normalizes ESP UART_CUR to 115200");
    check(uart_baud == TEST_BAUD_115200 && esp_baud == TEST_BAUD_115200,
          "both UART endpoints finish at 115200");
    check(hard_reset_count == 0u,
          "BridgeZX baud recovery avoids the physical ESP reset");
    check(!soft_escape_seen,
          "BridgeZX baud recovery avoids the transparent-mode escape delay");

    feed_uart("");
    uart_baud = TEST_BAUD_115200;
    esp_baud = 460800ul;
    at_baud_count = 0u;
    baud_115200_count = 0u;
    baud_230400_count = 0u;
    uart_115200_commands = 0u;
    hard_reset_count = 0u;
    netchesszx_esp_at_test_recovery_begin();
    check(spectrum_next_preflight_command_mode(),
          "unknown inherited baud reaches the bounded hard-reset fallback");
    check(uart_baud == TEST_BAUD_115200 &&
              baud_230400_count == 1u && baud_115200_count >= 1u,
          "failed BridgeZX probe restores the local 115200 invariant");
    check(uart_115200_commands == 0u,
          "failed 230400 probe does not send a UART reconfiguration");
    check(hard_reset_count == 1u,
          "unknown baud spends at most the existing single reset budget");
    bridgezx_baud_emulation = 0u;
}

static void test_next_hard_reset_fallback(void)
{
    uint16_t calls_after_first;

    feed_uart("");
    hard_reset_count = 0u;
    hard_reset_replies = 1u;
    netchesszx_esp_at_test_recovery_begin();
    check(spectrum_net_ensure_command_mode(),
          "Next hard reset retries command mode once");
    check(hard_reset_count == 1u, "Next recovery performs one hard reset");

    feed_uart("");
    hard_reset_count = 0u;
    hard_reset_replies = 0u;
    netchesszx_esp_at_test_recovery_begin();
    check(!spectrum_net_ensure_command_mode(),
          "Next recovery propagates failure after hard reset");
    check(hard_reset_count == 1u, "Next recovery never loops hard resets");

    feed_uart("");
    hard_reset_count = 0u;
    hard_reset_replies = 0u;
    uart_string_calls = 0u;
    netchesszx_esp_at_test_recovery_begin();
    check(!spectrum_net_ensure_command_mode(),
          "scoped recovery reports first command-mode failure");
    calls_after_first = uart_string_calls;
    soft_escape_seen = 0u;
    soft_recovery_replies = 1u;
    check(spectrum_net_ensure_command_mode(),
          "spent reset budget still permits soft command-mode recovery");
    soft_recovery_replies = 0u;
    check(hard_reset_count == 1u,
          "scoped recovery spends at most one hard reset across calls");
    check(uart_string_calls > (uint16_t)(calls_after_first + 1u),
          "spent recovery still performs the soft escape sequence");
}
#endif

int main(void)
{
    test_read_line();
    test_capture_ip();
#ifndef NETCHESSZX_NEXT
    test_capture_msdos_time();
#endif
    test_capture_rtc_year_bounds();
    test_at_tx_failure();
#ifdef NETCHESSZX_NEXT
    test_next_bridgezx_baud_recovery();
    test_next_hard_reset_fallback();
#endif
    puts("esp_at tests ok");
    return 0;
}
