#include "common/protocol/messages.h"

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

static void test_tags(void)
{
    check(netchesszx_protocol_classify("MOVE 1 e2e4") == NETCHESSZX_PROTOCOL_MOVE,
          "tag move");
    check(netchesszx_protocol_classify("CHAT hello") == NETCHESSZX_PROTOCOL_CHAT,
          "tag chat");
    check(netchesszx_protocol_classify("ACK 2") == NETCHESSZX_PROTOCOL_ACK,
          "tag ack");
    check(netchesszx_protocol_classify("NACK 2") == NETCHESSZX_PROTOCOL_NACK,
          "tag nack");
    check(netchesszx_protocol_classify("PING") == NETCHESSZX_PROTOCOL_PING,
          "tag ping");
    check(netchesszx_protocol_classify("PINGX") == NETCHESSZX_PROTOCOL_UNKNOWN,
          "tag rejects ping prefix");
    check(netchesszx_protocol_classify("BYE") == NETCHESSZX_PROTOCOL_BYE,
          "tag bye");
    check(netchesszx_protocol_classify("RESET") == NETCHESSZX_PROTOCOL_RESET,
          "tag reset");
    check(netchesszx_protocol_classify("GAME START WHITE=HOST") ==
              NETCHESSZX_PROTOCOL_GAME_START,
          "tag game start");
}

static void test_move_parser(void)
{
    char ply[8];
    char move[8];
    char notation[8];

    check(netchesszx_protocol_parse_move("MOVE 1 e2e4 3000",
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

    check(netchesszx_protocol_parse_move("MOVE 2 g1f3 Nf3",
                                         ply,
                                         sizeof(ply),
                                         move,
                                         sizeof(move),
                                         notation,
                                         sizeof(notation)),
          "move parse notation");
    check_text(notation, "Nf3", "move keeps notation");

    check(!netchesszx_protocol_parse_move("MOVE 3 z9z9",
                                          ply,
                                          sizeof(ply),
                                          move,
                                          sizeof(move),
                                          notation,
                                          sizeof(notation)),
          "move rejects bad coordinates");
    check(!netchesszx_protocol_parse_move("MOVE 4 e7e8k",
                                          ply,
                                          sizeof(ply),
                                          move,
                                          sizeof(move),
                                          notation,
                                          sizeof(notation)),
          "move rejects bad promotion");
}

static void test_chat_parser(void)
{
    char text[32];

    check(netchesszx_protocol_parse_chat("CHAT hello world", text, sizeof(text)),
          "chat parse");
    check_text(text, "hello world", "chat text");
    check(!netchesszx_protocol_parse_chat("PINGX", text, sizeof(text)),
          "chat rejects other verb");
}

static void test_payload_accessors(void)
{
    check_text(netchesszx_protocol_ack_payload("ACK 42"), "42", "ack payload");
    check_text(netchesszx_protocol_nack_payload("NACK 9"), "9", "nack payload");
    check(netchesszx_protocol_ack_payload("NACK 42") == 0, "ack rejects nack");
}

int main(void)
{
    test_tags();
    test_move_parser();
    test_chat_parser();
    test_payload_accessors();

    if (failures != 0) {
        printf("protocol tests failed: %d\n", failures);
        return 1;
    }
    printf("protocol tests ok\n");
    return 0;
}
