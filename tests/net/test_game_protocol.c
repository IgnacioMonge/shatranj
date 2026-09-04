#include "common/protocol/game_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *label)
{
    if (!ok) {
        printf("FAIL: %s\n", label);
        ++failures;
    }
}

static void check_text(const char *got, const char *expected, const char *label)
{
    if (strcmp(got, expected) != 0) {
        printf("FAIL: %s got='%s' expected='%s'\n", label, got, expected);
        ++failures;
    }
}

static void test_wire_constants(void)
{
    check_text(NETCHESS_PROTO_MOVE_PREFIX, "MOVE ", "move prefix constant");
    check_text(NETCHESS_PROTO_CHAT_PREFIX, "CHAT ", "chat prefix constant");
    check_text(NETCHESS_PROTO_ACK_PREFIX, "ACK ", "ack prefix constant");
    check_text(NETCHESS_PROTO_NACK_PREFIX, "NACK ", "nack prefix constant");
    check_text(NETCHESS_PROTO_GAME_START, "GAME START", "game start constant");
    check_text(NETCHESS_PROTO_RESET, "RESET", "reset constant");
    check_text(NETCHESS_PROTO_BYE, "BYE", "bye constant");
    check_text(NETCHESS_PROTO_DRAW, "DRAW", "draw constant");
    check_text(NETCHESS_PROTO_RESIGN, "RESIGN", "resign constant");
    check_text(NETCHESS_PROTO_TAKEBACK_PREFIX, "TAKEBACK ", "takeback constant");
    check_text(NETCHESS_PROTO_RESTORE_RQ, "RQ", "restore rq constant");
    check_text(NETCHESS_PROTO_RESTORE_RY, "RY", "restore ry constant");
    check_text(NETCHESS_PROTO_RESTORE_RN, "RN", "restore rn constant");
    check_text(NETCHESS_PROTO_RESTORE_RA, "RA", "restore ra constant");
    check_text(NETCHESS_PROTO_RESTORE_RS_PREFIX, "RS", "restore rs constant");
}

static void test_move_parser(void)
{
    char ply[8];
    char move[8];
    char spectrum_move[6];
    char notation[8];
    char short_notation[2];

    check(netchess_proto_parse_move("MOVE 1 e2e4 3000",
                                    ply,
                                    sizeof(ply),
                                    move,
                                    sizeof(move),
                                    notation,
                                    sizeof(notation)),
          "move parse clock");
    check_text(ply, "1", "move ply");
    check_text(move, "e2e4", "move move");
    check_text(notation, "", "move ignores clock");

    check(netchess_proto_parse_move("MOVE 2 g1f3 Nf3",
                                    ply,
                                    sizeof(ply),
                                    move,
                                    sizeof(move),
                                    notation,
                                    sizeof(notation)),
          "move parse notation");
    check_text(notation, "Nf3", "move keeps notation");

    check(netchess_proto_parse_move("MOVE 3 e7e8q Q",
                                    ply,
                                    sizeof(ply),
                                    move,
                                    sizeof(move),
                                    notation,
                                    sizeof(notation)),
          "move parse promotion");
    check_text(move, "e7e8q", "move promotion");

    check(!netchess_proto_parse_move("MOVE 4 e2e",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     notation,
                                     sizeof(notation)),
          "move rejects short move");
    check(!netchess_proto_parse_move("MOVE x e2e4",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     notation,
                                     sizeof(notation)),
          "move rejects bad ply");
    check(!netchess_proto_parse_move("MOVE 5 z9z9",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     notation,
                                     sizeof(notation)),
          "move rejects bad coords");
    check(!netchess_proto_parse_move("MOVE 6 e7e8k",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     notation,
                                     sizeof(notation)),
          "move rejects bad promotion");
    check(!netchess_proto_parse_move("MOVE 7 e7e8qjunk",
                                     ply,
                                     sizeof(ply),
                                     spectrum_move,
                                     sizeof(spectrum_move),
                                     notation,
                                     sizeof(notation)),
          "move rejects truncated Spectrum token");
    check(!netchess_proto_parse_move("MOVE 8 e2e4 Nf3 extra",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     notation,
                                     sizeof(notation)),
          "move rejects second notation token");
    check(netchess_proto_parse_move("MOVE 9 e2e4 3000",
                                    ply,
                                    sizeof(ply),
                                    move,
                                    sizeof(move),
                                    0,
                                    0u),
          "move accepts one legacy token without notation output");
    check(!netchess_proto_parse_move("MOVE 10 e2e4 Nf3 extra",
                                     ply,
                                     sizeof(ply),
                                     move,
                                     sizeof(move),
                                     0,
                                     0u),
          "move rejects second token without notation output");
    check(netchess_proto_parse_move("MOVE 11 e2e4 Nf3",
                                    ply,
                                    sizeof(ply),
                                    move,
                                    sizeof(move),
                                    short_notation,
                                    sizeof(short_notation)),
          "move accepts one notation larger than output");
    check_text(short_notation, "N", "move truncates notation output");
}

