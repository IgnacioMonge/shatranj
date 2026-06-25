#include "common/protocol/game_protocol.h"
#include "common/protocol/game_protocol_internal.h"

#include <string.h>

#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif

const char *netchess_after_prefix(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return text;
}

static uint8_t proto_copy_token(const char **p, char *out, uint8_t cap)
{
    uint8_t n = 0u;

    if (cap == 0u) {
        return 0u;
    }
    while (**p != '\0' && **p != ' ' && n + 1u < cap) {
        out[n++] = **p;
        ++*p;
    }
    out[n] = '\0';
    return n;
}

static uint8_t is_file_char(char c) NETCHESSZX_FASTCALL
{
    return (uint8_t)(c >= 'a' && c <= 'h');
}

static uint8_t is_rank_char(char c) NETCHESSZX_FASTCALL
{
    return (uint8_t)(c >= '1' && c <= '8');
}

static uint8_t is_promotion_char(char c) NETCHESSZX_FASTCALL
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

uint8_t netchess_proto_copy_digits(const char **p, char *out, uint8_t cap)
{
    uint8_t n = 0u;

    if (cap == 0u) {
        return 0u;
    }
    while (**p >= '0' && **p <= '9' && n + 1u < cap) {
        out[n++] = **p;
        ++*p;
    }
    out[n] = '\0';
    return n;
}

void netchess_proto_copy_rest(const char *p, char *out, uint8_t cap)
{
    uint8_t n = 0u;

    if (cap == 0u) {
        return;
    }
    while (*p != '\0' && n + 1u < cap) {
        out[n++] = *p++;
    }
    out[n] = '\0';
}

uint8_t netchess_proto_parse_move(const char *rx,
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

    if (netchess_after_prefix(rx, "MOVE ") == 0) {
        return 0u;
    }

    p = rx + 5u;
    n = netchess_proto_copy_digits(&p, ply, ply_cap);
    if (n == 0u || *p != ' ') {
        return 0u;
    }
    ++p;

    move_len = proto_copy_token(&p, move, move_cap);
    if (!move_syntax_ok(move, move_len)) {
        return 0u;
    }

    if (notation_cap != 0u) {
        notation[0] = '\0';
        if (*p == ' ') {
            ++p;
            proto_copy_token(&p, notation, notation_cap);
            if (notation[0] >= '0' && notation[0] <= '9') {
                notation[0] = '\0';
            }
        }
    }
    return 1u;
}

uint8_t netchess_proto_parse_chat(const char *rx,
                                  char *text,
                                  uint8_t text_cap)
{
    if (text_cap == 0u || netchess_after_prefix(rx, "CHAT ") == 0) {
        return 0u;
    }
    netchess_proto_copy_rest(rx + 5u, text, text_cap);
    return (uint8_t)(text[0] != '\0');
}
