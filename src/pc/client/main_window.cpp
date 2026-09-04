#include <QAbstractSocket>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSettings>
#include <QSet>
#include <QShowEvent>
#include <QSize>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QTableWidget>
#include <QUrl>
#include <QVector>
#include <QStyle>
#include <QTableWidgetItem>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QToolButton>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>
#include <QtGlobal>

#include <cstring>
#include <functional>

#include "main_window.h"

extern "C" {
#include "common/chess/position.h"
#include "common/protocol/platform_protocol.h"
#include "common/savegame/savegame_wire.h"
#include "common/chess/rules_compact.h"
#include "common/session/session.h"
}
#include "common/chess/legal.h"
#include "common/ui_messages.h"
#include "app_banner.h"
#include "chess_helpers.h"
#include "desktop_session_controller.h"
#include "desktop_transport_codec.h"
#include "piece_renderer.h"
#include "window_chrome.h"
#include "save_game_store.h"
#include "ui_theme.h"

namespace {
using namespace PieceRenderer;

constexpr int kBoardSquareSize = 52;
constexpr int kBoardCoordSize = 24;
constexpr int kPieceIconSize = 50;
constexpr int kActionButtonWidth = 100;
constexpr int kSidePanelWidth = 340;
constexpr int kChatTextMax = SESSION_CHAT_TEXT_MAX;
constexpr int kSpectrumFrameMs = 20;
constexpr int kPieceRevealStepMs = 5 * kSpectrumFrameMs;
constexpr int kPieceRevealMiddlePauseMs = 3 * kSpectrumFrameMs;
constexpr int kPieceMorphStepMs = 1 * kSpectrumFrameMs;
constexpr int kPieceMorphMiddlePauseMs = 1 * kSpectrumFrameMs;
constexpr int kPieceFlashStepMs = 90;
// A TCP listener may exist before the remote app is ready for its DIRECT handshake.
constexpr const char *kAppVersion = NETCHESSZX_APP_VERSION;
constexpr const char *kDirectHostBusyStatus = "Host busy";
constexpr qint64 kUiStallWarnMs = 2500;
constexpr qint64 kMoveSendWarnMs = 250;
constexpr qint64 kMaxClockSeconds = 359999; // 99h59m59s ceiling for save-slot elapsed clock
constexpr uint8_t kMqttLinkId = 1u;
constexpr int kMqttBufferedPublishMax = 8;

struct MqttBufferedPublish {
    QString suffix;
    QByteArray topic;
    QByteArray payload;
    bool retained = false;
};

static QIcon directIpHistoryIcon()
{
    QPixmap pixmap(16, 16);
    const QColor color(QStringLiteral(SHZ_ACCENT));

    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(2, 3, 8, 2, color);
    painter.fillRect(2, 7, 8, 2, color);
    painter.fillRect(2, 11, 8, 2, color);
    painter.fillRect(11, 6, 3, 2, color);
    painter.fillRect(12, 8, 1, 2, color);
    return QIcon(pixmap);
}

static QIcon sessionActionIcon(const char *kind)
{
    QPixmap pixmap(16, 16);
    const QColor color(QStringLiteral(SHZ_ACCENT));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    if (std::strcmp(kind, "takeback") == 0) {
        painter.drawLine(QPointF(12.5, 4.5), QPointF(12.5, 10.5));
        painter.drawLine(QPointF(12.5, 10.5), QPointF(4.5, 10.5));
        QPolygonF head;
        head << QPointF(4.5, 10.5) << QPointF(8.0, 7.5) << QPointF(8.0, 13.5);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
    } else if (std::strcmp(kind, "reset") == 0) {
        QRectF arc(3.0, 3.0, 10.0, 10.0);
        painter.drawArc(arc, 50 * 16, 260 * 16);
        QPolygonF head;
        head << QPointF(11.5, 3.0) << QPointF(15.0, 6.5) << QPointF(9.5, 6.5);
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
    } else {
        painter.drawLine(QPointF(3.5, 6.0), QPointF(12.5, 6.0));
        painter.drawLine(QPointF(3.5, 10.0), QPointF(12.5, 10.0));
    }
    return QIcon(pixmap);
}

static uint8_t localMachinePlatform()
{
#if defined(Q_OS_MACOS)
    return NETCHESS_PLAT_MAC;
#elif defined(Q_OS_LINUX)
    return NETCHESS_PLAT_LNX;
#elif defined(Q_OS_WIN)
    return NETCHESS_PLAT_PC;
#else
    return NETCHESS_PLAT_UNKNOWN;
#endif
}

static QByteArray localMachPayload()
{
    char text[16];

    if (!netchess_proto_format_mach(text, sizeof(text),
                                    localMachinePlatform())) {
        return QByteArray();
    }
    return QByteArray(text);
}

static bool isSessionControlCommand(const QString &command)
{
    return command == QStringLiteral("/draw") ||
           command == QStringLiteral("/resign") ||
           command == QStringLiteral("/takeback");
}

static bool chatCanSharePendingControl(const QString &command,
                                       uint8_t pending,
                                       bool promptOpen,
                                       bool decisionOpen)
{
    return !isSessionControlCommand(command) && !promptOpen && !decisionOpen &&
           (pending == SESSION_REQUEST_RESET ||
            pending == SESSION_REQUEST_DRAW ||
            pending == SESSION_REQUEST_RESIGN ||
             pending == SESSION_REQUEST_TAKEBACK);
}

static QMessageBox::StandardButton askQuestion(
    QWidget *parent, const QString &title, const QString &message,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    QMessageBox box(QMessageBox::NoIcon, title, message, buttons, parent);
    if (defaultButton != QMessageBox::NoButton) {
        box.setDefaultButton(defaultButton);
    }
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

static QString appStyleSheet()
{
    return QStringLiteral(
        "QMainWindow, QDialog { background:" SHZ_INK "; color:" SHZ_TEXT "; }"
        "QWidget { color:" SHZ_TEXT "; }"
        // Exact-class selector: plain layout containers must not paint the
        // window ground over the card they sit on. Real controls keep theirs.
        ".QWidget { background:transparent; }"
        "QWidget#topCard, QWidget#sidePanel { background:" SHZ_CARD ";"
        " border:1px solid " SHZ_BORDER_SOFT "; border-radius:" SHZ_R_LG "; }"
        "QLabel { color:" SHZ_TEXT "; background:transparent; }"
        "QLabel#caption { color:" SHZ_ACCENT "; font-weight:700; font-size:10px;"
        " letter-spacing:1px; padding:0; background:transparent; }"
        "QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QTextEdit {"
        " background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
        " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
        " padding:4px 8px; selection-background-color:" SHZ_ACCENT_DEEP ";"
        " selection-color:" SHZ_WELL "; }"
        "QLineEdit:hover, QSpinBox:hover, QComboBox:hover {"
        " border-color:" SHZ_ACCENT_DEEP "; }"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus,"
        " QPlainTextEdit:focus, QTextEdit:focus { border-color:" SHZ_ACCENT "; }"
        "QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled,"
        " QPlainTextEdit:disabled, QTextEdit:disabled { background:" SHZ_QUIET ";"
        " color:" SHZ_TEXT_MUTED "; border-color:" SHZ_BORDER_SOFT "; }"
        "QPlainTextEdit, QTextEdit { padding:6px; }"
        "QComboBox::drop-down { border:0; width:18px; }"
        "QComboBox QAbstractItemView { background:" SHZ_SURFACE ";"
        " color:" SHZ_TEXT "; border:1px solid " SHZ_BORDER ";"
        " selection-background-color:" SHZ_ACCENT_DEEP "; selection-color:" SHZ_WELL ";"
        " outline:0; }"
        "QPushButton { background:" SHZ_SURFACE_ALT "; color:" SHZ_TEXT ";"
        " border:1px solid " SHZ_SURFACE_ALT "; border-radius:" SHZ_R ";"
        " font-weight:600; font-size:9pt; padding:4px 12px; min-height:20px; }"
        "QPushButton:hover { background:" SHZ_HOVER "; border-color:" SHZ_HOVER "; }"
        "QPushButton:pressed { background:" SHZ_PRESSED ";"
        " border-color:" SHZ_PRESSED "; }"
        "QPushButton:disabled { background:" SHZ_DISABLED ";"
        " color:" SHZ_TEXT_MUTED "; border-color:" SHZ_DISABLED "; }"
        "QRadioButton, QCheckBox { color:" SHZ_TEXT "; spacing:6px;"
        " background:transparent; }"
        "QRadioButton::indicator { width:11px; height:11px; border-radius:6px;"
        " border:1px solid " SHZ_TEXT_MUTED "; background:" SHZ_SURFACE "; }"
        "QRadioButton::indicator:hover { border-color:" SHZ_ACCENT "; }"
        "QRadioButton::indicator:checked { background:" SHZ_ACCENT ";"
        " border:1px solid " SHZ_ACCENT "; }"
        "QRadioButton::indicator:disabled { background:" SHZ_DISABLED ";"
        " border-color:" SHZ_BORDER "; }"
        "QRadioButton::indicator:checked:disabled { background:" SHZ_TEXT_MUTED ";"
        " border:1px solid " SHZ_TEXT_MUTED "; }"
        "QRadioButton:checked:disabled { color:" SHZ_TEXT_DIM "; }"
        "QScrollBar:vertical { background:transparent; width:10px; margin:2px; }"
        "QScrollBar::handle:vertical { background:" SHZ_SCROLL ";"
        " border-radius:3px; min-height:28px; }"
        "QScrollBar::handle:vertical:hover { background:" SHZ_ACCENT_DEEP "; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background:transparent; }"
        "QToolTip { background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
        " border:1px solid " SHZ_BORDER "; padding:4px 7px; }"
        "QStatusBar { background:" SHZ_WELL ";"
        " border-top:1px solid " SHZ_BORDER_SOFT "; }"
        "QStatusBar::item { border:0; }"
        "QStatusBar QLabel { color:" SHZ_ACCENT "; font-weight:700;"
        " font-size:11px; letter-spacing:0.5px; }");
}

static QString boardFrameStyle()
{
    return QStringLiteral(
        "QWidget#boardFrame { background:%1;"
        " border:1px solid " SHZ_BOARD_EDGE "; border-radius:" SHZ_R_LG "; }")
        .arg(PieceRenderer::boardWellColor(kBoardSquareSize).name());
}

// The 8x8 block gets the same blunt vertex as the frame: only its four outer
// squares round off, and always by display position so a flip follows along.
static PieceRenderer::SquareCorner displayCorner(int displayRow, int displayCol)
{
    if (displayRow == 0 && displayCol == 0) return PieceRenderer::CornerTopLeft;
    if (displayRow == 0 && displayCol == 7) return PieceRenderer::CornerTopRight;
    if (displayRow == 7 && displayCol == 0) return PieceRenderer::CornerBottomLeft;
    if (displayRow == 7 && displayCol == 7) return PieceRenderer::CornerBottomRight;
    return PieceRenderer::CornerNone;
}

static QLabel *captionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("caption"));
    return label;
}

static void configureActionButton(QPushButton *button)
{
    if (button != nullptr) {
        button->setAutoDefault(false);
        button->setDefault(false);
        button->setMinimumSize(kActionButtonWidth, 32);
    }
}

static void setWidgetStyle(QWidget *widget, const QString &style)
{
    if (widget != nullptr && widget->styleSheet() != style) {
        widget->setStyleSheet(style);
    }
}

} // namespace

QByteArray mqttClientIdFor(bool host, quint64 nonce)
{
    return QStringLiteral("PC%1%2")
        .arg(host ? QLatin1Char('H') : QLatin1Char('J'))
        .arg(nonce, 16, 16, QLatin1Char('0'))
        .toUpper()
        .toLatin1();
}

QStringList directIpHistoryWith(const QStringList &history, const QString &host)
{
    QStringList result;
    const auto append = [&result](const QString &candidate) {
        const QString ip = candidate.trimmed();
        if (ChessHelpers::isDirectIpSyntaxOk(ip) && !result.contains(ip)) {
            result.append(ip);
        }
    };

    append(host);
    for (const QString &ip : history) {
        if (result.size() >= kDirectIpHistoryMax) {
            break;
        }
        append(ip);
    }
    return result;
}

class MainWindowImpl final : public QMainWindow {
    Q_DISABLE_COPY_MOVE(MainWindowImpl)
public:
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
    QTcpSocket *testSocket() const
    {
        return socket_;
    }

    bool testPrepareMqttGuestSession()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = false;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 0u;
        clearMqttSubscriptionState();
        mqttActiveSubscriptions_.insert(QStringLiteral("meta"));
        mqttTargetSubscriptions_ = mqttActiveSubscriptions_;
        mqttSubscribed_ = true;
        testMqttWriteFailure_ = false;
        if (!initializeMqttSession()) {
            return false;
        }
        mqttSessionLinked_ = true;
        (void)sessionController_.linkUp(kMqttLinkId);
        return true;
    }

    bool testPrepareMqttHostSession()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = true;
        hostPlaysWhite_ = true;
        pcPlaysWhite_ = true;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 77u;
        clearMqttSubscriptionState();
        testMqttWriteFailure_ = false;
        if (!initializeMqttSession()) {
            return false;
        }
        mqttActiveSubscriptions_.insert(QStringLiteral("meta"));
        mqttActiveSubscriptions_.insert(mqttInSuffix());
        mqttActiveSubscriptions_.insert(mqttInAckSuffix());
        mqttActiveSubscriptions_.insert(mqttPresenceSuffix());
        mqttActiveSubscriptions_.insert(mqttPeerPresenceSuffix());
        mqttTargetSubscriptions_ = mqttActiveSubscriptions_;
        mqttSubscribed_ = true;
        mqttSessionLinked_ = true;
        setConnectedUi(true);
        (void)sessionController_.linkUp(kMqttLinkId);
        return true;
    }

    bool testEndAndRelinkMqttSession()
    {
        (void)sessionController_.linkDown(kMqttLinkId);
        if (!sessionController_.initialized()) {
            return false;
        }
        (void)sessionController_.linkUp(kMqttLinkId);
        return sessionController_.initialized();
    }

    bool testPrepareMqttGuestBootstrap()
    {
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        directRadio_->setChecked(false);
        mqttRadio_->setChecked(true);
        pcIsHost_ = false;
        mqttRoom_ = QStringLiteral("room");
        mqttSessionId_ = 0u;
        clearMqttSubscriptionState();
        testMqttWriteFailure_ = false;
        return initializeMqttSession();
    }

    void testFeedMqtt(const QByteArray &suffix,
                      const QByteArray &payload,
                      bool retained)
    {
        handleMqttPayload(topicFor(QString::fromLatin1(suffix)).toLatin1(),
                          payload, retained);
    }

    bool testBeginMqttRestore()
    {
        const QByteArray snapshot(SESSION_RESTORE_BYTES, 'A');

        return submitSessionLocalRequest(SESSION_REQUEST_RESTORE, 0u,
                                         snapshot, SESSION_PHASE_READY) &&
               directUiBusy_ == SESSION_REQUEST_RESTORE;
    }

    bool testRestoreUiIdle() const
    {
        return directUiBusy_ == 0u && directDecisionRequestId_ == 0u &&
               directDecisionControl_ == 0u && directDecisionBox_ == nullptr;
    }

    void testSetMqttWriteFailure(bool enabled)
    {
        testMqttWriteFailure_ = enabled;
    }

    bool testSessionReady() const
    {
        return directSessionReady_;
    }

    QString testStatusContextText() const
    {
        return statusContextText();
    }

    bool testStatusBarAligned()
    {
        const bool wasVisible = isVisible();
        if (!wasVisible) {
            show();
        }
        QCoreApplication::processEvents();
        alignStatusBarToControls();
        QCoreApplication::processEvents();

        const int leftAnchor =
            flipBoardButton_->mapToGlobal(QPoint(0, 0)).x();
        const int leftStatus =
            statusStateLabel_->mapToGlobal(QPoint(0, 0)).x();
        const int rightAnchor = logToggleButton_->mapToGlobal(
            QPoint(logToggleButton_->width(), 0)).x();
        const int rightStatus = moveClockLabel_->mapToGlobal(
            QPoint(moveClockLabel_->width(), 0)).x();
        const bool aligned = qAbs(leftStatus - leftAnchor) <= 1 &&
                             qAbs(rightStatus - rightAnchor) <= 1;
        if (!wasVisible) {
            hide();
        }
        return aligned;
    }

    bool testDisconnectButtonAvailable() const
    {
        return connectButton_ != nullptr && connectButton_->isEnabled() &&
               connectButton_->text() == QStringLiteral("Disconnect");
    }

    QByteArray testMqttClientId(bool host) const
    {
        return mqttClientIdFor(host, mqttClientNonce_);
    }

    void testHandleMqttPacket(const QByteArray &packet)
    {
        handleMqttPacket(packet);
    }

    QHash<uint16_t, QString> testMqttPendingSubacks() const
    {
        return mqttSubackPending_;
    }

    QHash<uint16_t, QString> testMqttPendingUnsubacks() const
    {
        return mqttUnsubackPending_;
    }

    QSet<QString> testMqttActiveSubscriptions() const
    {
        return mqttActiveSubscriptions_;
    }

    bool testMqttOperational() const
    {
        return mqttSubscribed_ && mqttSideReady_;
    }

    void testStartDirectGuestConnection(const QString &host, quint16 port)
    {
        directRadio_->setChecked(true);
        roleGuestRadio_->setChecked(true);
        hostEdit_->setText(host);
        portSpin_->setValue(port);
        connectToOpponent();
    }

    bool testDirectRetryPending() const
    {
        return directConnectRetryActive_ && directConnectRetryCount_ == 1 &&
               directConnectRetryTimer_->isActive();
    }

    bool testStartDirectHostListener(quint16 port)
    {
        directRadio_->setChecked(true);
        roleHostRadio_->setChecked(true);
        portSpin_->setValue(port);
        connectToOpponent();
        return directServer_ != nullptr && directServer_->isListening();
    }

    bool testDirectListenerActive() const
    {
        return directServer_ != nullptr && directServer_->isListening();
    }

    void testClickConnectButton()
    {
        connectButton_->click();
    }

    bool testReplaceDirectClientBeforeDisconnect()
    {
        QTcpSocket *oldSocket = socket_;
        if (oldSocket == nullptr || directServer_ == nullptr) {
            return false;
        }

        const QSignalBlocker serverBlocker(directServer_);
        if (!directServer_->hasPendingConnections() &&
            !directServer_->waitForNewConnection(2000)) {
            return false;
        }
        {
            const QSignalBlocker socketBlocker(oldSocket);
            if (oldSocket->bytesAvailable() == 0 &&
                !oldSocket->waitForReadyRead(2000)) {
                return false;
            }
            consumeReadyRead(oldSocket);
        }
        if (directPrimaryLinkId_ != SESSION_LINK_NONE) {
            return false;
        }
        acceptDirectClient();
        QCoreApplication::sendPostedEvents(oldSocket, QEvent::DeferredDelete);
        return directSockets_.size() == 1 &&
               directSockets_.constBegin().value() == socket_;
    }

    bool testResignRestartUiProjection()
    {
        const auto terminalReasonVisible = [this](const char *reason) {
            const QString expected = QString::fromLatin1(reason);
            endGameOver(expected);
            return gameOver_ && statusMessage_ == expected;
        };
        const bool terminalReasons =
            terminalReasonVisible(NETCHESSZX_UI_EVENT_CHECKMATE_WON) &&
            terminalReasonVisible(NETCHESSZX_UI_EVENT_CHECKMATE_LOST) &&
            terminalReasonVisible(NETCHESSZX_UI_EVENT_STALEMATE) &&
            terminalReasonVisible(NETCHESSZX_UI_EVENT_DRAW_AGREED);

        gameOver_ = true;
        gameClockRunning_ = false;
        directLocalResignPending_ = true;
        directResignRestartPending_ = true;
        directUiBusy_ = SESSION_REQUEST_RESIGN;
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool pendingBlocked =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
        handleDirectControlResult(SESSION_REQUEST_RESIGN,
                                  SESSION_CONTROL_ACCEPTED);
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool completedBlocked =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
        handleDirectControlResult(SESSION_REQUEST_RESET,
                                  SESSION_CONTROL_REJECTED);
        const bool failedRestart =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
        gameOver_ = false;
        gameClockRunning_ = true;
        directResignRestartPending_ = false;
        directUiBusy_ = 0u;
        applyDirectResignTransition();
        const bool remoteResign =
            statusMessage_ ==
                QString::fromLatin1(NETCHESSZX_UI_EVENT_OPPONENT_RESIGN) &&
            directUiBusy_ == SESSION_REQUEST_RESET &&
            startGameButton_->text() == QStringLiteral("Restarting...") &&
            !startGameButton_->isEnabled();
        const bool controlHighlighted =
            chatLogEdit_->currentCharFormat().foreground().color() ==
            QColor(0xff, 0x5a, 0x5a);
        appendChat(pcChatName(), QStringLiteral("hello"));
        const bool regularChatNormal =
            chatLogEdit_->currentCharFormat().foreground().color() ==
            QColor(0xe8, 0xee, 0xf6);
        const bool chatSharesControls =
            chatCanSharePendingControl(QStringLiteral("hello"),
                                       SESSION_REQUEST_DRAW, false, false) &&
            chatCanSharePendingControl(QStringLiteral("hello"),
                                       SESSION_REQUEST_RESIGN, false, false) &&
            !chatCanSharePendingControl(QStringLiteral("/resign"),
                                         SESSION_REQUEST_DRAW, false, false);
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        directUiBusy_ = 0u;
        gameOver_ = false;
        chatEdit_->clear();
        setConnectedUi(false);
        return pendingBlocked && completedBlocked && failedRestart &&
               remoteResign && terminalReasons && controlHighlighted &&
               regularChatNormal && chatSharesControls;
    }

    bool testBoardStateProjection()
    {
        const auto resetRulesBoard = [this]() {
            return netchesszx_rules_reset() == NETCHESSZX_OK &&
                   syncBoardFromRules();
        };
        const auto pieceAt = [this](const char *square) {
            return board_['8' - square[1]][square[0] - 'a'];
        };
        const auto playMoves = [this](const char *const *moves, int count) {
            for (int index = 0; index < count; ++index) {
                if (!applyMoveToBoard(QString::fromLatin1(moves[index]))) {
                    return false;
                }
            }
            return true;
        };

        bool ok = resetRulesBoard();
        nextPly_ = 1;
        bool takebackOk = saveTakebackSnapshot(1, true) &&
                          applyMoveToBoard(QStringLiteral("e2e4")) &&
                          pieceAt("e2") == '.' && pieceAt("e4") == 'P';
        restoreTakebackSnapshot();
        takebackOk = takebackOk && pieceAt("e2") == 'P' && pieceAt("e4") == '.';
        ok = ok && takebackOk;

        static const char *const castleMoves[] = {
            "e2e4", "e7e5", "g1f3", "b8c6", "f1e2", "g8f6", "e1g1"
        };
        bool castleOk = resetRulesBoard() &&
                        playMoves(castleMoves,
                                  static_cast<int>(sizeof(castleMoves) /
                                                   sizeof(castleMoves[0])));
        castleOk = castleOk && pieceAt("g1") == 'K' && pieceAt("f1") == 'R' &&
                   pieceAt("e1") == '.' && pieceAt("h1") == '.';
        ok = ok && castleOk;

        static const char *const enPassantMoves[] = {
            "e2e4", "a7a6", "e4e5", "d7d5", "e5d6"
        };
        bool enPassantOk = resetRulesBoard() &&
            playMoves(enPassantMoves,
                      static_cast<int>(sizeof(enPassantMoves) /
                                       sizeof(enPassantMoves[0])));
        enPassantOk = enPassantOk && pieceAt("d6") == 'P' && pieceAt("d5") == '.';
        ok = ok && enPassantOk;

        netchesszx_save_state_t promotion = {};
        std::memset(promotion.cells, '.', sizeof(promotion.cells));
        promotion.cells[4] = 'k';
        promotion.cells[8] = 'P';
        promotion.cells[60] = 'K';
        promotion.side = NETCHESSZX_SAVE_SIDE_WHITE;
        promotion.ep = NETCHESSZX_SAVE_EP_NONE;
        promotion.host_color = NETCHESSZX_SAVE_HOST_WHITE;
        bool promotionOk = restoreApplyState(promotion) &&
                           applyMoveToBoard(QStringLiteral("a7a8q")) &&
                           pieceAt("a8") == 'Q' && pieceAt("a7") == '.';
        netchesszx_save_state_t projected = {};
        promotionOk = promotionOk && currentSaveState(&projected, false) &&
                      projected.cells[0] == 'Q' && projected.cells[8] == '.';
        ok = ok && promotionOk;

        const bool resetOk = resetRulesBoard();
        nextPly_ = 1;
        gameOver_ = false;
        gameCheck_ = false;
        clearTakebackState();
        clearMoveHistory();
        refreshBoard();
        return ok && resetOk;
    }

    bool testRestoredMoveProjection()
    {
        struct RestoreCase {
            uint16_t ply;
            uint8_t side;
            int nextPly;
            int rows;
            const char *row0Number;
            const char *row0White;
            const char *row0Black;
            const char *row1Number;
            const char *row1White;
        };
        static const RestoreCase cases[] = {
            {20u, NETCHESSZX_SAVE_SIDE_WHITE, 21, 2,
             "10.", "", "RESTORED", "11.", "E2E4"},
            {21u, NETCHESSZX_SAVE_SIDE_BLACK, 22, 1,
             "11.", "RESTORED", "E7E5", "", ""},
            {0u, NETCHESSZX_SAVE_SIDE_WHITE, 1, 2,
             "", "", "RESTORED", "1.", "E2E4"},
        };
        netchesszx_save_state_t base;
        bool ok = testBoardStateProjection() && currentSaveState(&base, false);
        const auto cellText = [this](int row, int col) {
            QTableWidgetItem *item = moveTable_->item(row, col);
            return item != nullptr ? item->text() : QString();
        };

        for (const RestoreCase &test : cases) {
            netchesszx_save_state_t state = base;
            state.ply = test.ply;
            state.side = test.side;
            if (!ok || !restoreApplyState(state)) {
                ok = false;
                break;
            }
            const int restoredColumn = (test.ply & 1u) != 0u ? 1 : 2;
            QTableWidgetItem *restoredItem =
                moveTable_->item(0, restoredColumn);
            appendMoveRecord(test.nextPly,
                             (test.nextPly & 1) != 0
                                 ? QStringLiteral("e2e4")
                                 : QStringLiteral("e7e5"),
                             QString());
            ok = moveTable_->rowCount() == test.rows &&
                 moveTable_->item(0, restoredColumn) == restoredItem &&
                 cellText(0, 0) == QString::fromLatin1(test.row0Number) &&
                 cellText(0, 1) == QString::fromLatin1(test.row0White) &&
                 cellText(0, 2) == QString::fromLatin1(test.row0Black);
            if (ok && test.rows == 2) {
                ok = cellText(1, 0) == QString::fromLatin1(test.row1Number) &&
                     cellText(1, 1) == QString::fromLatin1(test.row1White) &&
                     cellText(1, 2).isEmpty();
            }
            if (!ok) {
                break;
            }
        }

        clearMoveHistory();
        for (int ply = 1; ok && ply <= 400; ++ply) {
            appendMoveRecord(ply, QStringLiteral("e2e4"), QString());
        }
        QTableWidgetItem *middleItem = moveTable_->item(100, 1);
        appendMoveRecord(401, QStringLiteral("e2e4"), QString());
        appendMoveRecord(402, QStringLiteral("e7e5"), QString());
        ok = ok && moveHistoryRecords_.size() == 400 &&
             moveTable_->rowCount() == 200 &&
             moveTable_->item(99, 1) == middleItem &&
             cellText(0, 0) == QStringLiteral("2.") &&
             cellText(199, 0) == QStringLiteral("201.");
        clearMoveHistory();
        setConnectedUi(false);
        return ok;
    }

    bool testSessionEndPresentation()
    {
        struct EndCase {
            uint8_t reason;
            const char *expected;
        };
        static const EndCase cases[] = {
            {SESSION_END_REASON_LOCAL_BYE,
             NETCHESSZX_UI_PHASE_DISCONNECTED},
            {SESSION_END_REASON_REMOTE_BYE,
             NETCHESSZX_UI_PC_EVENT_OPPONENT_LEFT},
            {SESSION_END_REASON_TRANSPORT_LOST,
             NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST},
        };
        const QSignalBlocker directBlocker(directRadio_);
        const QSignalBlocker mqttBlocker(mqttRadio_);
        bool ok = true;

        directRadio_->setChecked(true);
        mqttRadio_->setChecked(false);
        for (const EndCase &test : cases) {
            directSessionReady_ = true;
            directEndStatus_.clear();
            localDisconnectPending_ = false;
            handleDirectSessionChanged(SESSION_CHANGED_ENDED, test.reason);
            const QString expected = QString::fromLatin1(test.expected);
            ok = ok && statusMessage_ == expected &&
                 statusStateText() == expected;
        }
        return ok;
    }

    bool testCancelPendingPieceFlash()
    {
        const bool pending = static_cast<bool>(flashDone_);

        invalidatePieceFlash();
        return pending && directUiBusy_ == 0u;
    }
