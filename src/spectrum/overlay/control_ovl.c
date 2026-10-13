#include "spectrum/session/event.h"

#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/savegame/savegame_format.h"

#ifdef NETCHESSZX_FIXED_LOW_RAM
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/overlay/overlay_api.h"
#include "spectrum/lowram_map.h"
#endif

#define SESSION_CONTROL_ACK 0u
#define SESSION_CONTROL_NACK 1u
#define SESSION_CONTROL_PAYLOAD 2u
typedef struct session_control_token {
    const char *token;
    uint8_t ack_event;
    uint8_t nack_event;
    uint8_t payload_event;
} session_control_token_t;

static const session_control_token_t session_control_tokens[] = {
    { NETCHESS_PROTO_GAME_START,
      NETCHESSZX_SESSION_EVENT_ACK_GAME_START,
      NETCHESSZX_SESSION_EVENT_NACK_GAME_START,
      NETCHESSZX_SESSION_EVENT_GAME_START },
    { NETCHESS_PROTO_RESET,
      NETCHESSZX_SESSION_EVENT_ACK_RESET,
      NETCHESSZX_SESSION_EVENT_NACK_RESET,
      NETCHESSZX_SESSION_EVENT_RESET },
    { NETCHESS_PROTO_DRAW,
      NETCHESSZX_SESSION_EVENT_ACK_DRAW,
      NETCHESSZX_SESSION_EVENT_NACK_DRAW,
      NETCHESSZX_SESSION_EVENT_DRAW },
    { NETCHESS_PROTO_RESIGN,
      NETCHESSZX_SESSION_EVENT_ACK_RESIGN,
      NETCHESSZX_SESSION_EVENT_UNKNOWN,
      NETCHESSZX_SESSION_EVENT_RESIGN }
};

/* Payload tokens with distinct first words, so their order is free. The
   first SESSION_ANY_TAIL_TOKENS take any tail; the rest only whole payloads. */
typedef struct session_payload_token {
    const char *token;
    uint8_t event;
} session_payload_token_t;

#define SESSION_ANY_TAIL_TOKENS 2u
static const session_payload_token_t session_payload_tokens[] = {
    { NETCHESS_PROTO_MOVE_PREFIX, NETCHESSZX_SESSION_EVENT_MOVE },
    { NETCHESS_PROTO_CHAT_PREFIX, NETCHESSZX_SESSION_EVENT_CHAT },
    { NETCHESS_PROTO_CANCEL_RESET, NETCHESSZX_SESSION_EVENT_CANCEL_RESET },
    { NETCHESS_PROTO_CANCEL_DRAW, NETCHESSZX_SESSION_EVENT_CANCEL_DRAW },
    { NETCHESS_PROTO_BYE, NETCHESSZX_SESSION_EVENT_BYE },
    { "BUSY", NETCHESSZX_SESSION_EVENT_HOST_BUSY }
};

static netchesszx_session_event_t session_classify_control(
    const char *text,
    uint8_t mode)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(session_control_tokens) /
                                sizeof(session_control_tokens[0])); ++i) {
        const session_control_token_t *entry = &session_control_tokens[i];
        const char *tail = netchess_after_prefix(text, entry->token);

        if (tail == 0) {
            continue;
        }
        if (*tail == '\0') {
            if (mode == SESSION_CONTROL_ACK) {
                return (netchesszx_session_event_t)entry->ack_event;
            }
            if (mode == SESSION_CONTROL_NACK) {
                return (netchesszx_session_event_t)entry->nack_event;
            }
            return (netchesszx_session_event_t)entry->payload_event;
        }
        if (*tail == ' ') {
            if (mode == SESSION_CONTROL_NACK &&
                entry->nack_event != NETCHESSZX_SESSION_EVENT_UNKNOWN) {
                return (netchesszx_session_event_t)entry->nack_event;
            }
            if (mode == SESSION_CONTROL_PAYLOAD &&
                entry->payload_event == NETCHESSZX_SESSION_EVENT_GAME_START) {
                return (netchesszx_session_event_t)entry->payload_event;
            }
        }
    }
    if (mode != SESSION_CONTROL_PAYLOAD) {
        /* Match the five-digit ply token accepted by the reply parsers. */
        for (i = 0u; i < 5u && *text >= '0' && *text <= '9'; ++i) {
            ++text;
        }
        if (i != 0u && (*text == '\0' || *text == ' ')) {
            return mode == SESSION_CONTROL_ACK
                ? NETCHESSZX_SESSION_EVENT_ACK_MOVE
                : NETCHESSZX_SESSION_EVENT_NACK_MOVE;
        }
    }
    return NETCHESSZX_SESSION_EVENT_UNKNOWN;
}

