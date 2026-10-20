#include "spectrum/overlay/overlay_api.h"
#include "spectrum/config/session.h"
#include "spectrum/platform/text.h"
#include "spectrum/session/event.h"
#include "spectrum/transport/link.h"
#include "spectrum/ui/layout.h"

#include <string.h>

#define STATUS_PHASE_CONNECTION_SETUP 0u
#define STATUS_PHASE_GAME_SETUP 1u
#define STATUS_PHASE_CONNECTING 2u
#define STATUS_PHASE_CONNECTED 3u
#define STATUS_PHASE_GAME 4u

static char status_line_ovl[SPECTRUM_OVL_STATUS_LINE_SIZE];

static char *status_append(char *p, const char *src, char *end)
{
    while (*src != '\0' && p < end) {
        *p++ = *src++;
    }
    *p = '\0';
    return p;
}

static char *status_append_char(char *p, char c, char *end)
{
    if (p < end) {
        *p++ = c;
        *p = '\0';
    }
    return p;
}

static char *status_append_mqtt_room(char *p, char *end)
{
    p = status_append(p, "MQTT ROOM ", end);
    if (netchesszx_mqtt_code[0] != '\0') {
        return status_append(p, netchesszx_mqtt_code, end);
    }
    return status_append(p, "-", end);
}

static char *status_append_direct_port(char *p, char *end)
{
    p = status_append_char(p, ':', end);
    if ((uint16_t)(end - p) >= 5u) {
        p = spectrum_append_u16(p, netchesszx_direct_port);
    }
    return p;
}

static char *status_append_endpoint(char *p, char *end)
{
    const char *host;

    if (netchesszx_transport_is_mqtt()) {
        return status_append_mqtt_room(p, end);
    }
#ifdef NETCHESSZX_SPECTRANEXT
    if (netchesszx_link_match) {
        return status_append(p, "MATCH", end);
    }
#endif
    p = status_append(p, "DIRECT ", end);
    host = netchesszx_session_is_host() ? spectrum_net_last_ip() :
                                         netchesszx_direct_host;
    p = status_append(p, host[0] != '\0' ? host : "-", end);
    return status_append_direct_port(p, end);
}

static char *status_append_role(char *p, char *end)
{
    return status_append(p, netchesszx_session_is_host() ? " (HOST)" :
                                                        " (GUEST)",
                         end);
}

/* The Qt client reserves this name for its engine autopilot; the suffix
   doubles as the name to compare against (skip the ": "). */
static const char status_bot_suffix[] = ": STOCKFISH";

/* Indexed by NETCHESS_PLAT_* (dense 0..6); the packed status field is 3 bits,
   so 7 is the only out-of-range value. A table is smaller than a switch. */
static const char *const status_platform_codes[8] = {
    "?", "ZX", "NXT", "MAC", "LNX", "PC", "SPCX", "?"
};

static void status_build_phase(uint8_t status)
{
    char *p = status_line_ovl;
    char *end = status_line_ovl + SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE;
    uint8_t phase = SPECTRUM_STATUS_UNPACK_PHASE(status);
    uint8_t platform = SPECTRUM_STATUS_UNPACK_PLATFORM(status);

    const char *tail;

    status_line_ovl[0] = '\0';
    if (phase == STATUS_PHASE_CONNECTION_SETUP) {
        tail = "CONNECTION SETUP";
    } else if (phase == STATUS_PHASE_GAME_SETUP) {
        tail = "GAME SETUP";
    } else if (phase >= STATUS_PHASE_CONNECTING && phase <= STATUS_PHASE_GAME) {
        p = status_append_endpoint(p, end);
        p = status_append_role(p, end);
        if (phase == STATUS_PHASE_CONNECTING) {
            if (!netchesszx_transport_is_mqtt() &&
#ifdef NETCHESSZX_SPECTRANEXT
                !netchesszx_link_match &&
#endif
                netchesszx_session_is_host()) {
                tail = " - LISTENING";
            } else {
                tail = " - CONNECTING";
            }
        } else if (phase == STATUS_PHASE_CONNECTED &&
                   !netchesszx_session_peer_ready()) {
            tail = " - WAITING";
        } else {
            p = status_append(p, " VS ", end);
            p = status_append(p, status_platform_codes[platform], end);
            if (!spectrum_streq(netchesszx_peer_name, status_bot_suffix + 2)) {
                return;
            }
            tail = status_bot_suffix;
        }
    } else {
        return;
    }
    (void)status_append(p, tail, end);
}

uint8_t status_phase_ovl(uint8_t *ctx) __z88dk_fastcall
{
    status_build_phase(ctx[SPECTRUM_OVL_CTX_STATUS_PHASE]);
    spectrum_gui_set_status(status_line_ovl);
    return 1u;
}

