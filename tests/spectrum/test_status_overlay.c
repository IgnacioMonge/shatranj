#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define __z88dk_fastcall

static uint8_t netchesszx_host_ula[6144];
#define MOVES_HEADER_SCREEN netchesszx_host_ula
/* Resident on the firmware: the text key each header label last showed. */
static uint8_t spectrum_moves_header_shown[2];

#include "spectrum/overlay/status_ovl.c"

uint8_t netchesszx_session_role;
uint8_t netchesszx_transport;
const char netchesszx_mqtt_host[] = "broker.example";
char netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX + 1u];
char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u];
uint16_t netchesszx_direct_port;

uint8_t netchesszx_local_color;
char netchesszx_player_name_line[NETCHESS_PROTO_NAME_LINE_SIZE] =
    NETCHESS_PROTO_NAME_PREFIX;
char netchesszx_peer_name[NETCHESS_PLAYER_NAME_SIZE];
static uint8_t test_panels_visible = 1u;
static char test_labels[2][24];
static uint8_t test_label_count;

uint8_t spectrum_gui_side_panels_visible(void)
{
    return test_panels_visible;
}

void spectrum_render_ikkle_at(const char *spec)
{
    /* Row, half-column and attribute, then the NUL-terminated label. */
    if (test_label_count < 2u) {
        snprintf(test_labels[test_label_count++], sizeof(test_labels[0]),
                 "%u,%u,%02x,%s", (unsigned)(uint8_t)spec[0],
                 (unsigned)(uint8_t)spec[1], (unsigned)(uint8_t)spec[2],
                 spec + 3);
    }
}

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

static uint8_t header_byte(uint8_t scan, uint8_t col)
{
    return netchesszx_host_ula[(NETCHESSZX_INFO_HEADER_ROW & 7u) * 32u +
                               scan * 256u + col];
}

static void expect_header(const char *white, const char *black,
                          const char *label)
{
    char expected[2][24];

    /* The resident panel restore blanks the row once and forgets what the
       labels showed; repaints are in place, so the labels always cover the
       full ten-character width. */
    memset(netchesszx_host_ula, 0, sizeof(netchesszx_host_ula));
    memset(spectrum_moves_header_shown, 0, sizeof(spectrum_moves_header_shown));
    test_label_count = 0u;
    (void)status_moves_header_ovl(0);
    snprintf(expected[0], sizeof(expected[0]), "%u,%u,%02x,%-10s",
             (unsigned)NETCHESSZX_INFO_HEADER_ROW,
             (unsigned)NETCHESSZX_MOVES_HEADER_WHITE_TEXT_COL,
             (unsigned)NETCHESSZX_ATTR_STATUS, white);
    snprintf(expected[1], sizeof(expected[1]), "%u,%u,%02x,%-10s",
             (unsigned)NETCHESSZX_INFO_HEADER_ROW,
             (unsigned)NETCHESSZX_MOVES_HEADER_BLACK_TEXT_COL,
             (unsigned)NETCHESSZX_ATTR_STATUS, black);
    if (test_label_count != 2u || strcmp(test_labels[0], expected[0]) != 0 ||
        strcmp(test_labels[1], expected[1]) != 0) {
        fprintf(stderr, "FAIL %s: labels '%s' '%s'\n", label,
                test_labels[0], test_labels[1]);
        ++failures;
    }
}

