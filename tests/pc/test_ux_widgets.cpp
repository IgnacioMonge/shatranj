#include <QApplication>
#include <QByteArray>
#include <QDialog>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <cstring>

#include "pc/client/board_widget.h"
#include "pc/client/command_input_widget.h"
#include "pc/client/ui_theme.h"

namespace {

QPushButton *squareButton(BoardWidget &board, const QString &name)
{
    const QString accessible = QStringLiteral("Board square %1").arg(name);
    const auto buttons = board.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->accessibleName() == accessible) {
            return button;
        }
    }
    return nullptr;
}

bool boardProjectionTest()
{
    char position[8][8];
    std::memset(position, '.', sizeof(position));
    position[6][4] = 'P';
    char original[8][8];
    std::memcpy(original, position, sizeof(position));

    BoardWidget board;
    board.setPosition(position);
    board.setPiecesVisible(true);
    board.setSelection(6, 4, QStringList{QStringLiteral("e4")});
    board.setShowHints(true);
    board.refresh();

    QPushButton *target = squareButton(board, QStringLiteral("E4"));
    QPushButton *piece = squareButton(board, QStringLiteral("E2"));
    const bool pieceVisible = piece != nullptr && !piece->icon().isNull();
    const bool hinted = target != nullptr &&
                        target->styleSheet().contains(QStringLiteral("qradialgradient"));
    board.setShowHints(false);
    board.refresh();
    const bool hidden = target != nullptr &&
                        !target->styleSheet().contains(QStringLiteral("qradialgradient"));

    int activatedRow = -1;
    int activatedCol = -1;
    QObject::connect(&board, &BoardWidget::squareActivated,
                     [&activatedRow, &activatedCol](int row, int col) {
        activatedRow = row;
        activatedCol = col;
    });
    if (piece != nullptr) {
        piece->click();
    }
    const bool rejectedIntentPreserved = piece != nullptr && !piece->icon().isNull();
    bool flipRequested = false;
    QObject::connect(&board, &BoardWidget::flipRequested,
                     [&flipRequested]() { flipRequested = true; });
    const auto buttons = board.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->accessibleName() == QStringLiteral("Flip board")) {
            button->click();
            break;
        }
    }
    const bool flipIsIntentOnly = flipRequested && !board.whiteAtBottom();
    board.setPiecesVisible(false);
    board.refresh();
    const bool piecesHidden = piece != nullptr && piece->icon().isNull();
    char restored[8][8];
    std::memset(restored, '.', sizeof(restored));
    restored[4][4] = 'P';
    board.setPosition(restored);
    board.setPiecesVisible(true);
    board.refresh();
    QPushButton *restoredPiece = squareButton(board, QStringLiteral("E4"));
    const bool positionRestored = restoredPiece != nullptr &&
                                  !restoredPiece->icon().isNull();
    board.setWhiteAtBottom(true);
    board.refresh();
    QPushButton *a8 = squareButton(board, QStringLiteral("A8"));
    if (a8 != nullptr) {
        a8->click();
    }
    board.setTarget(4, 4);
    const BoardWidget::SelectionState targeted = board.selection();
    const bool selectionOwned = targeted.selectedRow == 6 &&
                                targeted.selectedCol == 4 &&
                                targeted.targetRow == 4 &&
                                targeted.targetCol == 4 &&
                                targeted.legalTargets ==
                                    QStringList{QStringLiteral("e4")};
    board.clearSelection();
    const BoardWidget::SelectionState cleared = board.selection();
    const bool selectionCleared = cleared.selectedRow < 0 &&
                                  cleared.targetRow < 0 &&
                                  cleared.legalTargets.isEmpty();
    return hinted && hidden && pieceVisible && rejectedIntentPreserved &&
           flipIsIntentOnly && piecesHidden && positionRestored &&
           selectionOwned && selectionCleared &&
           activatedRow == 0 && activatedCol == 0 &&
           std::memcmp(position, original, sizeof(position)) == 0;
}

bool inputInteractionTest()
{
    CommandInputWidget input(64);
    int submissions = 0;
    QObject::connect(&input, &CommandInputWidget::submitRequested,
                     [&submissions]() { ++submissions; });

    input.setText(QStringLiteral("first"));
    input.rememberSubmission(input.text());
    input.setText(QStringLiteral("second"));
    input.rememberSubmission(input.text());
    input.clear();

    QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
    QApplication::sendEvent(&input, &up);
    const bool newest = input.text() == QStringLiteral("second") &&
                        input.hasSelectedText();
    QApplication::sendEvent(&input, &up);
    const bool oldest = input.text() == QStringLiteral("first");
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(&input, &down);
    QApplication::sendEvent(&input, &down);
    const bool blank = input.text().isEmpty();

    input.setText(QStringLiteral("submit"));
    input.setCursorPosition(2);
    input.setSubmissionEnabled(false);
    const bool editPreserved = input.text() == QStringLiteral("submit") &&
                               input.cursorPosition() == 2;
    QKeyEvent disabledReturn(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&input, &disabledReturn);
    const bool disabled = submissions == 0;
    input.setSubmissionEnabled(true);
    QKeyEvent controlReturn(QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier);
    QApplication::sendEvent(&input, &controlReturn);
    return newest && oldest && blank && editPreserved && disabled && submissions == 1;
}

