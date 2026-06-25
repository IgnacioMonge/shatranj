#include "spectrum/session/outgoing.h"

#include "spectrum/config/session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sent_text;

char *spectrum_append_text(char *dst, const char *src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

char *spectrum_append_u16(char *dst, uint16_t value)
{
    char buf[6];
    char *p = buf + sizeof(buf);

    *--p = '\0';
    do {
        *--p = (char)('0' + (value % 10u));
        value = (uint16_t)(value / 10u);
    } while (value != 0u);
    return spectrum_append_text(dst, p);
}

uint8_t spectrum_net_send_text(const char *text)
{
    sent_text = text;
    return 1u;
}

uint8_t spectrum_net_send_ping(void)
{
    sent_text = "PING";
    return 1u;
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void expect_sent(const char *expected, const char *message)
{
    check(sent_text != 0 && strcmp(sent_text, expected) == 0, message);
}

static void test_ack_nack(void)
{
    sent_text = 0;
    check(netchesszx_session_send_ack_move("12"), "ack move sent");
    expect_sent("ACK 12", "ack move text");
    check(netchesszx_session_send_nack_move("13"), "nack move sent");
    expect_sent("NACK 13", "nack move text");
}

static void test_ping_response(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    check(netchesszx_session_send_ping(), "direct ping sent");
    expect_sent("PING", "direct ping text");
    check(netchesszx_session_send_ack_ping("PING"), "ack ping sent");
    expect_sent("ACK PING", "ack ping text");
}

static void test_reset_start(void)
{
    check(netchesszx_session_send_ack_reset(), "ack reset sent");
    expect_sent("ACK RESET", "ack reset text");
    check(netchesszx_session_send_ack_game_start(), "ack start sent");
    expect_sent("ACK GAME START", "ack start text");

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    check(netchesszx_session_send_start_game(), "mqtt start sent");
    expect_sent("GAME START", "mqtt start text");
}

int main(void)
{
    test_ack_nack();
    test_ping_response();
    test_reset_start();
    puts("session outgoing tests ok");
    return 0;
}
