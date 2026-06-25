#ifndef NETCHESSZX_COMMON_PROTOCOL_MESSAGES_H
#define NETCHESSZX_COMMON_PROTOCOL_MESSAGES_H

#include <stdint.h>

typedef enum netchesszx_protocol_tag {
    NETCHESSZX_PROTOCOL_UNKNOWN = 0,
    NETCHESSZX_PROTOCOL_MOVE,
    NETCHESSZX_PROTOCOL_CHAT,
    NETCHESSZX_PROTOCOL_ACK,
    NETCHESSZX_PROTOCOL_NACK,
    NETCHESSZX_PROTOCOL_PING,
    NETCHESSZX_PROTOCOL_BYE,
    NETCHESSZX_PROTOCOL_RESET,
    NETCHESSZX_PROTOCOL_GAME_START
} netchesszx_protocol_tag_t;

netchesszx_protocol_tag_t netchesszx_protocol_classify(const char *rx);

uint8_t netchesszx_protocol_parse_move(const char *rx,
                                       char *ply,
                                       uint8_t ply_cap,
                                       char *move,
                                       uint8_t move_cap,
                                       char *notation,
                                       uint8_t notation_cap);
uint8_t netchesszx_protocol_parse_chat(const char *rx,
                                       char *text,
                                       uint8_t text_cap);

const char *netchesszx_protocol_ack_payload(const char *rx);
const char *netchesszx_protocol_nack_payload(const char *rx);

#endif
