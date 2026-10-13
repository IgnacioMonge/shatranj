#include "common/chess/legal.h"
#include "common/protocol/game_protocol.h"
#include "spectrum/board/board.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Existing rules_save/restore snapshot contract (also consumed by desktop).
   Compare fields, never padding or a checksum. This is not a wire encoder. */
typedef struct {
    int8_t board[64];
    uint8_t side;
    uint8_t castle;
    int8_t ep;
} snapshot_t;

static void save(snapshot_t *state)
{
    assert(netchesszx_rules_state_size() == sizeof(*state));
    assert(netchesszx_rules_save(state, sizeof(*state)) == NETCHESSZX_OK);
}

static void equal(const snapshot_t *a, const snapshot_t *b)
{
    unsigned i;
    for (i = 0; i < 64; ++i) {
        assert(a->board[i] == b->board[i]);
    }
    assert(a->side == b->side && a->castle == b->castle && a->ep == b->ep);
}

static void rejected(const netchesszx_action_t *action)
{
    snapshot_t before, after;
    save(&before);
    assert(netchesszx_rules_apply_action(action) != NETCHESSZX_OK);
    save(&after);
    equal(&before, &after);
}

static void step(const char *text)
{
    netchesszx_action_t direct, decoded;
    snapshot_t before, expected, actual;
    spectrum_board_snapshot_t zx;
    spectrum_board_undo_t undo;
    char token[6], frame[48], wanted[48], ply[8], received[6], notation[16];
    unsigned i;

    save(&before);
    assert(netchesszx_action_decode(text, &direct) == NETCHESSZX_OK);
    assert(netchesszx_rules_apply_action(&direct) == NETCHESSZX_OK);
    save(&expected);
    assert(netchesszx_action_encode(&direct, token, sizeof(token)) == NETCHESSZX_OK);
    assert(netchess_proto_format_move(frame, sizeof(frame), "7", token, "note"));
    snprintf(wanted, sizeof(wanted), "MOVE 7 %s note", token);
    assert(strcmp(frame, wanted) == 0);
    assert(netchess_proto_parse_move(frame, ply, sizeof(ply), received,
                                     sizeof(received), notation, sizeof(notation)));
    assert(strcmp(ply, "7") == 0 && strcmp(notation, "note") == 0);
    assert(netchesszx_action_decode(received, &decoded) == NETCHESSZX_OK);
    assert(decoded.from == direct.from && decoded.to == direct.to &&
           decoded.promo == direct.promo);
    assert(netchesszx_rules_restore(&before, sizeof(before)) == NETCHESSZX_OK);
    /* The published text entry point used by both local and remote desktop. */
    assert(netchesszx_rules_play(received) == NETCHESSZX_OK);
    save(&actual);
    equal(&expected, &actual);

    /* Product Spectrum admission followed by its accepted-action adapter. */
    assert(spectrum_board_is_legal_move(received));
    assert(spectrum_board_apply_trusted_move_with_undo(received, &undo));
    spectrum_board_snapshot_save(&zx);
    for (i = 0; i < 64; ++i) {
        assert(spectrum_board_test_rule_cell((uint8_t)i) == expected.board[i]);
    }
    assert(zx.side == expected.side && zx.castle == expected.castle && zx.ep == expected.ep);
    spectrum_board_undo_restore(&undo);
    assert(spectrum_board_is_legal_move(received));
    assert(spectrum_board_apply_trusted_move_with_undo(received, &undo));
}

static void sequence(const char *const *moves, unsigned count)
{
    unsigned i;
    assert(netchesszx_rules_reset() == NETCHESSZX_OK);
    spectrum_board_reset();
    for (i = 0; i < count; ++i) {
        step(moves[i]);
    }
}

