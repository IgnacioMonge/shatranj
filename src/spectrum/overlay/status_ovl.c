#include "spectrum/overlay/overlay_api.h"
#include "spectrum/config/session.h"
#include "spectrum/session/event.h"
#include "spectrum/transport/esp_at.h"

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
    p = status_append(p, "DIRECT ", end);
    host = netchesszx_session_is_host() ? spectrum_esp_at_last_ip() :
                                         netchesszx_direct_host;
    p = status_append(p, host[0] != '\0' ? host : "-", end);
    return status_append_direct_port(p, end);
}

static char *status_append_role(char *p, char *end)
{
    return status_append(p, netchesszx_session_is_host() ? "(HOST)" :
                                                        "(GUEST)",
                         end);
}

static const char *status_platform_code(uint8_t platform)
{
    switch (platform) {
    case NETCHESS_PLAT_ZX: return "ZX";
    case NETCHESS_PLAT_NXT: return "NXT";
    case NETCHESS_PLAT_MAC: return "MAC";
    case NETCHESS_PLAT_LNX: return "LNX";
    case NETCHESS_PLAT_PC: return "PC";
    case NETCHESS_PLAT_SPCX: return "SPCX";
    default: return "?";
    }
}

static void status_build_phase(uint8_t status)
{
    char *p = status_line_ovl;
    char *end = status_line_ovl + SPECTRUM_OVL_STATUS_LINE_TEXT_SIZE;
    uint8_t phase = SPECTRUM_STATUS_UNPACK_PHASE(status);
    uint8_t platform = SPECTRUM_STATUS_UNPACK_PLATFORM(status);

    status_line_ovl[0] = '\0';
    if (phase == STATUS_PHASE_CONNECTION_SETUP) {
        (void)status_append(p, "CONNECTION SETUP", end);
    } else if (phase == STATUS_PHASE_GAME_SETUP) {
        (void)status_append(p, "GAME SETUP", end);
    } else if (phase >= STATUS_PHASE_CONNECTING && phase <= STATUS_PHASE_GAME) {
        p = status_append_endpoint(p, end);
        p = status_append_char(p, ' ', end);
        p = status_append_role(p, end);
        if (phase == STATUS_PHASE_CONNECTING) {
            if (!netchesszx_transport_is_mqtt() &&
                netchesszx_session_is_host()) {
                (void)status_append(p, " - LISTENING", end);
            } else {
                (void)status_append(p, " - CONNECTING", end);
            }
        } else if (phase == STATUS_PHASE_CONNECTED &&
                   !netchesszx_session_peer_ready()) {
            (void)status_append(p, " - WAITING", end);
        } else {
            p = status_append(p, " VS ", end);
            (void)status_append(p, status_platform_code(platform), end);
        }
    }
}

uint8_t status_phase_ovl(uint8_t *ctx) __z88dk_fastcall
{
    status_build_phase(ctx[SPECTRUM_OVL_CTX_STATUS_PHASE]);
    spectrum_gui_set_status(status_line_ovl);
    return 1u;
}