netchesszx_session_event_t netchesszx_session_classify_game_payload(
    const char *payload)
{
    const char *tail;
    uint16_t takeback_ply;
    uint8_t platform;
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(session_payload_tokens) /
                                sizeof(session_payload_tokens[0])); ++i) {
        tail = netchess_after_prefix(payload, session_payload_tokens[i].token);
        if (tail != 0 && (i < SESSION_ANY_TAIL_TOKENS || *tail == '\0')) {
            return (netchesszx_session_event_t)session_payload_tokens[i].event;
        }
    }
    takeback_ply = netchess_proto_takeback_ply(payload);
    if (takeback_ply != 0u) {
        return payload[0] == 'C' ? NETCHESSZX_SESSION_EVENT_CANCEL_TAKEBACK
                                 : NETCHESSZX_SESSION_EVENT_TAKEBACK;
    }
    if (netchess_proto_parse_mach(payload, &platform)) {
        return (netchesszx_session_event_t)
            (NETCHESSZX_SESSION_EVENT_MACH_BASE | platform);
    }
    if (netchess_proto_name_tail(payload) != 0) {
        return NETCHESSZX_SESSION_EVENT_PEER_NAME;
    }
    tail = netchess_after_prefix(payload, NETCHESS_PROTO_ACK_PREFIX);
    if (tail != 0) {
        return session_classify_control(tail, SESSION_CONTROL_ACK);
    }
    tail = netchess_after_prefix(payload, NETCHESS_PROTO_NACK_PREFIX);
    if (tail != 0) {
        return session_classify_control(tail, SESSION_CONTROL_NACK);
    }
    {
        netchesszx_session_event_t event =
            session_classify_control(payload, SESSION_CONTROL_PAYLOAD);

        if (event != NETCHESSZX_SESSION_EVENT_UNKNOWN) {
            return event;
        }
    }
    if (payload[0] == 'R') {
        if (payload[2] == '\0') {
            switch (payload[1]) {
            case 'Q': return NETCHESSZX_SESSION_EVENT_RESTORE_RQ;
            case 'Y': return NETCHESSZX_SESSION_EVENT_RESTORE_RY;
            case 'N': return NETCHESSZX_SESSION_EVENT_RESTORE_RN;
            case 'A': return NETCHESSZX_SESSION_EVENT_RESTORE_RA;
            default: break;
            }
        } else if (payload[1] == 'S' && payload[2] == '0' &&
            (payload[3] == '0' || payload[3] == '1') &&
            payload[4] == ' ' &&
            payload[NETCHESSZX_SAVE_RESTORE_FRAME_MAX] == '\0') {
            return NETCHESSZX_SESSION_EVENT_RESTORE_RS;
        }
    }
    return NETCHESSZX_SESSION_EVENT_UNKNOWN;
}

#ifdef NETCHESSZX_FIXED_LOW_RAM
uint8_t control_classify_ovl(volatile uint8_t *ctx) __z88dk_fastcall
{
    uint16_t payload_addr =
        (uint16_t)ctx[SPECTRUM_OVL_CTX_PTR_LO] |
        ((uint16_t)ctx[SPECTRUM_OVL_CTX_PTR_HI] << 8);

    return (uint8_t)netchesszx_session_classify_game_payload(
        (const char *)payload_addr);
}

#ifdef NETCHESSZX_SPECTRANEXT
uint8_t control_format_busy_ovl(volatile uint8_t *ctx) __z88dk_fastcall
{
    const char *ply = (const char *)((uint16_t)ctx[0] |
                                     ((uint16_t)ctx[1] << 8));
    char *payload = (char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR;
    char *out = spectrum_append_text(payload, "NACK ");

    if (ply == 0) {
        (void)spectrum_append_text(out, "GAME START BUSY");
    } else {
        out = spectrum_append_text(out, ply);
        (void)spectrum_append_text(out, " BUSY");
    }
    return 1u;
}
#endif
#endif
