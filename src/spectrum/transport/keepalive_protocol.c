#include "spectrum/transport/keepalive_protocol.h"
#include "common/protocol/game_protocol.h"

/* The Spectrum session classifier calls netchess_text_is() directly. */
#ifndef NETCHESSZX_SDCC_IY
uint8_t spectrum_keepalive_is_ping(const char *payload) NETCHESSZX_FASTCALL
{
    return netchess_text_is(payload, NETCHESS_PROTO_PING);
}

uint8_t spectrum_keepalive_is_ack_ping(const char *payload) NETCHESSZX_FASTCALL
{
    return netchess_text_is(payload, NETCHESS_PROTO_ACK_PING);
}
#endif