/* MOVES column header, pixels only (resident code owns the attributes).
   Each colour disc sits two pixels right of its column's byte boundary and
   straddles two bytes; each label starts one ikkle column after its disc.
   Discs are 6x6 on scanlines 1..6, one pixel clear of the cell top and
   bottom. The repaint is in place: divider and discs are ORed (idempotent)
   and each label is padded to the full name width, so the ikkle renderer
   overwrites every cell once instead of the row being blanked and redrawn.
   The resident panel restore blanks the row when it first appears.
   A known player name replaces the colour word under that player's colour. */
#ifndef MOVES_HEADER_SCREEN
#define MOVES_HEADER_SCREEN ((uint8_t *)0x4000u) /* host tests substitute */
#endif
#define MOVES_HEADER_PIXELS \
    (MOVES_HEADER_SCREEN + \
     (NETCHESSZX_INFO_HEADER_ROW & 0x18u) * 0x100u + \
     (NETCHESSZX_INFO_HEADER_ROW & 0x07u) * 0x20u + \
     NETCHESSZX_INFO_PANEL_COL)
#define MOVES_HEADER_BLACK_BYTE \
    (NETCHESSZX_MOVES_HEADER_BLACK_ICON_BYTE_COL - NETCHESSZX_INFO_PANEL_COL)

/* Per scanline: white disc (two bytes), then black disc with the divider in
   bit 7 of its first byte. Discs are 0x3c,0x42x4,0x3c (outline) and
   0x3c,0x7ex4,0x3c (filled), shifted right by the layout's two pixels. */
#if NETCHESSZX_MOVES_HEADER_DISC_SHIFT != 2u
#error "moves header disc table assumes a two-pixel shift"
#endif
static const uint8_t moves_header_rows[8][4] = {
    { 0x00u, 0x00u, 0x80u, 0x00u },
    { 0x0fu, 0x00u, 0x8fu, 0x00u },
    { 0x10u, 0x80u, 0x9fu, 0x80u },
    { 0x10u, 0x80u, 0x9fu, 0x80u },
    { 0x10u, 0x80u, 0x9fu, 0x80u },
    { 0x10u, 0x80u, 0x9fu, 0x80u },
    { 0x0fu, 0x00u, 0x8fu, 0x00u },
    { 0x00u, 0x00u, 0x80u, 0x00u }
};

/* A resident cache per label (white, black) records the text it last showed,
   zero after the header row is blanked. An unchanged label is not repainted,
   so a peer name arriving only rewrites that one label. */
#ifdef NETCHESSZX_HOST_TEST
/* Host model of the firmware pair in entry_menu_logic.asm; the including
   test owns the two text buffers. */
static uint8_t moves_header_new(const char *text, char *shown)
{
    uint8_t changed = (uint8_t)(strcmp(text, shown) != 0);

    strcpy(shown, text);
    return changed;
}
#define moves_header_white_new(text) \
    moves_header_new((text), spectrum_moves_header_shown[0])
#define moves_header_black_new(text) \
    moves_header_new((text), spectrum_moves_header_shown[1])
#else
extern uint8_t moves_header_white_new(const char *text) __z88dk_fastcall;
extern uint8_t moves_header_black_new(const char *text) __z88dk_fastcall;
#endif

static void moves_header_label(uint8_t col, const char *text)
{
    char *p;
    char *end = status_line_ovl + 3 + NETCHESS_PLAYER_NAME_MAX;

    status_line_ovl[0] = (char)NETCHESSZX_INFO_HEADER_ROW;
    status_line_ovl[1] = (char)col;
    status_line_ovl[2] = (char)NETCHESSZX_ATTR_STATUS;
    p = status_append(status_line_ovl + 3, text, end);
    while (p < end) {
        *p++ = ' ';
    }
    *p = '\0';
    spectrum_render_ikkle_at(status_line_ovl);
}

uint8_t status_moves_header_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t *p = MOVES_HEADER_PIXELS;
    const char *white = "WHITE";
    const char *black = "BLACK";
    const char *local = netchesszx_player_name;
    const char *peer = netchesszx_peer_name;
    uint8_t scan;

    (void)ctx;
    if (!spectrum_gui_side_panels_visible()) {
        return 1u;
    }
    {
        const uint8_t *row = moves_header_rows[0];

        for (scan = 8u; scan != 0u; --scan) {
            *p++ |= *row++;
            *p |= *row++;
            p += MOVES_HEADER_BLACK_BYTE - 1u;
            *p++ |= *row++;
            *p |= *row++;
            p += 0x100u - MOVES_HEADER_BLACK_BYTE - 1u;
        }
    }
    if (!netchesszx_local_is_white()) {
        local = netchesszx_peer_name;
        peer = netchesszx_player_name;
    }
    if (local[0] != '\0') {
        white = local;
    }
    if (peer[0] != '\0') {
        black = peer;
    }
    if (moves_header_white_new(white)) {
        moves_header_label(NETCHESSZX_MOVES_HEADER_WHITE_TEXT_COL, white);
    }
    if (moves_header_black_new(black)) {
        moves_header_label(NETCHESSZX_MOVES_HEADER_BLACK_TEXT_COL, black);
    }
    return 1u;
}
