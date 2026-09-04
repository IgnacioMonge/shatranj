#include "pc/client/chess_helpers.h"
#include "common/chess/rules_compact.h"

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

int main()
{
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
    static const char pieces[] = ".PNBRQKpnbrqk";
    static const int8_t compact[] = {
        NETCHESSZX_RULE_EMPTY,
        NETCHESSZX_RULE_WP, NETCHESSZX_RULE_WN, NETCHESSZX_RULE_WB,
        NETCHESSZX_RULE_WR, NETCHESSZX_RULE_WQ, NETCHESSZX_RULE_WK,
        NETCHESSZX_RULE_BP, NETCHESSZX_RULE_BN, NETCHESSZX_RULE_BB,
        NETCHESSZX_RULE_BR, NETCHESSZX_RULE_BQ, NETCHESSZX_RULE_BK
    };

    for (int index = 0; index < 13; ++index) {
        check(ChessHelpers::compactPieceFromAscii(pieces[index]) == compact[index] &&
                  ChessHelpers::asciiPieceFromCompact(compact[index]) == pieces[index],
              "compact piece round trip");
    }
    check(ChessHelpers::compactPieceFromAscii('?') == NETCHESSZX_RULE_EMPTY &&
              ChessHelpers::asciiPieceFromCompact(7) == '.',
          "invalid compact piece");

    check(ChessHelpers::squareName(7, 4) == QStringLiteral("e1"),
          "square name");
    check(ChessHelpers::moveCoords(QStringLiteral("e2e4"),
                                   &fromRow, &fromCol, &toRow, &toCol) &&
              fromRow == 6 && fromCol == 4 && toRow == 4 && toCol == 4,
          "move coordinates");
    check(!ChessHelpers::moveCoords(QStringLiteral("e9e4"),
                                    &fromRow, &fromCol, &toRow, &toCol),
          "invalid move coordinates");
    check(ChessHelpers::isMoveSyntaxOk(QStringLiteral("a7a8q")),
          "promotion syntax");
    check(!ChessHelpers::isMoveSyntaxOk(QStringLiteral("a7a8k")),
          "invalid promotion syntax");
    check(ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("NC12AF")),
          "six-character room syntax");
    check(!ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("NC12A")),
          "short room syntax");
    check(!ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("NC12AF0")),
          "overlong room syntax");
    check(!ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("NC12AG")),
          "non-hexadecimal room syntax");
    check(!ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("AB12AF")),
          "room prefix syntax");
    check(!ChessHelpers::isMqttRoomSyntaxOk(QStringLiteral("nc12af")),
          "lowercase room syntax");
    check(ChessHelpers::isDirectIpSyntaxOk(QStringLiteral("127.0.0.1")),
          "IPv4 syntax");
    check(!ChessHelpers::isDirectIpSyntaxOk(QStringLiteral("localhost")),
          "hostname is not direct IPv4");

    if (failures != 0) {
        return 1;
    }
    std::printf("chess helper tests ok\n");
    return 0;
}
