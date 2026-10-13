#include "board_widget.h"

#include <QGridLayout>
#include <QApplication>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <cstring>
#include <utility>

#include "piece_renderer.h"
#include "ui_theme.h"

namespace {

constexpr int kBoardSquareSize = 52;
constexpr int kBoardCoordSize = 24;
constexpr int kPieceIconSize = 50;
constexpr int kSpectrumFrameMs = 20;
constexpr int kPieceRevealStepMs = 5 * kSpectrumFrameMs;
constexpr int kPieceRevealMiddlePauseMs = 3 * kSpectrumFrameMs;
constexpr int kPieceMorphStepMs = kSpectrumFrameMs;
constexpr int kPieceMorphMiddlePauseMs = kSpectrumFrameMs;
constexpr int kPieceFlashStepMs = 90;

QString boardFrameStyle(bool bot)
{
    return QStringLiteral(
        "QWidget#boardFrame { background:%1;"
        " border:%2; border-radius:" SHZ_R_LG "; }")
        .arg(PieceRenderer::boardWellColor(kBoardSquareSize).name(),
             bot ? QStringLiteral("3px solid " SHZ_BOT)
                 : QStringLiteral("1px solid " SHZ_BOARD_EDGE));
}

PieceRenderer::SquareCorner displayCorner(int row, int col)
{
    if (row == 0 && col == 0) return PieceRenderer::CornerTopLeft;
    if (row == 0 && col == 7) return PieceRenderer::CornerTopRight;
    if (row == 7 && col == 0) return PieceRenderer::CornerBottomLeft;
    if (row == 7 && col == 7) return PieceRenderer::CornerBottomRight;
    return PieceRenderer::CornerNone;
}

QString squareName(int row, int col)
{
    return QString(QChar('a' + col)) + QChar('8' - row);
}

QLabel *coordLabel(QWidget *parent, const QSize &size)
{
    auto *label = new QLabel(parent);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(size);
    label->setStyleSheet(
        "QLabel { color:" SHZ_TEXT "; background:transparent;"
        " font-weight:700; font-size:11px; letter-spacing:1px;"
        " padding:0; margin:0; }");
    return label;
}

QString squareStyle(int row, int col, bool selected, bool target,
                    bool legalTarget, bool feedback, bool hasPiece,
                    bool showHints, PieceRenderer::SquareCorner corner,
                    bool bot = false)
{
    const bool light = ((row + col) % 2) == 0;
    const bool texturedBoard = !PieceRenderer::boardTexture().isEmpty();
    QString background;
    QString border;

    if (feedback) {
        background = bot ? SHZ_BOT_HIT : SHZ_SQ_HIT;
        border = bot ? "4px solid " SHZ_BOT_HIT_EDGE : "4px solid " SHZ_SQ_HIT_EDGE;
    } else if (selected) {
        background = SHZ_SQ_SEL;
        border = "3px solid " SHZ_SQ_SEL_EDGE;
    } else if (target) {
        if (legalTarget && showHints && !hasPiece && !texturedBoard) {
            const QString dotColor = QStringLiteral("rgba(0, 0, 0, 0.28)");
            background = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 " SHZ_SQ_TGT ", stop:1.0 " SHZ_SQ_TGT ")").arg(dotColor);
        } else {
            background = SHZ_SQ_TGT;
        }
        border = "3px solid " SHZ_SQ_TGT_EDGE;
    } else if (legalTarget && showHints) {
        if (hasPiece) {
            background = light ? SHZ_SQ_HINT_L : SHZ_SQ_HINT_D;
            border = "2px solid " SHZ_SQ_HINT_EDGE;
        } else {
            const QString baseBackground = light ? SHZ_SQ_LIGHT : SHZ_SQ_DARK;
            const QString dotColor = QStringLiteral("rgba(0, 0, 0, 0.25)");
            background = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 %2, stop:1.0 %2)").arg(dotColor, baseBackground);
            border = "1px solid " SHZ_SQ_EDGE;
        }
    } else {
        background = light ? SHZ_SQ_LIGHT : SHZ_SQ_DARK;
        border = "1px solid " SHZ_SQ_EDGE;
    }

    const QString foreground = light || selected || target || feedback
        ? QStringLiteral("#1e1e1e") : QStringLiteral("#ffffff");
    return PieceRenderer::boardSquareStyle(row, col, kBoardSquareSize,
                                           background, foreground, border, corner);
}

} // namespace

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
    std::memset(board_, '.', sizeof(board_));
    setObjectName(QStringLiteral("boardFrame"));
    // Style sheets paint the frame (edge, well, engine violet) only on plain
    // QWidgets; a subclass has to ask for it.
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2,
                 kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2);
    setStyleSheet(boardFrameStyle(false));

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    for (int col = 0; col < 8; ++col) {
        fileLabelsTop_[col] = coordLabel(this, QSize(kBoardSquareSize, kBoardCoordSize));
        fileLabelsBottom_[col] = coordLabel(this, QSize(kBoardSquareSize, kBoardCoordSize));
        layout->addWidget(fileLabelsTop_[col], 0, col + 1, Qt::AlignCenter);
        layout->addWidget(fileLabelsBottom_[col], 9, col + 1, Qt::AlignCenter);
    }
    for (int row = 0; row < 8; ++row) {
        rankLabelsLeft_[row] = coordLabel(this, QSize(kBoardCoordSize, kBoardSquareSize));
        rankLabelsRight_[row] = coordLabel(this, QSize(kBoardCoordSize, kBoardSquareSize));
        layout->addWidget(rankLabelsLeft_[row], row + 1, 0, Qt::AlignCenter);
        layout->addWidget(rankLabelsRight_[row], row + 1, 9, Qt::AlignCenter);
    }
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            auto *button = new QPushButton(this);
            button->setFixedSize(kBoardSquareSize, kBoardSquareSize);
            button->setIconSize(QSize(kBoardSquareSize, kBoardSquareSize));
            button->setFocusPolicy(Qt::StrongFocus);
            connect(button, &QPushButton::clicked, this, [this, row, col]() {
                emit squareActivated(boardRowForDisplay(row), boardColForDisplay(col));
            });
            squares_[row][col] = button;
            setSquareStyle(row, col,
                           squareStyle(row, col, false, false, false, false,
                                       false, true, displayCorner(row, col)));
            layout->addWidget(button, row + 1, col + 1);
        }
    }

    flipButton_ = new QPushButton(QString(QChar(0x21bb)), this);
    flipButton_->setAccessibleName(QStringLiteral("Flip board"));
    flipButton_->setToolTip(QStringLiteral("Flip board"));
    flipButton_->setFixedSize(kBoardCoordSize - 4, kBoardCoordSize - 4);
    flipButton_->setFocusPolicy(Qt::StrongFocus);
    flipButton_->setGeometry(2, height() - kBoardCoordSize + 2,
                             kBoardCoordSize - 4, kBoardCoordSize - 4);
    flipButton_->setStyleSheet(
        "QPushButton { background:" SHZ_QUIET "; color:" SHZ_ACCENT ";"
        " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
        " font-weight:700; font-size:11px; padding:0; min-height:0; }"
        "QPushButton:hover { background:" SHZ_SURFACE_ALT ";"
        " border-color:" SHZ_ACCENT "; color:" SHZ_TEXT "; }");
    connect(flipButton_, &QPushButton::clicked, this, &BoardWidget::flipRequested);
    flipButton_->raise();

    revealTimer_ = new QTimer(this);
    revealTimer_->setSingleShot(true);
    connect(revealTimer_, &QTimer::timeout, this, &BoardWidget::advancePieceReveal);
    flashTimer_ = new QTimer(this);
    flashTimer_->setSingleShot(true);
    connect(flashTimer_, &QTimer::timeout, this, &BoardWidget::advancePieceFlash);
    feedbackTimer_ = new QTimer(this);
    feedbackTimer_->setSingleShot(true);
    connect(feedbackTimer_, &QTimer::timeout, this, &BoardWidget::advanceDestinationFeedback);

    refreshCoordinates(true);
}

