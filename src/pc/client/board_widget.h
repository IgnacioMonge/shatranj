#pragma once

#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTimer;

class BoardWidget final : public QWidget {
    Q_OBJECT

public:
    struct GameStatsView {
        QString result;
        QString names[2];
        QStringList metrics[2];  // TIME, AVG, LONGEST, AT, FASTEST, AT
        QString share[2];        // share of both clocks, empty when unknown
        QString material[2];     // value of the pieces each side captured
        QString captured[2];
        bool partial = false;
    };

    struct SelectionState {
        int selectedRow = -1;
        int selectedCol = -1;
        int targetRow = -1;
        int targetCol = -1;
        QStringList legalTargets;
    };

    explicit BoardWidget(QWidget *parent = nullptr);
    ~BoardWidget() override;

    void setPosition(const char board[8][8]);
    void setShowHints(bool showHints);
    SelectionState selection() const;
    void setSelection(int row, int col, const QStringList &legalTargets);
    void setTarget(int row, int col);
    void clearSelection();
    bool isLegalTarget(int row, int col) const;
    void refresh();
    void refreshChangedSquares(const char previous[8][8]);
    void refreshSelectionFootprint(const SelectionState &previous);

    bool whiteAtBottom() const;
    bool setWhiteAtBottom(bool whiteAtBottom);
    bool piecesVisible() const;
    bool hasPendingPieceFlash() const;
    void setPiecesVisible(bool visible);
    QPoint flipButtonGlobalPosition() const;

    void refreshFrameStyle();
    void setBotMode(bool bot);
    void animatePiecesIn();
    void animatePiecesMorph();
    void stopAnimations();
    void invalidatePieceFlash();
    void invalidateDestinationFeedback();
    void flashPieceAt(int row, int col, std::function<void(bool)> done);
    void showDestinationFeedback(int row, int col);
    void showGameStats(const GameStatsView &view);
    void hideGameStats();
    bool gameStatsVisible() const;

signals:
    void squareActivated(int row, int col);
    void flipRequested();
    void gameStatsVisibilityChanged(bool visible);

private:
    void invalidatePieceReveal();
    int boardRowForDisplay(int displayRow) const;
    int boardColForDisplay(int displayCol) const;
    int displayRowForBoard(int boardRow) const;
    int displayColForBoard(int boardCol) const;
    void refreshCoordinates(bool force = false);
    void setSquareStyle(int displayRow, int displayCol, const QString &style);
    void refreshSquareStyle(int row, int col);
    void refreshSquareVisual(int row, int col);
    void refreshSquareIcon(int row, int col, bool visible);
    void refreshSquareStyleByName(const QString &square);
    bool isFeedbackSquare(int row, int col) const;
    void revealSquarePair(int rowA, int colA, int rowB, int colB);
    void advancePieceReveal();
    void animatePiecePairs(int stepMs, int pauseMs, bool hideFirst);
    void advancePieceFlash();
    void advanceDestinationFeedback();

    QLabel *fileLabelsTop_[8] = {};
    QLabel *fileLabelsBottom_[8] = {};
    QLabel *rankLabelsLeft_[8] = {};
    QLabel *rankLabelsRight_[8] = {};
    QPushButton *squares_[8][8] = {};
    QPushButton *flipButton_ = nullptr;
    QWidget *statsPanel_ = nullptr;
    QTimer *revealTimer_ = nullptr;
    QTimer *flashTimer_ = nullptr;
    QTimer *feedbackTimer_ = nullptr;
    QString squareStyleCache_[8][8];
    char board_[8][8] = {};
    SelectionState selection_;
    QVector<QPoint> morphSquares_;
    std::function<void(bool)> flashDone_;
    int feedbackRow_ = -1;
    int feedbackCol_ = -1;
    int revealStep_ = 0;
    int revealStepMs_ = 0;
    int revealPauseMs_ = 0;
    int morphMid_ = 0;
    int flashStep_ = 0;
    int flashRow_ = -1;
    int flashCol_ = -1;
    int feedbackStep_ = 0;
    bool showHints_ = true;
    bool botMode_ = false;
    bool feedbackOn_ = false;
    bool revealIsMorph_ = false;
    bool coordinatesInitialized_ = false;
    bool lastCoordinateWhiteAtBottom_ = false;
    bool whiteAtBottom_ = false;
    bool piecesVisible_ = false;
};