static void test_chat_parser(void)
{
    char text[32];
    char inplace[32];

    check(netchess_proto_parse_chat("CHAT hello world", text, sizeof(text)),
          "chat parse");
    check_text(text, "hello world", "chat text");
    strcpy(inplace, "CHAT in place");
    check(netchess_proto_parse_chat(inplace, inplace, sizeof(inplace)),
          "chat parse in-place");
    check_text(inplace, "in place", "chat in-place text");
    check(!netchess_proto_parse_chat("PINGX", text, sizeof(text)),
          "chat rejects other verb");
    check(!netchess_proto_parse_chat("CHAT ", text, sizeof(text)),
          "chat rejects empty");
}

static void test_ack_nack_parser(void)
{
    char ply[8];
    char text[16];

    check(netchess_proto_parse_ack("ACK 12 Nf3", ply, sizeof(ply),
                                   text, sizeof(text)),
          "ack parse notation");
    check_text(ply, "12", "ack ply");
    check_text(text, "Nf3", "ack notation");

    check(netchess_proto_parse_ack("ACK 12", ply, sizeof(ply),
                                   text, sizeof(text)),
          "ack parse no notation");
    check_text(text, "", "ack empty notation");
    check(netchess_proto_is_ack("ACK x"),
          "ack recognises malformed verb");
    check(!netchess_proto_is_ack("NACK 12"),
          "ack rejects nack verb");

    check(netchess_proto_parse_nack("NACK 13 ILLEGAL", ply, sizeof(ply),
                                    text, sizeof(text)),
          "nack parse reason");
    check_text(ply, "13", "nack ply");
    check_text(text, "ILLEGAL", "nack reason");

    check(!netchess_proto_parse_ack("ACK x", ply, sizeof(ply),
                                    text, sizeof(text)),
          "ack rejects bad ply");
    check(netchess_proto_is_nack("NACK x"),
          "nack recognises malformed verb");
    check(!netchess_proto_is_nack("ACK 12"),
          "nack rejects ack verb");
}

static void test_fixed_messages(void)
{
    char detail[24];

    check(netchess_proto_parse_game_start("GAME START",
                                          detail,
                                          sizeof(detail)),
          "game start parse");
    check_text(detail, "", "game start empty detail");

    check(netchess_proto_parse_game_start("GAME START WHITE=HOST",
                                          detail,
                                          sizeof(detail)),
          "game start direct detail");
    check_text(detail, "WHITE=HOST", "game start detail");

    check(!netchess_proto_parse_game_start("GAME STARTED",
                                           detail,
                                           sizeof(detail)),
          "game start rejects longer token");
    check(netchess_proto_is_reset("RESET"), "reset exact");
    check(!netchess_proto_is_reset("RESET NOW"), "reset rejects suffix");
    check(netchess_proto_is_bye("BYE"), "bye exact");
}

