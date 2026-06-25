#include "common/protocol/messages.h"

#include <string.h>

static const char ack_prefix[] = "ACK ";
static const char nack_prefix[] = "NACK ";

static uint8_t starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t is_file_char(char c)
{
    return (uint8_t)(c >= 'a' && c <= 'h');
}

static uint8_t is_rank_char(char c)
{
    return (uint8_t)(c >= '1' && c <= '8');
}

static uint8_t is_promotion_char(char c)
{
    return (uint8_t)(c == 'q' || c == 'r' || c == 'b' || c == 'n');
}

static uint8_t move_syntax_ok(const char *move, uint8_t len)
{
    if (len != 4u && len != 5u) {
        return 0u;
    }
    if (!is_file_char(move[0]) || !is_rank_char(move[1]) ||
        !is_file_char(move[2]) || !is_rank_char(move[3])) {
        return 0u;
    }
    return (uint8_t)(len == 4u || is_promotion_char(move[4]));
}

netchesszx_protocol_tag_t netchesszx_protocol_classify(const char *rx)
{
    switch (rx[0]) {
    case 'A':
        if (starts_with(rx, ack_prefix)) {
            return NETCHESSZX_PROTOCOL_ACK;
        }
        break;
    case 'B':
        if (strcmp(rx, "BYE") == 0) {
            return NETCHESSZX_PROTOCOL_BYE;
        }
        break;
    case 'C':
        if (starts_with(rx, "CHAT ")) {
            return NETCHESSZX_PROTOCOL_CHAT;
        }
        break;
    case 'G':
        if (starts_with(rx, "GAME START") &&
            (rx[10u] == '\0' || rx[10u] == ' ')) {
            return NETCHESSZX_PROTOCOL_GAME_START;
        }
        break;
    case 'M':
        if (starts_with(rx, "MOVE ")) {
            return NETCHESSZX_PROTOCOL_MOVE;
        }
        break;
    case 'N':
        if (starts_with(rx, nack_prefix)) {
            return NETCHESSZX_PROTOCOL_NACK;
        }
        break;
    case 'P':
        if (strcmp(rx, "PING") == 0) {
            return NETCHESSZX_PROTOCOL_PING;
        }
        break;
    case 'R':
        if (strcmp(rx, "RESET") == 0) {
            return NETCHESSZX_PROTOCOL_RESET;
        }
        break;
    default:
        break;
    }
    return NETCHESSZX_PROTOCOL_UNKNOWN;
}

uint8_t netchesszx_protocol_parse_move(const char *rx,
                                       char *ply,
                                       uint8_t ply_cap,
                                       char *move,
                                       uint8_t move_cap,
                                       char *notation,
                                       uint8_t notation_cap)
{
    const char *p;
    uint8_t n;
    uint8_t move_len;

    if (ply_cap == 0u || move_cap == 0u ||
        netchesszx_protocol_classify(rx) != NETCHESSZX_PROTOCOL_MOVE) {
        return 0u;
    }

    p = rx + 5u;
    n = 0u;
    while (*p >= '0' && *p <= '9' && n + 1u < ply_cap) {
        ply[n++] = *p++;
    }
    ply[n] = '\0';

    if (n == 0u || *p != ' ') {
        return 0u;
    }
    ++p;

    n = 0u;
    while (*p != '\0' && *p != ' ' && n + 1u < move_cap) {
        move[n++] = *p++;
    }
    move[n] = '\0';
    move_len = n;

    if (notation_cap != 0u) {
        notation[0] = '\0';
        if (*p == ' ') {
            ++p;
            n = 0u;
            while (*p != '\0' && *p != ' ' && n + 1u < notation_cap) {
                notation[n++] = *p++;
            }
            notation[n] = '\0';
            if (notation[0] >= '0' && notation[0] <= '9') {
                notation[0] = '\0';
            }
        }
    }
    return move_syntax_ok(move, move_len);
}

uint8_t netchesszx_protocol_parse_chat(const char *rx,
                                       char *text,
                                       uint8_t text_cap)
{
    if (text_cap == 0u ||
        netchesszx_protocol_classify(rx) != NETCHESSZX_PROTOCOL_CHAT) {
        return 0u;
    }
    strncpy(text, rx + 5u, (uint8_t)(text_cap - 1u));
    text[(uint8_t)(text_cap - 1u)] = '\0';
    return (uint8_t)(text[0] != '\0');
}

const char *netchesszx_protocol_ack_payload(const char *rx)
{
    return netchesszx_protocol_classify(rx) == NETCHESSZX_PROTOCOL_ACK ? rx + 4u : 0;
}

const char *netchesszx_protocol_nack_payload(const char *rx)
{
    return netchesszx_protocol_classify(rx) == NETCHESSZX_PROTOCOL_NACK ? rx + 5u : 0;
}