static void test_moves_header(void)
{
    uint8_t scan;
    uint8_t ok = 1u;

    /* Geometry: white disc ink x147..152, divider x200, black disc ink
       x203..208, labels at ikkle columns 39 and 53 (x156 and x212). */
    netchesszx_local_color = NETCHESSZX_COLOR_WHITE;
    expect_header("WHITE", "BLACK", "header colour labels");
    for (scan = 0u; scan < 8u; ++scan) {
        ok &= (uint8_t)((header_byte(scan, 25u) & 0x80u) != 0u);
    }
    /* Disc outline row 2 (0x42): x147 and x152; filled row 2 (0x7e). */
    ok &= (uint8_t)(header_byte(2u, 18u) == 0x10u);
    ok &= (uint8_t)(header_byte(2u, 19u) == 0x80u);
    ok &= (uint8_t)(header_byte(2u, 25u) == 0x9fu);
    ok &= (uint8_t)(header_byte(2u, 26u) == 0x80u);
    /* 6x6 discs on scanlines 1..6: top/bottom rows (0x3c) x148..151,
       one clear scanline above and below inside the 8-pixel cell. */
    ok &= (uint8_t)(header_byte(1u, 18u) == 0x0fu);
    ok &= (uint8_t)(header_byte(5u, 18u) == 0x10u);
    ok &= (uint8_t)(header_byte(5u, 25u) == 0x9fu);
    ok &= (uint8_t)(header_byte(6u, 18u) == 0x0fu);
    ok &= (uint8_t)(header_byte(6u, 25u) == 0x8fu);
    ok &= (uint8_t)(header_byte(0u, 18u) == 0x00u);
    ok &= (uint8_t)(header_byte(7u, 18u) == 0x00u);
    ok &= (uint8_t)(header_byte(7u, 25u) == 0x80u);
    ok &= (uint8_t)(header_byte(7u, 30u) == 0x00u);
    /* A repaint over the drawn header changes no pixel (no blank-redraw
       flicker when the peer name arrives). */
    {
        static uint8_t before[sizeof(netchesszx_host_ula)];

        memcpy(before, netchesszx_host_ula, sizeof(before));
        test_label_count = 0u;
        (void)status_moves_header_ovl(0);
        ok &= (uint8_t)(memcmp(before, netchesszx_host_ula,
                               sizeof(before)) == 0);
        /* Unchanged labels are not even redrawn. */
        ok &= (uint8_t)(test_label_count == 0u);
        /* The peer name arrives: only that label is repainted. */
        strcpy(netchesszx_peer_name, "PEPE");
        (void)status_moves_header_ovl(0);
        ok &= (uint8_t)(test_label_count == 1u &&
                        strstr(test_labels[0], "PEPE") != NULL);
        netchesszx_peer_name[0] = '\0';
    }
    if (!ok || NETCHESSZX_MOVES_HEADER_WHITE_TEXT_COL != 39u ||
        NETCHESSZX_MOVES_HEADER_BLACK_TEXT_COL != 53u) {
        fprintf(stderr, "FAIL moves header geometry\n");
        ++failures;
    }

    strcpy(netchesszx_player_name_line + NETCHESS_PROTO_NAME_PREFIX_LEN,
           "IGNACIO");
    expect_header("IGNACIO", "BLACK", "local white name before peer");
    strcpy(netchesszx_peer_name, "CONSTANTIN");
    expect_header("IGNACIO", "CONSTANTIN", "both names, local white");
    netchesszx_local_color = NETCHESSZX_COLOR_BLACK;
    expect_header("CONSTANTIN", "IGNACIO", "both names, local black");
    netchesszx_player_name_line[NETCHESS_PROTO_NAME_PREFIX_LEN] = '\0';
    expect_header("CONSTANTIN", "BLACK", "peer name only");
    netchesszx_peer_name[0] = '\0';
    expect_header("WHITE", "BLACK", "names cleared");

    test_panels_visible = 0u;
    test_label_count = 0u;
    netchesszx_host_ula[0] = 0x5au;
    (void)status_moves_header_ovl(0);
    if (test_label_count != 0u || netchesszx_host_ula[0] != 0x5au) {
        fprintf(stderr, "FAIL hidden panels must not be painted\n");
        ++failures;
    }
    test_panels_visible = 1u;
}

int main(void)
{
    test_moves_header();
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

    strcpy(netchesszx_peer_name, "STOCKFISH");
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_PC,
                  "DIRECT 255.255.255.255:65535 (GUEST) VS PC: STOCKFISH",
                  "stockfish peer in game");
    strcpy(netchesszx_peer_name, "STOCKFISH2");
    expect_status(STATUS_PHASE_GAME,
                  NETCHESS_PLAT_PC,
                  "DIRECT 255.255.255.255:65535 (GUEST) VS PC",
                  "only the exact reserved name marks the engine");
    netchesszx_peer_name[0] = '\0';

    if (failures != 0u) {
        return 1;
    }
    puts("status overlay tests passed");
    return 0;
}