BoardWidget::~BoardWidget()
{
    qApp->removeEventFilter(this);
    revealTimer_->stop();
    flashTimer_->stop();
    feedbackTimer_->stop();
    flashDone_ = {};
}

void BoardWidget::showGameStats(const GameStatsView &view)
{
    hideGameStats();
    delete statsPanel_;
    statsPanel_ = new QWidget(this);
    statsPanel_->setObjectName(QStringLiteral("gameStatsPanel"));
    statsPanel_->setGeometry(kBoardCoordSize + 1, kBoardCoordSize + 1,
                             kBoardSquareSize * 8, kBoardSquareSize * 8);
    statsPanel_->setFixedSize(kBoardSquareSize * 8, kBoardSquareSize * 8);
    statsPanel_->setStyleSheet(
        "QWidget#gameStatsPanel { background:" SHZ_CARD "; }"
        "QWidget#gameStatsPanel QLabel { background:transparent; color:" SHZ_TEXT
        "; font-size:12px; padding:0; }"
        "QWidget#gameStatsPanel QLabel[statsCaption=\"true\"] { color:" SHZ_ACCENT
        "; font-weight:700; }"
        "QWidget#gameStatsPanel QLabel#gameStatsResult { background:" SHZ_ACCENT
        "; color:" SHZ_INK "; font-weight:700; padding:4px; }"
        "QWidget#gameStatsPanel QLabel#gameStatsTitle { font-size:18px; }"
        "QWidget#gameStatsPanel QPushButton { font-size:12px; padding:4px; min-height:0; }");
    auto *layout = new QVBoxLayout(statsPanel_);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(5);
    const auto label = [this](const QString &text, bool caption = false) {
        auto *item = new QLabel(text, statsPanel_);
        item->setTextFormat(Qt::PlainText);
        item->setAlignment(Qt::AlignCenter);
        item->setProperty("statsCaption", caption);
        return item;
    };
    auto *title = label(QStringLiteral("GAME STATS"), true);
    title->setObjectName(QStringLiteral("gameStatsTitle"));
    layout->addWidget(title);
    auto *resultLabel = label(view.result);
    resultLabel->setObjectName(QStringLiteral("gameStatsResult"));
    resultLabel->setWordWrap(true);
    layout->addWidget(resultLabel);
    if (view.partial) {
        layout->addWidget(label(QStringLiteral("PARTIAL - since load"), true));
    }
    auto *table = new QGridLayout;
    table->setHorizontalSpacing(12);
    table->setVerticalSpacing(3);
    table->addWidget(label(QStringLiteral("\u25cb ") + view.names[0]), 0, 1);
    table->addWidget(label(QStringLiteral("\u25cf ") + view.names[1]), 0, 2);
    // Same rows as the Spectrum page; the share follows TIME.
    const QStringList captions = {QStringLiteral("TIME"), QStringLiteral("AVG"),
                                  QStringLiteral("MAX"), QStringLiteral("AT"),
                                  QStringLiteral("MIN"), QStringLiteral("AT")};
    const QStringList ids = {QStringLiteral("TIME"), QStringLiteral("AVG"),
                             QStringLiteral("LONGEST"), QStringLiteral("AT"),
                             QStringLiteral("FASTEST"), QStringLiteral("FASTAT")};
    for (int row = 0; row <= captions.size(); ++row) {
        auto *caption = label(row < captions.size() ? captions[row]
                                                    : QStringLiteral("CAPTURED"), true);
        caption->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->addWidget(caption, row + 1, 0);
    }
    for (int side = 0; side < 2; ++side) {
        const QStringList &values = view.metrics[side];
        for (int row = 0; row < values.size() && row < ids.size(); ++row) {
            auto *value = label(values[row]);
            value->setObjectName(QStringLiteral("gameStats%1%2").arg(ids[row]).arg(side));
            if (row == 0 && !view.share[side].isEmpty()) {
                auto *cell = new QHBoxLayout;
                cell->setSpacing(8);
                auto *share = label(view.share[side]);
                share->setObjectName(QStringLiteral("gameStatsShare%1").arg(side));
                cell->addWidget(value);
                cell->addWidget(share);
                table->addLayout(cell, row + 1, side + 1);
            } else {
                table->addWidget(value, row + 1, side + 1);
            }
        }
        auto *material = label(view.material[side].isEmpty() ? QStringLiteral("0")
                                                            : view.material[side], true);
        material->setObjectName(QStringLiteral("gameStatsMaterial%1").arg(side));
        table->addWidget(material, captions.size() + 1, side + 1);
        // The pieces sit in the side's own column, under its header.
        auto *captures = new QGridLayout;
        for (int col = 0; col < 5; ++col) captures->setColumnStretch(col, 1);
        int index = 0;
        for (QChar piece : view.captured[side]) {
            auto *icon = label(QString());
            icon->setPixmap(PieceRenderer::pieceIcon(piece.toLatin1()).pixmap(24, 24));
            icon->setAccessibleName(QStringLiteral("Captured %1").arg(piece));
            captures->addWidget(icon, index / 5, index % 5);
            ++index;
        }
        captures->setHorizontalSpacing(4);
        captures->setVerticalSpacing(2);
        table->addLayout(captures, captions.size() + 2, side + 1);
    }
    layout->addLayout(table);
    layout->addStretch();
    statsPanel_->show();
    statsPanel_->raise();
    emit gameStatsVisibilityChanged(true);
}