#endif

    MainWindowImpl()
        : sessionController_(this)
    {
        setWindowTitle(QStringLiteral("Shatranj %1").arg(
            QString::fromLatin1(kAppVersion)));
        setStyleSheet(appStyleSheet());
        netchesszx_rules_reset();
        (void)syncBoardFromRules();
        auto *root = new QWidget(this);
        auto *layout = new QVBoxLayout(root);
        layout->setContentsMargins(10, 10, 10, 6);
        layout->setSpacing(10);
        auto *banner = new AppBanner(root);
        banner->clicked = [this]() {
            showAboutDialog();
        };
        layout->addWidget(banner);
        auto *topCard = new QWidget(root);
        topCard->setObjectName(QStringLiteral("topCard"));
        auto *topRows = new QVBoxLayout(topCard);
        topRows->setContentsMargins(12, 6, 12, 6);
        topRows->setSpacing(4);
        auto *connectionRow = new QHBoxLayout();
        connectionRow->setContentsMargins(0, 0, 0, 0);
        connectionRow->setSpacing(0);
        auto *sessionRow = new QHBoxLayout();
        sessionRow->setContentsMargins(0, 0, 0, 0);
        sessionRow->setSpacing(6);

        QSettings settings;

        directRadio_ = new QRadioButton("Direct", root);
        mqttRadio_ = new QRadioButton("MQTT", root);
        auto *transportGroup = new QButtonGroup(root);
        transportGroup->addButton(directRadio_);
        transportGroup->addButton(mqttRadio_);
        const bool useMqtt = settings.value("connection/mqtt", false).toBool();
        directRadio_->setChecked(!useMqtt);
        mqttRadio_->setChecked(useMqtt);

        hostEdit_ = new QLineEdit(root);
        hostEdit_->setPlaceholderText(useMqtt ? "MQTT broker" : "Opponent IP");
        QString savedHost = settings.value("connection/host",
                                           useMqtt ? "broker.hivemq.com" : "192.168.0.").toString();
        if (useMqtt && savedHost == "test.mosquitto.org") {
            savedHost = "broker.hivemq.com";
        }
        const QStringList savedDirectIpHistory =
            settings.value(kDirectIpHistorySettingsKey).toStringList();
        const QStringList directIpHistory = directIpHistoryWith(
            savedDirectIpHistory, useMqtt ? QString() : savedHost);
        if (directIpHistory != savedDirectIpHistory) {
            settings.setValue(kDirectIpHistorySettingsKey, directIpHistory);
        }
        if (useMqtt) {
            mqttBrokerCache_ = savedHost;
            directIpCache_ = directIpHistory.value(0, QStringLiteral("192.168.0."));
        } else {
            directIpCache_ = directIpHistory.value(0, savedHost);
        }
        hostEdit_->setText(useMqtt ? savedHost : directIpCache_);
        hostEdit_->setAccessibleName(QStringLiteral("Host"));
        hostEdit_->setClearButtonEnabled(true);
        hostEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        directIpHistory_ = directIpHistory;

        directIpHistoryAction_ = hostEdit_->addAction(
            directIpHistoryIcon(), QLineEdit::LeadingPosition);
        directIpHistoryAction_->setText(QStringLiteral("Saved Direct IPs"));
        directIpHistoryAction_->setToolTip(QStringLiteral("Saved Direct IPs"));
        hostEdit_->setMinimumWidth(hostEdit_->sizeHint().width());
        directIpHistoryMenu_ = new QMenu(hostEdit_);
        directIpHistoryMenu_->setStyleSheet(
            "QMenu { background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R "; padding:4px; }"
            "QMenu::item { min-width:140px; padding:7px 12px; border-radius:4px; }"
            "QMenu::item:selected { background:" SHZ_ACCENT_DEEP "; color:" SHZ_WELL "; }");

        portSpin_ = new QSpinBox(root);
        portSpin_->setAccessibleName(QStringLiteral("Port"));
        portSpin_->setRange(1, 65535);
        portSpin_->setValue(settings.value("connection/port", useMqtt ? 1883 : 5000).toInt());
        portSpin_->setMinimumWidth(76);

        roomEdit_ = new QLineEdit(root);
        roomEdit_->setAccessibleName(QStringLiteral("Room"));
        roomEdit_->setPlaceholderText("NC0000");
        roomEdit_->setMaxLength(6);
        roomEdit_->setText(settings.value("connection/room", "NC0000").toString());
        roomEdit_->setClearButtonEnabled(true);
        roomEdit_->setMinimumWidth(100);

        connectButton_ = new QPushButton("Connect", root);
        startGameButton_ = new QPushButton("Start Game", root);
        startGameButton_->setEnabled(false);
        resetButton_ = new QPushButton("Reset Game", root);
        restoreButton_ = new QPushButton("Load Game", root);
        restoreButton_->setVisible(false);
        restoreButton_->setEnabled(false);
        configureActionButton(connectButton_);
        configureActionButton(startGameButton_);
        configureActionButton(resetButton_);
        configureActionButton(restoreButton_);

        roleHostRadio_ = new QRadioButton("Host", root);
        roleGuestRadio_ = new QRadioButton("Guest", root);
        auto *roleGroup = new QButtonGroup(root);
        roleGroup->addButton(roleHostRadio_);
        roleGroup->addButton(roleGuestRadio_);
        const bool pcIsHost = settings.value("connection/pcHost", false).toBool();
        roleHostRadio_->setChecked(pcIsHost);
        roleGuestRadio_->setChecked(!pcIsHost);

        hostWhiteRadio_ = new QRadioButton("White", root);
        hostBlackRadio_ = new QRadioButton("Black", root);
        auto *colorGroup = new QButtonGroup(root);
        colorGroup->addButton(hostWhiteRadio_);
        colorGroup->addButton(hostBlackRadio_);
        const bool hostWhite = settings.value("connection/hostWhite", true).toBool();
        hostWhiteRadio_->setChecked(hostWhite);
        hostBlackRadio_->setChecked(!hostWhite);
        auto *connectionWidget = new QWidget(root);
        connectionWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *connectionControls = new QHBoxLayout(connectionWidget);
        connectionControls->setContentsMargins(0, 0, 0, 0);
        connectionControls->setSpacing(8);
        connectionControls->addWidget(directRadio_);
        connectionControls->addWidget(mqttRadio_);
        hostCaptionLabel_ = captionLabel("LOCAL IP", root);
        hostCaptionLabel_->setMinimumWidth(
            hostCaptionLabel_->fontMetrics().horizontalAdvance(
                QStringLiteral("LOCAL IP")));
        hostCaptionLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        roomCaptionLabel_ = captionLabel("Room", root);
        connectionControls->addWidget(hostCaptionLabel_);
        connectionControls->addWidget(hostEdit_);
        connectionControls->addWidget(captionLabel("Port", root));
        connectionControls->addWidget(portSpin_);
        connectionControls->addWidget(roomCaptionLabel_);
        connectionControls->addWidget(roomEdit_);
        connectionRow->addWidget(connectionWidget, 1);
        connectionRow->addStretch(1);
        connectionRow->addWidget(connectButton_);
        auto *actionWidget = new QWidget(root);
        actionWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *actionRow = new QHBoxLayout(actionWidget);
        actionRow->setContentsMargins(0, 0, 0, 0);
        actionRow->setSpacing(6);
        actionRow->addWidget(startGameButton_);
        actionRow->addWidget(resetButton_);
        actionRow->addWidget(restoreButton_);

        auto *roleWidget = new QWidget(root);
        roleWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *roleRow = new QHBoxLayout(roleWidget);
        roleRow->setContentsMargins(0, 0, 0, 0);
        roleRow->setSpacing(8);
        roleRow->addWidget(captionLabel("Role", root), 0, Qt::AlignBaseline);
        roleRow->addWidget(roleHostRadio_, 0, Qt::AlignBaseline);
        roleRow->addWidget(roleGuestRadio_, 0, Qt::AlignBaseline);
        roleRow->addSpacing(10);

        hostColorLabel_ = captionLabel("Color", root);
        roleRow->addWidget(hostColorLabel_, 0, Qt::AlignBaseline);
        roleRow->addWidget(hostWhiteRadio_, 0, Qt::AlignBaseline);
        roleRow->addWidget(hostBlackRadio_, 0, Qt::AlignBaseline);
        resetButton_->hide();
        sessionRow->addWidget(roleWidget);
        sessionRow->addStretch(1);
        sessionRow->addWidget(actionWidget);
        topRows->addLayout(connectionRow);
        topRows->addLayout(sessionRow);
        layout->addWidget(topCard);

        auto *mainRow = new QHBoxLayout();
        mainRow->setContentsMargins(0, 0, 0, 0);
        mainRow->setSpacing(10);

        auto *boardWidget = new QWidget(root);
        boardWidget->setObjectName("boardFrame");
        boardFrame_ = boardWidget;
        boardWidget->setFixedSize(kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2,
                                  kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2);
        boardWidget->setStyleSheet(boardFrameStyle());
        auto *boardLayout = new QGridLayout(boardWidget);
        boardLayout->setContentsMargins(1, 1, 1, 1);
        boardLayout->setSpacing(0);
        addBoardCoordinates(boardLayout, boardWidget);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                auto *button = new QPushButton(boardWidget);
                button->setFixedSize(kBoardSquareSize, kBoardSquareSize);
                button->setIconSize(QSize(kBoardSquareSize, kBoardSquareSize));
                button->setFocusPolicy(Qt::StrongFocus);
                connect(button, &QPushButton::clicked, this, [this, row, col]() {
                    squareClicked(row, col);
                });
                squares_[row][col] = button;
                setSquareStyle(row, col, squareStyle(row, col, false, false, false, false,
                                                     false, true, displayCorner(row, col)));
                boardLayout->addWidget(button, row + 1, col + 1);
            }
        }
        flipBoardButton_ = new QPushButton(QString(QChar(0x21bb)), boardWidget);
        flipBoardButton_->setAccessibleName(QStringLiteral("Flip board"));
        flipBoardButton_->setToolTip("Flip board");
        flipBoardButton_->setFixedSize(kBoardCoordSize - 4, kBoardCoordSize - 4);
        flipBoardButton_->setFocusPolicy(Qt::StrongFocus);
        flipBoardButton_->setGeometry(2,
                                      boardWidget->height() - kBoardCoordSize + 2,
                                      kBoardCoordSize - 4,
                                      kBoardCoordSize - 4);
        flipBoardButton_->setStyleSheet(
            "QPushButton { background:" SHZ_QUIET "; color:" SHZ_ACCENT ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
            " font-weight:700; font-size:11px; padding:0; min-height:0; }"
            "QPushButton:hover { background:" SHZ_SURFACE_ALT ";"
            " border-color:" SHZ_ACCENT "; color:" SHZ_TEXT "; }");
        flipBoardButton_->raise();

        auto *sideWidget = new QWidget(root);
        sideWidget->setObjectName(QStringLiteral("sidePanel"));
        sideWidget->setFixedSize(kSidePanelWidth, boardWidget->height());
        auto *sidePanel = new QVBoxLayout(sideWidget);
        sidePanel->setContentsMargins(12, 10, 12, 10);
        sidePanel->setSpacing(6);

        sidePanel->addWidget(captionLabel("Status / Move", root));

        turnLabel_ = new QLabel("OFFLINE", root);
        turnLabel_->setAlignment(Qt::AlignCenter);
        turnLabel_->setMinimumHeight(30);
        sidePanel->addWidget(turnLabel_);

        selectedLabel_ = new QLabel(root);
        selectedLabel_->hide();
        selectedLabel_->setStyleSheet(
            "QLabel { color:" SHZ_TEXT_DIM "; font-size:10px; }");

        showHintsCheck_ = new QCheckBox("Enabled", root);
        showHintsCheck_->setAccessibleName(QStringLiteral("Show legal move hints"));
        showHintsCheck_->setChecked(settings.value("ui/showHints", true).toBool());
        showHintsCheck_->setStyleSheet(
            "QCheckBox { color:" SHZ_TEXT_DIM "; font-size:10px; spacing:6px; }"
            "QCheckBox::indicator { width:10px; height:10px; border-radius:3px;"
            " border:1px solid " SHZ_TEXT_MUTED "; background:" SHZ_SURFACE "; }"
            "QCheckBox::indicator:hover { border-color:" SHZ_ACCENT "; }"
            "QCheckBox::indicator:checked { background:" SHZ_ACCENT ";"
            " border:1px solid " SHZ_ACCENT "; }"
            "QCheckBox::indicator:disabled { background:" SHZ_DISABLED ";"
            " border-color:" SHZ_BORDER "; }"
            "QCheckBox::indicator:checked:disabled { background:" SHZ_TEXT_MUTED ";"
            " border:1px solid " SHZ_TEXT_MUTED "; }"
            "QCheckBox:checked:disabled { color:" SHZ_TEXT_DIM "; }"
        );

        auto *settingsButton = new QToolButton(root);
        settingsButton->setText(QString(QChar(0x2699)));
        settingsButton->setAccessibleName(QStringLiteral("Settings"));
        settingsButton->setToolTip("Settings");
        settingsButton->setPopupMode(QToolButton::InstantPopup);
        settingsButton->setFixedSize(32, 32);  // square, like the save/load buttons
        settingsButton->setFocusPolicy(Qt::StrongFocus);
        settingsButton->setStyleSheet(
            "QToolButton { background:" SHZ_QUIET "; color:" SHZ_ACCENT ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
            " font-weight:700; font-size:13px; padding:0; }"
            "QToolButton::menu-indicator { image:none; width:0; }"
            "QToolButton:hover { background:" SHZ_SURFACE_ALT ";"
            " border-color:" SHZ_ACCENT "; color:" SHZ_TEXT "; }");

        auto *chatHeaderRow = new QHBoxLayout();
        chatHeaderRow->setContentsMargins(0, 0, 0, 0);
        chatHeaderRow->setSpacing(6);
        chatHeaderRow->addWidget(captionLabel("Chat", root));
        chatHeaderRow->addStretch(1);
        actionRow->insertWidget(2, settingsButton);
        sidePanel->addLayout(chatHeaderRow);
        chatLogEdit_ = new QPlainTextEdit(root);
        chatLogEdit_->setAccessibleName(QStringLiteral("Chat log"));
        chatLogEdit_->setReadOnly(true);
        chatLogEdit_->setFixedHeight(132);
        chatLogEdit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        chatLogEdit_->setStyleSheet(
            "QPlainTextEdit { background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
            " font-size:10pt; border:1px solid " SHZ_BORDER ";"
            " border-radius:" SHZ_R "; padding:7px; }");
        chatLogEdit_->document()->setMaximumBlockCount(200);
        sidePanel->addWidget(chatLogEdit_);

        auto *chatControls = new QVBoxLayout();
        chatControls->setSpacing(4);

        chatEdit_ = new QLineEdit(root);
        chatEdit_->setAccessibleName(QStringLiteral("Chat message or move"));
        chatEdit_->setPlaceholderText("Message or move (e2e4)");
        chatEdit_->setClearButtonEnabled(true);
        chatEdit_->setMaxLength(kChatTextMax);
        chatEdit_->setMinimumHeight(32);
        chatEdit_->setStyleSheet(
            "QLineEdit { background:#202b35; color:#ffffff;"
            " border:1px solid " SHZ_ACCENT "; border-radius:" SHZ_R ";"
            " padding:4px 8px; selection-background-color:" SHZ_ACCENT_DEEP ";"
            " selection-color:" SHZ_WELL "; }"
            "QLineEdit:focus { background:#243444;"
            " border:1px solid " SHZ_ACCENT_SOFT "; }");
        chatEdit_->installEventFilter(this);
        moveEdit_ = chatEdit_;
        chatButton_ = new QPushButton("SEND", root);
        chatButton_->setEnabled(false);
        configureActionButton(chatButton_);
        chatButton_->setMinimumSize(72, 32);
        setWidgetStyle(chatButton_, chatButtonStyle(false));

        chatCountLabel_ = new QLabel(chatEdit_);
        chatCountLabel_->setAccessibleName(QStringLiteral("Chat character count"));
        chatCountLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        chatCountLabel_->setStyleSheet(
            "QLabel { color:" SHZ_TEXT_MUTED "; font-size:9px;"
            " background:transparent; padding:0; }");
        chatCountLabel_->setText(QStringLiteral("0/%1").arg(kChatTextMax));
        takebackButton_ = new QPushButton(root);
        resetIconButton_ = new QPushButton(root);
        drawButton_ = new QPushButton(root);
        takebackButton_->setIcon(sessionActionIcon("takeback"));
        resetIconButton_->setIcon(sessionActionIcon("reset"));
        drawButton_->setIcon(sessionActionIcon("draw"));
        takebackButton_->setAccessibleName(QStringLiteral("Request takeback"));
        resetIconButton_->setAccessibleName(QStringLiteral("Reset game"));
        drawButton_->setAccessibleName(QStringLiteral("Offer draw"));
        takebackButton_->setToolTip(QStringLiteral("Takeback"));
        resetIconButton_->setToolTip(QStringLiteral("Reset game"));
        drawButton_->setToolTip(QStringLiteral("Offer a draw"));
        const QString iconButtonStyle =
            "QPushButton { background:" SHZ_QUIET "; color:" SHZ_ACCENT ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
            " padding:0; min-height:0; min-width:0; }"
            "QPushButton:hover { background:" SHZ_SURFACE_ALT ";"
            " border-color:" SHZ_ACCENT "; }"
            "QPushButton:disabled { background:" SHZ_DISABLED ";"
            " border-color:" SHZ_DISABLED "; }";
        for (QPushButton *button : {takebackButton_, resetIconButton_, drawButton_}) {
            button->setIconSize(QSize(16, 16));
            button->setFixedSize(32, 32);
            button->setFocusPolicy(Qt::StrongFocus);
            button->setAutoDefault(false);
            button->setDefault(false);
            button->setStyleSheet(iconButtonStyle);
        }
        connect(takebackButton_, &QPushButton::clicked, this, [this]() {
            (void)requestTakeback();
            refreshSessionCommandButtons();
        });
        connect(resetIconButton_, &QPushButton::clicked, this, [this]() {
            if (resetButton_ != nullptr) {
                resetButton_->click();
            }
        });
        connect(drawButton_, &QPushButton::clicked, this, [this]() {
            (void)requestDraw();
            refreshSessionCommandButtons();
        });
        auto *chatActionRow = new QHBoxLayout();
        chatActionRow->setSpacing(6);
        chatActionRow->addWidget(takebackButton_);
        chatActionRow->addWidget(resetIconButton_);
        chatActionRow->addWidget(drawButton_);
        chatActionRow->addStretch(1);
        chatActionRow->addWidget(chatButton_);
        chatControls->addWidget(chatEdit_);
        chatControls->addLayout(chatActionRow);
        sidePanel->addLayout(chatControls);
        boardCombo_ = new QComboBox(root);
        boardCombo_->setAccessibleName(QStringLiteral("Chessboard"));
        boardCombo_->setToolTip("Chessboard");
        pieceCombo_ = new QComboBox(root);
        pieceCombo_->setAccessibleName(QStringLiteral("Piece set"));
        pieceCombo_->setToolTip("Piece set");
        boardCombo_->addItem("Default board");
        {
            const QStringList boards = PieceRenderer::boardTextures();
            for (const QString &b : boards) {
                QFileInfo fi(b);
                boardCombo_->addItem(fi.completeBaseName(), b);
            }
        }
        {
            const QStringList sets = PieceRenderer::pieceSets();
            for (const QString &s : sets) pieceCombo_->addItem(s, s);
        }

        auto *settingsMenu = new QMenu(settingsButton);
        settingsMenu->setStyleSheet(
            "QMenu { background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R "; }");
        auto *settingsPanel = new QWidget(settingsMenu);
        auto *settingsLayout = new QGridLayout(settingsPanel);
        settingsLayout->setContentsMargins(8, 8, 8, 8);
        settingsLayout->setHorizontalSpacing(8);
        settingsLayout->setVerticalSpacing(6);
        settingsLayout->addWidget(captionLabel("Chessboard", settingsPanel), 0, 0);
        settingsLayout->addWidget(boardCombo_, 0, 1);
        settingsLayout->addWidget(captionLabel("Piece Set", settingsPanel), 1, 0);
        settingsLayout->addWidget(pieceCombo_, 1, 1);
        settingsLayout->addWidget(captionLabel("Hints", settingsPanel), 2, 0);
        settingsLayout->addWidget(showHintsCheck_, 2, 1);
        settingsLayout->setColumnStretch(1, 1);
        auto *settingsAction = new QWidgetAction(settingsMenu);
        settingsAction->setDefaultWidget(settingsPanel);
        settingsMenu->addAction(settingsAction);
        settingsButton->setMenu(settingsMenu);
        connect(boardCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyBoardTexture(); });
        connect(pieceCombo_, &QComboBox::currentIndexChanged, this, [this]() { applyPieceSet(); });
        // Restore saved preferences
        {
            const QSignalBlocker boardBlocker(boardCombo_);
            const QSignalBlocker pieceBlocker(pieceCombo_);
            const QString savedBoard = settings.value("appearance/board", "").toString();
            const QString savedPieces = settings.value("appearance/pieces", "").toString();
            if (!savedBoard.isEmpty()) {
                int idx = boardCombo_->findData(savedBoard);
                if (idx >= 0) boardCombo_->setCurrentIndex(idx);
            }
            const QString initialPieces = savedPieces.isEmpty()
                ? QStringLiteral("merida")
                : savedPieces;
            int idx = pieceCombo_->findData(initialPieces);
            if (idx >= 0) pieceCombo_->setCurrentIndex(idx);
        }
        applyBoardTexture();
        applyPieceSet();

        logTitleLabel_ = captionLabel("Moves", root);
        sidePanel->addWidget(logTitleLabel_);

        logStack_ = new QStackedWidget(root);
        logStack_->setMinimumHeight(72);

        moveTable_ = new QTableWidget(logStack_);
        moveTable_->setColumnCount(3);
        moveTable_->setHorizontalHeaderLabels({QString(), "WHITE", "BLACK"});
        moveTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        moveTable_->setAlternatingRowColors(true);
        moveTable_->setFocusPolicy(Qt::NoFocus);
        moveTable_->setSelectionMode(QAbstractItemView::NoSelection);
        moveTable_->setShowGrid(false);
        moveTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        moveTable_->verticalHeader()->hide();
        moveTable_->verticalHeader()->setDefaultSectionSize(18);
        moveTable_->horizontalHeader()->setFixedHeight(21);
        moveTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        moveTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        moveTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        moveTable_->setStyleSheet(
            QStringLiteral(
                "QTableWidget { background:" SHZ_SURFACE "; color:#f1f0e8;"
                " alternate-background-color:" SHZ_ROW_ALT ";"
                " font-family:\"%1\"; font-size:9pt;"
                " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R "; }"
                "QTableWidget::item { padding:0 6px;"
                " border-bottom:1px solid " SHZ_BORDER_SOFT "; border-right:0; }"
                "QHeaderView::section { background:" SHZ_SURFACE ";"
                " color:#f1f0e8; font-family:\"%1\"; font-weight:700;"
                " font-size:8pt; letter-spacing:1px; border:0;"
                " border-bottom:1px solid #5b5b70; padding:0 6px; }")
                .arg(UiTheme::monoFamily()));
        logStack_->addWidget(moveTable_);

        logEdit_ = new QTextEdit(logStack_);
        logEdit_->setReadOnly(true);
        logEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
        logEdit_->setStyleSheet(
            QStringLiteral(
                "QTextEdit { background:" SHZ_SURFACE "; color:#f1f0e8;"
                " font-family:\"%1\"; font-size:9pt;"
                " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
                " padding:6px; }")
                .arg(UiTheme::monoFamily()));
        logEdit_->document()->setDocumentMargin(0);
        logEdit_->document()->setMaximumBlockCount(400);
        logStack_->addWidget(logEdit_);
        sidePanel->addWidget(logStack_, 1);

        auto *logActionRow = new QHBoxLayout();
        logActionRow->setContentsMargins(0, 0, 0, 0);
        logActionRow->setSpacing(6);
        saveGameButton_ = new QPushButton(root);
        saveGameButton_->setAccessibleName(QStringLiteral("Save game"));
        saveGameButton_->setToolTip("Save game");
        saveGameButton_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
        saveGameButton_->setIconSize(QSize(16, 16));
        saveGameButton_->setMinimumSize(32, 32);
        saveGameButton_->setFocusPolicy(Qt::StrongFocus);
        saveGameButton_->setEnabled(false);
        loadGameButton_ = new QPushButton(root);
        loadGameButton_->setAccessibleName(QStringLiteral("Load game"));
        loadGameButton_->setToolTip("Load game");
        loadGameButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
        loadGameButton_->setIconSize(QSize(16, 16));
        loadGameButton_->setMinimumSize(32, 32);
        loadGameButton_->setFocusPolicy(Qt::StrongFocus);
        loadGameButton_->setEnabled(false);
        logToggleButton_ = new QPushButton("Log", root);
        logToggleButton_->setMinimumSize(kActionButtonWidth, 32);
        logActionRow->addStretch(1);
        logActionRow->addWidget(saveGameButton_);
        logActionRow->addWidget(loadGameButton_);
        logActionRow->addWidget(logToggleButton_);
        sidePanel->addLayout(logActionRow);

        mainRow->addWidget(boardWidget, 0, Qt::AlignTop);
        mainRow->addWidget(sideWidget, 1, Qt::AlignTop);
        layout->addLayout(mainRow);
        setCentralWidget(root);
        socket_ = new QTcpSocket(this);
        directServer_ = new QTcpServer(this);
        statusBarContents_ = new QWidget(statusBar());
        statusBarLayout_ = new QHBoxLayout(statusBarContents_);
        statusBarLayout_->setContentsMargins(0, 0, 0, 0);
        statusBarLayout_->setSpacing(6);
        statusStateLabel_ = new QLabel("DISCONNECTED", statusBarContents_);
        statusContextLabel_ = new QLabel(QString(), statusBarContents_);
        gameClockLabel_ = new QLabel("GAME --:--", statusBarContents_);
        moveClockLabel_ = new QLabel("MOVE --:--", statusBarContents_);
        statusStateLabel_->setMinimumWidth(120);
        statusContextLabel_->setSizePolicy(QSizePolicy::Expanding,
                                           QSizePolicy::Preferred);
        gameClockLabel_->setMinimumWidth(80);
        moveClockLabel_->setMinimumWidth(80);
        statusContextLabel_->setStyleSheet(
            "QLabel { color:" SHZ_ACCENT "; font-weight:600; font-size:11px; }");
        statusContextLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        gameClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        moveClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusBar()->setSizeGripEnabled(false);
        statusBarLayout_->addWidget(statusStateLabel_);
        statusBarLayout_->addWidget(statusContextLabel_, 1);
        statusBarLayout_->addWidget(gameClockLabel_);
        statusBarLayout_->addWidget(moveClockLabel_);
        statusBar()->addWidget(statusBarContents_, 1);

        configureSessionFromUi();
        if (isMqttMode() && pcIsHost_) {
            roomEdit_->setText(generateMqttRoomCode());
        }
        updateSessionControlsEnabled();
        updateConnectionModeUi();
        renderLogView();
        refreshStatusBar();
        resizeToContent();

        clockTimer_ = new QTimer(this);
        connect(clockTimer_, &QTimer::timeout, this, [this]() {
            checkUiStall();
            updateClockLabels();
            checkConnectionHealth();
        });
        uiTickTimer_.start();
        clockTimer_->start(1000);

        directConnectRetryTimer_ = new QTimer(this);
        directConnectRetryTimer_->setSingleShot(true);
        directConnectRetryTimer_->setInterval(kDirectConnectRetryIntervalMs);
        connect(directConnectRetryTimer_, &QTimer::timeout, this, [this]() {
            retryDirectConnection();
        });

        revealTimer_ = new QTimer(this);
        revealTimer_->setSingleShot(true);
        connect(revealTimer_, &QTimer::timeout, this, [this]() {
            advancePieceReveal();
        });
        flashTimer_ = new QTimer(this);
        flashTimer_->setSingleShot(true);
        connect(flashTimer_, &QTimer::timeout, this, [this]() {
            advancePieceFlash();
        });
        feedbackTimer_ = new QTimer(this);
        feedbackTimer_->setSingleShot(true);
        connect(feedbackTimer_, &QTimer::timeout, this, [this]() {
            advanceDestinationFeedback();
        });

        configureSessionControllerCallbacks();

        connectUiSignals();

        setConnectedUi(false);
        layoutChatCountLabel();
        updateClockLabels();
        refreshBoard();
        animateBoardPiecesIn();
    }

    ~MainWindowImpl() override
    {
        if (revealTimer_ != nullptr) {
            revealTimer_->stop();
        }
        if (flashTimer_ != nullptr) {
            flashTimer_->stop();
        }
        if (feedbackTimer_ != nullptr) {
            feedbackTimer_->stop();
        }
        flashDone_ = {};
        for (QTcpSocket *sock : findChildren<QTcpSocket *>()) {
            QObject::disconnect(sock, nullptr, this, nullptr);
        }
    }