static void promotion_alphabet(void)
{
    spectrum_board_snapshot_t before, after;
    spectrum_board_undo_t undo;
    char text[] = "b7c8q";
    unsigned suffix, cell;

    spectrum_board_snapshot_save(&before);
    for (suffix = 0; suffix < 256; ++suffix) {
        uint8_t accepted = (uint8_t)(suffix != 0 && strchr("qrbnQRBN", (int)suffix) != NULL);
        text[4] = (char)suffix;
        assert(spectrum_board_apply_trusted_move_with_undo(text, &undo) == accepted);
        if (accepted) {
            spectrum_board_undo_restore(&undo);
        }
        spectrum_board_snapshot_save(&after);
        for (cell = 0; cell < 64; ++cell) {
            assert(after.cells[cell] == before.cells[cell]);
        }
        assert(after.side == before.side && after.castle == before.castle && after.ep == before.ep);
    }
}

int main(void)
{
    const char *const castles[] = {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4",
        "g8f6", "e1g1", "f8c5", "d2d3", "e8g8"};
    const char *const ep[] = {"e2e4", "a7a6", "e4e5", "d7d5", "e5d6"};
    const char *const long_castles[] = {"d2d4", "d7d5", "c1f4", "c8f5",
        "b1c3", "b8c6", "d1d2", "d8d7", "e1c1", "e8c8"};
    const char *const promotion[] = {"a2a4", "h7h5", "a4a5", "h5h4",
        "a5a6", "h4h3", "a6b7", "h3g2"};
    const char *const mate[] = {"f2f3", "e7e5", "g2g4", "d8h4"};
    const char *const malformed[] = {"", "e", "e2", "e2e", "i2e4", "e2e9",
        "e2e4qq", "e2e4x"};
    const netchesszx_action_t invalid[] = {{64, 0, 0}, {0, 255, 0}, {52, 36, 'x'},
        {52, 36, 'Q'}, {52, 36, 'q'}, {52, 28, 0}, {12, 28, 0}, {40, 32, 0}};
    const char promos[] = "qrbn";
    unsigned i;
    snapshot_t before, after;
    netchesszx_action_t action = {1, 2, 'n'};
    char output[6] = "guard";

    assert(sizeof(netchesszx_action_t) == 3);
    action.from = 52; action.to = 36; action.promo = 0;
    assert(netchesszx_action_encode(&action, output, sizeof(output)) == NETCHESSZX_OK);
    assert(strcmp(output, "e2e4") == 0);
    strcpy(output, "guard");
    sequence(castles, sizeof(castles) / sizeof(*castles));
    sequence(long_castles, sizeof(long_castles) / sizeof(*long_castles));
    sequence(ep, sizeof(ep) / sizeof(*ep));
    sequence(promotion, sizeof(promotion) / sizeof(*promotion));
    promotion_alphabet();
    for (i = 0; i < sizeof(promos) - 1; ++i) {
        char text[] = "b7c8q";
        sequence(promotion, sizeof(promotion) / sizeof(*promotion));
        text[4] = promos[i];
        step(text);
        text[0] = 'g'; text[1] = '2'; text[2] = 'f'; text[3] = '1';
        step(text);
    }
    sequence(mate, sizeof(mate) / sizeof(*mate));
    action.from = 48; action.to = 40; action.promo = 0;
    rejected(&action); /* Cannot ignore checkmate. */
    assert(netchesszx_rules_reset() == NETCHESSZX_OK);
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        rejected(&invalid[i]);
    }
    rejected(NULL);
    save(&before);
    for (i = 0; i < sizeof(malformed) / sizeof(*malformed); ++i) {
        action.from = 1; action.to = 2; action.promo = 'n';
        assert(netchesszx_action_decode(malformed[i], &action) != NETCHESSZX_OK);
        assert(action.from == 1 && action.to == 2 && action.promo == 'n');
        assert(netchesszx_rules_play(malformed[i]) != NETCHESSZX_OK);
    }
    save(&after);
    equal(&before, &after);
    assert(netchesszx_action_decode("a7a8N", &action) == NETCHESSZX_OK);
    assert(action.promo == 'n');
    assert(netchesszx_action_encode(&action, output, 5) == NETCHESSZX_ERR_BUFFER);
    assert(strcmp(output, "guard") == 0);
    assert(netchesszx_action_encode(&invalid[0], output, sizeof(output)) != NETCHESSZX_OK);
    assert(strcmp(output, "guard") == 0);
    puts("Chess actions: MOVE equivalence, Spectrum state/undo, special moves and atomic rejection passed");
    return 0;
}