void BoardWidget::hideGameStats()
{
    if (gameStatsVisible()) {
        statsPanel_->hide();
        emit gameStatsVisibilityChanged(false);
    }
}

bool BoardWidget::gameStatsVisible() const
{
    return statsPanel_ != nullptr && !statsPanel_->isHidden();
}

void BoardWidget::setPosition(const char board[8][8])
{
    std::memcpy(board_, board, sizeof(board_));
}

void BoardWidget::setShowHints(bool showHints)
{
    showHints_ = showHints;
}

BoardWidget::SelectionState BoardWidget::selection() const
{
    return selection_;
}

void BoardWidget::setSelection(int row, int col, const QStringList &legalTargets)
{
    selection_.selectedRow = row;
    selection_.selectedCol = col;
    selection_.targetRow = -1;
    selection_.targetCol = -1;
    selection_.legalTargets = legalTargets;
}

void BoardWidget::setTarget(int row, int col)
{
    selection_.targetRow = row;
    selection_.targetCol = col;
}

void BoardWidget::clearSelection()
{
    selection_ = SelectionState{};
}

void BoardWidget::refresh()
{
    refreshCoordinates();
    for (int displayRow = 0; displayRow < 8; ++displayRow) {
        for (int displayCol = 0; displayCol < 8; ++displayCol) {
            const int row = boardRowForDisplay(displayRow);
            const int col = boardColForDisplay(displayCol);
            squares_[displayRow][displayCol]->setAccessibleName(
                QStringLiteral("Board square %1").arg(squareName(row, col).toUpper()));
            squares_[displayRow][displayCol]->setText(QString());
            refreshSquareVisual(row, col);
        }
    }
}

