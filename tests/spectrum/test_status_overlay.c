#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define __z88dk_fastcall

#include "spectrum/overlay/status_ovl.c"

uint8_t netchesszx_session_role;
uint8_t netchesszx_transport;
const char netchesszx_mqtt_host[] = "broker.example";
char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u];
char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u];
uint16_t netchesszx_direct_port;

static uint8_t test_peer_ready;
static const char *test_local_ip;
static unsigned failures;

uint8_t netchesszx_session_peer_ready(void)
{
    return test_peer_ready;
}

const char *spectrum_net_last_ip(void)
{
    return test_local_ip;
}

char *spectrum_append_u16(char *dst, uint16_t value)
{
    char reversed[5];
    uint8_t count = 0u;

    do {
        reversed[count++] = (char)('0' + (value % 10u));
        value = (uint16_t)(value / 10u);
    } while (value != 0u);
    while (count != 0u) {
        *dst++ = reversed[--count];
    }
    *dst = '\0';
    return dst;
}

void spectrum_gui_set_status(const char *text)
{
    (void)text;
}

static void set_text(char *dst, size_t cap, const char *src)
{
    size_t len = strlen(src);

    if (len >= cap) {
        len = cap - 1u;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void expect_status(uint8_t phase,
                          uint8_t platform,
                          const char *expected,
                          const char *label)
{
    status_build_phase(SPECTRUM_STATUS_PACK(phase, platform));
    if (strcmp(status_line_ovl, expected) != 0) {
        fprintf(stderr,
                "FAIL %s: expected '%s', got '%s'\n",
                label,
                expected,
                status_line_ovl);
        ++failures;
    }
    if (strlen(status_line_ovl) > SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE) {
        fprintf(stderr, "FAIL %s: status line exceeds 53 characters\n", label);
        ++failures;
    }
}

int main(void)
{
    test_local_ip = "255.255.255.255";
    netchesszx_direct_port = 65535u;
    set_text(netchesszx_direct_host,
             sizeof(netchesszx_direct_host),
             "255.255.255.255");
    set_text(netchesszx_mqtt_code,
             sizeof(netchesszx_mqtt_code),
             "ABCDEFGHIJKLMNOP");

    expect_status(STATUS_PHASE_CONNECTION_SETUP,
                  NETCHESS_PLAT_UNKNOWN,
                  "CONNECTION SETUP",
                  "connection setup");
    expect_status(STATUS_PHASE_GAME_SETUP,
                  NETCHESS_PLAT_UNKNOWN,
                  "GAME SETUP",
                  "game setup");

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    test_peer_ready = 0u;
    expect_status(STATUS_PHASE_CONNECTING,
                  NETCHESS_PLAT_UNKNOWN,
                  "DIRECT 255.255.255.255:65535 (HOST) - LISTENING",
                  "direct host listening");
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_UNKNOWN,
                  "DIRECT 255.255.255.255:65535 (HOST) - WAITING",
                  "direct host waiting");

    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    expect_status(STATUS_PHASE_CONNECTING,
                  NETCHESS_PLAT_UNKNOWN,
                  "DIRECT 255.255.255.255:65535 (GUEST) - CONNECTING",
                  "direct guest connecting");
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_UNKNOWN,
                  "DIRECT 255.255.255.255:65535 (GUEST) - WAITING",
                  "direct guest waiting");

    netchesszx_transport = NETCHESSZX_TRANSPORT_MQTT;
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_HOST;
    expect_status(STATUS_PHASE_CONNECTING,
                  NETCHESS_PLAT_UNKNOWN,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (HOST) - CONNECTING",
                  "mqtt host connecting");
    netchesszx_session_role = NETCHESSZX_SESSION_ROLE_JOIN;
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_UNKNOWN,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (GUEST) - WAITING",
                  "mqtt guest waiting");

    test_peer_ready = 1u;
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_UNKNOWN,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (GUEST) VS ?",
                  "legacy peer");
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_ZX,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (GUEST) VS ZX",
                  "zx peer");
    expect_status(STATUS_PHASE_CONNECTED,
                  NETCHESS_PLAT_NXT,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (GUEST) VS NXT",
                  "next peer");
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_MAC,
                  "MQTT ROOM ABCDEFGHIJKLMNOP (GUEST) VS MAC",
                  "mac peer in game");

    netchesszx_transport = NETCHESSZX_TRANSPORT_DIRECT;
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_LNX,
                  "DIRECT 255.255.255.255:65535 (GUEST) VS LNX",
                  "linux peer in game");
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_PC,
                  "DIRECT 255.255.255.255:65535 (GUEST) VS PC",
                  "pc peer in game");
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_SPCX,
                  "DIRECT 255.255.255.255:65535 (GUEST) VS SPCX",
                  "spectranext peer in game");

    if (failures != 0u) {
        return 1;
    }
    puts("status overlay tests passed");
    return 0;
}