private:
    void connectUiSignals()
    {
        connect(connectButton_, &QPushButton::clicked, this, [this]() {
            if (isConnected()) {
                const QMessageBox::StandardButton answer = askQuestion(
                    this, QStringLiteral("Disconnect"),
                    QStringLiteral("Disconnect?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes || !isConnected()) {
                    return;
                }
                if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
                    directServer_->close();
                }
                if (isMqttMode()) {
                    localDisconnectPending_ = true;
                    directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                        socket_->disconnectFromHost();
                    }
                } else {
                    directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                        socket_->disconnectFromHost();
                    }
                }
                return;
            }
            if (isDirectListening()) {
                directServer_->close();
                setStatusText(NETCHESSZX_UI_NOTICE_LISTEN_CANCELLED);
                setConnectedUi(false);
                return;
            }
            if (isConnecting()) {
                cancelDirectConnectRetry();
                if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
                    directServer_->close();
                }
                if (socket_ != nullptr) {
                    if (!isMqttMode()) {
                        directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                    }
                    socket_->abort();
                }
                setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
                setConnectedUi(false);
                return;
            }
            connectToOpponent();
        });
        connect(flipBoardButton_, &QPushButton::clicked, this, [this]() {
            boardWhiteAtBottom_ = !boardWhiteAtBottom_;
            coordinatesInitialized_ = false;
            refreshBoard();
        });
        connect(directRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                cancelDirectConnectRetry();
                const QString host = hostEdit_->text().trimmed();
                if (!host.isEmpty() && looksLikeMqttHost(host)) {
                    mqttBrokerCache_ = host;
                }
                if (directIpCache_.isEmpty()) {
                    directIpCache_ = "192.168.0.";
                }
                hostEdit_->setPlaceholderText("Opponent IP");
                if (!pcIsHost_) {
                    hostEdit_->setText(directIpCache_);
                }
                portSpin_->setValue(5000);
                configureSessionFromUi();
                updateSessionControlsEnabled();
                updateConnectionModeUi();
                refreshStatusBar();
            }
        });
        connect(mqttRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                cancelDirectConnectRetry();
                const QString host = hostEdit_->text().trimmed();
                if (!directShowingLocalHost_ && !host.isEmpty() && !looksLikeMqttHost(host)) {
                    directIpCache_ = host;
                }
                if (mqttBrokerCache_.isEmpty()) {
                    mqttBrokerCache_ = "broker.hivemq.com";
                }
                hostEdit_->setPlaceholderText("MQTT broker");
                hostEdit_->setText(mqttBrokerCache_);
                portSpin_->setValue(1883);
                configureSessionFromUi();
                if (pcIsHost_) {
                    roomEdit_->setText(generateMqttRoomCode());
                }
                updateSessionControlsEnabled();
                updateConnectionModeUi();
                refreshStatusBar();
            }
        });
        connect(roleHostRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            cancelDirectConnectRetry();
            configureSessionFromUi();
            if (checked && isMqttMode()) {
                roomEdit_->setText(generateMqttRoomCode());
            }
            updateConnectionModeUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(roleGuestRadio_, &QRadioButton::toggled, this, [this]() {
            cancelDirectConnectRetry();
            configureSessionFromUi();
            updateConnectionModeUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(hostWhiteRadio_, &QRadioButton::toggled, this, [this]() {
            configureSessionFromUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(hostBlackRadio_, &QRadioButton::toggled, this, [this]() {
            configureSessionFromUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        #if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(showHintsCheck_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        #else
        connect(showHintsCheck_, &QCheckBox::stateChanged, this, [this](int state) {
        #endif
            QSettings settings;
            settings.setValue("ui/showHints", state == Qt::Checked);
            refreshBoard();
        });
        connect(hostEdit_, &QLineEdit::textChanged, this, [this]() {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(hostEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (connectButton_->isEnabled()) {
                connectButton_->animateClick();
            }
        });
        connect(directIpHistoryAction_, &QAction::triggered, this, [this]() {
            if (!directIpHistory_.isEmpty()) {
                rebuildDirectIpHistoryMenu();
                directIpHistoryMenu_->popup(
                    hostEdit_->mapToGlobal(QPoint(0, hostEdit_->height())));
            }
        });
        connect(portSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(roomEdit_, &QLineEdit::textChanged, this, [this]() {
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(roomEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (connectButton_->isEnabled()) {
                connectButton_->animateClick();
            }
        });
        connect(startGameButton_, &QPushButton::clicked, this, [this]() {
            if (gameOver_) {
                if (restoreBusy()) {
                    setStatusText("Load in progress");
                    return;
                }
                setStatusText(NETCHESSZX_UI_CONFIRM_RESTART_GAME);
                resetPromptOpen_ = true;
                if (askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE,
                                NETCHESSZX_UI_CONFIRM_RESTART_GAME,
                                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                    resetPromptOpen_ = false;
                    disconnectToSetup();
                    return;
                }
                resetPromptOpen_ = false;
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESTART_ACK);
                    setConnectedUi(true);
                }
                return;
            }
            sendGameStart();
        });
        connect(resetButton_, &QPushButton::clicked, this, [this]() {
            const bool connected = isConnected();
            if (connected) {
                if (restoreBusy()) {
                    setStatusText("Load in progress");
                    return;
                }
                resetPromptOpen_ = true;
                if (askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE,
                                NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST,
                                QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                    resetPromptOpen_ = false;
                    return;
                }
                resetPromptOpen_ = false;
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_RESET_REQUESTED_ACK);
                    setConnectedUi(true);
                }
                return;
            }
            resetGame("Game reset");
            stopGameClock();
            setConnectedUi(connected);
        });
        connect(chatButton_, &QPushButton::clicked, this, [this]() {
            sendChat();
        });
        connect(chatEdit_, &QLineEdit::textChanged, this,
                [this](const QString &text) {
            chatInputHistoryIndex_ = static_cast<int>(chatInputHistory_.size());
            if (chatCountLabel_ != nullptr) {
                chatCountLabel_->setText(
                    QStringLiteral("%1/%2").arg(text.size()).arg(kChatTextMax));
            }
            layoutChatCountLabel();
            refreshChatButton();
        });
        connect(chatEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (chatButton_->isEnabled()) {
                sendChat();
            }
        });
        connect(saveGameButton_, &QPushButton::clicked, this, [this]() {
            saveGameWithDialog();
        });
        connect(loadGameButton_, &QPushButton::clicked, this, [this]() {
            loadGameWithDialog();
        });
        connect(logToggleButton_, &QPushButton::clicked, this, [this]() {
            toggleLogView();
        });
        attachSocket(socket_);
        connect(directServer_, &QTcpServer::newConnection, this, [this]() {
            acceptDirectClient();
        });
    }

    struct MoveRecord {
        int ply = 0;
        QString move;
        QString notation;
    };

    struct TakebackSnapshot {
        QByteArray rules;
        QString lastMove;
        int ply = 0;
        int nextPly = 1;
        int historyCount = 0;
        bool pcTurn = false;
        bool gameCheck = false;
        bool localMove = false;
        bool valid = false;
    };

    void showEvent(QShowEvent *event) override
    {
        QMainWindow::showEvent(event);
        applyWindowChrome(this, QColor(QStringLiteral(SHZ_INK)),
                          QColor(QStringLiteral(SHZ_TEXT)));
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (isConnected()) {
            directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
            (void)submitSessionLocalRequest(SESSION_REQUEST_BYE);
        }
        if (directServer_ != nullptr) {
            directServer_->close();
        }
        QMainWindow::closeEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == chatEdit_ &&
            (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            layoutChatCountLabel();
        }
        if (watched == chatEdit_ && event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if ((keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) &&
                !chatInputHistory_.isEmpty()) {
                const int delta = keyEvent->key() == Qt::Key_Up ? -1 : 1;
                const int historySize = static_cast<int>(chatInputHistory_.size());
                const int target = qBound(0, chatInputHistoryIndex_ + delta,
                                          historySize);
                chatEdit_->setText(target == historySize
                                       ? QString()
                                       : chatInputHistory_.at(target));
                chatInputHistoryIndex_ = target;
                chatEdit_->selectAll();
                return true;
            }
            const bool isReturn = keyEvent->key() == Qt::Key_Return ||
                                  keyEvent->key() == Qt::Key_Enter;
            if (isReturn && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
                if (chatButton_->isEnabled()) {
                    sendChat();
                }
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    static QString buildStamp()
    {
        return QStringLiteral(__DATE__ " " __TIME__);
    }

    static QWidget *aboutHeader(QWidget *parent)
    {
        const QString path = PieceRenderer::assetPath(
            QStringLiteral("assets/pc-client/about/about-shatranj.png"));
        const QPixmap image(path);
        if (image.isNull()) return new AppBanner(parent);

        auto *label = new QLabel(parent);
        label->setAccessibleName(QStringLiteral("Shatranj about artwork"));
        label->setAlignment(Qt::AlignCenter);
        label->setFixedHeight(363);
        label->setPixmap(image.scaled(QSize(363, 363), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        label->setStyleSheet(
            "QLabel { background:#080706; border-bottom:1px solid #3a2a17; }");
        return label;
    }

    void showAboutDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("About Shatranj");
        dialog.setModal(true);
        dialog.setFixedWidth(363);
        dialog.setStyleSheet(
            "QDialog { background:#171821; color:#f2f2f0; }"
            "QLabel { color:#f2f2f0; background:#171821; }"
            "QLabel#muted { color:#c4c7cf; }"
            "QLabel#link { color:" SHZ_ACCENT "; }"
            "QPushButton { background:#292a36; color:#f2f2f0;"
            " border:1px solid #444654; border-radius:" SHZ_R ";"
            " padding:6px 18px; }"
            "QPushButton:hover { background:#343646; }");

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        layout->addWidget(aboutHeader(&dialog));

        auto *info = new QLabel(
            QString("<div style=\"color:#f0d58b; font-weight:700; font-size:10pt;\">"
                    "Qt Client for Shatranj Chess<br>"
                    "(C) 2026 M. Ignacio Monge Garcia<br>"
                    "Version %1 (Build %2)<br>"
                    "<a style=\"color:#f0d58b; text-decoration:none;\" "
                    "href=\"https://github.com/IgnacioMonge/Shatranj\">"
                    "github.com/IgnacioMonge/Shatranj</a>"
                    "</div><br>"
                    "<div style=\"color:#c4c7cf; font-size:9pt;\">"
                    "Shatranj: GNU GPL v2.0.<br>"
                    "Qt 6: LGPLv3 / GPLv2 / GPLv3.<br>"
                    "Lichess boards: lila authors and pirouetti, AGPLv3+.<br>"
                    "Pieces: California (Jerry S.) and Gioco (sadsnake1), "
                    "CC BY-NC-SA 4.0; Kiwen-suwi (neverRare), CC BY 4.0; "
                    "Merida (Armando H. Marroquin), GPLv2+; "
                    "Mpchess (Maxime Chupin), GPLv3+."
                    "</div>")
                .arg(QString::fromLatin1(kAppVersion), buildStamp()),
            &dialog);
        info->setAlignment(Qt::AlignCenter);
        info->setTextFormat(Qt::RichText);
        info->setOpenExternalLinks(true);
        info->setWordWrap(true);
        info->setStyleSheet(
            "QLabel { color:#c4c7cf; font-size:9pt;"
            " padding-left:28px; padding-right:28px; }");
        layout->addWidget(info);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->setContentsMargins(0, 0, 0, 8);
        auto *closeButton = new QPushButton("Close", &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttonRow->addWidget(closeButton, 0, Qt::AlignCenter);
        layout->addLayout(buttonRow);

        dialog.adjustSize();
        dialog.exec();
    }

    bool syncBoardFromRules()
    {
        CompactRulesState state = {};
        if (netchesszx_rules_save(&state, sizeof(state)) != NETCHESSZX_OK) {
            return false;
        }
        ChessHelpers::asciiBoardFromCompact(state.board, &board_[0][0]);
        return true;
    }

    void refreshBoard()
    {
        refreshBoardCoordinates();
        for (int displayRow = 0; displayRow < 8; ++displayRow) {
            for (int displayCol = 0; displayCol < 8; ++displayCol) {
                const int boardRow = boardRowForDisplay(displayRow);
                const int boardCol = boardColForDisplay(displayCol);
                squares_[displayRow][displayCol]->setAccessibleName(
                    QStringLiteral("Board square %1").arg(ChessHelpers::squareName(boardRow, boardCol).toUpper()));
                squares_[displayRow][displayCol]->setText(QString());
                refreshBoardSquareVisual(boardRow, boardCol);
            }
        }
    }

    void refreshChangedBoardSquares(const char previous[8][8])
    {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (previous[row][col] != board_[row][col]) {
                    refreshBoardSquareVisual(row, col);
                }
            }
        }
    }

    static QLabel *coordLabel(const QString &text, QWidget *parent, const QSize &size)
    {
        auto *label = new QLabel(text, parent);
        label->setAlignment(Qt::AlignCenter);
        label->setFixedSize(size);
        label->setStyleSheet(
            "QLabel { color:" SHZ_TEXT "; background:transparent;"
            " font-weight:700; font-size:11px; letter-spacing:1px;"
            " padding:0; margin:0; }");
        return label;
    }

    void addBoardCoordinates(QGridLayout *layout, QWidget *parent)
    {
        for (int col = 0; col < 8; ++col) {
            fileLabelsTop_[col] = coordLabel(QString(), parent,
                                             QSize(kBoardSquareSize, kBoardCoordSize));
            fileLabelsBottom_[col] = coordLabel(QString(), parent,
                                                QSize(kBoardSquareSize, kBoardCoordSize));
            layout->addWidget(fileLabelsTop_[col], 0, col + 1, Qt::AlignCenter);
            layout->addWidget(fileLabelsBottom_[col], 9, col + 1, Qt::AlignCenter);
        }

        for (int row = 0; row < 8; ++row) {
            rankLabelsLeft_[row] = coordLabel(QString(), parent,
                                              QSize(kBoardCoordSize, kBoardSquareSize));
            rankLabelsRight_[row] = coordLabel(QString(), parent,
                                               QSize(kBoardCoordSize, kBoardSquareSize));
            layout->addWidget(rankLabelsLeft_[row], row + 1, 0, Qt::AlignCenter);
            layout->addWidget(rankLabelsRight_[row], row + 1, 9, Qt::AlignCenter);
        }
        refreshBoardCoordinates(true);
    }

    QString checkSuffixAfterMove(bool *stalemate = nullptr) const
    {
        const uint8_t state = netchesszx_rules_check_state();

        if (stalemate != nullptr) {
            *stalemate = state == NETCHESSZX_RULE_STALEMATE;
        }
        if (state == NETCHESSZX_RULE_CHECK_MATE) {
            return QStringLiteral("#");
        }
        return state == NETCHESSZX_RULE_CHECK ? QStringLiteral("+")
                                             : QString();
    }

    QString disambiguationForMove(char piece, int fromRow, int fromCol,
                                  int toRow, int toCol) const
    {
        bool conflict = false;
        bool sameFile = false;
        bool sameRank = false;
        char moveText[5];

        moveText[2] = static_cast<char>('a' + toCol);
        moveText[3] = static_cast<char>('8' - toRow);
        moveText[4] = '\0';

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (row == fromRow && col == fromCol) {
                    continue;
                }
                if (board_[row][col] != piece) {
                    continue;
                }

                moveText[0] = static_cast<char>('a' + col);
                moveText[1] = static_cast<char>('8' - row);
                if (netchesszx_rules_can_play(moveText) == NETCHESSZX_OK) {
                    conflict = true;
                    sameFile = sameFile || col == fromCol;
                    sameRank = sameRank || row == fromRow;
                }
            }
        }

        if (!conflict) {
            return QString();
        }
        if (!sameFile) {
            return QString(QChar('a' + fromCol));
        }
        if (!sameRank) {
            return QString(QChar('8' - fromRow));
        }
        return ChessHelpers::squareName(fromRow, fromCol);
    }

    QString moveNotationBase(const QString &move) const
    {
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;
        if (!ChessHelpers::moveCoords(move, &fromRow, &fromCol, &toRow, &toCol)) {
            return move.toUpper();
        }

        const char piece = board_[fromRow][fromCol];
        if (piece == '.') {
            return move.toUpper();
        }

        const char kind = ChessHelpers::lowerPiece(piece);
        const bool pawn = kind == 'p';
        if (kind == 'k' && fromCol == 4 && (toCol == 6 || toCol == 2)) {
            return toCol == 6 ? QStringLiteral("O-O") : QStringLiteral("O-O-O");
        }

        const bool capture = board_[toRow][toCol] != '.' ||
                             (pawn && fromCol != toCol);
        QString notation;
        if (pawn) {
            if (capture) {
                notation += QChar('a' + fromCol);
            }
        } else {
            notation += ChessHelpers::sanPieceLetter(piece);
            notation += disambiguationForMove(piece, fromRow, fromCol, toRow, toCol);
        }
        if (capture) {
            notation += QStringLiteral("x");
        }
        notation += QChar('a' + toCol);
        notation += QChar('8' - toRow);
        if (move.size() == 5) {
            notation += QStringLiteral("=");
            notation += ChessHelpers::sanPieceLetter(move[4].toLatin1());
        }
        return notation;
    }

    int boardRowForDisplay(int displayRow) const
    {
        return boardWhiteAtBottom_ ? displayRow : 7 - displayRow;
    }

    int boardColForDisplay(int displayCol) const
    {
        return boardWhiteAtBottom_ ? displayCol : 7 - displayCol;
    }

    int displayRowForBoard(int boardRow) const
    {
        return boardWhiteAtBottom_ ? boardRow : 7 - boardRow;
    }

    int displayColForBoard(int boardCol) const
    {
        return boardWhiteAtBottom_ ? boardCol : 7 - boardCol;
    }

    QString displayFileLabel(int displayCol) const
    {
        return QString(QChar('A' + boardColForDisplay(displayCol)));
    }

    QString displayRankLabel(int displayRow) const
    {
        return QString(QChar('8' - boardRowForDisplay(displayRow)));
    }

    QString pcSideName() const
    {
        return pcPlaysWhite_ ? "WHITE" : "BLACK";
    }

    QString pcSideLetter() const
    {
        return pcPlaysWhite_ ? "W" : "B";
    }

    QString pcChatName() const
    {
        return QStringLiteral("PLAYER");
    }

    QString opponentChatName() const
    {
        return QStringLiteral("OPPONENT");
    }

    void syncBoardOrientationWithPcSide()
    {
        boardWhiteAtBottom_ = pcPlaysWhite_;
        coordinatesInitialized_ = false;
    }

    void refreshBoardCoordinates(bool force = false)
    {
        if (!force && coordinatesInitialized_ && lastCoordinatePcWhite_ == boardWhiteAtBottom_) {
            return;
        }
        coordinatesInitialized_ = true;
        lastCoordinatePcWhite_ = boardWhiteAtBottom_;

        for (int col = 0; col < 8; ++col) {
            const QString file = displayFileLabel(col);
            if (fileLabelsTop_[col] != nullptr) {
                fileLabelsTop_[col]->setText(file);
            }
            if (fileLabelsBottom_[col] != nullptr) {
                fileLabelsBottom_[col]->setText(file);
            }
        }
        for (int row = 0; row < 8; ++row) {
            const QString rank = displayRankLabel(row);
            if (rankLabelsLeft_[row] != nullptr) {
                rankLabelsLeft_[row]->setText(rank);
            }
            if (rankLabelsRight_[row] != nullptr) {
                rankLabelsRight_[row]->setText(rank);
            }
        }
    }

    bool isLegalTarget(int row, int col) const
    {
        return legalTargets_.contains(ChessHelpers::squareName(row, col));
    }

    void setSquareStyle(int displayRow, int displayCol, const QString &style)
    {
        if (displayRow < 0 || displayRow >= 8 || displayCol < 0 || displayCol >= 8 ||
            squares_[displayRow][displayCol] == nullptr ||
            squareStyleCache_[displayRow][displayCol] == style) {
            return;
        }

        squareStyleCache_[displayRow][displayCol] = style;
        squares_[displayRow][displayCol]->setStyleSheet(style);
    }

    void refreshBoardSquareStyle(int row, int col)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }

        const int displayRow = displayRowForBoard(row);
        const int displayCol = displayColForBoard(col);
        const bool selected = (row == selectedRow_ && col == selectedCol_);
        const bool target = (row == targetRow_ && col == targetCol_);
        const bool legalTarget = isLegalTarget(row, col);
        const bool feedback = isFeedbackSquare(row, col);
        const bool hasPiece = (board_[row][col] != '.');
        const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
        setSquareStyle(displayRow, displayCol,
                       squareStyle(row, col, selected, target, legalTarget, feedback, hasPiece,
                                   showHints, displayCorner(displayRow, displayCol)));
    }

    void refreshBoardSquareVisual(int row, int col)
    {
        refreshBoardSquareStyle(row, col);
        refreshBoardSquareIcon(row, col, true);
    }

    void refreshBoardSquareIcon(int row, int col, bool visible)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }

        const int displayRow = displayRowForBoard(row);
        const int displayCol = displayColForBoard(col);
        const bool selected = (row == selectedRow_ && col == selectedCol_);
        const bool target = (row == targetRow_ && col == targetCol_);
        const bool legalTarget = isLegalTarget(row, col);
        const bool feedback = isFeedbackSquare(row, col);
        const bool hasPiece = (board_[row][col] != '.');
        const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
        const bool styledBackground = selected || feedback ||
            (target && hasPiece) ||
            (legalTarget && showHints && hasPiece);
        const bool textured = !PieceRenderer::boardTexture().isEmpty() && !styledBackground;
        squares_[displayRow][displayCol]->setIconSize(
            QSize(textured ? kBoardSquareSize : kPieceIconSize,
                  textured ? kBoardSquareSize : kPieceIconSize));
        squares_[displayRow][displayCol]->setIcon(
            textured ? PieceRenderer::boardSquareIcon(board_[row][col], row, col,
                                                      kBoardSquareSize, kPieceIconSize,
                                                      boardPiecesVisible_ && visible,
                                                      legalTarget && showHints && !hasPiece,
                                                      target && !hasPiece,
                                                      displayCorner(displayRow, displayCol))
                     : (boardPiecesVisible_ && visible
                            ? PieceRenderer::pieceIcon(board_[row][col])
                            : QIcon()));
    }

    void invalidatePieceReveal()
    {
        if (revealTimer_ != nullptr) {
            revealTimer_->stop();
        }
        morphSquares_.clear();
    }

    void invalidatePieceFlash()
    {
        if (flashTimer_ != nullptr) {
            flashTimer_->stop();
        }
        auto done = std::move(flashDone_);
        flashDone_ = {};
        if (done) {
            done(true);
        }
    }

    void invalidateDestinationFeedback()
    {
        if (feedbackTimer_ != nullptr) {
            feedbackTimer_->stop();
        }
    }

    void revealBoardSquarePair(int rowA, int colA, int rowB, int colB)
    {
        refreshBoardSquareIcon(rowA, colA, true);
        refreshBoardSquareIcon(rowB, colB, true);
    }

    void advancePieceReveal()
    {
        if (revealIsMorph_) {
            if (revealStep_ >= morphSquares_.size()) {
                if (revealTimer_ != nullptr) {
                    revealTimer_->stop();
                }
                return;
            }
            const QPoint square = morphSquares_.at(revealStep_);
            refreshBoardSquareIcon(square.x(), square.y(), true);
            ++revealStep_;
            if (revealStep_ >= morphSquares_.size()) {
                if (revealTimer_ != nullptr) {
                    revealTimer_->stop();
                }
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
            if (revealTimer_ != nullptr) {
                revealTimer_->stop();
            }
            return;
        }
        const int i = revealStep_ < 8 ? revealStep_ : revealStep_ - 8;
        if (revealStep_ < 8) {
            revealBoardSquarePair(0, i, 7, 7 - i);
        } else {
            revealBoardSquarePair(1, 7 - i, 6, i);
        }
        ++revealStep_;
        if (revealStep_ >= 16) {
            if (revealTimer_ != nullptr) {
                revealTimer_->stop();
            }
            return;
        }
        int nextMs = revealStepMs_;
        if (revealStep_ == 8) {
            nextMs += revealPauseMs_;
        }
        revealTimer_->start(nextMs);
    }

    void animateBoardPiecePairs(int stepMs, int pauseMs, bool hideFirst)
    {
        invalidatePieceReveal();
        revealIsMorph_ = false;
        revealStep_ = 0;
        revealStepMs_ = stepMs;
        revealPauseMs_ = pauseMs;
        boardPiecesVisible_ = true;
        if (hideFirst) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    refreshBoardSquareIcon(row, col, false);
                }
            }
        }
        advancePieceReveal();
    }

    void animateBoardPiecesIn()
    {
        animateBoardPiecePairs(kPieceRevealStepMs, kPieceRevealMiddlePauseMs, true);
    }

    void animateBoardPiecesMorph()
    {
        if (!boardPiecesVisible_) {
            animateBoardPiecesIn();
            return;
        }

        // The setup reveal only walks the home ranks. A mid-game set change
        // must retarget every occupied square or moved pieces keep the old set.
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
                    refreshBoardSquareIcon(row, col, true);
                }
            }
        }
        morphMid_ = morphSquares_.size() / 2;
        if (!morphSquares_.isEmpty()) {
            advancePieceReveal();
        }
    }

    void advancePieceFlash()
    {
        ++flashStep_;
        if (flashRow_ < 0 || flashCol_ < 0 || flashStep_ > 5) {
            return;
        }
        refreshBoardSquareIcon(flashRow_, flashCol_, (flashStep_ % 2) != 0);
        if (flashStep_ < 5) {
            flashTimer_->start(kPieceFlashStepMs);
            return;
        }
        refreshBoardSquareIcon(flashRow_, flashCol_, true);
        auto done = std::move(flashDone_);
        flashDone_ = {};
        if (done) {
            done(false);
        }
    }

    void flashPieceAt(int row, int col, std::function<void(bool)> done)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8 ||
            board_[row][col] == '.' || !boardPiecesVisible_) {
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
        refreshBoardSquareIcon(flashRow_, flashCol_, false);
        flashTimer_->start(kPieceFlashStepMs);
    }

    void refreshBoardSquareStyleByName(const QString &square)
    {
        if (square.size() < 2) {
            return;
        }
        refreshBoardSquareVisual('8' - square[1].unicode(),
                                 square[0].unicode() - 'a');
    }

    void refreshSelectionFootprint(int oldSelectedRow,
                                   int oldSelectedCol,
                                   int oldTargetRow,
                                   int oldTargetCol,
                                   const QStringList &oldLegalTargets)
    {
        for (const QString &target : oldLegalTargets) {
            refreshBoardSquareStyleByName(target);
        }
        refreshBoardSquareVisual(oldSelectedRow, oldSelectedCol);
        refreshBoardSquareVisual(oldTargetRow, oldTargetCol);

        for (const QString &target : legalTargets_) {
            refreshBoardSquareStyleByName(target);
        }
        refreshBoardSquareVisual(selectedRow_, selectedCol_);
        refreshBoardSquareVisual(targetRow_, targetCol_);
        refreshTurnLabel();
    }

    void configureSessionControllerCallbacks()
    {
        DesktopSessionController::Callbacks callbacks;
        callbacks.mqttTransportReady = [this]() {
            return mqttSubscribed_ && mqttSideReady_;
        };

        callbacks.send = [this](DesktopSessionController::Mode mode,
                                const SessionAction &action,
                                const QByteArray &payload) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                const QByteArray suffix = sessionController_.mqttTopicSuffixForRoute(
                    action.data.send.route);
                return !suffix.isEmpty() &&
                    mqttPublish(QString::fromLatin1(suffix),
                                QString::fromLatin1(payload),
                                action.data.send.retained != 0u);
            }

            const QPointer<QTcpSocket> sock =
                directSocketForLink(action.data.send.link_id);
            QByteArray frame = payload;
            frame.append('\n');
            bool sent = sock != nullptr &&
                        sock->state() == QAbstractSocket::ConnectedState;
            if (sent) {
                const qint64 written = sock->write(frame);
                sent = written == frame.size();
                if (sent) {
                    sock->flush();
                    appendLog("TX: " + QString::fromLatin1(payload));
                } else {
                    appendLog(QString("ERROR: direct write %1/%2")
                                  .arg(written)
                                  .arg(frame.size()));
                }
            } else {
                appendLog("ERROR: direct link unavailable");
            }
            if (!sent && sock != nullptr) {
                sock->abort();
            }
            return sent;
        };
        callbacks.closeLink = [this](DesktopSessionController::Mode mode,
                                     uint8_t linkId) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                if (socket_ != nullptr) {
                    socket_->flush();
                    socket_->disconnectFromHost();
                }
                return;
            }
            const QPointer<QTcpSocket> sock = directSocketForLink(linkId);
            if (sock != nullptr) {
                sock->disconnectFromHost();
            }
        };
        callbacks.decision = [this](uint8_t requestId, uint8_t control,
                                    uint16_t value) {
            showDirectDecision(requestId, control, value);
        };
        callbacks.game = [this](DesktopSessionController::Mode,
                                uint8_t kind, uint8_t deliveryId,
                                uint16_t value, const QByteArray &payload,
                                QVector<DesktopSessionFollowup> &followups) {
            handleDirectGameAction(kind, deliveryId, value, payload, followups);
        };
        callbacks.sessionChanged = [this](uint8_t status, uint8_t endReason) {
            handleDirectSessionChanged(status, endReason);
        };
        callbacks.sideChanged = [this](DesktopSessionController::Mode mode,
                                       uint8_t color, uint16_t sessionId) {
            if (mode == DesktopSessionController::Mode::Mqtt) {
                handleMqttSideChanged(color, sessionId);
            } else {
                handleDirectSideChanged(color);
            }
        };
        callbacks.error = [this](const QString &message) {
            appendLog("ERROR: " + message);
        };
        sessionController_.setCallbacks(callbacks);
    }

    bool initializeDirectSession()
    {
        closeDirectDecisionPrompt();
        directSockets_.clear();
        transportCodec_.clear();
        directLinkUpSeen_.clear();
        directNextLinkId_ = 0u;
        directPrimaryLinkId_ = SESSION_LINK_NONE;
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        directEndStatus_.clear();
        resetPeerMachineState();

        const uint8_t role = pcIsHost_ ? SESSION_ROLE_HOST : SESSION_ROLE_GUEST;
        const uint8_t hostColor = pcIsHost_
                                      ? (hostPlaysWhite_ ? SESSION_COLOR_WHITE
                                                        : SESSION_COLOR_BLACK)
                                      : SESSION_COLOR_UNKNOWN;
        return sessionController_.initializeDirect(role, hostColor);
    }

    bool initializeMqttSession()
    {
        closeDirectDecisionPrompt();
        transportCodec_.clear();
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        mqttSessionLinked_ = false;
        mqttSideReady_ = pcIsHost_;
        resetPeerMachineState();
        const uint8_t role = pcIsHost_ ? SESSION_ROLE_HOST : SESSION_ROLE_GUEST;
        const uint8_t hostColor = pcIsHost_
                                      ? (hostPlaysWhite_ ? SESSION_COLOR_WHITE
                                                        : SESSION_COLOR_BLACK)
                                      : SESSION_COLOR_UNKNOWN;
        return sessionController_.initializeMqtt(role, hostColor,
                                                 mqttSessionId_);
    }

    void resetPeerMachineState()
    {
        peerPlatform_ = NETCHESS_PLAT_UNKNOWN;
        localMachSent_ = false;
    }

    void setPeerPlatform(uint8_t platform)
    {
        const char *code = netchess_proto_mach_code(platform);

        if (code == nullptr || peerPlatform_ == platform) {
            return;
        }
        peerPlatform_ = platform;
        appendLog("PEER MACHINE: " + QString::fromLatin1(code));
        refreshStatusBar();
    }

    void sendLocalMachAnnouncement()
    {
        if (localMachSent_ || !directSessionReady_) {
            return;
        }
        localMachSent_ = true;

        const QByteArray payload = localMachPayload();
        if (payload.isEmpty()) {
            appendLog("NOTICE: local machine identity unavailable");
            return;
        }

        if (isMqttMode()) {
            const QByteArray suffix =
                sessionController_.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME);
            if (suffix.isEmpty() ||
                !mqttPublish(QString::fromLatin1(suffix),
                             QString::fromLatin1(payload), false)) {
                appendLog("NOTICE: MACH announcement not sent");
            }
            return;
        }

        const QPointer<QTcpSocket> sock =
            directSocketForLink(directPrimaryLinkId_);
        if (sock == nullptr ||
            sock->state() != QAbstractSocket::ConnectedState) {
            appendLog("NOTICE: MACH announcement link unavailable");
            return;
        }

        QByteArray frame = payload;
        frame.append('\n');
        const qint64 written = sock->write(frame);
        if (written == frame.size()) {
            sock->flush();
            appendLog("TX: " + QString::fromLatin1(payload));
            return;
        }

        appendLog(QString("NOTICE: MACH write %1/%2")
                      .arg(written)
                      .arg(frame.size()));
        if (written > 0) {
            /* A partial TCP frame corrupts later line framing. */
            sock->abort();
        }
    }

    bool submitSessionLocalRequest(uint8_t request,
                                   uint16_t value = 0u,
                                   const QByteArray &payload = QByteArray(),
                                   uint8_t phase = SESSION_PHASE_IDLE)
    {
        if (!sessionController_.initialized() ||
            (isMqttMode() && (!mqttSubscribed_ || !mqttSideReady_))) {
            return false;
        }
        if (request != SESSION_REQUEST_CHAT) {
            directUiBusy_ = request;
        }
        const bool accepted =
            sessionController_.submitLocalRequest(request, value, payload, phase);
        if (!accepted && request != SESSION_REQUEST_CHAT) {
            directUiBusy_ = false;
        }
        if (request != SESSION_REQUEST_CHAT) {
            setConnectedUi(isMqttMode() ? isConnected() : directSessionReady_);
        }
        return accepted;
    }

    void submitSessionUserDecision(uint8_t requestId, uint8_t decision)
    {
        if (decision == SESSION_DECISION_REJECT) {
            directUiBusy_ = false;
        }
        sessionController_.submitUserDecision(requestId, decision);
    }

    void submitSessionGameResult(uint8_t deliveryId,
                                 uint16_t value,
                                 uint8_t result,
                                 const QByteArray &detail = QByteArray())
    {
        directUiBusy_ = false;
        sessionController_.submitGameResult(deliveryId, value, result, detail);
        setConnectedUi(isMqttMode() ? isConnected() : directSessionReady_);
    }
    uint8_t directLinkForSocket(QTcpSocket *sock) const
    {
        if (sock == nullptr) {
            return SESSION_LINK_NONE;
        }
        for (auto it = directSockets_.constBegin(); it != directSockets_.constEnd(); ++it) {
            if (it.value() == sock) {
                return it.key();
            }
        }
        return SESSION_LINK_NONE;
    }

    QPointer<QTcpSocket> directSocketForLink(uint8_t linkId) const
    {
        return directSockets_.value(linkId);
    }

    uint8_t registerDirectSocket(QTcpSocket *sock)
    {
        const uint8_t existing = directLinkForSocket(sock);
        if (existing != SESSION_LINK_NONE) {
            return existing;
        }
        for (int attempt = 0; attempt < 255; ++attempt) {
            const uint8_t linkId = directNextLinkId_++;
            if (linkId != SESSION_LINK_NONE && !directSockets_.contains(linkId)) {
                directSockets_.insert(linkId, QPointer<QTcpSocket>(sock));
                directLinkUpSeen_.insert(linkId, false);
                return linkId;
            }
        }
        return SESSION_LINK_NONE;
    }

    void forgetDirectSocket(uint8_t linkId)
    {
        transportCodec_.clearDirect(linkId);
        directLinkUpSeen_.remove(linkId);
        directSockets_.remove(linkId);
        if (directPrimaryLinkId_ == linkId) {
            directPrimaryLinkId_ = SESSION_LINK_NONE;
        }
    }

    void handleDirectSideChanged(uint8_t color)
    {
        if (color != SESSION_COLOR_WHITE && color != SESSION_COLOR_BLACK) {
            return;
        }
        pcPlaysWhite_ = color == SESSION_COLOR_WHITE;
        hostPlaysWhite_ = pcIsHost_ ? pcPlaysWhite_ : !pcPlaysWhite_;
        const bool oldWhiteAtBottom = boardWhiteAtBottom_;
        syncBoardOrientationWithPcSide();
        if (boardPiecesVisible_ && !gameClockRunning_ && !gameOver_ &&
            oldWhiteAtBottom != boardWhiteAtBottom_) {
            boardPiecesVisible_ = false;
            refreshBoard();
            animateBoardPiecesIn();
        } else {
            refreshBoard();
        }
        setConnectedUi(directSessionReady_);
    }

    void handleMqttSideChanged(uint8_t color, uint16_t sessionId)
    {
        if (color != SESSION_COLOR_WHITE && color != SESSION_COLOR_BLACK) {
            return;
        }
        const bool changed = pcPlaysWhite_ != (color == SESSION_COLOR_WHITE);
        mqttSessionId_ = sessionId;
        pcPlaysWhite_ = color == SESSION_COLOR_WHITE;
        hostPlaysWhite_ = pcIsHost_ ? pcPlaysWhite_ : !pcPlaysWhite_;
        if (!mqttSideReady_ || changed) {
            if (mqttSideTransitionPending_ ||
                !mqttSubackPending_.isEmpty() ||
                !mqttUnsubackPending_.isEmpty()) {
                failMqttConnection("MQTT side transition overlapped");
                return;
            }

            QSet<QString> target;
            target.insert(QStringLiteral("meta"));
            target.insert(mqttInSuffix());
            target.insert(mqttInAckSuffix());
            target.insert(mqttPeerPresenceSuffix());
            target.insert(mqttPresenceSuffix());

            QSet<QString> additions = target;
            additions.subtract(mqttActiveSubscriptions_);
            mqttObsoleteSubscriptions_ = mqttActiveSubscriptions_;
            mqttObsoleteSubscriptions_.subtract(target);
            mqttTargetSubscriptions_ = target;
            mqttSideTransitionPending_ = true;
            mqttSubscribed_ = false;
            mqttSideReady_ = false;

            for (const QString &suffix : additions) {
                if (!mqttSubscribe(suffix)) {
                    failMqttConnection("MQTT side subscribe send failed");
                    return;
                }
            }
            advanceMqttSubscriptionTransition();
            return;
        }
        const bool oldWhiteAtBottom = boardWhiteAtBottom_;
        syncBoardOrientationWithPcSide();
        if (boardPiecesVisible_ && !gameClockRunning_ && !gameOver_ &&
            oldWhiteAtBottom != boardWhiteAtBottom_) {
            boardPiecesVisible_ = false;
            refreshBoard();
            animateBoardPiecesIn();
        } else {
            refreshBoard();
        }
        setConnectedUi(isConnected());
    }

    void handleDirectSessionChanged(uint8_t status, uint8_t endReason)
    {
        if (status == SESSION_CHANGED_READY) {
            cancelDirectConnectRetry();
            directSessionReady_ = true;
            directUiBusy_ = false;
            directEndStatus_.clear();
            setStatusText(pcIsHost_ ? NETCHESSZX_UI_NOTICE_OPPONENT_READY_START
                                    : NETCHESSZX_UI_NOTICE_OPPONENT_READY_WAIT_START);
            setConnectedUi(true);
            sendLocalMachAnnouncement();
            return;
        }
        if (status == SESSION_CHANGED_BUSY) {
            cancelDirectConnectRetry();
            directSessionReady_ = false;
            resetPeerMachineState();
            directEndStatus_ = QString::fromLatin1(kDirectHostBusyStatus);
            setStatusText(directEndStatus_);
            setConnectedUi(false);
            return;
        }
        if (status == SESSION_CHANGED_STARTED) {
            directSessionReady_ = true;
            directUiBusy_ = false;
            if (!directStartTransitionApplied_) {
                applyDirectStartTransition();
            }
            return;
        }
        if (status == SESSION_CHANGED_ENDED) {
            const bool hadPeer = directSessionReady_;

            directSessionReady_ = false;
            resetPeerMachineState();
            directUiBusy_ = false;
            directStartTransitionApplied_ = false;
            directLocalResignPending_ = false;
            directResignRestartPending_ = false;
            directPrimaryLinkId_ = SESSION_LINK_NONE;
            closeDirectDecisionPrompt();
            QString ended = directEndStatus_;

            if (endReason == SESSION_END_REASON_LOCAL_BYE) {
                ended = QString::fromLatin1(NETCHESSZX_UI_PHASE_DISCONNECTED);
            } else if (endReason == SESSION_END_REASON_REMOTE_BYE) {
                ended = QString::fromLatin1(
                    NETCHESSZX_UI_PC_EVENT_OPPONENT_LEFT);
            } else if (endReason == SESSION_END_REASON_TRANSPORT_LOST &&
                       (hadPeer || ended.isEmpty())) {
                ended = QString::fromLatin1(
                    NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST);
            } else if (ended.isEmpty()) {
                ended = QString::fromLatin1(
                    NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED);
            }
            directEndStatus_.clear();
            resetGame(ended);
            clearChatLog();
            setStatusText(ended);
            const bool restartMqttSession =
                isMqttMode() && !localDisconnectPending_ &&
                mqttSessionLinked_ && mqttSubscribed_ && mqttSideReady_ &&
                isConnected();
            if (restartMqttSession) {
                (void)sessionController_.linkUp(kMqttLinkId);
            }
            setConnectedUi(restartMqttSession);
        }
    }

    void handleDirectControlResult(uint16_t control, uint8_t result)
    {
        if (result == SESSION_CONTROL_CANCELLED ||
            result == SESSION_CONTROL_EXPIRED) {
            if (control == SESSION_REQUEST_RESTORE) {
                directUiBusy_ = false;
                closeDirectDecisionPrompt();
                setStatusText(QStringLiteral("Load cancelled"));
                setConnectedUi(directSessionReady_);
                return;
            }
            if (control == SESSION_REQUEST_RESET &&
                directResignRestartPending_) {
                directUiBusy_ = false;
                directResignRestartPending_ = false;
                closeDirectDecisionPrompt();
                appendLog(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                setStatusText(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                setConnectedUi(directSessionReady_);
                return;
            }
            const QString name = control == SESSION_REQUEST_RESET
                                     ? QStringLiteral("RESET")
                                     : QStringLiteral("DRAW");
            const QString message = result == SESSION_CONTROL_CANCELLED
                                        ? name + QStringLiteral(" cancelled: no response")
                                        : name + QStringLiteral(" request expired");

            directUiBusy_ = false;
            closeDirectDecisionPrompt();
            appendLog(message);
            setStatusText(message);
            setConnectedUi(directSessionReady_);
            return;
        }
        const bool rejected = result == SESSION_CONTROL_REJECTED;
        switch (control) {
        case SESSION_REQUEST_START:
            directUiBusy_ = false;
            if (rejected) {
                setStatusText(NETCHESSZX_UI_ERROR_START_REJECTED_BY_OPPONENT);
            }
            break;
        case SESSION_REQUEST_MOVE:
            directUiBusy_ = false;
            if (rejected) {
                pcTurn_ = true;
                setStatusText(QStringLiteral("Move rejected by opponent"));
            }
            break;
        case SESSION_REQUEST_RESET:
            if (rejected) {
                directUiBusy_ = false;
                if (directResignRestartPending_) {
                    directResignRestartPending_ = false;
                    appendLog(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                    setStatusText(NETCHESSZX_UI_ERROR_RESTART_FAILED_GAME_OVER);
                } else {
                    setStatusText(gameOver_ ? NETCHESSZX_UI_ERROR_RESTART_REJECTED
                                            : NETCHESSZX_UI_ERROR_RESET_REJECTED);
                }
            } else {
                applyDirectStartTransition();
            }
            break;
        case SESSION_REQUEST_DRAW:
            if (rejected) {
                directUiBusy_ = false;
                setStatusText(NETCHESSZX_UI_ERROR_DRAW_REJECTED);
            } else {
                const QString result =
                    QString::fromLatin1(NETCHESSZX_UI_EVENT_DRAW_AGREED);
                appendControlEvent(true, result);
                endGameOver(result);
            }
            break;
        case SESSION_REQUEST_RESIGN:
            directLocalResignPending_ = false;
            if (rejected) {
                directUiBusy_ = false;
                directResignRestartPending_ = false;
            } else {
                directUiBusy_ = SESSION_REQUEST_RESET;
                directResignRestartPending_ = true;
                setStatusText(NETCHESSZX_UI_NOTICE_RESTARTING_GAME);
            }
            break;
        case SESSION_REQUEST_TAKEBACK:
            directUiBusy_ = false;
            setStatusText(rejected ? QStringLiteral("Takeback rejected")
                                   : QStringLiteral("Takeback accepted"));
            break;
        case SESSION_REQUEST_RESTORE:
            directUiBusy_ = false;
            closeDirectDecisionPrompt();
            if (rejected) {
                setStatusText(QStringLiteral("Load declined"));
            }
            break;
        default:
            break;
        }
        setConnectedUi(directSessionReady_);
    }

    void handleDirectGameAction(uint8_t kind,
                                uint8_t deliveryId,
                                uint16_t value,
                                const QByteArray &payload,
                                QVector<DesktopSessionFollowup> &followups)
    {
        switch (kind) {
        case SESSION_DELIVER_REMOTE_MOVE: {
            directUiBusy_ = true;
            setConnectedUi(directSessionReady_);
            QByteArray failure;
            if (!applyDirectRemoteMoveAnimated(deliveryId, value,
                                               QString::fromLatin1(payload).toLower(),
                                               &failure)) {
                followups.append(DesktopSessionFollowup{
                    SESSION_EV_GAME_RESULT, deliveryId, SESSION_GAME_REJECTED,
                    value, failure});
                directUiBusy_ = false;
                setConnectedUi(directSessionReady_);
            }
            break;
        }
        case SESSION_DELIVER_LOCAL_MOVE:
            directUiBusy_ = false;
            applyDirectLocalMove(value, QString::fromLatin1(payload).toLower());
            break;
        case SESSION_DELIVER_CHAT:
            appendChat(value == SESSION_CHAT_LOCAL ? pcChatName()
                                                   : opponentChatName(),
                       QString::fromLatin1(payload));
            break;
        case SESSION_DELIVER_PLATFORM:
            setPeerPlatform(static_cast<uint8_t>(value));
            break;
        case SESSION_DELIVER_CONTROL:
            if (value == SESSION_REQUEST_RESET) {
                appendControlEvent(false, QStringLiteral("RESET"));
                applyDirectStartTransition();
            } else if (value == SESSION_REQUEST_DRAW) {
                const QString result =
                    QString::fromLatin1(NETCHESSZX_UI_EVENT_DRAW_AGREED);
                appendControlEvent(false, result);
                endGameOver(result);
            } else if (value == SESSION_REQUEST_RESIGN) {
                applyDirectResignTransition();
            }
            break;
        case SESSION_DELIVER_CONTROL_RESULT:
            handleDirectControlResult(value, deliveryId);
            break;
        case SESSION_DELIVER_RESTORE:
            if (deliveryId != 0u) {
                uint16_t restoredPly = 0u;
                uint8_t restoredPhase = SESSION_PHASE_IDLE;
                const bool accepted = applyDirectRestore(
                    payload, &restoredPly, &restoredPhase);
                if (accepted) {
                    appendControlEvent(false, QStringLiteral("RESTORE"));
                }
                followups.append(DesktopSessionFollowup{
                    SESSION_EV_GAME_RESULT, deliveryId,
                    static_cast<uint8_t>(accepted ? SESSION_GAME_ACCEPTED
                                                  : SESSION_GAME_REJECTED),
                    restoredPly,
                    accepted
                        ? QByteArray(1, static_cast<char>(restoredPhase))
                        : QByteArray("INVALID")});
            } else {
                uint16_t restoredPly = 0u;
                const bool accepted = applyDirectRestore(
                    payload, &restoredPly, nullptr);
                if (accepted) {
                    appendControlEvent(true, QStringLiteral("RESTORE"));
                }
                if (!accepted || restoredPly != value) {
                    appendLog("ERROR: restored ply differs from session core");
                }
            }
            directUiBusy_ = false;
            setConnectedUi(directSessionReady_);
            break;
        case SESSION_DELIVER_TAKEBACK: {
            const bool local = takebackSnapshot_.valid &&
                               takebackSnapshot_.localMove;
            const bool accepted = applyDirectTakeback(value);
            if (accepted) {
                appendControlEvent(local, QStringLiteral("TAKEBACK"));
            }
            followups.append(DesktopSessionFollowup{
                SESSION_EV_GAME_RESULT, deliveryId,
                static_cast<uint8_t>(accepted ? SESSION_GAME_ACCEPTED
                                              : SESSION_GAME_REJECTED),
                value,
                QByteArray()});
            directUiBusy_ = false;
            setConnectedUi(directSessionReady_);
            break;
        }
        default:
            appendLog(QString("ERROR: unknown direct game action %1").arg(kind));
            break;
        }
    }

    void closeDirectDecisionPrompt()
    {
        directDecisionRequestId_ = 0u;
        directDecisionControl_ = 0u;
        if (directDecisionBox_ != nullptr) {
            QMessageBox *box = directDecisionBox_;
            directDecisionBox_.clear();
            box->close();
        }
    }

    void applyDirectResignTransition()
    {
        const bool local = directLocalResignPending_;
        const QString result = QString::fromLatin1(
            local ? NETCHESSZX_UI_EVENT_RESIGNATION_LOST
                  : NETCHESSZX_UI_EVENT_RESIGNATION_WON);
        invalidatePieceFlash();
        invalidatePieceReveal();
        invalidateDestinationFeedback();
        closeDirectDecisionPrompt();
        directResignRestartPending_ = true;
        directUiBusy_ = local ? SESSION_REQUEST_RESIGN : SESSION_REQUEST_RESET;
        appendControlEvent(local, result);
        endGameOver(result);
        setStatusText(local ? NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK : result);
    }

    void applyDirectStartTransition()
    {
        if (directStartTransitionApplied_) {
            return;
        }
        directStartTransitionApplied_ = true;
        directUiBusy_ = false;
        startGameFromAck();
        QTimer::singleShot(0, this, [this]() {
            directStartTransitionApplied_ = false;
        });
    }

    void showDirectDecision(uint8_t requestId, uint8_t control, uint16_t value)
    {
        if (resetPromptOpen_ ||
            (control == SESSION_REQUEST_TAKEBACK &&
            (!takebackSnapshot_.valid || takebackSnapshot_.localMove ||
             takebackSnapshot_.ply != value || value != nextPly_ - 1))) {
            QTimer::singleShot(0, this, [this, requestId]() {
                submitSessionUserDecision(requestId, SESSION_DECISION_REJECT);
            });
            return;
        }

        closeDirectDecisionPrompt();
        directUiBusy_ = true;
        directDecisionRequestId_ = requestId;
        directDecisionControl_ = control;

        QString title;
        QString message;
        if (control == SESSION_REQUEST_DRAW) {
            title = NETCHESSZX_UI_CONFIRM_PC_DRAW_TITLE;
            message = NETCHESSZX_UI_CONFIRM_PC_ACCEPT_DRAW;
            setStatusText(NETCHESSZX_UI_NOTICE_DRAW_OFFERED);
        } else if (control == SESSION_REQUEST_RESET) {
            title = gameOver_ ? NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE
                              : NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE;
            message = gameOver_ ? NETCHESSZX_UI_CONFIRM_PC_RESTART_REQUEST
                                : NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST_LONG;
            setStatusText(gameOver_ ? NETCHESSZX_UI_CONFIRM_RESTART_REQUEST
                                    : NETCHESSZX_UI_CONFIRM_RESET_REQUEST);
        } else if (control == SESSION_REQUEST_TAKEBACK) {
            title = NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_TITLE;
            message = NETCHESSZX_UI_CONFIRM_PC_ACCEPT_TAKEBACK;
            setStatusText(NETCHESSZX_UI_CONFIRM_TAKEBACK_REQUEST);
        } else if (control == SESSION_REQUEST_RESTORE) {
            title = QStringLiteral("Load Game");
            message = QStringLiteral("Opponent wants to load a saved game. Accept?");
            setStatusText(QStringLiteral("Load requested"));
        } else {
            QTimer::singleShot(0, this, [this, requestId]() {
                submitSessionUserDecision(requestId, SESSION_DECISION_REJECT);
            });
            return;
        }

        setConnectedUi(directSessionReady_);
        auto *box = new QMessageBox(QMessageBox::NoIcon, title, message,
                                    QMessageBox::Yes | QMessageBox::No, this);
        box->setAttribute(Qt::WA_DeleteOnClose);
        directDecisionBox_ = box;
        connect(box, &QDialog::finished, this, [this, requestId, control](int result) {
            if (directDecisionRequestId_ != requestId ||
                directDecisionControl_ != control) {
                return;
            }
            directDecisionBox_.clear();
            directDecisionRequestId_ = 0u;
            directDecisionControl_ = 0u;
            const bool accepted = result == QMessageBox::Yes;
            submitSessionUserDecision(requestId, accepted ? SESSION_DECISION_ACCEPT
                                                          : SESSION_DECISION_REJECT);
            if (!accepted) {
                setStatusText(control == SESSION_REQUEST_RESTORE
                                  ? QStringLiteral("Load declined")
                                  : QStringLiteral("Request rejected"));
                setConnectedUi(directSessionReady_);
            }
        });
        box->open();
    }

    void attachSocket(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        connect(sock, &QTcpSocket::connected, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                handleSocketConnected(sock);
            }
        });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE) {
                handleSocketDisconnected(sock);
            } else if (sock == socket_) {
                if (ignoreNextDisconnect_) {
                    ignoreNextDisconnect_ = false;
                    return;
                }
                handleSocketDisconnected(sock);
            }
        });
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                consumeReadyRead(sock);
            }
        });
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError) {
            if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                handleSocketError(sock);
            }
        });