void BoardWidget::refreshChangedSquares(const char previous[8][8])
{
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (previous[row][col] != board_[row][col]) {
                refreshSquareVisual(row, col);
            }
        }
    }
}

void BoardWidget::refreshSelectionFootprint(const SelectionState &previous)
{
    for (const QString &target : previous.legalTargets) {
        refreshSquareStyleByName(target);
    }
    refreshSquareVisual(previous.selectedRow, previous.selectedCol);
    refreshSquareVisual(previous.targetRow, previous.targetCol);
    for (const QString &target : selection_.legalTargets) {
        refreshSquareStyleByName(target);
    }
    refreshSquareVisual(selection_.selectedRow, selection_.selectedCol);
    refreshSquareVisual(selection_.targetRow, selection_.targetCol);
}

bool BoardWidget::whiteAtBottom() const
{
    return whiteAtBottom_;
}

bool BoardWidget::setWhiteAtBottom(bool whiteAtBottom)
{
    if (whiteAtBottom_ == whiteAtBottom) {
        return false;
    }
    whiteAtBottom_ = whiteAtBottom;
    coordinatesInitialized_ = false;
    return true;
}

bool BoardWidget::piecesVisible() const
{
    return piecesVisible_;
}

bool BoardWidget::hasPendingPieceFlash() const
{
    return static_cast<bool>(flashDone_);
}

void BoardWidget::setPiecesVisible(bool visible)
{
    piecesVisible_ = visible;
}

QPoint BoardWidget::flipButtonGlobalPosition() const
{
    return flipButton_->mapToGlobal(QPoint(0, 0));
}

void BoardWidget::refreshFrameStyle()
{
    setStyleSheet(boardFrameStyle(botMode_));
    style()->unpolish(this);
    style()->polish(this);
    update();
}

int BoardWidget::boardRowForDisplay(int row) const
{
    return whiteAtBottom_ ? row : 7 - row;
}

int BoardWidget::boardColForDisplay(int col) const
{
    return whiteAtBottom_ ? col : 7 - col;
}

int BoardWidget::displayRowForBoard(int row) const
{
    return whiteAtBottom_ ? row : 7 - row;
}

int BoardWidget::displayColForBoard(int col) const
{
    return whiteAtBottom_ ? col : 7 - col;
}

