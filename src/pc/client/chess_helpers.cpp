#include "chess_helpers.h"
#include "common/chess/rules_compact.h"
#include <QHostAddress>

namespace ChessHelpers {

static constexpr char kWhitePieces[] = ".PNBRQK";
static constexpr char kBlackPieces[] = ".pnbrqk";

static bool isMoveFile(QChar ch)
{
    return ch >= QLatin1Char('a') && ch <= QLatin1Char('h');
}

static bool isMoveRank(QChar ch)
{
    return ch >= QLatin1Char('1') && ch <= QLatin1Char('8');
}

static bool isPromotionPiece(QChar ch)
{
    return ch == QLatin1Char('q') || ch == QLatin1Char('r') ||
           ch == QLatin1Char('b') || ch == QLatin1Char('n');
}

static bool moveCoreSyntaxOk(const QString &move)
{
    return (move.size() == 4 || move.size() == 5) &&
           isMoveFile(move.at(0)) && isMoveRank(move.at(1)) &&
           isMoveFile(move.at(2)) && isMoveRank(move.at(3));
}

int8_t compactPieceFromAscii(char piece)
{
    for (int8_t value = NETCHESSZX_RULE_PAWN;
         value <= NETCHESSZX_RULE_KING;
         ++value) {
        if (piece == kWhitePieces[value]) {
            return value;
        }
        if (piece == kBlackPieces[value]) {
            return static_cast<int8_t>(-value);
        }
    }
    return NETCHESSZX_RULE_EMPTY;
}

char asciiPieceFromCompact(int8_t piece)
{
    if (piece >= NETCHESSZX_RULE_PAWN && piece <= NETCHESSZX_RULE_KING) {
        return kWhitePieces[piece];
    }
    if (piece <= NETCHESSZX_RULE_BP && piece >= NETCHESSZX_RULE_BK) {
        return kBlackPieces[-piece];
    }
    return '.';
}

void asciiBoardFromCompact(const int8_t compact[64], char ascii[64])
{
    for (int index = 0; index < 64; ++index) {
        ascii[index] = asciiPieceFromCompact(compact[index]);
    }
}

QString squareName(int row, int col)
{
    const QChar file('a' + col);
    const QChar rank('8' - row);
    return QString(file) + QString(rank);
}

bool moveCoords(const QString &move, int *fromRow, int *fromCol, int *toRow, int *toCol)
{
    if (!moveCoreSyntaxOk(move)) return false;
    *fromCol = move[0].unicode() - 'a';
    *fromRow = '8' - move[1].unicode();
    *toCol = move[2].unicode() - 'a';
    *toRow = '8' - move[3].unicode();
    return true;
}

char lowerPiece(char piece)
{
    return piece >= 'A' && piece <= 'Z'
        ? static_cast<char>(piece + ('a' - 'A')) : piece;
}

QString sanPieceLetter(char piece)
{
    switch (lowerPiece(piece)) {
    case 'n': return QStringLiteral("N");
    case 'b': return QStringLiteral("B");
    case 'r': return QStringLiteral("R");
    case 'q': return QStringLiteral("Q");
    case 'k': return QStringLiteral("K");
    default: return QString();
    }
}

bool isMoveSyntaxOk(const QString &move)
{
    if (!moveCoreSyntaxOk(move)) return false;
    if (move.size() == 4) return true;
    return isPromotionPiece(move.at(4));
}

bool isMqttRoomSyntaxOk(const QString &room)
{
    if (room.size() != 6 || !room.startsWith(QStringLiteral("NC"))) return false;
    for (const QChar ch : room.sliced(2)) {
        const ushort c = ch.unicode();
        if ((c >= 'A' && c <= 'F') || (c >= '0' && c <= '9')) continue;
        return false;
    }
    return true;
}

bool isDirectIpSyntaxOk(const QString &host)
{
    QHostAddress address;
    return address.setAddress(host) && address.protocol() == QAbstractSocket::IPv4Protocol && !address.isNull();
}

} // namespace ChessHelpers