#else
        connect(sock, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this, [this, sock](QAbstractSocket::SocketError) {
                    if (directLinkForSocket(sock) != SESSION_LINK_NONE || sock == socket_) {
                        handleSocketError(sock);
                    }
                });
#endif
    }

    void cancelDirectConnectRetry()
    {
        if (directConnectRetryTimer_ != nullptr) {
            directConnectRetryTimer_->stop();
        }
        directConnectRetryActive_ = false;
        directConnectRetryCount_ = 0;
    }

    bool scheduleDirectConnectRetry(QTcpSocket *sock)
    {
        if (sock != socket_ || !directConnectRetryActive_ ||
            isMqttMode() || pcIsHost_ || directSessionReady_) {
            return false;
        }
        if (directConnectRetryTimer_->isActive()) {
            return true;
        }

        ++directConnectRetryCount_;
        directConnectRetryTimer_->start();
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            if (directLinkUpSeen_.value(directLink, false)) {
                (void)sessionController_.linkDown(directLink);
            }
            forgetDirectSocket(directLink);
        }
        sock->abort();
        directEndStatus_.clear();
        lastSocketError_.clear();
        setStatusText(QString("Opponent not ready - retry %1 in 2 s")
                          .arg(directConnectRetryCount_));
        setConnectedUi(false);
        return true;
    }

    void retryDirectConnection()
    {
        if (!directConnectRetryActive_ || isMqttMode() || pcIsHost_ ||
            socket_->state() != QAbstractSocket::UnconnectedState) {
            cancelDirectConnectRetry();
            setConnectedUi(isConnected());
            return;
        }

        const uint8_t linkId = registerDirectSocket(socket_);
        if (linkId == SESSION_LINK_NONE) {
            cancelDirectConnectRetry();
            resetGame(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            clearChatLog();
            setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            setConnectedUi(false);
            return;
        }

        directPrimaryLinkId_ = linkId;
        appendLog(QString("RETRY %1 CONNECT %2:%3")
                      .arg(directConnectRetryCount_)
                      .arg(hostEdit_->text().trimmed())
                      .arg(portSpin_->value()));
        setStatusText(NETCHESSZX_UI_NOTICE_CONNECTING_PC);
        socket_->connectToHost(hostEdit_->text().trimmed(),
                               static_cast<quint16>(portSpin_->value()));
        setConnectedUi(false);
    }

    void handleSocketError(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        const QString socketError = sock->errorString();
        appendLog("ERROR: " + socketError);
        if (scheduleDirectConnectRetry(sock)) {
            return;
        }
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            const bool primary = directLink == directPrimaryLinkId_ ||
                                 sock == socket_;
            if (!primary) {
                return;
            }
            lastSocketError_ = socketError;
            const QString status = directSessionReady_
                                       ? QString::fromLatin1(
                                             NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST)
                                       : socketError.contains(
                                             "refused", Qt::CaseInsensitive)
                                             ? "Connection refused - opponent not ready"
                                             : "Connection failed - " + socketError;
            directEndStatus_ = status;
            setStatusText(status);
            if (!directLinkUpSeen_.value(directLink, false) && sock == socket_) {
                sock->abort();
                resetGame(status);
                clearChatLog();
                setConnectedUi(false);
            }
            return;
        }
        if (!isMqttMode()) {
            return;
        }
        lastSocketError_ = socketError;
        setStatusText(lastSocketError_);
        if (!isConnected()) {
            setConnectedUi(false);
        }
    }

    void handleSocketConnected(QTcpSocket *sock)
    {
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            directLinkUpSeen_[directLink] = true;
            if (directLink == directPrimaryLinkId_) {
                pcTurn_ = false;
                clearSelection();
                chatLogEdit_->clear();
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_OPPONENT_APP);
                setConnectedUi(false);
                refreshBoard();
                appendLog("CONNECTED TCP");
            }
            (void)sessionController_.linkUp(directLink);
            return;
        }
        if (sock != socket_ || !isMqttMode()) {
            return;
        }
        linkWatch_.restart();
        pcTurn_ = false;
        clearSelection();
        chatLogEdit_->clear();
        setConnectedUi(true);
        mqttHandshake();
    }

    void handleSocketDisconnected(QTcpSocket *sock)
    {
        if (scheduleDirectConnectRetry(sock)) {
            return;
        }
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink != SESSION_LINK_NONE) {
            const bool wasLinked = directLinkUpSeen_.value(directLink, false);
            const bool primary = directLink == directPrimaryLinkId_ ||
                                 sock == socket_;
            if (primary && directEndStatus_.isEmpty()) {
                directEndStatus_ = wasLinked
                                       ? QString::fromLatin1(
                                             NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST)
                                       : lastSocketError_.isEmpty()
                                             ? QString::fromLatin1(
                                                   NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST)
                                             : lastSocketError_;
            }
            if (wasLinked) {
                (void)sessionController_.linkDown(directLink);
            }
            forgetDirectSocket(directLink);
            if (!primary) {
                sock->deleteLater();
            } else if (pcIsHost_ && directServer_ != nullptr &&
                       directServer_->isListening()) {
                setConnectedUi(false);
            }
            lastSocketError_.clear();
            appendLog("DISCONNECTED");
            return;
        }
        if (sock != socket_ || !isMqttMode()) {
            return;
        }
        QString status;

        if (localDisconnectPending_) {
            status = NETCHESSZX_UI_PHASE_DISCONNECTED;
            localDisconnectPending_ = false;
        } else if (!lastSocketError_.isEmpty()) {
            status = NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST;
        } else {
            status = NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST;
        }
        clearMqttSubscriptionState();
        bool handled = false;
        if (mqttSessionLinked_) {
            mqttSessionLinked_ = false;
            directEndStatus_ = status;
            handled = sessionController_.linkDown(kMqttLinkId);
        }
        if (handled) {
            lastSocketError_.clear();
            appendLog("DISCONNECTED");
            return;
        }
        resetGame(status);
        clearChatLog();
        setConnectedUi(false);
        setStatusText(status);
        lastSocketError_.clear();
        appendLog("DISCONNECTED");
    }

    void disconnectToSetup()
    {
        cancelDirectConnectRetry();
        if (!isMqttMode() && pcIsHost_ && directServer_ != nullptr) {
            directServer_->close();
        }
        if (isConnected()) {
            if (isMqttMode()) {
                localDisconnectPending_ = true;
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                    socket_->disconnectFromHost();
                }
            } else {
                directEndStatus_ = NETCHESSZX_UI_PHASE_DISCONNECTED;
                if (!submitSessionLocalRequest(SESSION_REQUEST_BYE)) {
                    socket_->disconnectFromHost();
                }
                return;
            }
        } else if (isDirectListening()) {
            directServer_->close();
        } else if (isConnecting() && socket_ != nullptr) {
            socket_->abort();
        }
        resetGame(NETCHESSZX_UI_PHASE_DISCONNECTED);
        stopGameClock();
        clearChatLog();
        setConnectedUi(false);
        setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
    }

    void failMqttConnection(const QString &status)
    {
        const bool linked = mqttSessionLinked_;
        clearMqttSubscriptionState();
        mqttSessionLinked_ = false;
        if (linked) {
            directEndStatus_ = status;
            (void)sessionController_.linkDown(kMqttLinkId);
        }
        ignoreNextDisconnect_ = true;
        appendLog("ERROR: " + status);
        if (socket_ != nullptr) {
            socket_->abort();
        }
        resetGame(status);
        clearChatLog();
        setStatusText(status);
        setConnectedUi(false);
    }

    void acceptDirectClient()
    {
        QTcpSocket *accepted = directServer_->nextPendingConnection();
        if (accepted == nullptr) {
            return;
        }
        const uint8_t linkId = registerDirectSocket(accepted);
        if (linkId == SESSION_LINK_NONE) {
            accepted->disconnectFromHost();
            accepted->deleteLater();
            return;
        }
        attachSocket(accepted);
        if (accepted->state() != QAbstractSocket::ConnectedState) {
            forgetDirectSocket(linkId);
            accepted->deleteLater();
            return;
        }
        if (directPrimaryLinkId_ == SESSION_LINK_NONE) {
            if (socket_ != nullptr && socket_ != accepted) {
                const uint8_t oldLink = directLinkForSocket(socket_);
                if (oldLink != SESSION_LINK_NONE) {
                    forgetDirectSocket(oldLink);
                }
                socket_->deleteLater();
            }
            socket_ = accepted;
            directPrimaryLinkId_ = linkId;
        }
        appendLog(QString("ACCEPT %1:%2")
                      .arg(accepted->peerAddress().toString())
                      .arg(accepted->peerPort()));
        handleSocketConnected(accepted);
    }

    bool isFeedbackSquare(int row, int col) const
    {
        return feedbackOn_ && row == feedbackRow_ && col == feedbackCol_;
    }

    static QString squareStyle(int row, int col, bool selected, bool target,
                               bool legalTarget, bool feedback, bool hasPiece = false,
                               bool showHints = true,
                               PieceRenderer::SquareCorner corner = PieceRenderer::CornerNone)
    {
        const bool light = ((row + col) % 2) == 0;
        const bool texturedBoard = !PieceRenderer::boardTexture().isEmpty();
        QString bg;
        QString border;

        if (feedback) {
            bg = SHZ_SQ_HIT;
            border = "4px solid " SHZ_SQ_HIT_EDGE;
        } else if (selected) {
            bg = SHZ_SQ_SEL;
            border = "3px solid " SHZ_SQ_SEL_EDGE;
        } else if (target) {
            if (legalTarget && showHints && !hasPiece && !texturedBoard) {
                const QString dotColor = "rgba(0, 0, 0, 0.28)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 " SHZ_SQ_TGT ", stop:1.0 " SHZ_SQ_TGT ")").arg(dotColor);
            } else {
                bg = SHZ_SQ_TGT;
            }
            border = "3px solid " SHZ_SQ_TGT_EDGE;
        } else if (legalTarget && showHints) {
            if (hasPiece) {
                bg = light ? SHZ_SQ_HINT_L : SHZ_SQ_HINT_D;
                border = "2px solid " SHZ_SQ_HINT_EDGE;
            } else {
                const QString baseBg = light ? SHZ_SQ_LIGHT : SHZ_SQ_DARK;
                const QString dotColor = "rgba(0, 0, 0, 0.25)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 %2, stop:1.0 %2)").arg(dotColor, baseBg);
                border = "1px solid " SHZ_SQ_EDGE;
            }
        } else {
            bg = light ? SHZ_SQ_LIGHT : SHZ_SQ_DARK;
            border = "1px solid " SHZ_SQ_EDGE;
        }

        const QString fg = light || selected || target || feedback ? "#1e1e1e" : "#ffffff";
        return PieceRenderer::boardSquareStyle(row, col, kBoardSquareSize, bg, fg, border,
                                               corner);
    }

    void advanceDestinationFeedback()
    {
        ++feedbackStep_;
        feedbackOn_ = (feedbackStep_ % 2) == 0;
        refreshBoardSquareVisual(feedbackRow_, feedbackCol_);
        if (feedbackStep_ < 5) {
            feedbackTimer_->start(kPieceFlashStepMs);
            return;
        }
        const int row = feedbackRow_;
        const int col = feedbackCol_;
        feedbackRow_ = -1;
        feedbackCol_ = -1;
        feedbackOn_ = false;
        refreshBoardSquareVisual(row, col);
    }

    void showDestinationFeedback(int row, int col)
    {
        invalidateDestinationFeedback();
        feedbackRow_ = row;
        feedbackCol_ = col;
        feedbackOn_ = true;
        feedbackStep_ = 0;
        refreshBoardSquareVisual(row, col);
        feedbackTimer_->start(kPieceFlashStepMs);
    }

    QStringList legalTargetsFrom(const QString &from)
    {
        char targets[96];
        const QByteArray fromBytes = from.toLatin1();
        const int rc = netchesszx_rules_legal_targets(fromBytes.constData(), targets, sizeof(targets));

        if (rc != NETCHESSZX_OK) {
            appendLog(QString("ERROR: target list failed for %1 (%2)")
                          .arg(from, QString::fromLatin1(netchesszx_error_string(rc))));
            return {};
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return QString::fromLatin1(targets).split(' ', Qt::SkipEmptyParts);
#else
        return QString::fromLatin1(targets).split(' ', QString::SkipEmptyParts);
#endif
    }

    void clearSelection()
    {
        selectedRow_ = -1;
        selectedCol_ = -1;
        targetRow_ = -1;
        targetCol_ = -1;
        legalTargets_.clear();
    }

    void clearSelectionVisuals()
    {
        const int oldSelectedRow = selectedRow_;
        const int oldSelectedCol = selectedCol_;
        const int oldTargetRow = targetRow_;
        const int oldTargetCol = targetCol_;
        const QStringList oldLegalTargets = legalTargets_;
        clearSelection();
        refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                  oldTargetRow, oldTargetCol, oldLegalTargets);
    }

    void squareClicked(int displayRow, int displayCol)
    {
        const int row = boardRowForDisplay(displayRow);
        const int col = boardColForDisplay(displayCol);
        const QString clicked = ChessHelpers::squareName(row, col);
        const int oldSelectedRow = selectedRow_;
        const int oldSelectedCol = selectedCol_;
        const int oldTargetRow = targetRow_;
        const int oldTargetCol = targetCol_;
        const QStringList oldLegalTargets = legalTargets_;

        if (!canPcMove()) {
            clearSelection();
            selectedLabel_->setText("Selected: none");
            if (socket_->state() != QAbstractSocket::ConnectedState) {
                setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
                appendLog(QString("CLICK: %1 ignored, not connected").arg(clicked));
            } else if (directUiBusy_) {
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
                appendLog(QString("CLICK: %1 ignored, ACK pending").arg(clicked));
            } else {
                setStatusBarText("Not your turn");
                appendLog(QString("CLICK: %1 ignored, opponent turn").arg(clicked));
            }
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        if (selectedRow_ < 0) {
            if (board_[row][col] == '.') {
                selectedLabel_->setText(QString("Selected: none (%1 empty)").arg(clicked));
                setStatusText(QString("%1 is empty").arg(clicked));
                appendLog(QString("CLICK: %1 empty").arg(clicked));
                return;
            }

            if (!isPcPiece(board_[row][col])) {
                selectedLabel_->setText("Selected: none");
                setStatusText(QString("You play %1").arg(pcSideName()));
                appendLog(QString("CLICK: %1 ignored, not your piece").arg(clicked));
                return;
            }

            legalTargets_ = legalTargetsFrom(clicked);
            if (legalTargets_.isEmpty()) {
                clearSelection();
                selectedLabel_->setText(QString("Selected: none (%1 has no legal moves)").arg(clicked));
                setStatusText(QString("%1 has no legal moves").arg(clicked));
                appendLog(QString("CLICK: %1 no legal targets").arg(clicked));
                refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                          oldTargetRow, oldTargetCol,
                                          oldLegalTargets);
                return;
            }

            selectedRow_ = row;
            selectedCol_ = col;
            targetRow_ = -1;
            targetCol_ = -1;
            selectedLabel_->setText(QString("Selected: %1").arg(clicked));
            chatEdit_->clear();
            refreshChatButton();
            setStatusText(QString("Selected %1 (%2 legal)")
                                      .arg(clicked).arg(legalTargets_.size()));
            appendLog(QString("CLICK: selected %1 targets %2")
                          .arg(clicked, legalTargets_.join(',')));
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        if (selectedRow_ == row && selectedCol_ == col) {
            clearSelection();
            selectedLabel_->setText("Selected: none");
            setStatusText(NETCHESSZX_UI_NOTICE_SELECTION_CLEARED);
            appendLog(QString("CLICK: cleared %1").arg(clicked));
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        if (isPcPiece(board_[row][col])) {
            legalTargets_ = legalTargetsFrom(clicked);
            if (legalTargets_.isEmpty()) {
                clearSelection();
                selectedLabel_->setText(QString("Selected: none (%1 has no legal moves)").arg(clicked));
                setStatusText(QString("%1 has no legal moves").arg(clicked));
                appendLog(QString("CLICK: %1 no legal targets").arg(clicked));
                refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                          oldTargetRow, oldTargetCol,
                                          oldLegalTargets);
                return;
            }

            selectedRow_ = row;
            selectedCol_ = col;
            targetRow_ = -1;
            targetCol_ = -1;
            selectedLabel_->setText(QString("Selected: %1").arg(clicked));
            chatEdit_->clear();
            refreshChatButton();
            setStatusText(QString("Selected %1 (%2 legal)")
                                      .arg(clicked).arg(legalTargets_.size()));
            appendLog(QString("CLICK: selected %1 targets %2")
                          .arg(clicked, legalTargets_.join(',')));
            refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                      oldTargetRow, oldTargetCol,
                                      oldLegalTargets);
            return;
        }

        if (!isLegalTarget(row, col)) {
            setStatusText(QString("Illegal target: %1").arg(clicked));
            appendLog(QString("CLICK: illegal target %1 for %2")
                          .arg(clicked, ChessHelpers::squareName(selectedRow_, selectedCol_)));
            return;
        }

        const char movingPiece = board_[selectedRow_][selectedCol_];
        QString promotionSuffix;
        if ((movingPiece == 'P' && row == 0) ||
            (movingPiece == 'p' && row == 7)) {
            const int promoGeneration = gameGeneration_;
            const int promoSelectedRow = selectedRow_;
            const int promoSelectedCol = selectedCol_;
            promotionSuffix = askPromotionPiece();
            if (promotionSuffix.isEmpty()) {
                return;
            }
            // exec() ran a nested event loop: session signals may have reset or
            // restored the game while the dialog was open.
            if (gameGeneration_ != promoGeneration ||
                !canPcMove() ||
                selectedRow_ != promoSelectedRow ||
                selectedCol_ != promoSelectedCol ||
                board_[selectedRow_][selectedCol_] != movingPiece ||
                !isLegalTarget(row, col)) {
                appendLog("CLICK: promotion aborted, game state changed");
                return;
            }
        }

        const QString move = ChessHelpers::squareName(selectedRow_, selectedCol_) + ChessHelpers::squareName(row, col) + promotionSuffix;
        moveEdit_->setText(move);
        targetRow_ = row;
        targetCol_ = col;
        selectedLabel_->setText(QString("Selected: %1 -> %2")
                                    .arg(ChessHelpers::squareName(selectedRow_, selectedCol_), clicked));
        setStatusBarText(QStringLiteral("Move ready"));
        refreshChatButton();
        appendLog(QString("CLICK: move %1").arg(move));
        refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                  oldTargetRow, oldTargetCol,
                                  oldLegalTargets);
        refreshTurnLabel();
    }

    QString askPromotionPiece()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("Promote pawn");
        dialog.setModal(true);
        dialog.resize(240, 80);
        dialog.setMinimumSize(240, 80);
        dialog.setStyleSheet(
            "QDialog { background:#1e1f2c; }"
            "QPushButton { background:#292a36; color:#f2f2f0;"
            " border:1px solid #444654; border-radius:" SHZ_R ";"
            " font-weight:700; font-size:18px; padding:8px 12px; min-width:44px; }"
            "QPushButton:hover { background:#343646; }");

        auto *layout = new QHBoxLayout(&dialog);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        QString result;
        const struct { const char *label; const char *piece; } choices[] = {
            {"Q", "q"}, {"R", "r"}, {"B", "b"}, {"N", "n"},
        };
        for (const auto &ch : choices) {
            auto *btn = new QPushButton(ch.label, &dialog);
            const QString piece = QString::fromLatin1(ch.piece);
            connect(btn, &QPushButton::clicked, [&result, &dialog, piece]() {
                result = piece;
                dialog.accept();
            });
            layout->addWidget(btn);
        }

        dialog.exec();
        return result;
    }

    void connectToOpponent()
    {
        cancelDirectConnectRetry();
        const QString host = hostEdit_->text().trimmed();
        const quint16 port = static_cast<quint16>(portSpin_->value());
        const QString room = roomEdit_->text().trimmed().toUpper();

        configureSessionFromUi();

        if (host.isEmpty() && (isMqttMode() || !pcIsHost_)) {
            appendLog("ERROR: empty host");
            setStatusText(isMqttMode() ? NETCHESSZX_UI_ERROR_EMPTY_BROKER : NETCHESSZX_UI_ERROR_INVALID_IP);
            return;
        }
        if (!isMqttMode() && !pcIsHost_ && !ChessHelpers::isDirectIpSyntaxOk(host)) {
            appendLog("ERROR: invalid direct IP");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_IP);
            setConnectedUi(false);
            return;
        }
        if (isMqttMode() && room.isEmpty()) {
            appendLog("ERROR: empty MQTT room");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_ROOM);
            return;
        }
        if (isMqttMode() && !ChessHelpers::isMqttRoomSyntaxOk(room)) {
            appendLog("ERROR: MQTT room must be NC plus four hexadecimal characters");
            setStatusText(NETCHESSZX_UI_ERROR_INVALID_MQTT_ROOM);
            return;
        }
        if (isMqttMode() && roomEdit_->text() != room) {
            roomEdit_->setText(room);
        }
        if (socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->abort();
        }
        if (directServer_ != nullptr && directServer_->isListening()) {
            directServer_->close();
        }

        QSettings settings;
        if (isMqttMode()) {
            mqttBrokerCache_ = host;
        } else if (!pcIsHost_) {
            directIpCache_ = host;
            rememberDirectGuestIp(settings, host);
        }
        const QString savedConnectionHost =
            (!isMqttMode() && pcIsHost_) ?
                (directIpCache_.isEmpty() ? QStringLiteral("192.168.0.") : directIpCache_) :
                host;

        settings.setValue("connection/host", savedConnectionHost);
        settings.setValue("connection/port", port);
        settings.setValue("connection/mqtt", isMqttMode());
        settings.setValue("connection/room", room);
        settings.setValue("connection/pcHost", pcIsHost_);
        settings.setValue("connection/hostWhite", hostPlaysWhite_);

        transportCodec_.clearMqtt();
        clearMqttSubscriptionState();
        mqttSessionId_ = (isMqttMode() && pcIsHost_) ? newMqttSessionId() : 0;
        lastSocketError_.clear();
        mqttNextPacketId_ = 1;
        mqttRoom_ = room;

        if ((isMqttMode() && !initializeMqttSession()) ||
            (!isMqttMode() && !initializeDirectSession())) {
            appendLog("ERROR: session init failed");
            setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
            setConnectedUi(false);
            return;
        }

        if (!isMqttMode() && pcIsHost_) {
            appendLog(QString("LISTEN :%1").arg(port));
            appendLog(QString("SESSION HOST host=%1 pc=%2")
                          .arg(hostSideLetter(), pcSideLetter()));
            if (!directServer_->listen(QHostAddress::Any, port)) {
                appendLog("ERROR: listen failed: " + directServer_->errorString());
                setStatusText(NETCHESSZX_UI_ERROR_LISTEN_FAILED);
                setConnectedUi(false);
                return;
            }
            setStatusText(NETCHESSZX_UI_NOTICE_LISTENING_OPPONENT);
            setConnectedUi(false);
            return;
        }

        appendLog(QString("CONNECT %1:%2").arg(host).arg(port));
        if (isMqttMode()) {
            appendLog(pcIsHost_
                          ? QString("SESSION HOST host=%1 pc=%2")
                                .arg(hostSideLetter(), pcSideLetter())
                          : QString("SESSION GUEST host=auto pc=auto"));
        }
        setStatusText(NETCHESSZX_UI_NOTICE_CONNECTING_PC);
        if (!isMqttMode()) {
            const uint8_t linkId = registerDirectSocket(socket_);
            if (linkId == SESSION_LINK_NONE) {
                appendLog("ERROR: no direct link id available");
                setStatusText(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
                setConnectedUi(false);
                return;
            }
            directPrimaryLinkId_ = linkId;
            directConnectRetryActive_ = !pcIsHost_;
        }
        socket_->connectToHost(host, port);
        setConnectedUi(false);
    }

    void resetGame(const QString &status)
    {
        ++gameGeneration_;
        invalidatePieceFlash();
        invalidatePieceReveal();
        invalidateDestinationFeedback();
        stopGameClock();
        clearTakebackState();
        closeControlPrompt();
        closeDirectDecisionPrompt();
        directUiBusy_ = false;
        gameOver_ = false;
        gameCheck_ = false;
        if (!isMqttMode()) {
            transportCodec_.clearMqtt();
            clearMqttSubscriptionState();
        }
        pcTurn_ = false;
        lastMove_.clear();
        clearMoveHistory();
        boardPiecesVisible_ = false;
        clearSelection();
        netchesszx_rules_reset();
        (void)syncBoardFromRules();
        nextPly_ = 1;
        moveEdit_->clear();
        selectedLabel_->setText("Selected: none");
        setStatusText(status);
        refreshBoard();
        animateBoardPiecesIn();
        refreshTurnLabel();
    }

    static void splitElapsed(qint64 elapsedMs, uint8_t *hour,
                             uint8_t *minute, uint8_t *second)
    {
        qint64 total = elapsedMs > 0 ? elapsedMs / 1000 : 0;
        if (total > kMaxClockSeconds) {
            total = kMaxClockSeconds;
        }
        *hour = static_cast<uint8_t>(total / 3600);
        *minute = static_cast<uint8_t>((total / 60) % 60);
        *second = static_cast<uint8_t>(total % 60);
    }

    static qint64 elapsedFromSave(uint8_t hour, uint8_t minute, uint8_t second)
    {
        return (static_cast<qint64>(hour) * 3600 +
                static_cast<qint64>(minute) * 60 +
                static_cast<qint64>(second)) * 1000;
    }

    bool currentSaveState(netchesszx_save_state_t *state, bool forPeer) const
    {
        if (state == nullptr) {
            return false;
        }
        std::memset(state, 0, sizeof(*state));
        CompactRulesState rulesState = {};
        if (netchesszx_rules_save(&rulesState, sizeof(rulesState)) != NETCHESSZX_OK) {
            return false;
        }
        ChessHelpers::asciiBoardFromCompact(rulesState.board, state->cells);
        state->ply = static_cast<uint16_t>(nextPly_ > 0 ? nextPly_ - 1 : 0);
        state->side = static_cast<uint8_t>(state->ply & 1u);
        state->castle = rulesState.castle;
        state->ep = rulesState.ep < 0 ? NETCHESSZX_SAVE_EP_NONE
                                    : static_cast<uint8_t>(rulesState.ep);
        state->host_color = hostPlaysWhite_ ? NETCHESSZX_SAVE_HOST_WHITE
                                            : NETCHESSZX_SAVE_HOST_BLACK;
        state->flags = 0;
        if (gameClockRunning_) {
            state->flags |= NETCHESSZX_SAVE_FLAG_ACTIVE;
        }
        if (gameOver_) {
            state->flags |= NETCHESSZX_SAVE_FLAG_GAME_OVER;
        }
        if (gameCheck_) {
            state->flags |= NETCHESSZX_SAVE_FLAG_CHECK;
        }
        splitElapsed(gameClockRunning_ ? gameTimerOffsetMs_ + gameTimer_.elapsed() : 0,
                      &state->game_hour, &state->game_minute, &state->game_second);
        splitElapsed(gameClockRunning_ ? moveTimerOffsetMs_ + moveTimer_.elapsed() : 0,
                      &state->move_hour, &state->move_minute, &state->move_second);
        const bool viewWhite = forPeer ? !pcPlaysWhite_ : boardWhiteAtBottom_;
        state->view_flags = viewWhite ? 0u : NETCHESSZX_SAVE_VIEW_FLIPPED;
        return netchesszx_save_state_validate(state) == NETCHESSZX_SAVE_OK;
    }

    bool writeSaveFilePath(const QString &path)
    {
        if (!canSaveGameFile()) {
            setStatusText("Save failed");
            return false;
        }
        const QString fullPath = SaveGameStore::ensureExtension(path);
        netchesszx_save_state_t state;

        if (!currentSaveState(&state, false) ||
            !SaveGameStore::write(fullPath, state)) {
            setStatusText("Save failed");
            return false;
        }
        appendLog("SAVE: " + fullPath);
        setStatusText("Game saved");
        return true;
    }

    bool writeSaveFile(const QString &name)
    {
        return writeSaveFilePath(SaveGameStore::pathForName(
            SaveGameStore::defaultDirectory(), name));
    }

    bool restoreBusy() const
    {
        return directUiBusy_ || resetPromptOpen_ ||
               directDecisionBox_ != nullptr;
    }

    bool resignCanPreemptBusy() const
    {
        return directUiBusy_ == SESSION_REQUEST_MOVE &&
               !resetPromptOpen_ && directDecisionBox_ == nullptr;
    }

    bool restorePeerReady() const
    {
        if (!isConnected()) {
            return false;
        }
        return directSessionReady_;
    }

    bool canSaveGameFile() const
    {
        return restorePeerReady() && boardPiecesVisible_ && !restoreBusy();
    }

    bool canLoadGameFile() const
    {
        return restorePeerReady() && !restoreBusy();
    }

    uint8_t currentSaveHostColor() const
    {
        return hostPlaysWhite_ ? NETCHESSZX_SAVE_HOST_WHITE
                               : NETCHESSZX_SAVE_HOST_BLACK;
    }

    bool restoreHostColorOk(const netchesszx_save_state_t &state) const
    {
        return state.host_color == currentSaveHostColor();
    }

    static uint8_t directRestorePhase(uint8_t flags)
    {
        if ((flags & NETCHESSZX_SAVE_FLAG_GAME_OVER) != 0u) {
            return SESSION_PHASE_OVER;
        }
        return (flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u
            ? SESSION_PHASE_ACTIVE : SESSION_PHASE_READY;
    }

    void refreshSaveLoadButtons()
    {
        if (saveGameButton_ != nullptr) {
            saveGameButton_->setEnabled(canSaveGameFile());
        }
        if (loadGameButton_ != nullptr) {
            loadGameButton_->setEnabled(canLoadGameFile());
        }
    }

    static void setPeerView(netchesszx_save_state_t *state, bool pcIsHost)
    {
        const bool hostWhite = state->host_color == NETCHESSZX_SAVE_HOST_WHITE;
        const bool pcWhite = pcIsHost ? hostWhite : !hostWhite;
        const bool peerWhite = !pcWhite;
        state->view_flags = peerWhite ? 0u : NETCHESSZX_SAVE_VIEW_FLIPPED;
    }

    bool loadSaveFilePath(const QString &path)
    {
        if (!canLoadGameFile()) {
            setStatusText("Load failed");
            return false;
        }
        netchesszx_save_state_t state;
        if (!SaveGameStore::read(path, &state) || !restoreHostColorOk(state)) {
            setStatusText("Load failed");
            return false;
        }
        setPeerView(&state, pcIsHost_);
        uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
        char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
        if (netchesszx_save_wire_pack(wire, sizeof(wire), &state) !=
                NETCHESSZX_SAVE_OK ||
            netchesszx_save_wire_b64_encode(b64, sizeof(b64), wire,
                                            sizeof(wire)) !=
                NETCHESSZX_SAVE_OK ||
            !submitSessionLocalRequest(
                SESSION_REQUEST_RESTORE, state.ply,
                QByteArray(b64, NETCHESSZX_SAVE_WIRE_B64_SIZE),
                directRestorePhase(state.flags))) {
            setStatusText(QStringLiteral("Load failed"));
            return false;
        }
        setStatusText(QStringLiteral("Waiting opponent approval"));
        refreshSaveLoadButtons();
        return true;
    }

    bool loadSaveFile(const QString &name)
    {
        return loadSaveFilePath(SaveGameStore::pathForName(
            SaveGameStore::defaultDirectory(), name));
    }

    /* Save button: Spectrum-style one-click save into the first free slot;
       the file name (NNYMDHMM.stj) carries the slot and timestamp exactly
       like the Spectrum FILE browser, no native dialog involved. */
    void saveGameWithDialog()
    {
        if (!canSaveGameFile()) {
            return;
        }
        const QString savesDirectory = SaveGameStore::defaultDirectory();
        const QVector<SaveSlotEntry> saveSlots =
            SaveGameStore::scanSlots(savesDirectory);

        for (int i = 0; i < saveSlots.size(); ++i) {
            if (saveSlots[i].used) {
                continue;
            }
            const QString base =
                SaveGameStore::slotBaseName(
                    i + 1, QDateTime::currentDateTimeUtc());
            if (writeSaveFilePath(
                    SaveGameStore::slotFilePath(savesDirectory, base))) {
                setStatusText(QStringLiteral("Saved GAME%1").arg(i + 1));
            }
            refreshSaveLoadButtons();
            return;
        }
        setStatusText(QStringLiteral("All save slots are full"));
        openSavedGamesDialog();
    }

    void loadGameWithDialog()
    {
        if (!canLoadGameFile()) {
            return;
        }
        openSavedGamesDialog();
    }

    void openSavedGamesDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Saved games"));
        dialog.resize(380, 430);
        dialog.setMinimumSize(380, 430);
        const QString savesDirectory = SaveGameStore::defaultDirectory();

        auto *layout = new QVBoxLayout(&dialog);
        auto *table = new QTableWidget(SaveGameStore::kSlotCount, 3, &dialog);

        table->setHorizontalHeaderLabels(QStringList()
                                         << QStringLiteral("NAME")
                                         << QStringLiteral("DATE")
                                         << QStringLiteral("TIME"));
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setStyleSheet(QStringLiteral(
            "QTableWidget { background:#101018; color:#e0e0e0;"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
            " selection-background-color:#2a6b8a;"
            " selection-color:" SHZ_TEXT "; }"
            "QTableWidget::item { padding:2px 6px; }"
            "QHeaderView::section { background:#202030; color:#f0f0f0;"
            " font-weight:700; letter-spacing:1px; border:0;"
            " border-bottom:1px solid " SHZ_BORDER "; padding:4px; }"));
        layout->addWidget(table);

        auto *buttons = new QHBoxLayout();
        auto *loadButton = new QPushButton(QStringLiteral("Load"), &dialog);
        auto *eraseButton = new QPushButton(QStringLiteral("Erase"), &dialog);
        auto *folderButton =
            new QPushButton(QStringLiteral("Open folder"), &dialog);
        auto *closeButton = new QPushButton(QStringLiteral("Close"), &dialog);

        buttons->addWidget(loadButton);
        buttons->addWidget(eraseButton);
        buttons->addStretch(1);
        buttons->addWidget(folderButton);
        buttons->addWidget(closeButton);
        layout->addLayout(buttons);

        QVector<SaveSlotEntry> saveSlots;
        const bool canLoad = canLoadGameFile();
        const QLocale locale = QLocale::system();

        const auto refresh = [&]() {
            saveSlots = SaveGameStore::scanSlots(savesDirectory);
            for (int i = 0; i < SaveGameStore::kSlotCount; ++i) {
                const SaveSlotEntry &entry = saveSlots[i];
                auto *name = new QTableWidgetItem(
                    entry.used ? QStringLiteral("GAME%1%2").arg(i + 1).arg(entry.duplicate ? QStringLiteral(" !") : QString())
                               : QStringLiteral("- free -"));
                auto *date = new QTableWidgetItem(
                    entry.duplicate ? QStringLiteral("DUPLICATE") :
                    entry.used ? locale.toString(entry.when.date(), QLocale::ShortFormat)
                               : QString());
                auto *time = new QTableWidgetItem(
                    entry.used ? locale.toString(entry.when.time(), QLocale::ShortFormat)
                               : QString());

                name->setForeground(QBrush(entry.used
                                               ? QColor(0x40, 0xd0, 0xd0)
                                               : QColor(0x70, 0x70, 0x80)));
                table->setItem(i, 0, name);
                table->setItem(i, 1, date);
                table->setItem(i, 2, time);
            }
        };
        const auto selectionUsed = [&]() {
            const int row = table->currentRow();
            return row >= 0 && row < saveSlots.size() && saveSlots[row].used;
        };
        const auto refreshButtons = [&]() {
            loadButton->setEnabled(canLoad && selectionUsed());
            eraseButton->setEnabled(selectionUsed());
        };

        connect(table, &QTableWidget::itemSelectionChanged, &dialog,
                refreshButtons);
        connect(table, &QTableWidget::cellDoubleClicked, &dialog,
                [&](int, int) {
                    if (loadButton->isEnabled()) {
                        loadButton->click();
                    }
                });
        connect(loadButton, &QPushButton::clicked, &dialog, [&]() {
            const int row = table->currentRow();

            if (row >= 0 && saveSlots[row].used &&
                loadSaveFilePath(SaveGameStore::slotFilePath(
                    savesDirectory, saveSlots[row].baseName))) {
                refreshSaveLoadButtons();
                dialog.accept();
            }
        });
        connect(eraseButton, &QPushButton::clicked, &dialog, [&]() {
            const int row = table->currentRow();

            if (row >= 0 && saveSlots[row].used) {
                const QString slotName = QStringLiteral("GAME%1").arg(row + 1);
                const bool erase =
                    askQuestion(&dialog,
                                QStringLiteral("Erase saved game"),
                                QStringLiteral("Erase %1?").arg(slotName),
                                QMessageBox::Yes | QMessageBox::No) ==
                    QMessageBox::Yes;
                if (!erase) {
                    return;
                }
                if (!SaveGameStore::remove(SaveGameStore::slotFilePath(
                        savesDirectory, saveSlots[row].baseName))) {
                    QMessageBox::warning(&dialog,
                                         QStringLiteral("Erase saved game"),
                                         QStringLiteral("Could not erase %1.").arg(slotName));
                }
                refresh();
                refreshButtons();
            }
        });
        connect(folderButton, &QPushButton::clicked, &dialog, [&]() {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(savesDirectory));
        });
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);

        refresh();
        refreshButtons();
        (void)dialog.exec();
        refreshSaveLoadButtons();
    }

    void sendChat()
    {
        QString text = chatEdit_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }

        text.replace('\r', ' ');
        text.replace('\n', ' ');
        const QString cmd = text.toLower();
        if (ChessHelpers::isMoveSyntaxOk(cmd)) {
            sendMove(true);
            return;
        }
        chatInputHistory_.append(text);
        while (chatInputHistory_.size() > 5) {
            chatInputHistory_.removeFirst();
        }
        chatInputHistoryIndex_ = static_cast<int>(chatInputHistory_.size());
        if (cmd == "/save" || cmd.startsWith(QStringLiteral("/save "))) {
            if (writeSaveFile(text.mid(5).trimmed())) {
                chatEdit_->clear();
                refreshChatButton();
            }
            return;
        }
        if (cmd == "/load" || cmd.startsWith(QStringLiteral("/load "))) {
            if (loadSaveFile(text.mid(5).trimmed())) {
                chatEdit_->clear();
                refreshChatButton();
            }
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directResignRestartPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directUiBusy_ != 0u &&
            !(cmd == QStringLiteral("/resign") &&
              resignCanPreemptBusy())) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_ACK);
            return;
        }
        if (cmd == "/resign") {
            if (!gameClockRunning_) {
                setStatusText(NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED);
                return;
            }
            if (restoreBusy() && !resignCanPreemptBusy()) {
                setStatusText("Load in progress");
                return;
            }
            resetPromptOpen_ = true;
            const QMessageBox::StandardButton answer =
                askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_RESIGN_TITLE,
                            NETCHESSZX_UI_CONFIRM_RESIGN,
                            QMessageBox::Yes | QMessageBox::No);
            if (!resetPromptOpen_) {
                return;
            }
            resetPromptOpen_ = false;
            if (answer != QMessageBox::Yes) {
                return;
            }
            directLocalResignPending_ = true;
            if (submitSessionLocalRequest(SESSION_REQUEST_RESIGN)) {
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
                chatEdit_->clear();
            } else {
                directLocalResignPending_ = false;
            }
            return;
        }
        if (cmd == "/draw") {
            if (requestDraw()) {
                chatEdit_->clear();
            }
            return;
        }
        if (cmd == "/takeback") {
            if (requestTakeback()) {
                chatEdit_->clear();
            }
            refreshChatButton();
            return;
        }
        if (!submitSessionLocalRequest(SESSION_REQUEST_CHAT, 0u,
                                       text.toLatin1())) {
            return;
        }
        chatEdit_->clear();
        refreshChatButton();
    }

    bool requestDraw()
    {
        if (directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
            return false;
        }
        if (directResignRestartPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
            return false;
        }
        if (directUiBusy_ != 0u) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_ACK);
            return false;
        }
        if (!gameClockRunning_ || restoreBusy()) {
            setStatusText(NETCHESSZX_UI_ERROR_CANNOT_OFFER_DRAW);
            return false;
        }
        resetPromptOpen_ = true;
        const QMessageBox::StandardButton answer =
            askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_DRAW_TITLE,
                        NETCHESSZX_UI_CONFIRM_DRAW,
                        QMessageBox::Yes | QMessageBox::No);
        if (!resetPromptOpen_) {
            return false;
        }
        resetPromptOpen_ = false;
        if (answer != QMessageBox::Yes) {
            return false;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_DRAW)) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_DRAW_ACK);
            return true;
        }
        return false;
    }

    bool requestTakeback()
    {
        if (directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_RESIGN_ACK);
            return false;
        }
        if (directResignRestartPending_) {
            setStatusText(NETCHESSZX_UI_NOTICE_RESIGN_ALREADY_APPLIED);
            return false;
        }
        if (directUiBusy_ != 0u) {
            setStatusText(NETCHESSZX_UI_NOTICE_WAITING_ACK);
            return false;
        }
        if (!canRequestTakeback()) {
            appendLog("ERROR: No move to take back");
            setStatusText("No move to take back");
            return false;
        }
        resetPromptOpen_ = true;
        const QMessageBox::StandardButton answer =
            askQuestion(this, NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_TITLE,
                        NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_REQUEST,
                        QMessageBox::Yes | QMessageBox::No);
        if (!resetPromptOpen_) {
            return false;
        }
        resetPromptOpen_ = false;
        if (answer != QMessageBox::Yes) {
            return false;
        }
        if (!canRequestTakeback()) {
            setStatusText(QStringLiteral("No move to take back"));
            return false;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_TAKEBACK,
                                      takebackSnapshot_.ply)) {
            setStatusText(QStringLiteral("Takeback requested"));
            return true;
        }
        return false;
    }

    void sendGameStart()
    {
        if (socket_->state() != QAbstractSocket::ConnectedState) {
            setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
            return;
        }
        if (gameClockRunning_) {
            setStatusText(NETCHESSZX_UI_ERROR_GAME_ALREADY_RUNNING);
            return;
        }
        if (restoreBusy()) {
            setStatusText("Load in progress");
            return;
        }
        if (!pcIsHost_) {
            setStatusText(isMqttMode() ? NETCHESSZX_UI_PHASE_WAITING_HOST_START :
                                         NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            appendLog("START ignored: only host starts");
            return;
        }
        if (!directSessionReady_) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT);
            appendLog("START ignored: peer not ready");
            return;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_START)) {
            syncBoardOrientationWithPcSide();
            gameOver_ = false;
            setStatusText(NETCHESSZX_UI_NOTICE_STARTING_GAME_PC);
        }
        setConnectedUi(true);
    }

    void sendMove(bool confirmed)
    {
        const QString move = moveEdit_->text().trimmed().toLower();
        if (!confirmed) {
            setStatusText(NETCHESSZX_UI_NOTICE_MOVE_READY);
            return;
        }
        if (!canPcMove()) {
            if (socket_->state() != QAbstractSocket::ConnectedState) {
                appendLog("ERROR: not connected");
                setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
            } else if (directUiBusy_) {
                appendLog(QStringLiteral("ERROR: waiting session core"));
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
            } else {
                appendLog("ERROR: opponent moves first; local side waits");
                setStatusBarText("Not your turn");
            }
            return;
        }
        if (!ChessHelpers::isMoveSyntaxOk(move)) {
            appendLog("ERROR: move syntax must be e2e4 or e7e8q/r/b/n");
            return;
        }
        const int fromCol = move[0].unicode() - 'a';
        const int fromRow = '8' - move[1].unicode();
        const int toCol = move[2].unicode() - 'a';
        const int toRow = '8' - move[3].unicode();
        if (!isPcPiece(board_[fromRow][fromCol])) {
            appendLog(QString("ERROR: you can only move %1 pieces: %2")
                          .arg(pcSideName(), move.left(2)));
            setStatusText(QString("You play %1").arg(pcSideName()));
            return;
        }

        QElapsedTimer sendPrepTimer;
        sendPrepTimer.start();
        const QByteArray moveBytes = move.toLatin1();
        if (!pendingMoveCameFromSelection(move)) {
            const int legalRc = netchesszx_rules_can_play(moveBytes.constData());
            if (legalRc != NETCHESSZX_OK) {
                appendLog(QString("ERROR: %1: %2")
                              .arg(QString::fromLatin1(netchesszx_error_string(legalRc)), move));
                setStatusText(QString("Illegal move: %1").arg(move));
                return;
            }
        }

        if (!submitSessionLocalRequest(SESSION_REQUEST_MOVE, 0u,
                                       moveBytes)) {
            return;
        }
        const qint64 prepMs = sendPrepTimer.elapsed();
        if (prepMs > kMoveSendWarnMs) {
            appendLog(QString("WARN: move send prep took %1 ms").arg(prepMs));
        }

        clearSelectionVisuals();
        moveEdit_->clear();
        showDestinationFeedback(toRow, toCol);
        setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
        setConnectedUi(true);
    }

    void consumeReadyRead(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        const QByteArray data = sock->readAll();
        const uint8_t directLink = directLinkForSocket(sock);
        if (directLink == SESSION_LINK_NONE) {
            if (sock != socket_ || !isMqttMode()) {
                return;
            }
            linkWatch_.restart();
            consumeMqttBytes(data);
            return;
        }

        const DesktopTransportCodec::DirectFeedResult result =
            transportCodec_.feedDirect(directLink, data,
                [this, directLink](const QByteArray &lineBytes) {
                    appendLog("RX: " + QString::fromLatin1(lineBytes));
                    sessionController_.receiveDirect(directLink, lineBytes);
                });
        if (result.overflow) {
            appendLog("ERROR: direct RX line too long");
            if (directLink == directPrimaryLinkId_) {
                directEndStatus_ = NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST;
            }
            if (sock != nullptr) {
                sock->abort();
            }
            return;
        }
        if (result.delivered) {
            sessionController_.pump();
        }
    }

    bool isMqttMode() const
    {
        return mqttRadio_ != nullptr && mqttRadio_->isChecked();
    }

    void configureSessionFromUi()
    {
        pcIsHost_ = roleHostRadio_ != nullptr && roleHostRadio_->isChecked();
        if (pcIsHost_) {
            hostPlaysWhite_ = hostWhiteRadio_ == nullptr || hostWhiteRadio_->isChecked();
        } else if (!isConnected() && !isConnecting()) {
            hostPlaysWhite_ = true;
        }
        pcPlaysWhite_ = pcIsHost_ ? hostPlaysWhite_ : !hostPlaysWhite_;
        if (!isMqttMode()) {
            mqttSideReady_ = true;
        }
        if (isConnected() || isConnecting() || pcIsHost_) {
            syncBoardOrientationWithPcSide();
        }
    }

    void updateSessionControlsEnabled()
    {
        const bool enabled = !isConnected() && !isConnecting();
        const bool colorVisible = pcIsHost_;
        const bool colorEnabled = enabled && pcIsHost_;

        if (roleHostRadio_) {
            roleHostRadio_->setEnabled(enabled);
        }
        if (roleGuestRadio_) {
            roleGuestRadio_->setEnabled(enabled);
        }
        if (hostColorLabel_) {
            hostColorLabel_->setVisible(colorVisible);
        }
        if (hostWhiteRadio_) {
            hostWhiteRadio_->setVisible(colorVisible);
            hostWhiteRadio_->setEnabled(colorEnabled);
        }
        if (hostBlackRadio_) {
            hostBlackRadio_->setVisible(colorVisible);
            hostBlackRadio_->setEnabled(colorEnabled);
        }
    }

    QString hostSideLetter() const
    {
        return hostPlaysWhite_ ? "W" : "B";
    }

    static quint16 newMqttSessionId()
    {
        return static_cast<quint16>(
            QRandomGenerator::global()->bounded(1, 65536));
    }

    QString topicFor(const QString &suffix) const
    {
        // NOTE: mqttRoom_ is the pairing identifier and travels in cleartext
        // (MQTT over plain TCP, port 1883 — the Spectrum peer cannot do TLS).
        // The room code is NOT a security boundary: an on-path observer can read
        // it and inject frames. Do not treat it as a secret.
        return QString("netchesszx/v1/%1/%2").arg(mqttRoom_, suffix);
    }

    QString mqttInSuffix() const
    {
        return pcPlaysWhite_ ? "b2w" : "w2b";
    }

    QString mqttInAckSuffix() const
    {
        return pcPlaysWhite_ ? "ack_w" : "ack_b";
    }

    QString mqttPresenceSuffix() const
    {
        return pcPlaysWhite_ ? "pres_w" : "pres_b";
    }

    QString mqttPeerPresenceSuffix() const
    {
        return pcPlaysWhite_ ? "pres_b" : "pres_w";
    }

    uint16_t mqttPacketId()
    {
        if (mqttNextPacketId_ == 0) {
            mqttNextPacketId_ = 1;
        }
        return mqttNextPacketId_++;
    }

    bool writeMqttPacket(const QByteArray &packet, const QString &label)
    {
        if (packet.isEmpty()) {
            appendLog("ERROR: MQTT encode failed: " + label);
            return false;
        }
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
        if (testMqttWriteFailure_) {
            appendLog("ERROR: forced MQTT write failure: " + label);
            return false;
        }
#endif
        const qint64 written = socket_->write(packet);
        if (written < 0) {
            appendLog("ERROR: MQTT write failed: " + socket_->errorString());
            return false;
        }
        if (written != packet.size()) {
            appendLog(QString("ERROR: MQTT partial write %1/%2")
                          .arg(written)
                          .arg(packet.size()));
            socket_->abort();
            return false;
        }
        socket_->flush();
        appendLog("MQTT TX: " + label);
        return true;
    }

    void mqttHandshake()
    {
        const QByteArray clientId = mqttClientIdFor(pcIsHost_, mqttClientNonce_);
        QByteArray willTopic;
        QByteArray willPayload;
        // Host-only will: it knows the session id at CONNECT time, so peers
        // can validate it. A guest will would be id-less and receivers drop
        // id-less F (any stray client could arm one and kill a live game);
        // guest death is detected by the session-core liveness timer instead.
        const bool useWill = pcIsHost_;

        if (useWill) {
            willTopic = topicFor(mqttPresenceSuffix()).toLatin1();
            willPayload = QString("F %1 %2")
                              .arg(pcSideLetter())
                              .arg(mqttSessionId_)
                              .toLatin1();
        }
        const QByteArray packet = DesktopTransportCodec::encodeMqttConnect(
            clientId, 20, willTopic, willPayload, useWill);
        appendLog("CONNECTED TCP MQTT");
        setStatusText(NETCHESSZX_UI_NOTICE_MQTT_CONNECTING);
        (void)writeMqttPacket(packet, "CONNECT");
    }

    void clearMqttSubscriptionState()
    {
        mqttSubscribed_ = false;
        mqttSideReady_ = false;
        mqttSideTransitionPending_ = false;
        mqttSubackPending_.clear();
        mqttUnsubackPending_.clear();
        mqttActiveSubscriptions_.clear();
        mqttTargetSubscriptions_.clear();
        mqttObsoleteSubscriptions_.clear();
        mqttBufferedPublishes_.clear();
    }

    bool mqttSubscribe(const QString &suffix)
    {
        const QByteArray topic = topicFor(suffix).toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet =
            DesktopTransportCodec::encodeMqttSubscribe(id, topic);
        if (!writeMqttPacket(packet, "SUB " + topicFor(suffix))) {
            return false;
        }
        mqttSubackPending_.insert(id, suffix);
        return true;
    }

    bool mqttUnsubscribe(const QString &suffix)
    {
        const QByteArray topic = topicFor(suffix).toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet =
            DesktopTransportCodec::encodeMqttUnsubscribe(id, topic);
        if (!writeMqttPacket(packet, "UNSUB " + topicFor(suffix))) {
            return false;
        }
        mqttUnsubackPending_.insert(id, suffix);
        return true;
    }

    void replayMqttBufferedPublishes()
    {
        while (mqttSubscribed_ && sessionController_.initialized() &&
               !mqttBufferedPublishes_.isEmpty()) {
            const MqttBufferedPublish buffered =
                mqttBufferedPublishes_.takeFirst();
            if (!mqttActiveSubscriptions_.contains(buffered.suffix) ||
                !mqttTargetSubscriptions_.contains(buffered.suffix)) {
                continue;
            }
            (void)sessionController_.receiveMqtt(
                kMqttLinkId, buffered.topic, buffered.retained,
                buffered.payload);
        }
    }

    void advanceMqttSubscriptionTransition()
    {
        if (!mqttSubackPending_.isEmpty() ||
            !mqttUnsubackPending_.isEmpty()) {
            const int pending = mqttSubackPending_.size() +
                                mqttUnsubackPending_.size();
            setStatusText(QString("MQTT subscribing... %1").arg(pending));
            return;
        }

        if (mqttSideTransitionPending_ &&
            !mqttObsoleteSubscriptions_.isEmpty()) {
            const QSet<QString> obsolete = mqttObsoleteSubscriptions_;
            mqttObsoleteSubscriptions_.clear();
            for (const QString &suffix : obsolete) {
                if (!mqttUnsubscribe(suffix)) {
                    failMqttConnection("MQTT side unsubscribe send failed");
                    return;
                }
            }
            if (!mqttUnsubackPending_.isEmpty()) {
                setStatusText(QString("MQTT switching side... %1")
                                  .arg(mqttUnsubackPending_.size()));
                return;
            }
        }

        if (mqttActiveSubscriptions_ != mqttTargetSubscriptions_) {
            failMqttConnection("MQTT subscription transition incomplete");
            return;
        }

        mqttSubscribed_ = true;
        if (mqttSideTransitionPending_) {
            mqttSideTransitionPending_ = false;
            mqttSideReady_ = true;
            syncBoardOrientationWithPcSide();
            refreshBoard();
            appendLog("MQTT SIDE READY");
            setConnectedUi(isConnected());
            sessionController_.pump();
            replayMqttBufferedPublishes();
            return;
        }

        mqttSideReady_ = pcIsHost_;
        setConnectedUi(true);
        setStatusText(pcIsHost_ ? "MQTT link established - waiting opponent"
                                : "MQTT link established - waiting host");
        appendLog("MQTT READY");
        if (!mqttSessionLinked_) {
            mqttSessionLinked_ = true;
            (void)sessionController_.linkUp(kMqttLinkId);
        }
        replayMqttBufferedPublishes();
    }

    bool mqttPublish(const QString &suffix, const QString &payload,
                     bool retain = false)
    {
        if (!mqttSubscribed_ || !mqttSideReady_) {
            return false;
        }
        const QByteArray topic = topicFor(suffix).toLatin1();
        const QByteArray body = payload.toLatin1();
        const uint16_t id = mqttPacketId();
        const QByteArray packet = DesktopTransportCodec::encodeMqttPublish(
            id, topic, body, retain);
        return writeMqttPacket(packet, "PUB " + suffix + " " + payload);
    }

    void consumeMqttBytes(const QByteArray &data)
    {
        bool malformed = false;
        const QVector<QByteArray> packets = transportCodec_.feedMqtt(data, &malformed);
        for (const QByteArray &packet : packets) {
            handleMqttPacket(packet);
        }
        if (malformed) {
            appendLog("ERROR: malformed MQTT packet");
            socket_->abort();
        }
    }

    void handleMqttPacket(const QByteArray &raw)
    {
        DesktopTransportCodec::MqttPacket packet;
        if (!DesktopTransportCodec::decodeMqttPacket(raw, &packet)) {
            appendLog("ERROR: MQTT parse failed");
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Connack) {
            if (packet.returnCode != 0) {
                failMqttConnection(QString("MQTT CONNACK %1").arg(packet.returnCode));
                return;
            }
            clearMqttSubscriptionState();
            mqttTargetSubscriptions_.insert(QStringLiteral("meta"));
            if (pcIsHost_) {
                mqttTargetSubscriptions_.insert(mqttInSuffix());
                mqttTargetSubscriptions_.insert(mqttInAckSuffix());
                mqttTargetSubscriptions_.insert(mqttPeerPresenceSuffix());
            }
            if ((pcIsHost_ &&
                 (!mqttSubscribe(mqttInSuffix()) ||
                  !mqttSubscribe(mqttInAckSuffix()) ||
                  !mqttSubscribe(mqttPeerPresenceSuffix()))) ||
                !mqttSubscribe(QStringLiteral("meta"))) {
                failMqttConnection("MQTT subscribe send failed");
                return;
            }
            setStatusText(NETCHESSZX_UI_NOTICE_MQTT_SUBSCRIBING);
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Suback) {
            const auto pending = mqttSubackPending_.constFind(packet.packetId);
            if (pending == mqttSubackPending_.constEnd()) {
                appendLog(QString("IGNORE MQTT SUBACK id=%1 (not pending)")
                              .arg(packet.packetId));
                return;
            }
            if (packet.returnCode == 0x80) {
                failMqttConnection(QString("MQTT SUBACK failed id=%1")
                                       .arg(packet.packetId));
                return;
            }
            const QString suffix = pending.value();
            mqttSubackPending_.remove(packet.packetId);
            mqttActiveSubscriptions_.insert(suffix);
            advanceMqttSubscriptionTransition();
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Unsuback) {
            const auto pending = mqttUnsubackPending_.constFind(packet.packetId);
            if (pending == mqttUnsubackPending_.constEnd()) {
                appendLog(QString("IGNORE MQTT UNSUBACK id=%1 (not pending)")
                              .arg(packet.packetId));
                return;
            }
            const QString suffix = pending.value();
            mqttUnsubackPending_.remove(packet.packetId);
            mqttActiveSubscriptions_.remove(suffix);
            advanceMqttSubscriptionTransition();
            return;
        }

        if (packet.type == DesktopTransportCodec::MqttPacketType::Publish) {
            appendLog(QString("MQTT RX %1%2: %3")
                          .arg(QString::fromLatin1(packet.topic),
                               packet.retained ? QString(" [retained]") : QString(),
                               QString::fromLatin1(packet.payload)));
            if (packet.packetId != 0) {
                writeMqttPacket(
                    DesktopTransportCodec::encodeMqttPuback(packet.packetId),
                    QString("PUBACK %1").arg(packet.packetId));
            }
            handleMqttPayload(packet.topic, packet.payload, packet.retained);
            return;
        }
    }

    void handleMqttPayload(const QByteArray &topic,
                           const QByteArray &payload,
                           bool retained = false)
    {
        const int slash = topic.lastIndexOf('/');
        const QString suffix = QString::fromLatin1(
            slash < 0 ? topic : topic.mid(slash + 1));
        const bool exactTopic = topic == topicFor(suffix).toLatin1();
        const bool metaReady =
            suffix == QStringLiteral("meta") &&
            mqttActiveSubscriptions_.contains(suffix);
        const bool sideReady =
            mqttSubscribed_ && mqttSideReady_ &&
            mqttActiveSubscriptions_.contains(suffix) &&
            mqttTargetSubscriptions_.contains(suffix);
        const bool subscriptionPending =
            !mqttSubackPending_.isEmpty() ||
            !mqttUnsubackPending_.isEmpty();
        const bool sessionReady =
            sessionController_.initialized() &&
            sessionController_.mode() == DesktopSessionController::Mode::Mqtt;

        if (exactTopic && sessionReady && subscriptionPending &&
            mqttTargetSubscriptions_.contains(suffix)) {
            if (mqttBufferedPublishes_.size() >= kMqttBufferedPublishMax) {
                failMqttConnection("MQTT subscription publish buffer full");
                return;
            }
            mqttBufferedPublishes_.append(
                MqttBufferedPublish{suffix, topic, payload, retained});
            appendLog("QUEUE MQTT topic " + QString::fromLatin1(topic));
            return;
        }

        if (!exactTopic || (!metaReady && !sideReady) ||
            !sessionReady ||
            !sessionController_.receiveMqtt(kMqttLinkId, topic,
                                            retained, payload)) {
            appendLog("IGNORE MQTT topic " + QString::fromLatin1(topic));
            return;
        }

    }

    void endGameOver(const QString &message)
    {
        closeControlPrompt();
        clearTakebackState();
        gameOver_ = true;
        pcTurn_ = false;
        stopGameClock();
        appendLog(message);
        setStatusText(message);
        setConnectedUi(true);
    }

    void closeControlPrompt()
    {
        if (!resetPromptOpen_) {
            return;
        }
        resetPromptOpen_ = false;
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            box->close();
        }
    }

    bool restoreApplyState(const netchesszx_save_state_t &st)
    {
        CompactRulesState rulesState = {};
        for (int i = 0; i < 64; ++i) {
            rulesState.board[i] = ChessHelpers::compactPieceFromAscii(st.cells[i]);
        }
        rulesState.side = st.side;
        rulesState.castle = st.castle;
        rulesState.ep = st.ep == NETCHESSZX_SAVE_EP_NONE
            ? static_cast<int8_t>(NETCHESSZX_RULE_NO_SQUARE)
            : static_cast<int8_t>(st.ep);
        if (netchesszx_rules_restore(&rulesState, sizeof(rulesState)) != NETCHESSZX_OK) {
            return false;
        }
        if (!syncBoardFromRules()) {
            return false;
        }
        ++gameGeneration_;
        invalidatePieceFlash();
        invalidateDestinationFeedback();
        hostPlaysWhite_ = (st.host_color == NETCHESSZX_SAVE_HOST_WHITE);
        pcPlaysWhite_ = pcIsHost_ ? hostPlaysWhite_ : !hostPlaysWhite_;
        clearTakebackState();
        closeControlPrompt();
        closeDirectDecisionPrompt();
        directUiBusy_ = false;
        nextPly_ = static_cast<int>(st.ply) + 1;
        const bool restoredActive = (st.flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u;
        gameOver_ = (st.flags & NETCHESSZX_SAVE_FLAG_GAME_OVER) != 0u;
        gameCheck_ = (st.flags & NETCHESSZX_SAVE_FLAG_CHECK) != 0u;
        pcTurn_ = restoredActive && !gameOver_ &&
            ((st.side == NETCHESSZX_SAVE_SIDE_WHITE) == pcPlaysWhite_);
        lastMove_.clear();
        clearMoveHistory();
        appendMoveRecord(static_cast<int>(st.ply),
                         QStringLiteral(NETCHESSZX_UI_EVENT_RESTORED),
                         QString());
        boardPiecesVisible_ = true;
        clearSelection();
        moveEdit_->clear();
        selectedLabel_->setText("Selected: none");
        syncBoardOrientationWithPcSide();
        coordinatesInitialized_ = false;
        refreshBoard();
        if (restoredActive && !gameOver_) {
            startGameClock(elapsedFromSave(st.game_hour, st.game_minute, st.game_second),
                           elapsedFromSave(st.move_hour, st.move_minute, st.move_second));
        } else if (!restoredActive || gameOver_) {
            stopGameClock();
        }
        refreshTurnLabel();
        setConnectedUi(true);
        return true;
    }

    bool applyDirectRestore(const QByteArray &payload,
                            uint16_t *restoredPly,
                            uint8_t *restoredPhase)
    {
        uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
        netchesszx_save_state_t state = {};
        if (payload.size() != NETCHESSZX_SAVE_WIRE_B64_SIZE ||
            netchesszx_save_wire_b64_decode(
                wire, sizeof(wire), payload.constData(),
                static_cast<size_t>(payload.size())) != NETCHESSZX_SAVE_OK ||
            netchesszx_save_wire_unpack(&state, wire, sizeof(wire)) !=
                NETCHESSZX_SAVE_OK ||
            !restoreHostColorOk(state) || !restoreApplyState(state)) {
            appendLog("RX: restore decode/apply failed");
            setStatusText(QStringLiteral("Load failed"));
            return false;
        }
        if (restoredPly != nullptr) {
            *restoredPly = state.ply;
        }
        if (restoredPhase != nullptr) {
            *restoredPhase = directRestorePhase(state.flags);
        }
        appendLog("RX: game restored");
        setStatusText(QStringLiteral("Game loaded"));
        return true;
    }

    void clearTakebackState()
    {
        takebackSnapshot_ = TakebackSnapshot{};
    }

    bool saveTakebackSnapshot(int ply, bool localMove)
    {
        TakebackSnapshot snapshot;

        snapshot.rules.resize(static_cast<int>(netchesszx_rules_state_size()));
        if (snapshot.rules.isEmpty() ||
            netchesszx_rules_save(snapshot.rules.data(),
                                  static_cast<size_t>(snapshot.rules.size())) != NETCHESSZX_OK) {
            appendLog("ERROR: could not save takeback snapshot");
            return false;
        }
        snapshot.lastMove = lastMove_;
        snapshot.ply = ply;
        snapshot.nextPly = nextPly_;
        snapshot.historyCount = moveHistoryRecords_.size();
        snapshot.pcTurn = pcTurn_;
        snapshot.gameCheck = gameCheck_;
        snapshot.localMove = localMove;
        snapshot.valid = true;
        takebackSnapshot_ = snapshot;
        return true;
    }

    void restoreTakebackSnapshot()
    {
        if (!takebackSnapshot_.valid) {
            return;
        }
        char previous[8][8];
        std::memcpy(previous, board_, sizeof(board_));
        (void)netchesszx_rules_restore(takebackSnapshot_.rules.constData(),
                                       static_cast<size_t>(takebackSnapshot_.rules.size()));
        (void)syncBoardFromRules();
        while (moveHistoryRecords_.size() > takebackSnapshot_.historyCount) {
            moveHistoryRecords_.removeLast();
        }
        lastMove_ = takebackSnapshot_.lastMove;
        nextPly_ = takebackSnapshot_.nextPly;
        pcTurn_ = takebackSnapshot_.pcTurn;
        gameCheck_ = takebackSnapshot_.gameCheck;
        gameOver_ = false;
        selectedLabel_->setText("Selected: none");
        clearTakebackState();
        clearSelectionVisuals();
        refreshChangedBoardSquares(previous);
        renderLogView();
        restartMoveClock();
        setConnectedUi(true);
    }

    bool applyDirectTakeback(uint16_t ply)
    {
        if (!takebackSnapshot_.valid || takebackSnapshot_.ply != ply ||
            ply != nextPly_ - 1) {
            appendLog(QString("ERROR: cannot apply TAKEBACK %1").arg(ply));
            return false;
        }
        restoreTakebackSnapshot();
        setStatusText(QStringLiteral("Takeback accepted"));
        return true;
    }

    bool canRequestTakeback() const
    {
        return gameClockRunning_ && !gameOver_ && !restoreBusy() &&
               takebackSnapshot_.valid && takebackSnapshot_.localMove &&
               takebackSnapshot_.ply == nextPly_ - 1;
    }

    bool applyMoveToBoard(const QString &move, QString *checkSuffix = nullptr,
                          bool *stalemate = nullptr)
    {
        const QByteArray moveBytes = move.toLatin1();
        const int legalRc = netchesszx_rules_play(moveBytes.constData());
        if (legalRc != NETCHESSZX_OK) {
            appendLog(QString("ERROR: move rejected by rules: %1 (%2)")
                          .arg(move, QString::fromLatin1(netchesszx_error_string(legalRc))));
            setStatusText(NETCHESSZX_UI_ERROR_RULES_REJECTED_MOVE);
            return false;
        }

        char previous[8][8];
        std::memcpy(previous, board_, sizeof(board_));
        if (!syncBoardFromRules()) {
            setStatusText(NETCHESSZX_UI_ERROR_BOARD_REJECTED_MOVE);
            return false;
        }
        if (checkSuffix != nullptr) {
            *checkSuffix = checkSuffixAfterMove(stalemate);
        } else if (stalemate != nullptr) {
            (void)checkSuffixAfterMove(stalemate);
        }
        selectedLabel_->setText("Selected: none");
        clearSelectionVisuals();
        refreshChangedBoardSquares(previous);
        return true;
    }

    void finishAppliedMove(int ply, const QString &move, const QString &notation,
                           bool nextPcTurn, const QString &normalStatus,
                           bool stalemate = false)
    {
        lastMove_ = move;
        appendMoveRecord(ply, move, notation);
        nextPly_ = ply + 1;
        if (notation.contains('#')) {
            const QString message = nextPcTurn ? QString::fromLatin1(NETCHESSZX_UI_EVENT_CHECKMATE_LOST)
                                               : QString::fromLatin1(NETCHESSZX_UI_EVENT_CHECKMATE_WON);
            gameOver_ = true;
            gameCheck_ = false;
            pcTurn_ = false;
            stopGameClock();
            appendLog(message + QString(" (%1)").arg(notation));
            setStatusText(message);
            setConnectedUi(true);
            return;
        }
        if (stalemate) {
            gameOver_ = true;
            gameCheck_ = false;
            pcTurn_ = false;
            stopGameClock();
            appendLog(QString::fromLatin1(NETCHESSZX_UI_EVENT_STALEMATE) + QString(" (%1)").arg(notation));
            setStatusText(NETCHESSZX_UI_EVENT_STALEMATE);
            setConnectedUi(true);
            return;
        }
        gameOver_ = false;
        gameCheck_ = notation.contains('+');
        pcTurn_ = nextPcTurn;
        restartMoveClock();
        setStatusText(normalStatus);
        setConnectedUi(true);
    }

    void applyDirectLocalMove(uint16_t plyValue, const QString &move)
    {
        const int ply = static_cast<int>(plyValue);
        const QString notationBase = moveNotationBase(move);
        QString checkSuffix;
        bool stalemate = false;

        (void)saveTakebackSnapshot(ply, true);
        if (!ChessHelpers::isMoveSyntaxOk(move) ||
            !applyMoveToBoard(move, &checkSuffix, &stalemate)) {
            pcTurn_ = false;
            setStatusText(NETCHESSZX_UI_ERROR_BOARD_REJECTED_MOVE);
            setConnectedUi(true);
            return;
        }
        const QString notation = notationBase + checkSuffix;
        finishAppliedMove(ply, move, notation, false,
                          QString("Move %1 confirmed").arg(notation),
                          stalemate);
    }

    bool applyDirectRemoteMoveAnimated(uint8_t deliveryId,
                                       uint16_t plyValue,
                                       const QString &move,
                                       QByteArray *failure)
    {
        const int ply = static_cast<int>(plyValue);
        const auto reject = [failure](const char *reason) {
            if (failure != nullptr) {
                *failure = QByteArray(reason);
            }
            return false;
        };
        if (!gameClockRunning_ || !boardPiecesVisible_) {
            return reject("START");
        }
        if (!ChessHelpers::isMoveSyntaxOk(move)) {
            return reject("SYNTAX");
        }
        if (pcTurn_) {
            return reject("TURN");
        }
        if (ply != nextPly_) {
            return reject("SYNC");
        }

        const int fromCol = move[0].unicode() - 'a';
        const int fromRow = '8' - move[1].unicode();
        const int toCol = move[2].unicode() - 'a';
        const int toRow = '8' - move[3].unicode();
        if (board_[fromRow][fromCol] == '.' ||
            isPcPiece(board_[fromRow][fromCol])) {
            return reject("TURN");
        }
        const QByteArray moveBytes = move.toLatin1();
        if (netchesszx_rules_can_play(moveBytes.constData()) != NETCHESSZX_OK) {
            return reject("ILLEGAL");
        }
        const QString notationBase = moveNotationBase(move);
        const int moveGeneration = gameGeneration_;

        flashPieceAt(fromRow, fromCol,
                     [this, deliveryId, plyValue, ply, move, toRow, toCol,
                      notationBase, moveGeneration](bool cancelled) {
            if (cancelled || gameGeneration_ != moveGeneration) {
                submitSessionGameResult(deliveryId, plyValue,
                                        SESSION_GAME_REJECTED);
                return;
            }
            QString checkSuffix;
            bool stalemate = false;
            (void)saveTakebackSnapshot(ply, false);
            if (!applyMoveToBoard(move, &checkSuffix, &stalemate)) {
                submitSessionGameResult(deliveryId, plyValue,
                                        SESSION_GAME_REJECTED,
                                        QByteArray("ILLEGAL"));
                return;
            }

            const QString notation = notationBase + checkSuffix;
            finishAppliedMove(ply, move, notation, true,
                              QString("Opponent move %1 - your move")
                                  .arg(notation),
                              stalemate);
            submitSessionGameResult(deliveryId, plyValue,
                                    SESSION_GAME_ACCEPTED,
                                    notation.toLatin1());
            flashPieceAt(toRow, toCol, [](bool) {});
        });
        return true;
    }

    bool isStartingBoard() const
    {
        static const char *const rows[8] = {
            "rnbqkbnr",
            "pppppppp",
            "........",
            "........",
            "........",
            "........",
            "PPPPPPPP",
            "RNBQKBNR"
        };

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (board_[row][col] != rows[row][col]) {
                    return false;
                }
            }
        }
        return true;
    }

    void startGameFromAck()
    {
        const bool oldWhiteAtBottom = boardWhiteAtBottom_;
        const bool keepSetupPreview = boardPiecesVisible_ && !gameOver_ &&
            !gameClockRunning_ && isStartingBoard();

        invalidatePieceFlash();
        invalidateDestinationFeedback();
        directLocalResignPending_ = false;
        directResignRestartPending_ = false;
        netchesszx_rules_reset();
        (void)syncBoardFromRules();
        clearTakebackState();
        nextPly_ = 1;
        gameOver_ = false;
        gameCheck_ = false;
        pcTurn_ = pcPlaysWhite_;
        lastMove_.clear();
        clearMoveHistory();
        clearSelection();
        moveEdit_->clear();
        selectedLabel_->setText("Selected: none");
        syncBoardOrientationWithPcSide();
        if (!keepSetupPreview || oldWhiteAtBottom != boardWhiteAtBottom_) {
            boardPiecesVisible_ = false;
            refreshBoard();
            animateBoardPiecesIn();
        }
        startGameClock();
        setStatusText(NETCHESSZX_UI_NOTICE_GAME_STARTED_WHITE);
        setConnectedUi(true);
    }

    static QString generateMqttRoomCode()
    {
        return QString("NC%1")
            .arg(QRandomGenerator::global()->generate() & 0xffffu,
                 4,
                 16,
                 QLatin1Char('0'))
            .toUpper();
    }

    static QString formatElapsed(qint64 msecs)
    {
        const qint64 totalSeconds = msecs / 1000;
        const qint64 seconds = totalSeconds % 60;
        const qint64 minutes = (totalSeconds / 60) % 60;
        const qint64 hours = totalSeconds / 3600;

        if (hours > 0) {
            return QString("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'));
        }
        return QString("%1:%2")
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    void startGameClock(qint64 gameOffsetMs = 0, qint64 moveOffsetMs = 0)
    {
        gameClockRunning_ = true;
        gameTimerOffsetMs_ = gameOffsetMs;
        moveTimerOffsetMs_ = moveOffsetMs;
        gameTimer_.restart();
        moveTimer_.restart();
        updateClockLabels();
    }

    void stopGameClock()
    {
        gameClockRunning_ = false;
        gameTimerOffsetMs_ = 0;
        moveTimerOffsetMs_ = 0;
        updateClockLabels();
    }

    void restartMoveClock()
    {
        if (gameClockRunning_) {
            moveTimerOffsetMs_ = 0;
            moveTimer_.restart();
            updateClockLabels();
        }
    }

    void updateClockLabels()
    {
        if (!gameClockLabel_ || !moveClockLabel_) {
            return;
        }
        if (!gameClockRunning_) {
            setLabelText(gameClockLabel_, "GAME --:--");
            setLabelText(moveClockLabel_, "MOVE --:--");
            return;
        }
        setLabelText(gameClockLabel_,
                     "GAME " + formatElapsed(gameTimerOffsetMs_ + gameTimer_.elapsed()));
        setLabelText(moveClockLabel_,
                     "MOVE " + formatElapsed(moveTimerOffsetMs_ + moveTimer_.elapsed()));
    }

    void checkUiStall()
    {
        if (!uiTickTimer_.isValid()) {
            uiTickTimer_.start();
            return;
        }

        const qint64 elapsed = uiTickTimer_.elapsed();
        uiTickTimer_.restart();
        if (elapsed > kUiStallWarnMs) {
            appendLog(QString("WARN: UI stalled %1 ms").arg(elapsed));
        }
    }

    void checkConnectionHealth()
    {
        if (!isMqttMode() || socket_ == nullptr ||
            socket_->state() != QAbstractSocket::ConnectedState ||
            !linkWatch_.isValid()) {
            return;
        }

        // Must stay well under the 20s MQTT keepalive or the broker drops us
        // and fires our own Last Will.
        if (linkWatch_.elapsed() <= 10000) {
            return;
        }

        (void)writeMqttPacket(DesktopTransportCodec::encodeMqttPing(), "PINGREQ");
        linkWatch_.restart();
    }

    static QString turnLabelStyle(const char *background, const char *foreground)
    {
        return QStringLiteral("QLabel { background:%1; color:%2;"
                              " border:1px solid " SHZ_BORDER_SOFT ";"
                              " border-radius:" SHZ_R "; font-weight:700;"
                              " font-size:13px; letter-spacing:1px; padding:7px; }")
            .arg(QLatin1String(background), QLatin1String(foreground));
    }

    void refreshTurnLabel()
    {
        if (!turnLabel_) {
            return;
        }

        const bool connected = socket_ != nullptr &&
                               socket_->state() == QAbstractSocket::ConnectedState;
        QString text;
        QString style;
        if (!connected && statusMessage_ == kDirectHostBusyStatus) {
            text = "HOST BUSY";
            style = turnLabelStyle(SHZ_ALERT, SHZ_ALERT_TEXT);
        } else if (!connected && isConnectionErrorStatus()) {
            text = "CONNECT FAILED";
            style = turnLabelStyle(SHZ_ALERT, SHZ_ALERT_TEXT);
        } else if (!connected) {
            text = isConnecting() ? "CONNECTING" : "OFFLINE";
            style = isConnecting() ? turnLabelStyle(SHZ_QUIET, SHZ_ACCENT)
                                   : turnLabelStyle(SHZ_DISABLED, SHZ_TEXT_DIM);
        } else if (gameOver_) {
            text = statusMessage_;
            style = turnLabelStyle(SHZ_ALERT, SHZ_ALERT_TEXT);
        } else if (directUiBusy_) {
            text = "WAITING OPPONENT ACK";
            style = turnLabelStyle(SHZ_WAIT, SHZ_WAIT_TEXT);
        } else if (!gameClockRunning_) {
            if (pcIsHost_ && !directSessionReady_) {
                text = "WAITING OPPONENT";
            } else if (!pcIsHost_ && !directSessionReady_) {
                text = "WAITING HOST";
            } else {
                text = pcIsHost_ ? "PRESS START GAME" : "WAITING OPPONENT START";
            }
            style = turnLabelStyle(SHZ_QUIET, SHZ_ACCENT);
        } else if (gameCheck_) {
            text = pcTurn_ ? "YOUR KING IN CHECK" : "OPPONENT IN CHECK";
            style = pcTurn_ ? turnLabelStyle(SHZ_QUIET, SHZ_CHECK)
                            : turnLabelStyle(SHZ_CHECK, "#101010");
        } else if (pcTurn_) {
            text = QString("YOUR TURN - %1").arg(pcSideName());
            style = turnLabelStyle(SHZ_ACCENT_FILL, "#ffffff");
        } else {
            text = QString("%1 TO MOVE").arg(pcPlaysWhite_ ? "BLACK" : "WHITE");
            style = turnLabelStyle(SHZ_QUIET, SHZ_ACCENT);
        }
        setLabelText(turnLabel_, text);
        setWidgetStyle(turnLabel_, style);

        refreshChatButton();
    }

    // Emphasis buttons: filled when actionable, flat and muted when not.
    static QString actionButtonStyle(const char *background, const char *hover,
                                     const char *foreground, const char *edge)
    {
        return QStringLiteral(
                   "QPushButton { background:%1; color:%3;"
                   " border:1px solid %4; border-radius:" SHZ_R ";"
                   " font-weight:700; font-size:9pt; padding:4px 12px;"
                   " min-height:20px; }"
                   "QPushButton:hover { background:%2; }")
            .arg(QLatin1String(background), QLatin1String(hover),
                 QLatin1String(foreground), QLatin1String(edge));
    }

    static QString idleButtonStyle()
    {
        return actionButtonStyle(SHZ_DISABLED, SHZ_DISABLED, SHZ_TEXT_MUTED,
                                 SHZ_DISABLED);
    }

    static QString moveButtonStyle(bool ready, bool destinationReady)
    {
        if (destinationReady) {
            return actionButtonStyle(SHZ_GO, SHZ_GO_HOVER, SHZ_GO_TEXT, SHZ_GO);
        }
        if (ready) {
            return actionButtonStyle(SHZ_ACCENT_FILL, SHZ_ACCENT_FILL_HOVER,
                                     "#ffffff", SHZ_ACCENT_FILL);
        }
        return idleButtonStyle();
    }

    static QString chatButtonStyle(bool ready)
    {
        return ready ? actionButtonStyle(SHZ_SEND, SHZ_SEND_HOVER,
                                         SHZ_SEND_TEXT, SHZ_SEND)
                     : idleButtonStyle();
    }

    static QString startButtonStyle(bool ready)
    {
        return ready ? actionButtonStyle(SHZ_ACCENT_FILL, SHZ_ACCENT_FILL_HOVER,
                                         "#ffffff", SHZ_ACCENT_FILL)
                     : idleButtonStyle();
    }

    static QString quietButtonStyle(bool ready)
    {
        return ready ? actionButtonStyle(SHZ_QUIET, SHZ_SURFACE_ALT, SHZ_TEXT_DIM,
                                         SHZ_BORDER)
                     : idleButtonStyle();
    }

    void setConnectedUi(bool connected)
    {
        const bool connecting = isConnecting();
        const bool canConnect = connected || connecting || isDirectListening() ||
                                canStartConnection();
        connectButton_->setEnabled(canConnect);
        connectButton_->setText(connected ? "Disconnect" :
                                connecting ? "Cancel" : "Connect");
        startGameButton_->setText(
            directResignRestartPending_ ? "Restarting..." :
            gameOver_ ? "Restart Game" : "Start Game");
        const bool startBaseReady = connected && !gameClockRunning_ &&
                                     !restoreBusy() && directSessionReady_;
        const bool startReady = startBaseReady && pcIsHost_;
        startGameButton_->setEnabled(startReady);
        setWidgetStyle(startGameButton_, startButtonStyle(startReady));
        const bool resetReady = connected && gameClockRunning_ && !restoreBusy();
        resetButton_->setEnabled(resetReady);
        setWidgetStyle(resetButton_, quietButtonStyle(resetReady));
        if (restoreButton_ != nullptr) {
            restoreButton_->setEnabled(false);
            setWidgetStyle(restoreButton_, startButtonStyle(false));
        }
        refreshSaveLoadButtons();
        refreshChatButton();
        refreshSessionCommandButtons();
        hostEdit_->setEnabled(!connected && !connecting);
        portSpin_->setEnabled(!connected && !connecting);
        roomEdit_->setEnabled(!connected && !connecting);
        directRadio_->setEnabled(!connected && !connecting);
        mqttRadio_->setEnabled(!connected && !connecting);
        updateSessionControlsEnabled();
        if (isDirectListening()) {
            setStatusText(NETCHESSZX_UI_NOTICE_LISTENING_OPPONENT);
        } else if (!connected && !connecting && statusMessage_.isEmpty()) {
            setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
        } else if (connected && !directUiBusy_ &&
                   statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED) {
            setStatusText(pcTurn_ ? "Connected - your move" :
                                    "Connected - opponent to move");
        }
        refreshStatusBar();
        refreshTurnLabel();
    }

    bool canStartConnection() const
    {
        const QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();

        if (isMqttMode()) {
            const QString room = roomEdit_ ? roomEdit_->text().trimmed().toUpper() :
                                             QString();
            return !host.isEmpty() && ChessHelpers::isMqttRoomSyntaxOk(room);
        }
        return pcIsHost_ || ChessHelpers::isDirectIpSyntaxOk(host);
    }

    bool canPcMove() const
    {
        return socket_ != nullptr &&
               socket_->state() == QAbstractSocket::ConnectedState &&
               (!isMqttMode() || (mqttSubscribed_ && mqttSideReady_)) &&
               gameClockRunning_ &&
               pcTurn_ && !directUiBusy_ &&
               !restoreBusy();
    }

    bool hasSelectedMoveTarget() const
    {
        return selectedRow_ >= 0 &&
               selectedCol_ >= 0 &&
               targetRow_ >= 0 &&
               targetCol_ >= 0 &&
               isLegalTarget(targetRow_, targetCol_);
    }

    bool pendingMoveCameFromSelection(const QString &move) const
    {
        if (!hasSelectedMoveTarget()) {
            return false;
        }

        const QString selectedMove = ChessHelpers::squareName(selectedRow_, selectedCol_) +
                                     ChessHelpers::squareName(targetRow_, targetCol_);
        return move == selectedMove || move == selectedMove + "q";
    }

    bool canSendChat() const
    {
        if (chatEdit_ == nullptr) {
            return false;
        }
        const QString text = chatEdit_->text().trimmed();
        if (text.isEmpty()) {
            return false;
        }
        const QString cmd = text.toLower();
        if (ChessHelpers::isMoveSyntaxOk(cmd)) {
            return canPcMove();
        }
        if (cmd == "/save" || cmd.startsWith(QStringLiteral("/save ")) ||
            cmd == "/load" || cmd.startsWith(QStringLiteral("/load "))) {
            return true;
        }
        const bool chatDuringControl =
            chatCanSharePendingControl(cmd, directUiBusy_, resetPromptOpen_,
                                       directDecisionBox_ != nullptr);
        if (!isConnected() ||
            (restoreBusy() &&
             (cmd != "/resign" || !resignCanPreemptBusy()) &&
             !chatDuringControl)) {
            return false;
        }

        return directSessionReady_ &&
               (!isMqttMode() || (mqttSubscribed_ && mqttSideReady_)) &&
               statusMessage_ != NETCHESSZX_UI_ERROR_CONNECTION_LOST;
    }

    void refreshChatButton()
    {
        if (chatButton_ == nullptr) {
            return;
        }
        const QString text = chatEdit_->text().trimmed().toLower();
        const bool moveText = ChessHelpers::isMoveSyntaxOk(text);
        const bool ready = canSendChat();
        chatButton_->setText(QStringLiteral("SEND"));
        chatButton_->setEnabled(ready);
        setWidgetStyle(chatButton_, moveText
                                    ? moveButtonStyle(ready, ready && hasSelectedMoveTarget())
                                    : chatButtonStyle(ready));
    }

    void refreshSessionCommandButtons()
    {
        const bool inGame = isConnected() && gameClockRunning_ && !gameOver_;
        const bool canControl = inGame && !restoreBusy() &&
                                (directUiBusy_ == 0u || resignCanPreemptBusy());
        if (drawButton_ != nullptr) {
            drawButton_->setEnabled(canControl);
        }
        if (takebackButton_ != nullptr) {
            takebackButton_->setEnabled(canControl && canRequestTakeback());
        }
        if (resetIconButton_ != nullptr) {
            resetIconButton_->setEnabled(inGame && !restoreBusy());
        }
    }

    void layoutChatCountLabel()
    {
        if (chatEdit_ == nullptr || chatCountLabel_ == nullptr) {
            return;
        }
        chatCountLabel_->adjustSize();
        const int clearGutter = 22;
        const int pad = 8;
        const QSize hint = chatCountLabel_->sizeHint();
        const int x = chatEdit_->width() - hint.width() - clearGutter;
        const int y = (chatEdit_->height() - hint.height()) / 2;
        chatCountLabel_->move(qMax(pad, x), qMax(0, y));
        const QFontMetrics metrics(chatEdit_->font());
        const int textWidth = metrics.horizontalAdvance(chatEdit_->text());
        const int textRight = chatEdit_->textMargins().left() + pad + textWidth;
        chatCountLabel_->setVisible(textRight + 6 < chatCountLabel_->x());
    }

    void resizeToContent()
    {
        if (QWidget *root = centralWidget()) {
            root->layout()->activate();
            const QSize contentSize = root->sizeHint();
            const QSize chromeSize(0, statusBar()->sizeHint().height());
            const QSize windowSize = contentSize + chromeSize;
            setFixedSize(windowSize);
            QTimer::singleShot(0, this, [this]() {
                alignStatusBarToControls();
            });
        }
    }

    void alignStatusBarToControls()
    {
        if (statusBarContents_ == nullptr || statusBarLayout_ == nullptr ||
            flipBoardButton_ == nullptr || logToggleButton_ == nullptr) {
            return;
        }

        const int leftMargin = statusBarContents_->mapFromGlobal(
            flipBoardButton_->mapToGlobal(QPoint(0, 0))).x();
        const int logRight = statusBarContents_->mapFromGlobal(
            logToggleButton_->mapToGlobal(
                QPoint(logToggleButton_->width(), 0))).x();
        const int rightMargin = statusBarContents_->width() - logRight;
        if (leftMargin < 0 || rightMargin < 0) {
            return;
        }

        const QMargins alignedMargins(leftMargin, 0, rightMargin, 0);
        if (statusBarLayout_->contentsMargins() != alignedMargins) {
            statusBarLayout_->setContentsMargins(alignedMargins);
        }
    }

    void updateConnectionModeUi()
    {
        const bool mqtt = isMqttMode();
        const bool directHost = !mqtt && pcIsHost_;

        if (hostCaptionLabel_ != nullptr) {
            hostCaptionLabel_->setText(mqtt ? QStringLiteral("HOST")
                                            : (directHost ? QStringLiteral("LOCAL IP")
                                                          : QStringLiteral("HOST IP")));
        }
        if (roomCaptionLabel_ != nullptr) {
            roomCaptionLabel_->setVisible(mqtt);
        }
        if (roomEdit_ != nullptr) {
            roomEdit_->setVisible(mqtt);
        }
        if (hostEdit_ != nullptr) {
            if (mqtt) {
                hostEdit_->setReadOnly(false);
                hostEdit_->setPlaceholderText("MQTT broker");
                if (directShowingLocalHost_) {
                    hostEdit_->setText(mqttBrokerCache_.isEmpty() ?
                                           QStringLiteral("broker.hivemq.com") :
                                           mqttBrokerCache_);
                } else if (!hostEdit_->text().trimmed().isEmpty() &&
                           looksLikeMqttHost(hostEdit_->text().trimmed())) {
                    mqttBrokerCache_ = hostEdit_->text().trimmed();
                }
                directShowingLocalHost_ = false;
            } else if (directHost) {
                const QString current = hostEdit_->text().trimmed();
                if (!directShowingLocalHost_ && !current.isEmpty() &&
                    !looksLikeMqttHost(current)) {
                    directIpCache_ = current;
                }
                hostEdit_->setPlaceholderText("Local IP");
                hostEdit_->setText(localDirectIpAddress());
                hostEdit_->setReadOnly(true);
                directShowingLocalHost_ = true;
            } else {
                    hostEdit_->setPlaceholderText("Opponent IP");
                hostEdit_->setReadOnly(false);
                if (directShowingLocalHost_) {
                    hostEdit_->setText(directIpCache_);
                }
                directShowingLocalHost_ = false;
            }
        }
        if (directIpHistoryAction_ != nullptr) {
            if (mqtt) {
                directIpHistoryAction_->setVisible(false);
            } else {
                QPixmap spacer(16, 16);
                spacer.fill(Qt::transparent);
                const bool showHistory = !pcIsHost_;
                directIpHistoryAction_->setVisible(true);
                directIpHistoryAction_->setIcon(
                    showHistory ? directIpHistoryIcon() : QIcon(spacer));
                directIpHistoryAction_->setEnabled(
                    showHistory && !directIpHistory_.isEmpty());
            }
        }
    }

    static bool looksLikeMqttHost(const QString &host)
    {
        return host.contains("broker", Qt::CaseInsensitive) ||
               host.contains("mqtt", Qt::CaseInsensitive) ||
               host.contains("hivemq", Qt::CaseInsensitive) ||
               host.contains("mosquitto", Qt::CaseInsensitive);
    }

    void rememberDirectGuestIp(QSettings &settings, const QString &host)
    {
        const QStringList history = directIpHistoryWith(
            settings.value(kDirectIpHistorySettingsKey).toStringList(), host);
        settings.setValue(kDirectIpHistorySettingsKey, history);
        directIpHistory_ = history;
        directIpHistoryAction_->setEnabled(!history.isEmpty());
    }

    void rebuildDirectIpHistoryMenu()
    {
        directIpHistoryMenu_->clear();
        for (const QString &ip : directIpHistory_) {
            QAction *action = directIpHistoryMenu_->addAction(ip);
            connect(action, &QAction::triggered, this, [this, ip]() {
                hostEdit_->setText(ip);
                hostEdit_->setFocus();
            });
        }
    }

    static QString localDirectIpAddress()
    {
        const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        for (const QHostAddress &address : addresses) {
            const QString text = address.toString();
            if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                !address.isLoopback() &&
                !text.startsWith("169.254.")) {
                return text;
            }
        }
        return QHostAddress(QHostAddress::LocalHost).toString();
    }

    bool isPcPiece(char piece) const
    {
        if (pcPlaysWhite_) {
            return piece >= 'A' && piece <= 'Z';
        }
        return piece >= 'a' && piece <= 'z';
    }

    static void setLabelText(QLabel *label, const QString &text)
    {
        if (label != nullptr && label->text() != text) {
            label->setText(text);
        }
    }

    QString endpointText() const
    {
        QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();
        if (host.isEmpty()) {
            host = "-";
        }
        const int port = portSpin_ ? portSpin_->value() : 0;
        if (isMqttMode()) {
            const QString room = roomEdit_ ? roomEdit_->text().trimmed().toUpper() : QString("-");
            return QString("MQTT %1:%2 | room %3").arg(host).arg(port).arg(room);
        }
        if (pcIsHost_) {
            return QString("IP LISTEN:%1").arg(port);
        }
        return QString("IP %1:%2").arg(host).arg(port);
    }

    bool isConnected() const
    {
        if (socket_ == nullptr ||
            socket_->state() != QAbstractSocket::ConnectedState) {
            return false;
        }
        return isMqttMode() || directSessionReady_;
    }

    bool isDirectListening() const
    {
        return directServer_ != nullptr && directServer_->isListening() &&
               (socket_ == nullptr ||
                socket_->state() == QAbstractSocket::UnconnectedState);
    }

    bool isConnectionErrorStatus() const
    {
        return statusMessage_.startsWith("Connection refused") ||
               statusMessage_.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
               statusMessage_ == NETCHESSZX_UI_ERROR_INVALID_IP ||
               statusMessage_ == kDirectHostBusyStatus ||
               statusMessage_ == NETCHESSZX_UI_ERROR_OPPONENT_APP_NOT_READY;
    }

    enum class StatusSeverity {
        Info,
        Waiting,
        Success,
        Error
    };

    static StatusSeverity statusBarTextSeverity(const QString &text)
    {
        if (text == NETCHESSZX_UI_PHASE_DISCONNECTED ||
            text == NETCHESSZX_UI_PHASE_CONNECTION_FAILED ||
            text == NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST ||
            text == NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED ||
            text.startsWith("CHECKMATE") ||
            text.startsWith("STALEMATE") ||
            text.startsWith("RESIGNATION") ||
            text.startsWith("RESTART rejected") ||
            text.startsWith("Connection refused") ||
            text.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
            text.contains("disconnected", Qt::CaseInsensitive) ||
            text.contains("rejected", Qt::CaseInsensitive) ||
            text.contains("failed", Qt::CaseInsensitive) ||
            text.contains("invalid", Qt::CaseInsensitive) ||
            text.contains("illegal", Qt::CaseInsensitive) ||
            text.contains("busy", Qt::CaseInsensitive) ||
            text.contains("not ready", Qt::CaseInsensitive)) {
            return StatusSeverity::Error;
        }
        if (text.contains("confirmed", Qt::CaseInsensitive)) {
            return StatusSeverity::Success;
        }
        if (text.contains("waiting", Qt::CaseInsensitive) ||
            text.contains("pending", Qt::CaseInsensitive) ||
            text.contains("requested", Qt::CaseInsensitive) ||
            text.contains("confirm", Qt::CaseInsensitive) ||
            text.contains("starting", Qt::CaseInsensitive)) {
            return StatusSeverity::Waiting;
        }
        if (text.contains("accepted", Qt::CaseInsensitive) ||
            text.contains("loaded", Qt::CaseInsensitive) ||
            text.contains("saved", Qt::CaseInsensitive) ||
            text.contains("started", Qt::CaseInsensitive) ||
            text.contains("ready", Qt::CaseInsensitive)) {
            return StatusSeverity::Success;
        }
        return StatusSeverity::Info;
    }

    static QString statusBarLabelStyle(StatusSeverity severity)
    {
#define SHZ_STATUS_SHAPE " font-weight:700; font-size:11px;" \
                         " letter-spacing:0.5px; }"
        switch (severity) {
        case StatusSeverity::Error:
            return "QLabel { color:" SHZ_ALERT_BRIGHT ";" SHZ_STATUS_SHAPE;
        case StatusSeverity::Waiting:
            return "QLabel { color:" SHZ_WAIT_BRIGHT ";" SHZ_STATUS_SHAPE;
        case StatusSeverity::Success:
            return "QLabel { color:" SHZ_ACCENT ";" SHZ_STATUS_SHAPE;
        case StatusSeverity::Info:
        default:
            return "QLabel { color:" SHZ_ACCENT ";" SHZ_STATUS_SHAPE;
        }
#undef SHZ_STATUS_SHAPE
    }

    bool isConnecting() const
    {
        if (directConnectRetryActive_) {
            return true;
        }
        if (isDirectListening()) {
            return true;
        }
        if (socket_ == nullptr) {
            return false;
        }
        if (!isMqttMode() &&
            socket_->state() == QAbstractSocket::ConnectedState &&
            !directSessionReady_) {
            return true;
        }
        return socket_->state() == QAbstractSocket::HostLookupState ||
               socket_->state() == QAbstractSocket::ConnectingState;
    }

    QString statusStateText() const
    {
        if (isDirectListening()) {
            return NETCHESSZX_UI_PHASE_LISTENING;
        }
        if (isConnecting()) {
            return NETCHESSZX_UI_PHASE_CONNECTING;
        }
        if (statusMessage_ == NETCHESSZX_UI_PC_EVENT_OPPONENT_LEFT ||
            statusMessage_ == NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST) {
            return statusMessage_;
        }
        if (!isConnected() && isConnectionErrorStatus()) {
            return NETCHESSZX_UI_PHASE_CONNECTION_FAILED;
        }
        if (!isConnected()) {
            return NETCHESSZX_UI_PHASE_DISCONNECTED;
        }
        if (statusMessage_ == NETCHESSZX_UI_ERROR_CONNECTION_LOST) {
            return NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED;
        }
        if (gameOver_) {
            return statusMessage_;
        }
        if (!gameClockRunning_) {
            if (!directSessionReady_) {
                return pcIsHost_ ? NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT
                                 : NETCHESSZX_UI_PHASE_WAITING_HOST;
            }
            return statusMessage_.isEmpty() ? NETCHESSZX_UI_PHASE_OPPONENT_LINKED :
                                             statusMessage_;
        }
        if (directUiBusy_) {
            return NETCHESSZX_UI_NOTICE_WAITING_ACK;
        }
        if (!statusMessage_.isEmpty() && !isTurnStatusMessage(statusMessage_)) {
            return statusMessage_;
        }
        return QStringLiteral("LIVE");
    }

    static bool isTurnStatusMessage(const QString &text)
    {
        const QString lower = text.toLower();
        return text.compare(QLatin1String(NETCHESSZX_UI_PHASE_YOUR_TURN),
                            Qt::CaseInsensitive) == 0 ||
               text.compare(QLatin1String(NETCHESSZX_UI_PHASE_OPPONENT_TURN),
                            Qt::CaseInsensitive) == 0 ||
               lower.contains(QStringLiteral("to move")) ||
               lower.contains(QStringLiteral("your move")) ||
               lower.contains(QStringLiteral("your turn")) ||
               lower.contains(QStringLiteral("opponent turn")) ||
               lower.startsWith(QStringLiteral("move ready"));
    }

    QString statusContextText() const
    {
        if (statusMessage_.compare(NETCHESSZX_UI_ERROR_CONNECTION_LOST, Qt::CaseInsensitive) == 0) {
            return QString();
        }

        if (!isConnected()) {
            const bool connectionStarting = isConnecting() || isDirectListening();
            if (!connectionStarting) {
                if (statusMessage_.isEmpty() ||
                    statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED ||
                    statusMessage_ == statusStateText()) {
                    return QString();
                }
                return statusMessage_;
            }
            if (statusMessage_.isEmpty() || statusMessage_ == statusStateText()) {
                if (!isMqttMode() && !pcIsHost_ &&
                    !ChessHelpers::isDirectIpSyntaxOk(hostEdit_ ? hostEdit_->text().trimmed() :
                                                    QString())) {
                    return endpointText() + " | Invalid IP";
                }
                return endpointText();
            }
            return endpointText() + " | " + statusMessage_;
        }

        if (!gameClockRunning_) {
            if (gameOver_) {
                QStringList parts;
                parts << QString("Side %1").arg(pcSideName());
                if (directResignRestartPending_) {
                    parts << QString(NETCHESSZX_UI_NOTICE_RESTARTING_GAME);
                } else {
                    parts << QString(NETCHESSZX_UI_CONTEXT_PRESS_RESTART);
                }
                parts << endpointText();
                const QString peer = peerMachineStatusText();
                if (!peer.isEmpty()) {
                    parts << peer;
                }
                return parts.join(" | ");
            }
            QString action = pcIsHost_ ? QString(NETCHESSZX_UI_CONTEXT_PRESS_START) :
                                         QString(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            if (!directSessionReady_) {
                action = pcIsHost_ ? QString(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT)
                                   : QString(NETCHESSZX_UI_PHASE_WAITING_HOST);
            }

            QStringList parts;
            parts << QString("Side %1").arg(pcSideName());
            parts << action << endpointText();
            if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText()) {
                parts << statusMessage_;
            }
            const QString peer = peerMachineStatusText();
            if (!peer.isEmpty()) {
                parts << peer;
            }
            return parts.join(" | ");
        }

        QStringList parts;
        parts << QString("Side %1").arg(pcSideName());
        parts << QString("Ply %1").arg(nextPly_);
        if (!lastMove_.isEmpty()) {
            parts << QString("Last %1").arg(lastMove_.toUpper());
        }
        if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText() &&
            statusMessage_ != NETCHESSZX_UI_PHASE_WAITING_OPPONENT &&
            !isTurnStatusMessage(statusMessage_)) {
            parts << statusMessage_;
        }
        const QString peer = peerMachineStatusText();
        if (!peer.isEmpty()) {
            parts << peer;
        }
        return parts.join(" | ");
    }

    QString peerMachineStatusText() const
    {
        if (!directSessionReady_) {
            return QString();
        }
        const char *code = netchess_proto_mach_code(peerPlatform_);
        return QString("VS %1").arg(code == nullptr ? "?" : code);
    }

    void refreshStatusBar()
    {
        const QString stateText = statusStateText();

        setWidgetStyle(statusStateLabel_,
                       statusBarLabelStyle(statusBarTextSeverity(stateText)));
        setLabelText(statusStateLabel_, stateText.toUpper());
        if (statusContextLabel_ != nullptr) {
            const QString contextText = statusContextText();
            setWidgetStyle(statusContextLabel_,
                           statusBarLabelStyle(statusBarTextSeverity(contextText)));
            setLabelText(statusContextLabel_, contextText.toUpper());
        }
    }

    void setStatusText(const QString &text)
    {
        statusMessage_ = text;
        refreshStatusBar();
        refreshTurnLabel();
    }

    void setStatusBarText(const QString &text)
    {
        statusMessage_ = text;
        refreshStatusBar();
    }

    void appendLog(const QString &text)
    {
        static const QLocale locale = QLocale::system();
        const QString line = QLatin1Char('[')
            + locale.toString(QTime::currentTime(), QLocale::LongFormat)
            + QStringLiteral("] ")
            + text;
        logLines_.append(line);
        trimLines(logLines_);
        if (!showingMoveHistory_ && logEdit_ != nullptr) {
            logEdit_->append(line);
            if (QScrollBar *bar = logEdit_->verticalScrollBar()) {
                bar->setValue(bar->maximum());
            }
        }
    }

    static void trimLines(QStringList &lines)
    {
        constexpr int kMaxLines = 400;
        while (lines.size() > kMaxLines) {
            lines.removeFirst();
        }
    }

    void appendMoveRecord(int ply, const QString &move, const QString &notation)
    {
        constexpr int kMaxRecords = 400;
        const MoveRecord record{ply, move, notation};
        MoveRecord trimmedRecord;
        bool trimmed = false;

        moveHistoryRecords_.append(record);
        if (moveHistoryRecords_.size() > kMaxRecords) {
            trimmedRecord = moveHistoryRecords_.first();
            moveHistoryRecords_.removeFirst();
            trimmed = true;
        }
        if (showingMoveHistory_ && logEdit_ != nullptr) {
            updateMoveHistoryTable(record,
                                   trimmed ? &trimmedRecord : nullptr);
        }
    }

    void clearMoveHistory()
    {
        moveHistoryRecords_.clear();
        if (showingMoveHistory_) {
            renderLogView();
        }
    }

    void toggleLogView()
    {
        showingMoveHistory_ = !showingMoveHistory_;
        renderLogView();
    }

    void renderLogView()
    {
        if (logEdit_ == nullptr || logStack_ == nullptr) {
            return;
        }

        if (showingMoveHistory_) {
            renderMoveHistoryTable();
            logStack_->setCurrentWidget(moveTable_);
        } else {
            logEdit_->setLineWrapMode(QTextEdit::NoWrap);
            logEdit_->setPlainText(logLines_.join("\n"));
            logStack_->setCurrentWidget(logEdit_);
        }
        if (logTitleLabel_ != nullptr) {
            logTitleLabel_->setText(showingMoveHistory_ ? "MOVES" : "LOG");
        }
        if (logToggleButton_ != nullptr) {
            logToggleButton_->setText(showingMoveHistory_ ? "Log" : "Moves");
        }
        if (!showingMoveHistory_) {
            if (QScrollBar *bar = logEdit_->verticalScrollBar()) {
                bar->setValue(bar->maximum());
            }
        }
    }

    static QString moveCellText(const MoveRecord &record)
    {
        if (record.move.isEmpty()) {
            return QString();
        }

        const QString uci = record.move.toUpper();
        if (record.notation.isEmpty()) {
            return uci;
        }
        return QStringLiteral("%1 (%2)").arg(uci, record.notation);
    }

    static int moveNumberForPly(int ply)
    {
        return (ply + 1) / 2;
    }

    void setMoveTableItem(int row, int column, const QString &text,
                          Qt::Alignment alignment)
    {
        QTableWidgetItem *item = moveTable_->item(row, column);
        if (item == nullptr) {
            item = new QTableWidgetItem();
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(alignment);
            moveTable_->setItem(row, column, item);
        }
        item->setText(text);
    }

    void initializeMoveHistoryRow(int row, int moveNumber)
    {
        const QString number = moveNumber == 0
            ? QString() : QStringLiteral("%1.").arg(moveNumber);

        setMoveTableItem(row, 0, number,
                         Qt::AlignRight | Qt::AlignVCenter);
        setMoveTableItem(row, 1, QString(),
                         Qt::AlignLeft | Qt::AlignVCenter);
        setMoveTableItem(row, 2, QString(),
                         Qt::AlignLeft | Qt::AlignVCenter);
        moveTable_->setRowHeight(row, 18);
    }

    void syncMoveHistoryRow(int moveNumber)
    {
        const int row = moveNumber - moveTableFirstMoveNumber_;
        if (row < 0 || row >= moveTable_->rowCount()) {
            return;
        }
        const MoveRecord *whiteMove = nullptr;
        const MoveRecord *blackMove = nullptr;
        for (const MoveRecord &record : moveHistoryRecords_) {
            if (moveNumberForPly(record.ply) != moveNumber) {
                continue;
            }
            if ((record.ply % 2) == 1) {
                whiteMove = &record;
            } else {
                blackMove = &record;
            }
        }
        setMoveTableItem(row, 1,
                         whiteMove == nullptr
                             ? QString() : moveCellText(*whiteMove),
                         Qt::AlignLeft | Qt::AlignVCenter);
        setMoveTableItem(row, 2,
                         blackMove == nullptr
                             ? QString() : moveCellText(*blackMove),
                         Qt::AlignLeft | Qt::AlignVCenter);
    }

    void setMoveHistoryRecord(const MoveRecord &record)
    {
        const int moveNumber = moveNumberForPly(record.ply);
        const int row = moveNumber - moveTableFirstMoveNumber_;
        if (row < 0 || row >= moveTable_->rowCount()) {
            return;
        }
        setMoveTableItem(row, (record.ply % 2) == 1 ? 1 : 2,
                         moveCellText(record),
                         Qt::AlignLeft | Qt::AlignVCenter);
    }

    void scrollMoveHistoryToEnd()
    {
        if (QScrollBar *bar = moveTable_->verticalScrollBar()) {
            bar->setValue(bar->maximum());
        }
    }

    void updateMoveHistoryTable(const MoveRecord &record,
                                const MoveRecord *trimmedRecord)
    {
        if (moveTable_ == nullptr || moveHistoryRecords_.isEmpty()) {
            return;
        }
        const int firstMoveNumber =
            moveNumberForPly(moveHistoryRecords_.first().ply);
        const int appendedMoveNumber = moveNumberForPly(record.ply);

        if (moveTable_->rowCount() == 0) {
            moveTableFirstMoveNumber_ = firstMoveNumber;
        } else if (moveTableFirstMoveNumber_ > firstMoveNumber ||
                   appendedMoveNumber < moveTableFirstMoveNumber_) {
            renderMoveHistoryTable();
            return;
        }

        while (moveTable_->rowCount() != 0 &&
               moveTableFirstMoveNumber_ < firstMoveNumber) {
            moveTable_->removeRow(0);
            ++moveTableFirstMoveNumber_;
        }
        if (moveTable_->rowCount() == 0) {
            moveTableFirstMoveNumber_ = firstMoveNumber;
        }

        const int requiredRows =
            appendedMoveNumber - moveTableFirstMoveNumber_ + 1;
        while (moveTable_->rowCount() < requiredRows) {
            const int row = moveTable_->rowCount();

            moveTable_->insertRow(row);
            initializeMoveHistoryRow(row,
                                     moveTableFirstMoveNumber_ + row);
        }
        if (trimmedRecord != nullptr) {
            syncMoveHistoryRow(moveNumberForPly(trimmedRecord->ply));
        }
        setMoveHistoryRecord(record);
        scrollMoveHistoryToEnd();
    }

    void renderMoveHistoryTable()
    {
        if (moveTable_ == nullptr) {
            return;
        }
        if (moveHistoryRecords_.isEmpty()) {
            moveTable_->setRowCount(0);
            moveTableFirstMoveNumber_ = 0;
            return;
        }

        int firstMoveNumber = moveNumberForPly(moveHistoryRecords_.first().ply);
        int lastMoveNumber = firstMoveNumber;

        for (const MoveRecord &record : moveHistoryRecords_) {
            const int moveNumber = moveNumberForPly(record.ply);
            if (moveNumber < firstMoveNumber) {
                firstMoveNumber = moveNumber;
            }
            if (moveNumber > lastMoveNumber) {
                lastMoveNumber = moveNumber;
            }
        }

        moveTableFirstMoveNumber_ = firstMoveNumber;
        moveTable_->setRowCount(lastMoveNumber - firstMoveNumber + 1);

        for (int row = 0; row < moveTable_->rowCount(); ++row) {
            initializeMoveHistoryRow(row, firstMoveNumber + row);
        }
        for (const MoveRecord &record : moveHistoryRecords_) {
            setMoveHistoryRecord(record);
        }
        scrollMoveHistoryToEnd();
    }

    void appendChat(const QString &sender, const QString &text,
                    bool highlighted = false)
    {
        const QString now = QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
        QTextCharFormat format = chatLogEdit_->currentCharFormat();
        format.setFontWeight(sender == opponentChatName() ? QFont::Bold
                                                          : QFont::Normal);
        format.setForeground(highlighted ? QColor(0xff, 0x5a, 0x5a)
                                         : QColor(0xe8, 0xee, 0xf6));
        chatLogEdit_->setCurrentCharFormat(format);
        chatLogEdit_->appendPlainText(QString("%1 %2: %3")
                                          .arg(now, sender, text));
    }

    void appendControlEvent(bool local, const QString &event)
    {
        const bool terminal =
            event == QString::fromLatin1(NETCHESSZX_UI_EVENT_DRAW_AGREED) ||
            event == QString::fromLatin1(NETCHESSZX_UI_EVENT_RESIGNATION_WON) ||
            event == QString::fromLatin1(NETCHESSZX_UI_EVENT_RESIGNATION_LOST);
        appendChat(local ? pcChatName() : opponentChatName(), event,
                   terminal || event == QStringLiteral("DRAW") ||
                       event == QStringLiteral("RESIGN"));
    }

    void clearChatLog()
    {
        if (chatLogEdit_ != nullptr) {
            chatLogEdit_->clear();
        }
    }

    QTcpSocket *socket_ = nullptr;
    QTcpServer *directServer_ = nullptr;
    DesktopSessionController sessionController_;
    DesktopTransportCodec transportCodec_;
    QHash<uint8_t, QPointer<QTcpSocket>> directSockets_;
    QHash<uint8_t, bool> directLinkUpSeen_;
    QPointer<QMessageBox> directDecisionBox_;
    QString directEndStatus_;
    QString mqttBrokerCache_;
    QString directIpCache_;
    bool directShowingLocalHost_ = false;
    QRadioButton *directRadio_ = nullptr;
    QRadioButton *mqttRadio_ = nullptr;
    QRadioButton *roleHostRadio_ = nullptr;
    QRadioButton *roleGuestRadio_ = nullptr;
    QLabel *hostColorLabel_ = nullptr;
    QRadioButton *hostWhiteRadio_ = nullptr;
    QRadioButton *hostBlackRadio_ = nullptr;
    QLabel *hostCaptionLabel_ = nullptr;
    QLabel *roomCaptionLabel_ = nullptr;
    QLabel *chatCountLabel_ = nullptr;
    QPushButton *drawButton_ = nullptr;
    QPushButton *takebackButton_ = nullptr;
    QPushButton *resetIconButton_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QAction *directIpHistoryAction_ = nullptr;
    QMenu *directIpHistoryMenu_ = nullptr;
    QStringList directIpHistory_;
    QLineEdit *roomEdit_ = nullptr;
    QCheckBox *showHintsCheck_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *startGameButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    QLineEdit *moveEdit_ = nullptr;
    QPushButton *flipBoardButton_ = nullptr;
    QLineEdit *chatEdit_ = nullptr;
    QPushButton *chatButton_ = nullptr;
    QPlainTextEdit *chatLogEdit_ = nullptr;
    QLabel *turnLabel_ = nullptr;
    QLabel *selectedLabel_ = nullptr;
    QLabel *statusStateLabel_ = nullptr;
    QLabel *statusContextLabel_ = nullptr;
    QLabel *gameClockLabel_ = nullptr;
    QLabel *moveClockLabel_ = nullptr;
    QWidget *statusBarContents_ = nullptr;
    QHBoxLayout *statusBarLayout_ = nullptr;
    QLabel *logTitleLabel_ = nullptr;
    QStackedWidget *logStack_ = nullptr;
    QTableWidget *moveTable_ = nullptr;
    QPushButton *logToggleButton_ = nullptr;
    QPushButton *saveGameButton_ = nullptr;
    QPushButton *loadGameButton_ = nullptr;
    QTextEdit *logEdit_ = nullptr;
    QTimer *clockTimer_ = nullptr;
    QTimer *directConnectRetryTimer_ = nullptr;
    QTimer *revealTimer_ = nullptr;
    QTimer *flashTimer_ = nullptr;
    QTimer *feedbackTimer_ = nullptr;
    QVector<QPoint> morphSquares_;
    std::function<void(bool)> flashDone_;
    int revealStep_ = 0;
    int revealStepMs_ = 0;
    int revealPauseMs_ = 0;
    int morphMid_ = 0;
    int flashStep_ = 0;
    int flashRow_ = -1;
    int flashCol_ = -1;
    int feedbackStep_ = 0;
    bool revealIsMorph_ = false;
    QLabel *fileLabelsTop_[8] = {};
    QLabel *fileLabelsBottom_[8] = {};
    QLabel *rankLabelsLeft_[8] = {};
    QLabel *rankLabelsRight_[8] = {};
    QPushButton *squares_[8][8] = {};
    QString squareStyleCache_[8][8];
    char board_[8][8] = {};
    QString mqttRoom_;
    QString statusMessage_;
    QString lastSocketError_;
    QString lastMove_;
    QStringList chatInputHistory_;
    QStringList logLines_;
    QVector<MoveRecord> moveHistoryRecords_;
    TakebackSnapshot takebackSnapshot_;
    QStringList legalTargets_;
    QElapsedTimer gameTimer_;
    QElapsedTimer moveTimer_;
    QElapsedTimer linkWatch_;
    QElapsedTimer uiTickTimer_;
    qint64 gameTimerOffsetMs_ = 0;
    qint64 moveTimerOffsetMs_ = 0;
    int nextPly_ = 1;
    int moveTableFirstMoveNumber_ = 0;
    int selectedRow_ = -1;
    int selectedCol_ = -1;
    int chatInputHistoryIndex_ = 0;
    int targetRow_ = -1;
    int targetCol_ = -1;
    int feedbackRow_ = -1;
    int feedbackCol_ = -1;
    int gameGeneration_ = 0;
    int directConnectRetryCount_ = 0;
    uint8_t directNextLinkId_ = 0u;
    uint8_t directPrimaryLinkId_ = SESSION_LINK_NONE;
    uint8_t directDecisionRequestId_ = 0u;
    uint8_t directDecisionControl_ = 0u;
    uint8_t peerPlatform_ = NETCHESS_PLAT_UNKNOWN;
    quint16 mqttSessionId_ = 0;
    quint64 mqttClientNonce_ = QRandomGenerator::global()->generate64();
    bool feedbackOn_ = false;
    bool mqttSubscribed_ = false;
    bool mqttSideReady_ = false;
    bool mqttSessionLinked_ = false;
    bool directSessionReady_ = false;
    bool localMachSent_ = false;
    uint8_t directUiBusy_ = 0u;
    bool directStartTransitionApplied_ = false;
    bool directLocalResignPending_ = false;
    bool directResignRestartPending_ = false;
    bool resetPromptOpen_ = false;
    bool pcTurn_ = false;
    bool pcIsHost_ = false;
    bool ignoreNextDisconnect_ = false;
    bool localDisconnectPending_ = false;
    bool directConnectRetryActive_ = false;
    bool hostPlaysWhite_ = true;
    bool pcPlaysWhite_ = false;
    bool coordinatesInitialized_ = false;
    bool lastCoordinatePcWhite_ = false;
    void applyBoardTexture() {
        const QString tex = boardCombo_ ? boardCombo_->currentData().toString() : QString();
        PieceRenderer::setBoardTexture(tex);
        QSettings s;
        s.setValue("appearance/board", tex);
        if (boardFrame_) {
            boardFrame_->setStyleSheet(boardFrameStyle());
            // A second setStyleSheet on an unshown widget keeps the rules parsed
            // for the first one, so the new well colour needs an explicit repolish.
            boardFrame_->style()->unpolish(boardFrame_);
            boardFrame_->style()->polish(boardFrame_);
            boardFrame_->update();
        }
        refreshBoard();
    }

    void applyPieceSet() {
        const QString set = pieceCombo_ ? pieceCombo_->currentData().toString() : QString();
        PieceRenderer::setPieceSet(set);
        QSettings s;
        s.setValue("appearance/pieces", set);
        if (boardPiecesVisible_) {
            animateBoardPiecesMorph();
        } else {
            refreshBoard();
        }
    }

    QWidget *boardFrame_ = nullptr;
    QComboBox *boardCombo_ = nullptr;
    QComboBox *pieceCombo_ = nullptr;
    bool boardWhiteAtBottom_ = false;
    bool boardPiecesVisible_ = false;
    bool showingMoveHistory_ = true;
    bool gameOver_ = false;
    bool gameCheck_ = false;
    bool gameClockRunning_ = false;
    QHash<uint16_t, QString> mqttSubackPending_;
    QHash<uint16_t, QString> mqttUnsubackPending_;
    QSet<QString> mqttActiveSubscriptions_;
    QSet<QString> mqttTargetSubscriptions_;
    QSet<QString> mqttObsoleteSubscriptions_;
    QVector<MqttBufferedPublish> mqttBufferedPublishes_;
    bool mqttSideTransitionPending_ = false;
    uint16_t mqttNextPacketId_ = 1;
