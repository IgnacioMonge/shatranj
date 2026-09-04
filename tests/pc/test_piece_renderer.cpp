#include "pc/client/piece_renderer.h"

#include <QColor>
#include <QCoreApplication>
#include <QImage>

#include <cstdio>

static bool scaledBoardRetainsColor(const QString &name)
{
    constexpr int squareSize = 52;

    PieceRenderer::setBoardTexture(name);
    const QImage board = PieceRenderer::boardTextureImage(squareSize);
    if (board.size() != QSize(squareSize * 8, squareSize * 8)) {
        std::fprintf(stderr, "FAIL: %s board texture did not load\n",
                     name.toUtf8().constData());
        return false;
    }

    const QColor darkSquare = board.pixelColor(squareSize + squareSize / 2,
                                                squareSize / 2);
    if (darkSquare.red() == darkSquare.green() &&
        darkSquare.green() == darkSquare.blue()) {
        std::fprintf(stderr,
                     "FAIL: %s lost its color palette while scaling\n",
                     name.toUtf8().constData());
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    return scaledBoardRetainsColor(QStringLiteral("blue.png")) &&
                   scaledBoardRetainsColor(QStringLiteral("green.png"))
               ? 0
               : 1;
}