void BoardWidget::refreshCoordinates(bool force)
{
    if (!force && coordinatesInitialized_ &&
        lastCoordinateWhiteAtBottom_ == whiteAtBottom_) {
        return;
    }
    coordinatesInitialized_ = true;
    lastCoordinateWhiteAtBottom_ = whiteAtBottom_;
    for (int col = 0; col < 8; ++col) {
        const QString file(QChar('A' + boardColForDisplay(col)));
        fileLabelsTop_[col]->setText(file);
        fileLabelsBottom_[col]->setText(file);
    }
    for (int row = 0; row < 8; ++row) {
        const QString rank(QChar('8' - boardRowForDisplay(row)));
        rankLabelsLeft_[row]->setText(rank);
        rankLabelsRight_[row]->setText(rank);
    }
}

bool BoardWidget::isLegalTarget(int row, int col) const
{
    return selection_.legalTargets.contains(squareName(row, col));
}

bool BoardWidget::isFeedbackSquare(int row, int col) const
{
    return feedbackOn_ && row == feedbackRow_ && col == feedbackCol_;
}

void BoardWidget::setSquareStyle(int row, int col, const QString &styleText)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8 ||
        squareStyleCache_[row][col] == styleText) {
        return;
    }
    squareStyleCache_[row][col] = styleText;
    squares_[row][col]->setStyleSheet(styleText);
}

void BoardWidget::refreshSquareStyle(int row, int col)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8) {
        return;
    }
    const int displayRow = displayRowForBoard(row);
    const int displayCol = displayColForBoard(col);
    const bool selected = row == selection_.selectedRow && col == selection_.selectedCol;
    const bool target = row == selection_.targetRow && col == selection_.targetCol;
    const bool legal = isLegalTarget(row, col);
    const bool feedback = isFeedbackSquare(row, col);
    const bool hasPiece = board_[row][col] != '.';
    setSquareStyle(displayRow, displayCol,
                   squareStyle(row, col, selected, target, legal, feedback,
                               hasPiece, showHints_, displayCorner(displayRow, displayCol),
                               botMode_));
}

void BoardWidget::refreshSquareVisual(int row, int col)
{
    refreshSquareStyle(row, col);
    refreshSquareIcon(row, col, true);
}

void BoardWidget::refreshSquareIcon(int row, int col, bool visible)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8) {
        return;
    }
    const int displayRow = displayRowForBoard(row);
    const int displayCol = displayColForBoard(col);
    const bool selected = row == selection_.selectedRow && col == selection_.selectedCol;
    const bool target = row == selection_.targetRow && col == selection_.targetCol;
    const bool legal = isLegalTarget(row, col);
    const bool feedback = isFeedbackSquare(row, col);
    const bool hasPiece = board_[row][col] != '.';
    const bool styledBackground = selected || feedback || (target && hasPiece) ||
                                  (legal && showHints_ && hasPiece);
    const bool textured = !PieceRenderer::boardTexture().isEmpty() && !styledBackground;
    squares_[displayRow][displayCol]->setIconSize(
        QSize(textured ? kBoardSquareSize : kPieceIconSize,
              textured ? kBoardSquareSize : kPieceIconSize));
    squares_[displayRow][displayCol]->setIcon(
        textured ? PieceRenderer::boardSquareIcon(
                       board_[row][col], row, col, kBoardSquareSize, kPieceIconSize,
                       piecesVisible_ && visible, legal && showHints_ && !hasPiece,
                       target && !hasPiece, displayCorner(displayRow, displayCol))
                 : (piecesVisible_ && visible
                        ? PieceRenderer::pieceIcon(board_[row][col]) : QIcon()));
}

void BoardWidget::invalidatePieceReveal()
{
    revealTimer_->stop();
    morphSquares_.clear();
}

void BoardWidget::stopAnimations()
{
    invalidatePieceFlash();
    invalidatePieceReveal();
    invalidateDestinationFeedback();
}

void BoardWidget::invalidatePieceFlash()
{
    flashTimer_->stop();
    auto done = std::move(flashDone_);
    flashDone_ = {};
    if (done) {
        done(true);
    }
}

void BoardWidget::invalidateDestinationFeedback()
{
    feedbackTimer_->stop();
}

void BoardWidget::revealSquarePair(int rowA, int colA, int rowB, int colB)
{
    refreshSquareIcon(rowA, colA, true);
    refreshSquareIcon(rowB, colB, true);
}