#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
    bool testMqttWriteFailure_ = false;
#endif
};

MainWindow::MainWindow()
    : impl_(std::make_unique<MainWindowImpl>())
{
}

MainWindow::~MainWindow() = default;

QIcon MainWindow::appIcon()
{
    return makeShatranjIcon();
}

void MainWindow::setWindowIcon(const QIcon &icon)
{
    impl_->setWindowIcon(icon);
}

void MainWindow::showNormal()
{
    impl_->showNormal();
}

#ifdef NETCHESSZX_PC_MQTT_TX_FAILURE_TEST
QTcpSocket *MainWindow::testSocket() const { return impl_->testSocket(); }
bool MainWindow::testPrepareMqttGuestSession()
{
    return impl_->testPrepareMqttGuestSession();
}
bool MainWindow::testPrepareMqttHostSession()
{
    return impl_->testPrepareMqttHostSession();
}
bool MainWindow::testEndAndRelinkMqttSession()
{
    return impl_->testEndAndRelinkMqttSession();
}
bool MainWindow::testPrepareMqttGuestBootstrap()
{
    return impl_->testPrepareMqttGuestBootstrap();
}
void MainWindow::testFeedMqtt(const QByteArray &suffix,
                              const QByteArray &payload, bool retained)
{
    impl_->testFeedMqtt(suffix, payload, retained);
}
bool MainWindow::testBeginMqttRestore()
{
    return impl_->testBeginMqttRestore();
}
bool MainWindow::testRestoreUiIdle() const
{
    return impl_->testRestoreUiIdle();
}
void MainWindow::testSetMqttWriteFailure(bool enabled)
{
    impl_->testSetMqttWriteFailure(enabled);
}
bool MainWindow::testSessionReady() const { return impl_->testSessionReady(); }
QString MainWindow::testStatusContextText() const
{
    return impl_->testStatusContextText();
}
bool MainWindow::testStatusBarAligned()
{
    return impl_->testStatusBarAligned();
}
bool MainWindow::testDisconnectButtonAvailable() const
{
    return impl_->testDisconnectButtonAvailable();
}
QByteArray MainWindow::testMqttClientId(bool host) const
{
    return impl_->testMqttClientId(host);
}
void MainWindow::testHandleMqttPacket(const QByteArray &packet)
{
    impl_->testHandleMqttPacket(packet);
}
QHash<uint16_t, QString> MainWindow::testMqttPendingSubacks() const
{
    return impl_->testMqttPendingSubacks();
}
QHash<uint16_t, QString> MainWindow::testMqttPendingUnsubacks() const
{
    return impl_->testMqttPendingUnsubacks();
}
QSet<QString> MainWindow::testMqttActiveSubscriptions() const
{
    return impl_->testMqttActiveSubscriptions();
}
bool MainWindow::testMqttOperational() const
{
    return impl_->testMqttOperational();
}
void MainWindow::testStartDirectGuestConnection(const QString &host, quint16 port)
{
    impl_->testStartDirectGuestConnection(host, port);
}
bool MainWindow::testDirectRetryPending() const
{
    return impl_->testDirectRetryPending();
}
bool MainWindow::testStartDirectHostListener(quint16 port)
{
    return impl_->testStartDirectHostListener(port);
}
bool MainWindow::testDirectListenerActive() const
{
    return impl_->testDirectListenerActive();
}
void MainWindow::testClickConnectButton()
{
    impl_->testClickConnectButton();
}
bool MainWindow::testReplaceDirectClientBeforeDisconnect()
{
    return impl_->testReplaceDirectClientBeforeDisconnect();
}
bool MainWindow::testResignRestartUiProjection()
{
    return impl_->testResignRestartUiProjection();
}
bool MainWindow::testRestoredMoveProjection()
{
    return impl_->testRestoredMoveProjection();
}
bool MainWindow::testSessionEndPresentation()
{
    return impl_->testSessionEndPresentation();
}
bool MainWindow::testCancelPendingPieceFlash()
{
    return impl_->testCancelPendingPieceFlash();
}
#endif
