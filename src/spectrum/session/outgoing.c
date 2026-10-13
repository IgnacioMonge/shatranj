#include "spectrum/session/outgoing.h"

#include "common/protocol/game_protocol.h"
#include "spectrum/config/session.h"
#include "spectrum/session/route.h"
#include "spectrum/platform/text.h"
#include "spectrum/transport/link.h"

uint8_t netchesszx_session_send_text(const char *text) NETCHESSZX_FASTCALL
{
    spectrum_net_mqtt_set_publish_flags(NETCHESSZX_SESSION_ROUTE_GAME);
    return spectrum_link_send_text(text);
}

uint8_t netchesszx_session_send_control_text(const char *text)
    NETCHESSZX_FASTCALL
{
    spectrum_net_mqtt_set_publish_flags(NETCHESSZX_SESSION_ROUTE_CONTROL);
    return spectrum_link_send_text(text);
}

uint8_t netchesszx_session_send_ack_text(const char *text) NETCHESSZX_FASTCALL
{
    spectrum_net_mqtt_set_publish_flags(NETCHESSZX_SESSION_ROUTE_ACK);
    return spectrum_link_send_text(text);
}

#ifndef NETCHESSZX_SDCC_IY
/* Host fallback for the Z80 implementations in shrink_kernels.asm. */
#define MOVE_PLY_TEXT_MAX 5u
#define MOVE_REPLY_CAP ((sizeof("NACK ") - 1u) + MOVE_PLY_TEXT_MAX + 1u)
typedef char move_reply_capacity_check[
    MOVE_REPLY_CAP == sizeof("NACK 65535") ? 1 : -1];

static uint8_t send_prefixed_text(const char *prefix, const char *text)
{
    char reply[MOVE_REPLY_CAP];

    (void)spectrum_append_text(spectrum_append_text(reply, prefix), text);
    return netchesszx_session_send_ack_text(reply);
}

uint8_t netchesszx_session_send_ack_move(const char *ply)
{
    return send_prefixed_text(NETCHESS_PROTO_ACK_PREFIX, ply);
}

uint8_t netchesszx_session_send_nack_move(const char *ply)
{
    return send_prefixed_text(NETCHESS_PROTO_NACK_PREFIX, ply);
}
#endif

uint8_t netchesszx_session_send_ping(void)
{
    spectrum_net_mqtt_set_publish_flags(NETCHESSZX_SESSION_ROUTE_GAME);
    return spectrum_link_send_text(NETCHESS_PROTO_PING);
}

uint8_t netchesszx_session_send_ack_ping(void)
{
    return netchesszx_session_send_ack_text(NETCHESS_PROTO_ACK_PING);
}

uint8_t netchesszx_session_send_start_game(void)
{
    return netchesszx_session_send_control_text(netchesszx_session_start_text());
}