void BoardWidget::advancePieceReveal()
{
    if (revealIsMorph_) {
        if (revealStep_ >= morphSquares_.size()) {
            revealTimer_->stop();
            return;
        }
        const QPoint square = morphSquares_.at(revealStep_);
        refreshSquareIcon(square.x(), square.y(), true);
        ++revealStep_;
        if (revealStep_ >= morphSquares_.size()) {
            revealTimer_->stop();
            return;
        }
        int nextMs = kPieceMorphStepMs;
        if (revealStep_ == morphMid_) {
            nextMs += kPieceMorphMiddlePauseMs;
        }
        revealTimer_->start(nextMs);
        return;
    }
    if (revealStep_ >= 16) {
        revealTimer_->stop();
        return;
    }
    const int index = revealStep_ < 8 ? revealStep_ : revealStep_ - 8;
    if (revealStep_ < 8) {
        revealSquarePair(0, index, 7, 7 - index);
    } else {
        revealSquarePair(1, 7 - index, 6, index);
    }
    ++revealStep_;
    if (revealStep_ >= 16) {
        revealTimer_->stop();
        return;
    }
    int nextMs = revealStepMs_;
    if (revealStep_ == 8) {
        nextMs += revealPauseMs_;
    }
    revealTimer_->start(nextMs);
}

void BoardWidget::animatePiecePairs(int stepMs, int pauseMs, bool hideFirst)
{
    invalidatePieceReveal();
    revealIsMorph_ = false;
    revealStep_ = 0;
    revealStepMs_ = stepMs;
    revealPauseMs_ = pauseMs;
    piecesVisible_ = true;
    if (hideFirst) {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                refreshSquareIcon(row, col, false);
            }
        }
    }
    advancePieceReveal();
}

void BoardWidget::animatePiecesIn()
{
    animatePiecePairs(kPieceRevealStepMs, kPieceRevealMiddlePauseMs, true);
}

void BoardWidget::animatePiecesMorph()
{
    if (!piecesVisible_) {
        animatePiecesIn();
        return;
    }
    invalidatePieceReveal();
    revealIsMorph_ = true;
    revealStep_ = 0;
    morphSquares_.clear();
    morphSquares_.reserve(32);
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (board_[row][col] != '.') {
                morphSquares_.append(QPoint(row, col));
            } else {
                refreshSquareIcon(row, col, true);
            }
        }
    }
    morphMid_ = morphSquares_.size() / 2;
    if (!morphSquares_.isEmpty()) {
        advancePieceReveal();
    }
}

void BoardWidget::advancePieceFlash()
{
    ++flashStep_;
    if (flashRow_ < 0 || flashCol_ < 0 || flashStep_ > 5) {
        return;
    }
    refreshSquareIcon(flashRow_, flashCol_, (flashStep_ % 2) != 0);
    if (flashStep_ < 5) {
        flashTimer_->start(kPieceFlashStepMs);
        return;
    }
    refreshSquareIcon(flashRow_, flashCol_, true);
    auto done = std::move(flashDone_);
    flashDone_ = {};
    if (done) {
        done(false);
    }
}

void BoardWidget::flashPieceAt(int row, int col, std::function<void(bool)> done)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8 ||
        board_[row][col] == '.' || !piecesVisible_) {
        if (done) {
            done(false);
        }
        return;
    }
    invalidatePieceFlash();
    flashRow_ = row;
    flashCol_ = col;
    flashStep_ = 0;
    flashDone_ = std::move(done);
    refreshSquareIcon(row, col, false);
    flashTimer_->start(kPieceFlashStepMs);
}

void BoardWidget::refreshSquareStyleByName(const QString &square)
{
    if (square.size() >= 2) {
        refreshSquareVisual('8' - square[1].unicode(), square[0].unicode() - 'a');
    }
}

void BoardWidget::advanceDestinationFeedback()
{
    ++feedbackStep_;
    feedbackOn_ = (feedbackStep_ % 2) == 0;
    refreshSquareVisual(feedbackRow_, feedbackCol_);
    if (feedbackStep_ < 5) {
        feedbackTimer_->start(kPieceFlashStepMs);
        return;
    }
    const int row = feedbackRow_;
    const int col = feedbackCol_;
    feedbackRow_ = -1;
    feedbackCol_ = -1;
    feedbackOn_ = false;
    refreshSquareVisual(row, col);
}

void BoardWidget::setBotMode(bool bot)
{
    if (botMode_ == bot) {
        return;
    }
    botMode_ = bot;
    refreshFrameStyle();
}

void BoardWidget::showDestinationFeedback(int row, int col)
{
    invalidateDestinationFeedback();
    feedbackRow_ = row;
    feedbackCol_ = col;
    feedbackOn_ = true;
    feedbackStep_ = 0;
    refreshSquareVisual(row, col);
    feedbackTimer_->start(kPieceFlashStepMs);
}
