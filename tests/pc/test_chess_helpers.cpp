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

    CompactRulesState state = {};
    static const char start[] =
        "rnbqkbnr" "pppppppp" "........" "........"
        "....P..." "........" "PPPP.PPP" "RNBQKBNR";
    for (int index = 0; index < 64; ++index) {
        state.board[index] = ChessHelpers::compactPieceFromAscii(start[index]);
    }
    state.side = NETCHESSZX_RULE_BLACK;
    state.castle = NETCHESSZX_RULE_CASTLE_WK | NETCHESSZX_RULE_CASTLE_WQ |
                   NETCHESSZX_RULE_CASTLE_BK | NETCHESSZX_RULE_CASTLE_BQ;
    state.ep = 5 * 8 + 4;
    check(ChessHelpers::fenFromCompact(state, 1) ==
              QStringLiteral("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"),
          "FEN after 1.e4");
    state.castle = NETCHESSZX_RULE_CASTLE_BK;
    state.ep = NETCHESSZX_RULE_NO_SQUARE;
    check(ChessHelpers::fenFromCompact(state, 7).endsWith(QStringLiteral(" b k - 0 7")),
          "FEN castle and en passant fields");
    check(ChessHelpers::uciBestMove("bestmove e7e8q ponder a1a2\r") == QStringLiteral("e7e8q"),
          "UCI bestmove with ponder");
    check(ChessHelpers::uciBestMove("bestmove (none)").isEmpty() &&
              ChessHelpers::uciBestMove("info depth 3").isEmpty(),
          "UCI non-move lines");

    if (failures != 0) {
        return 1;
    }
    std::printf("chess helper tests ok\n");
    return 0;
}