static void test_formatters(void)
{
    char out[32];

    check(netchess_proto_format_move(out, sizeof(out), "7", "e2e4", "e4"),
          "format move");
    check_text(out, "MOVE 7 e2e4 e4", "move format text");

    check(netchess_proto_format_chat(out, sizeof(out), "hello"),
          "format chat");
    check_text(out, "CHAT hello", "chat format text");

    check(netchess_proto_format_ack(out, sizeof(out), "7", "e4"),
          "format ack");
    check_text(out, "ACK 7 e4", "ack format text");

    check(netchess_proto_format_nack(out, sizeof(out), "7", "BUSY"),
          "format nack");
    check_text(out, "NACK 7 BUSY", "nack format text");

    check(netchess_proto_format_game_start(out, sizeof(out), "WHITE=HOST"),
          "format game start");
    check_text(out, "GAME START WHITE=HOST", "game start format text");

    check(netchess_proto_format_reset(out, sizeof(out)), "format reset");
    check_text(out, "RESET", "reset format text");

    check(netchess_proto_format_bye(out, sizeof(out)), "format bye");
    check_text(out, "BYE", "bye format text");

    check(netchess_proto_format_mach(out, sizeof(out), NETCHESS_PLAT_ZX),
          "format mach zx");
    check_text(out, "MACH ZX", "mach zx text");
    check(netchess_proto_format_mach(out, sizeof(out), NETCHESS_PLAT_NXT),
          "format mach nxt");
    check_text(out, "MACH NXT", "mach nxt text");
    check(netchess_proto_format_mach(out, sizeof(out), NETCHESS_PLAT_MAC),
          "format mach mac");
    check_text(out, "MACH MAC", "mach mac text");
    check(netchess_proto_format_mach(out, sizeof(out), NETCHESS_PLAT_SPCX),
          "format mach spcx");
    check_text(out, "MACH SPCX", "mach spcx text");
    check(!netchess_proto_format_mach(out, sizeof(out), NETCHESS_PLAT_UNKNOWN),
          "format rejects unknown plat");

    check(!netchess_proto_format_move(out, 8u, "123", "e2e4", "e4"),
          "format detects overflow");
}

static void test_mach_parser(void)
{
    uint8_t plat = 0xffu;

    check(netchess_proto_parse_mach("MACH ZX", &plat) &&
              plat == NETCHESS_PLAT_ZX,
          "parse mach zx");
    check(netchess_proto_parse_mach("MACH NXT", &plat) &&
              plat == NETCHESS_PLAT_NXT,
          "parse mach nxt");
    check(netchess_proto_parse_mach("MACH LNX", &plat) &&
              plat == NETCHESS_PLAT_LNX,
          "parse mach lnx");
    check(netchess_proto_parse_mach("MACH PC", &plat) &&
              plat == NETCHESS_PLAT_PC,
          "parse mach pc");
    check(netchess_proto_parse_mach("MACH SPCX", &plat) &&
              plat == NETCHESS_PLAT_SPCX,
          "parse mach spcx");
    check(!netchess_proto_parse_mach("MACH SPC", &plat), "reject short spcx");
    check(!netchess_proto_parse_mach("MACH ZZ", &plat), "reject bad code");
    check(!netchess_proto_parse_mach("MACH ZX ", &plat), "reject trailing");
    check(!netchess_proto_parse_mach("PLAT ZX", &plat), "reject non-mach");
    check(!netchess_proto_parse_mach("MOVE e2e4", &plat), "reject move");
}

int main(void)
{
    test_wire_constants();
    test_move_parser();
    test_chat_parser();
    test_ack_nack_parser();
    test_fixed_messages();
    test_formatters();
    test_mach_parser();

    if (failures != 0) {
        printf("game protocol tests failed: %d\n", failures);
        return 1;
    }
    printf("game protocol tests ok\n");
    return 0;
}