bool stopAnimationsTest()
{
    char position[8][8];
    std::memset(position, '.', sizeof(position));
    position[6][4] = 'P';
    BoardWidget board;
    board.setPosition(position);
    board.setPiecesVisible(true);
    board.animatePiecesMorph();
    int cancellations = 0;
    board.flashPieceAt(6, 4, [&](bool cancelled) {
        if (cancelled) {
            ++cancellations;
        }
    });
    board.showDestinationFeedback(4, 4);
    board.stopAnimations();
    board.stopAnimations();
    for (QTimer *timer : board.findChildren<QTimer *>()) {
        if (timer->isActive()) {
            return false;
        }
    }
    return cancellations == 1;
}

// The frame's style sheet must reach the screen: its outer edge, and the
// engine's violet while Stockfish plays.
bool boardFrameTest()
{
    BoardWidget board;
    board.show();
    QApplication::processEvents();
    const auto edge = [&board]() {
        const QImage image = board.grab().toImage();
        return image.pixelColor(0, image.height() / 2);
    };
    const bool framed = edge() == QColor(QStringLiteral(SHZ_BOARD_EDGE));
    board.setBotMode(true);
    const bool violet = edge() == QColor(QStringLiteral(SHZ_BOT));
    return framed && violet;
}

bool gameStatsOverlayTest()
{
    BoardWidget::GameStatsView view;
    view.result = QStringLiteral("Agreed draw");
    view.names[0] = QStringLiteral("NACHO");
    view.names[1] = QStringLiteral("PEPE");
    view.partial = true;
    view.metrics[0] = view.metrics[1] = QStringList{
        QStringLiteral("15:00:00"), QStringLiteral("1:00:00"),
        QStringLiteral("1:00:00"), QStringLiteral("1"),
        QStringLiteral("0:01"), QStringLiteral("255+")
    };
    view.share[0] = QStringLiteral("50%");
    view.share[1] = QStringLiteral("50%");
    view.material[0] = view.material[1] = QStringLiteral("39");
    view.captured[0] = QStringLiteral("qrrbbnnpppppppp");
    view.captured[1] = QStringLiteral("QRRBBNNPPPPPPPP");
    BoardWidget board;
    board.show();
    board.showGameStats(view);
    QApplication::processEvents();
    auto *panel = board.findChild<QWidget *>(QStringLiteral("gameStatsPanel"));
    if (!board.gameStatsVisible() || panel == nullptr || panel->size() != QSize(416, 416)) return false;
    QLabel *headers[2] = {};
    for (QLabel *label : panel->findChildren<QLabel *>()) {
        if (label->text().startsWith(QStringLiteral("\u25cb "))) headers[0] = label;
        if (label->text().startsWith(QStringLiteral("\u25cf "))) headers[1] = label;
    }
    if (headers[0] == nullptr || headers[1] == nullptr) return false;
    int captures = 0;
    for (QLabel *label : panel->findChildren<QLabel *>()) {
        if (label->accessibleName().startsWith(QStringLiteral("Captured "))) {
            ++captures;
            const QRect icon(label->mapTo(panel, QPoint()), label->size());
            if (!panel->rect().contains(icon)) return false;
            // Each side's pieces sit under its own header: White took the
            // lower-case (black) pieces.
            QLabel *header = headers[label->accessibleName().back().isLower() ? 0 : 1];
            const QRect column(header->mapTo(panel, QPoint()), header->size());
            if (icon.left() < column.left() || icon.right() > column.right()) return false;
        }
    }
    auto *total = panel->findChild<QLabel *>(QStringLiteral("gameStatsTIME0"));
    auto *fastAt = panel->findChild<QLabel *>(QStringLiteral("gameStatsFASTAT1"));
    auto *share = panel->findChild<QLabel *>(QStringLiteral("gameStatsShare0"));
    auto *material = panel->findChild<QLabel *>(QStringLiteral("gameStatsMaterial1"));
    if (captures != 30 || total == nullptr || total->text() != QStringLiteral("15:00:00") ||
        fastAt == nullptr || fastAt->text() != QStringLiteral("255+") ||
        share == nullptr || share->text() != QStringLiteral("50%") ||
        material == nullptr || material->text() != QStringLiteral("39") ||
        !panel->findChildren<QPushButton *>().isEmpty()) return false;
    for (QWidget *child : panel->findChildren<QWidget *>()) {
        if (child->isVisible() &&
            !panel->rect().contains(QRect(child->mapTo(panel, QPoint()), child->size()))) return false;
    }
    // The page has no button of its own and no key closes it: the owner's
    // toggle hides it and hears about every change.
    int shown = 0, hidden = 0;
    QObject::connect(&board, &BoardWidget::gameStatsVisibilityChanged,
                     [&shown, &hidden](bool visible) { ++(visible ? shown : hidden); });
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&board, &key);
    if (!board.gameStatsVisible()) return false;
    board.hideGameStats();
    board.hideGameStats();
    if (board.gameStatsVisible() || hidden != 1) return false;
    board.showGameStats(view);
    return board.gameStatsVisible() && shown == 1;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);
    return boardProjectionTest() && inputInteractionTest() && stopAnimationsTest() &&
           boardFrameTest() && gameStatsOverlayTest() ? 0 : 1;
}
