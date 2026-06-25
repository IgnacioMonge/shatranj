#include "spectrum/session/outgoing.h"

#include "spectrum/config/session.h"
#include "spectrum/platform/text.h"
#include "spectrum/transport/link.h"

static const char ack_prefix[] = "ACK ";
static const char nack_prefix[] = "NACK ";

static uint8_t send_prefixed_text(const char *prefix, const char *text)
{
    char reply[64];
    char *p;

    p = spectrum_append_text(reply, prefix);
    while (*text != '\0' && p < reply + sizeof(reply) - 1u) {
        *p++ = *text++;
    }
    *p = '\0';
    return spectrum_link_send_text(reply);
}

uint8_t netchesszx_session_send_ack_move(const char *ply)
{
    return send_prefixed_text(ack_prefix, ply);
}

uint8_t netchesszx_session_send_nack_move(const char *ply)
{
    return send_prefixed_text(nack_prefix, ply);
}

uint8_t netchesszx_session_send_ping(void)
{
    return spectrum_link_send_ping();
}

uint8_t netchesszx_session_send_ack_ping(const char *rx)
{
    (void)rx;
    return spectrum_link_send_text("ACK PING");
}

uint8_t netchesszx_session_send_ack_reset(void)
{
    return spectrum_link_send_text("ACK RESET");
}

uint8_t netchesszx_session_send_nack_reset(void)
{
    return spectrum_link_send_text("NACK RESET");
}

uint8_t netchesszx_session_send_ack_game_start(void)
{
    return spectrum_link_send_text("ACK GAME START");
}

uint8_t netchesszx_session_send_start_game(void)
{
    return spectrum_link_send_text(netchesszx_session_start_text());
}
