#include "common/savegame/savegame_wire.h"

static const char save_piece_table[] = NETCHESSZX_SAVE_PIECE_TABLE;

static int save_piece_nibble(char piece)
{
    uint8_t nibble;

    for (nibble = 0u; nibble < NETCHESSZX_SAVE_PIECE_COUNT; ++nibble) {
        if (nibble != NETCHESSZX_SAVE_PIECE_RESERVED &&
            save_piece_table[nibble] == piece) {
            return nibble;
        }
    }
    return -1;
}

static int save_timer_ok(uint8_t hour, uint8_t minute, uint8_t second)
{
    return hour <= 99u && minute < 60u && second < 60u;
}

static int save_nibble_to_piece(uint8_t nibble, char *out)
{
    if (nibble >= NETCHESSZX_SAVE_PIECE_COUNT ||
        nibble == NETCHESSZX_SAVE_PIECE_RESERVED) {
        return NETCHESSZX_SAVE_ERR_BOARD;
    }
    *out = save_piece_table[nibble];
    return NETCHESSZX_SAVE_OK;
}

static uint8_t save_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0u;
    uint8_t i;
    uint8_t bit;

    for (i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static char save_b64_char(uint8_t value)
{
    if (value < 26u) {
        return (char)('A' + value);
    }
    if (value < 52u) {
        return (char)('a' + (value - 26u));
    }
    if (value < 62u) {
        return (char)('0' + (value - 52u));
    }
    return value == 62u ? '-' : '_';
}

static int save_b64_value(char ch, uint8_t *out)
{
    if (ch >= 'A' && ch <= 'Z') {
        *out = (uint8_t)(ch - 'A');
        return NETCHESSZX_SAVE_OK;
    }
    if (ch >= 'a' && ch <= 'z') {
        *out = (uint8_t)(26u + (uint8_t)(ch - 'a'));
        return NETCHESSZX_SAVE_OK;
    }
    if (ch >= '0' && ch <= '9') {
        *out = (uint8_t)(52u + (uint8_t)(ch - '0'));
        return NETCHESSZX_SAVE_OK;
    }
    if (ch == '-') {
        *out = 62u;
        return NETCHESSZX_SAVE_OK;
    }
    if (ch == '_') {
        *out = 63u;
        return NETCHESSZX_SAVE_OK;
    }
    return NETCHESSZX_SAVE_ERR_TOKEN;
}

int netchesszx_save_state_validate(const netchesszx_save_state_t *state)
{
    uint8_t i;
    uint8_t kings = 0u;
    char piece;

    if (state == 0) {
        return NETCHESSZX_SAVE_ERR_NULL;
    }
    if (state->host_color > NETCHESSZX_SAVE_HOST_BLACK ||
        state->side > NETCHESSZX_SAVE_SIDE_BLACK ||
        state->side != (uint8_t)(state->ply & 1u)) {
        return NETCHESSZX_SAVE_ERR_FIELD;
    }
    if ((state->castle & (uint8_t)~0x0fu) != 0u ||
        (state->flags & (uint8_t)~NETCHESSZX_SAVE_FLAGS_MASK) != 0u ||
        (state->view_flags & (uint8_t)~NETCHESSZX_SAVE_VIEW_MASK) != 0u) {
        return NETCHESSZX_SAVE_ERR_FIELD;
    }
    if (!NETCHESSZX_SAVE_EP_VALID(state->side, state->ep)) {
        return NETCHESSZX_SAVE_ERR_FIELD;
    }
    if (!save_timer_ok(state->game_hour, state->game_minute, state->game_second) ||
        !save_timer_ok(state->move_hour, state->move_minute, state->move_second)) {
        return NETCHESSZX_SAVE_ERR_FIELD;
    }
    for (i = 0u; i < 64u; ++i) {
        piece = state->cells[i];
        if (save_piece_nibble(piece) < 0 ||
            ((i < 8u || i >= 56u) && (piece == 'P' || piece == 'p'))) {
            return NETCHESSZX_SAVE_ERR_BOARD;
        }
        if (piece == 'K') {
            if ((kings & 1u) != 0u) {
                return NETCHESSZX_SAVE_ERR_BOARD;
            }
            kings |= 1u;
        } else if (piece == 'k') {
            if ((kings & 2u) != 0u) {
                return NETCHESSZX_SAVE_ERR_BOARD;
            }
            kings |= 2u;
        }
    }
    return kings == 3u ? NETCHESSZX_SAVE_OK : NETCHESSZX_SAVE_ERR_BOARD;
}

int netchesszx_save_wire_pack(uint8_t *wire,
                              size_t cap,
                              const netchesszx_save_state_t *state)
{
    uint8_t i;
    int hi;
    int lo;
    int rc;

    if (wire == 0) {
        return NETCHESSZX_SAVE_ERR_NULL;
    }
    if (cap < NETCHESSZX_SAVE_WIRE_SIZE) {
        return NETCHESSZX_SAVE_ERR_BUFFER;
    }
    rc = netchesszx_save_state_validate(state);
    if (rc != NETCHESSZX_SAVE_OK) {
        return rc;
    }
    for (i = 0u; i < 32u; ++i) {
        hi = save_piece_nibble(state->cells[(uint8_t)(i * 2u)]);
        if (hi < 0) {
            return NETCHESSZX_SAVE_ERR_BOARD;
        }
        lo = save_piece_nibble(state->cells[(uint8_t)(i * 2u + 1u)]);
        if (lo < 0) {
            return NETCHESSZX_SAVE_ERR_BOARD;
        }
        wire[i] = (uint8_t)((hi << 4) | lo);
    }
    wire[32] = (uint8_t)((state->side & 1u) |
                         ((state->host_color & 1u) << 1) |
                         ((state->castle & 0x0fu) << 2) |
                         ((state->view_flags & NETCHESSZX_SAVE_VIEW_FLIPPED) << 6));
    wire[33] = state->ep;
    wire[34] = (uint8_t)state->ply;
    wire[35] = (uint8_t)(state->ply >> 8);
    wire[36] = state->flags;
    wire[37] = state->game_hour;
    wire[38] = state->game_minute;
    wire[39] = state->game_second;
    wire[40] = state->move_hour;
    wire[41] = state->move_minute;
    wire[42] = state->move_second;
    wire[43] = NETCHESSZX_SAVE_WIRE_VERSION;
    wire[44] = save_crc8(wire, 44u);
    return NETCHESSZX_SAVE_OK;
}

int netchesszx_save_wire_unpack(netchesszx_save_state_t *state,
                                const uint8_t *wire,
                                size_t len)
{
    uint8_t i;
    uint8_t meta;
    int rc;

    if (state == 0 || wire == 0) {
        return NETCHESSZX_SAVE_ERR_NULL;
    }
    if (len != NETCHESSZX_SAVE_WIRE_SIZE) {
        return NETCHESSZX_SAVE_ERR_BUFFER;
    }
    if (wire[43] != NETCHESSZX_SAVE_WIRE_VERSION ||
        save_crc8(wire, 44u) != wire[44]) {
        return NETCHESSZX_SAVE_ERR_CRC;
    }
    for (i = 0u; i < 32u; ++i) {
        rc = save_nibble_to_piece((uint8_t)(wire[i] >> 4),
                                  &state->cells[(uint8_t)(i * 2u)]);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
        rc = save_nibble_to_piece((uint8_t)(wire[i] & 0x0fu),
                                  &state->cells[(uint8_t)(i * 2u + 1u)]);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
    }
    meta = wire[32];
    if ((meta & 0x80u) != 0u) {
        return NETCHESSZX_SAVE_ERR_FIELD;
    }
    state->side = (uint8_t)(meta & 1u);
    state->host_color = (uint8_t)((meta >> 1) & 1u);
    state->castle = (uint8_t)((meta >> 2) & 0x0fu);
    state->view_flags = (uint8_t)((meta >> 6) & NETCHESSZX_SAVE_VIEW_FLIPPED);
    state->ep = wire[33];
    state->ply = (uint16_t)wire[34] | ((uint16_t)wire[35] << 8);
    state->flags = wire[36];
    state->game_hour = wire[37];
    state->game_minute = wire[38];
    state->game_second = wire[39];
    state->move_hour = wire[40];
    state->move_minute = wire[41];
    state->move_second = wire[42];
    return netchesszx_save_state_validate(state);
}

int netchesszx_save_wire_b64_encode(char *out,
                                    size_t cap,
                                    const uint8_t *wire,
                                    size_t len)
{
    uint8_t i;
    uint8_t j = 0u;
    uint32_t block;

    if (out == 0 || wire == 0) {
        return NETCHESSZX_SAVE_ERR_NULL;
    }
    if (cap < NETCHESSZX_SAVE_WIRE_B64_SIZE ||
        len != NETCHESSZX_SAVE_WIRE_SIZE) {
        return NETCHESSZX_SAVE_ERR_BUFFER;
    }
    for (i = 0u; i < NETCHESSZX_SAVE_WIRE_SIZE; i = (uint8_t)(i + 3u)) {
        block = ((uint32_t)wire[i] << 16) |
                ((uint32_t)wire[(uint8_t)(i + 1u)] << 8) |
                (uint32_t)wire[(uint8_t)(i + 2u)];
        out[j++] = save_b64_char((uint8_t)((block >> 18) & 0x3fu));
        out[j++] = save_b64_char((uint8_t)((block >> 12) & 0x3fu));
        out[j++] = save_b64_char((uint8_t)((block >> 6) & 0x3fu));
        out[j++] = save_b64_char((uint8_t)(block & 0x3fu));
    }
    return NETCHESSZX_SAVE_OK;
}

int netchesszx_save_wire_b64_decode(uint8_t *wire,
                                    size_t cap,
                                    const char *text,
                                    size_t len)
{
    uint8_t i;
    uint8_t j = 0u;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    int rc;

    if (wire == 0 || text == 0) {
        return NETCHESSZX_SAVE_ERR_NULL;
    }
    if (cap < NETCHESSZX_SAVE_WIRE_SIZE ||
        len != NETCHESSZX_SAVE_WIRE_B64_SIZE) {
        return NETCHESSZX_SAVE_ERR_BUFFER;
    }
    for (i = 0u; i < NETCHESSZX_SAVE_WIRE_B64_SIZE; i = (uint8_t)(i + 4u)) {
        rc = save_b64_value(text[i], &a);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
        rc = save_b64_value(text[(uint8_t)(i + 1u)], &b);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
        rc = save_b64_value(text[(uint8_t)(i + 2u)], &c);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
        rc = save_b64_value(text[(uint8_t)(i + 3u)], &d);
        if (rc != NETCHESSZX_SAVE_OK) {
            return rc;
        }
        wire[j++] = (uint8_t)((a << 2) | (b >> 4));
        wire[j++] = (uint8_t)((b << 4) | (c >> 2));
        wire[j++] = (uint8_t)((c << 6) | d);
    }
    return NETCHESSZX_SAVE_OK;
}
