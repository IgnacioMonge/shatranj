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
#include <QFileDialog>
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
#include <QMenuBar>
#include <QShortcut>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSettings>
#include <QSet>
#include <QShowEvent>
#include <QSize>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
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
#include "common/chess/game_stats.h"
#include "common/protocol/platform_protocol.h"
#include "common/savegame/savegame_wire.h"
#include "common/chess/rules_compact.h"
#include "common/session/session.h"
}
#include "common/chess/legal.h"
#include "common/ui_messages.h"
#include "app_banner.h"
#include "board_widget.h"
#include "chess_helpers.h"
#include "command_input_widget.h"
#include "desktop_session_controller.h"
#include "desktop_transport_codec.h"
#include "engine_player.h"
#include "help_window.h"
#include "preferences.h"
#include "preferences_dialog.h"
#include "piece_renderer.h"
#include "window_chrome.h"
#include "save_game_store.h"
#include "ui_theme.h"

namespace {
using namespace PieceRenderer;

constexpr int kActionButtonWidth = 100;
constexpr int kSidePanelWidth = 340;
constexpr int kChatTextMax = SESSION_CHAT_TEXT_MAX;
// A TCP listener may exist before the remote app is ready for its DIRECT handshake.
constexpr const char *kAppVersion = NETCHESSZX_APP_VERSION;
constexpr const char *kDirectHostBusyStatus = "Host busy";
constexpr qint64 kUiStallWarnMs = 2500;
constexpr qint64 kMoveSendWarnMs = 250;
constexpr int kEngineMoveTimeMs = 800;
// Restarts of a dead engine before it answers again; reset by each answer.
constexpr int kEngineRestartLimit = 1;
// Pause between attempts to announce the engine while the core refuses CHAT.
constexpr int kEngineAnnounceRetryMs = 1000;
// Takeback, draw offers and resignation belong to the engine's side, not to
// the operator: refused while Stockfish plays.
constexpr char kBotControlsRefused[] = "Not while Stockfish plays";
// Peer rejections of the engine's move in one position before it stops asking.
constexpr int kEngineRejectLimit = 3;
using Preferences::kBotName;
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

// Three rising bars: dim while the statistics are hidden, accent while shown.
static QIcon gameStatsIcon()
{
    QIcon icon;
    for (const bool shown : {false, true}) {
        QPixmap pixmap(16, 16);
        const QColor color(QLatin1String(shown ? SHZ_ACCENT : SHZ_TEXT_DIM));
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.fillRect(2, 9, 3, 4, color);
        painter.fillRect(7, 6, 3, 7, color);
        painter.fillRect(12, 3, 3, 10, color);
        icon.addPixmap(pixmap, QIcon::Normal, shown ? QIcon::On : QIcon::Off);
    }
    return icon;
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

static QString normalizedPlayerName(const QString &text)
{
    const QByteArray name = text.trimmed().toUpper().toLatin1();

    return netchess_player_name_valid(name.constData())
               ? QString::fromLatin1(name)
               : QString();
}

static QByteArray localNamePayload(const QString &name)
{
    char text[NETCHESS_PROTO_NAME_LINE_SIZE];
    const QByteArray latin = name.toLatin1();

    if (!netchess_proto_format_name(text, sizeof(text), latin.constData())) {
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
        "QCheckBox::indicator { width:11px; height:11px; border-radius:3px;"
        " border:1px solid " SHZ_TEXT_MUTED "; background:" SHZ_SURFACE "; }"
        "QCheckBox::indicator:hover { border-color:" SHZ_ACCENT "; }"
        "QCheckBox::indicator:checked { background:" SHZ_ACCENT ";"
        " border:1px solid " SHZ_ACCENT "; }"
        "QCheckBox::indicator:disabled { background:" SHZ_DISABLED ";"
        " border-color:" SHZ_BORDER "; }"
        "QCheckBox::indicator:checked:disabled { background:" SHZ_TEXT_MUTED ";"
        " border:1px solid " SHZ_TEXT_MUTED "; }"
        "QTabWidget::pane { background:" SHZ_CARD "; border:1px solid " SHZ_BORDER ";"
        " border-radius:" SHZ_R "; top:-1px; }"
        "QTabBar::tab { background:" SHZ_QUIET "; color:" SHZ_TEXT_DIM ";"
        " border:1px solid " SHZ_BORDER "; padding:5px 14px; margin-right:2px;"
        " border-top-left-radius:" SHZ_R "; border-top-right-radius:" SHZ_R "; }"
        "QTabBar::tab:selected { background:" SHZ_CARD "; color:" SHZ_ACCENT ";"
        " border-bottom-color:" SHZ_CARD "; }"
        "QListWidget { background:" SHZ_SURFACE "; color:" SHZ_TEXT ";"
        " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R "; padding:4px; }"
        "QListWidget::item { padding:2px 4px; }"
        "QListWidget::item:selected { background:" SHZ_ACCENT_DEEP ";"
        " color:" SHZ_WELL "; }"
        "QProgressBar { background:" SHZ_SURFACE "; border:1px solid " SHZ_BORDER ";"
        " border-radius:" SHZ_R "; color:" SHZ_TEXT "; text-align:center; }"
        "QProgressBar::chunk { background:" SHZ_ACCENT_DEEP "; border-radius:" SHZ_R "; }"
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

    QString testChatLogText() const
    {
        return chatLogEdit_->toPlainText();
    }

    QString testChatDraft() const
    {
        return chatEdit_->text();
    }

    void testSetChatDraft(const QString &text)
    {
        chatEdit_->setText(text);
    }

    QString testStatusContextText() const
    {
        return statusContextText();
    }

    // Needs a linked session: a later notice keeps the game result, and an
    // opponent request awaiting this side's answer is not shown as waiting.
    bool testStatusBarProjection()
    {
        const bool savedOver = gameOver_;
        const QString savedResult = gameResult_;
        const QString savedMessage = statusMessage_;

        gameOver_ = true;
        gameResult_ = QString::fromLatin1(NETCHESSZX_UI_EVENT_CHECKMATE_WON);
        setStatusText(QStringLiteral("Game saved"));
        const bool resultKept =
            statusStateText() == gameResult_ && turnLabel_->text() == gameResult_ &&
            statusContextText().startsWith(QStringLiteral("Game saved | "));

        directDecisionControl_ = SESSION_REQUEST_DRAW;
        setStatusText(NETCHESSZX_UI_NOTICE_DRAW_OFFERED);
        const bool answerShown =
            statusStateText() == QLatin1String(NETCHESSZX_UI_NOTICE_DRAW_OFFERED) &&
            turnLabel_->text() == QStringLiteral("YOUR ANSWER");

        directDecisionControl_ = 0u;
        gameOver_ = savedOver;
        gameResult_ = savedResult;
        setStatusText(savedMessage);
        return resultKept && answerShown;
    }

    void testSetPlayerName(const QString &name)
    {
        prefs_.playerName = Preferences::userName(name);
    }

    // Bot identity without launching an engine: reserved name, saved user
    // name, chat label and locked board input.
    bool testBotIdentity()
    {
        const ClientPreferences saved = prefs_;
        ClientPreferences bot = prefs_;
        bot.playerName = QStringLiteral("TESTER");
        bot.enginePlays = true;
        bot.engineLevel = 7;
        applyPreferences(bot, false);
        const bool identity = localPlayerName() == QLatin1String(kBotName) &&
                              pcChatName() == QLatin1String(kBotName) &&
                              engineButton_->isChecked() &&
                              statusContextText().startsWith(QStringLiteral("STOCKFISH L7"));
        // The engine plays: the operator cannot take back, offer a draw or
        // resign on its behalf, by button or by chat command.
        bool commandsRefused = true;
        for (const QString &command : {QStringLiteral("/takeback"), QStringLiteral("/draw"),
                                       QStringLiteral("/resign")}) {
            chatEdit_->setText(command);
            sendChat();
            commandsRefused = commandsRefused &&
                              statusMessage_ == QLatin1String(kBotControlsRefused) &&
                              chatEdit_->text().isEmpty();
        }
        const bool controlsHidden = takebackButton_->isHidden() && drawButton_->isHidden();
        ClientPreferences human = bot;
        human.enginePlays = false;
        applyPreferences(human, false);
        const bool restored = localPlayerName() == QStringLiteral("TESTER") &&
                              !engineButton_->isChecked() &&
                              !statusContextText().contains(QStringLiteral("STOCKFISH")) &&
                              !takebackButton_->isHidden() && !drawButton_->isHidden();
        applyPreferences(saved, false);
        return identity && commandsRefused && controlsHidden && restored &&
               Preferences::userName(QStringLiteral("stockfish")).isEmpty();
    }

    QStringList testMoveHeaderLabels() const
    {
        return {moveHeaderWhite_, moveHeaderBlack_};
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

        const int leftAnchor = boardWidget_->flipButtonGlobalPosition().x();
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
        gameResult_ = QString::fromLatin1(NETCHESSZX_UI_EVENT_RESIGNATION_LOST);
        directUiBusy_ = SESSION_REQUEST_RESIGN;
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool pendingBlocked =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_PC_NOTICE_RESIGNING);
        handleDirectControlResult(SESSION_REQUEST_RESIGN,
                                  SESSION_CONTROL_ACCEPTED);
        chatEdit_->setText(QStringLiteral("/resign"));
        sendChat();
        const bool completedBlocked =
            statusMessage_ == gameResult_ && directUiBusy_ == 0u;
        handleDirectControlResult(SESSION_REQUEST_RESET,
                                  SESSION_CONTROL_REJECTED);
        const bool failedRestart =
            statusMessage_ ==
            QString::fromLatin1(NETCHESSZX_UI_ERROR_RESTART_REJECTED) &&
            gameResult_ == QString::fromLatin1(NETCHESSZX_UI_EVENT_RESIGNATION_LOST);
        gameOver_ = false;
        gameClockRunning_ = true;
        directUiBusy_ = 0u;
        const bool originalHost = pcIsHost_;
        pcIsHost_ = false;
        directSessionReady_ = true;
        applyDirectResignTransition();
        const bool remoteResign =
            statusMessage_ ==
                QString::fromLatin1(NETCHESSZX_UI_EVENT_RESIGNATION_WON) &&
            directUiBusy_ == 0u &&
            startGameButton_->text() == QStringLiteral("Restart Game") &&
            !startGameButton_->isEnabled() && gameStatsButton_->isEnabled();
        // Only the host asks for a new game; the guest waits and answers.
        pcIsHost_ = true;
        setConnectedUi(true);
        const bool hostRestarts = startGameButton_->isEnabled();
        directUiBusy_ = SESSION_REQUEST_RESET;
        setConnectedUi(true);
        const bool statsHiddenWhileRestartPending = !gameStatsButton_->isEnabled();
        directUiBusy_ = 0u;
        pcIsHost_ = false;
        setConnectedUi(true);
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
        directUiBusy_ = 0u;
        gameOver_ = false;
        gameClockRunning_ = false;
        setConnectedUi(true);
        const bool guestCannotStart = !startGameButton_->isEnabled();
        pcIsHost_ = originalHost;
        directSessionReady_ = false;
        chatEdit_->clear();
        setConnectedUi(false);
        return pendingBlocked && completedBlocked && failedRestart &&
               remoteResign && hostRestarts && statsHiddenWhileRestartPending &&
               terminalReasons && controlHighlighted &&
               regularChatNormal && chatSharesControls && guestCannotStart;
    }

    bool testGameStatsProjection()
    {
        startGameFromAck();
        const auto move = [this](const char *text, qint64 seconds) {
            moveTimerOffsetMs_ = seconds * 1000;
            moveTimer_.restart();
            applyDirectLocalMove(static_cast<uint16_t>(nextPly_), QString::fromLatin1(text));
        };
        move("e2e4", 3);
        move("d7d5", 5);
        move("e4d5", 7);
        bool ok = game_stats_total(&gameStats_, 0) == 10 &&
            game_stats_total(&gameStats_, 1) == 5 &&
            game_stats_longest(&gameStats_, 0) == 7 &&
            game_stats_at(&gameStats_, 0) == 2 &&
            game_stats_captured(&gameStats_, 0, 1) == 1;
        const game_stats_t beforeRejectedLoad = gameStats_;
        ok = ok && !applyDirectRestore(QByteArray("invalid"), nullptr, nullptr) &&
            std::memcmp(&gameStats_, &beforeRejectedLoad, sizeof(gameStats_)) == 0;
        restoreTakebackSnapshot();
        ok = ok && game_stats_total(&gameStats_, 0) == 3 &&
            game_stats_captured(&gameStats_, 0, 1) == 0;
        startGameFromAck();
        for (const char *text : {"e2e4", "a7a6", "e4e5", "d7d5", "e5d6"}) move(text, 1);
        ok = ok && game_stats_captured(&gameStats_, 0, 1) == 1;
        restoreTakebackSnapshot();
        ok = ok && game_stats_captured(&gameStats_, 0, 1) == 0;
        netchesszx_save_state_t state = {};
        std::memset(state.cells, '.', sizeof(state.cells));
        state.cells[0] = 'Q';  // A previously promoted queen counts as a queen.
        state.cells[1] = 'r';
        state.cells[4] = 'k';
        state.cells[60] = 'K';
        state.side = NETCHESSZX_SAVE_SIDE_BLACK;
        state.ply = 1;
        state.ep = NETCHESSZX_SAVE_EP_NONE;
        state.host_color = NETCHESSZX_SAVE_HOST_WHITE;
        state.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
        ok = restoreApplyState(state) && ok;
        move("b8a8", 9);
        ok = ok && game_stats_captured(&gameStats_, 1, 5) == 1 &&
            game_stats_balance(&gameStats_) == -9;
        restoreTakebackSnapshot();
        ok = ok && game_stats_captured(&gameStats_, 1, 5) == 0;
        state.cells[0] = '.';
        state.cells[1] = '.';
        state.cells[8] = 'P';
        state.side = NETCHESSZX_SAVE_SIDE_WHITE;
        state.ply = 20;
        ok = restoreApplyState(state) && ok;
        ok = ok && (gameStats_.bytes[GAME_STATS_FLAGS] & GAME_STATS_PARTIAL) &&
            game_stats_total(&gameStats_, 0) == 0 &&
            game_stats_moves(&gameStats_, 20, 0) == 0;
        move("a7a8q", 12);
        ok = ok && game_stats_balance(&gameStats_) == 0 &&
            game_stats_moves(&gameStats_, 21, 0) == 1;
        endGameOver(QStringLiteral("Agreed draw"));
        setStatusText(QStringLiteral("Waiting for restart"));
        gameStatsButton_->click();
        auto *result = boardWidget_->findChild<QLabel *>(QStringLiteral("gameStatsResult"));
        ok = ok && boardWidget_->gameStatsVisible() && result != nullptr &&
            result->text() == QStringLiteral("Agreed draw");
        // A key no longer closes the page; the lit button does.
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" "));
        QApplication::sendEvent(chatEdit_, &key);
        ok = ok && boardWidget_->gameStatsVisible() && gameStatsButton_->isChecked();
        gameStatsButton_->click();
        ok = ok && !boardWidget_->gameStatsVisible() && !gameStatsButton_->isChecked();
        gameStatsButton_->click();
        ok = restoreApplyState(state) && ok && !boardWidget_->gameStatsVisible();
        startGameFromAck();
        ok = ok && gameStats_.bytes[GAME_STATS_FLAGS] == 0 &&
            game_stats_total(&gameStats_, 0) == 0;
        endGameOver(QStringLiteral("Agreed draw"));
        gameStatsButton_->click();
        resetGame(NETCHESSZX_UI_PHASE_DISCONNECTED);
        return ok && !boardWidget_->gameStatsVisible() &&
            gameStats_.bytes[GAME_STATS_FLAGS] == 0 && gameResult_.isEmpty();
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
        const bool pending = boardWidget_->hasPendingPieceFlash();

        boardWidget_->invalidatePieceFlash();
        return pending && directUiBusy_ == 0u;
    }

    bool testClickDrawButton()
    {
        if (drawButton_ == nullptr || !drawButton_->isEnabled()) {
            return false;
        }
        drawButton_->click();
        return true;
    }

    QString testTurnLabelText() const
    {
        return turnLabel_ != nullptr ? turnLabel_->text() : QString();
    }

    bool testEngineAnnounceKeepsRetrying()
    {
        const ClientPreferences saved = prefs_;
        ClientPreferences bot = prefs_;
        bot.enginePlays = true;
        applyPreferences(bot, false);
        // No session: every attempt is refused.
        gameClockRunning_ = true;
        engineAnnounced_ = false;
        engineAnnounceAttempts_ = 0;
        engineAnnounceTimer_.setInterval(5);
        announceEngine();
        QElapsedTimer wait;
        wait.start();
        while (wait.elapsed() < 200) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        const bool keepsTrying = engineAnnounceTimer_.isActive() &&
                                 engineAnnounceAttempts_ > 8;
        engineAnnounceTimer_.stop();
        engineAnnounceTimer_.setInterval(kEngineAnnounceRetryMs);
        gameClockRunning_ = false;
        engineAnnounceAttempts_ = 0;
        applyPreferences(saved, false);
        return keepsTrying;
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
        /* COLOR is last: it hides for guests and must not move the rest. */
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

        auto *boardWidget = new BoardWidget(root);
        boardWidget_ = boardWidget;
        boardWidget_->setPosition(board_);

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

        auto *settingsButton = new QToolButton(root);
        settingsButton->setText(QString(QChar(0x2699)));
        settingsButton->setAccessibleName(QStringLiteral("Preferences"));
        settingsButton->setToolTip(QStringLiteral("Preferences (Ctrl+,)"));
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
        // Stockfish switch: a square glyph button like the gear, shown only
        // when the engine is installed and enabled in Preferences.
        engineButton_ = new QToolButton(root);
        engineButton_->setText(QString(QChar(0x265E)));
        engineButton_->setCheckable(true);
        engineButton_->setAccessibleName(QStringLiteral("Stockfish plays this side"));
        engineButton_->setFixedSize(32, 32);
        engineButton_->setFocusPolicy(Qt::StrongFocus);
        // Off is a grey glyph. On wears the engine's violet (the board frame
        // and turn label colours) and keeps it while locked in a session, so
        // a game never starts with Stockfish on unnoticed.
        engineButton_->setStyleSheet(
            "QToolButton { background:" SHZ_QUIET "; color:" SHZ_TEXT_MUTED ";"
            " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R ";"
            " font-weight:700; font-size:17px; padding:0; }"
            "QToolButton:hover { background:" SHZ_SURFACE_ALT ";"
            " border-color:" SHZ_ACCENT "; color:" SHZ_TEXT "; }"
            "QToolButton:checked { background:" SHZ_BOT_FILL ";"
            " border:2px solid " SHZ_BOT "; color:#ffffff; }"
            "QToolButton:checked:hover { background:" SHZ_BOT_HIT_EDGE ";"
            " border-color:" SHZ_BOT_HIT "; }"
            "QToolButton:disabled { color:" SHZ_BORDER "; }"
            "QToolButton:checked:disabled { background:" SHZ_BOT_FILL ";"
            " border-color:" SHZ_BOT "; color:" SHZ_TEXT_DIM "; }");
        actionRow->insertWidget(2, engineButton_);
        actionRow->insertWidget(3, settingsButton);
        auto *helpButton = new QToolButton(root);
        helpButton->setText(QStringLiteral("?"));
        helpButton->setAccessibleName(QStringLiteral("Help"));
        helpButton->setToolTip(QStringLiteral("Help and manual (F1)"));
        helpButton->setFixedSize(32, 32);
        helpButton->setFocusPolicy(Qt::StrongFocus);
        helpButton->setStyleSheet(settingsButton->styleSheet());
        actionRow->insertWidget(4, helpButton);
        connect(settingsButton, &QToolButton::clicked, this, [this]() { openPreferences(); });
        connect(engineButton_, &QToolButton::clicked, this, [this](bool on) {
            setEnginePlays(on);
        });
        connect(helpButton, &QToolButton::clicked, this, [this]() { openHelp(); });
        connect(new QShortcut(QKeySequence::HelpContents, this), &QShortcut::activated,
                this, [this]() { openHelp(); });
        connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this),
                &QShortcut::activated, this, [this]() { openPreferences(); });
#ifdef Q_OS_MACOS
        {
            QMenu *appMenu = menuBar()->addMenu(QStringLiteral("Shatranj"));
            QAction *prefsAction = appMenu->addAction(QStringLiteral("Preferences..."));
            prefsAction->setMenuRole(QAction::PreferencesRole);
            connect(prefsAction, &QAction::triggered, this, [this]() { openPreferences(); });
            QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("Help"));
            connect(helpMenu->addAction(QStringLiteral("Shatranj Help")), &QAction::triggered,
                    this, [this]() { openHelp(); });
        }
#endif
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

        chatEdit_ = new CommandInputWidget(kChatTextMax, root);
        chatButton_ = new QPushButton("SEND", root);
        chatButton_->setEnabled(false);
        configureActionButton(chatButton_);
        chatButton_->setMinimumSize(72, 32);
        setWidgetStyle(chatButton_, chatButtonStyle(false));

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
        // Player names start where each column's moves start.
        moveTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft |
                                                            Qt::AlignVCenter);
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
        // A toggle: the icon lights up while the statistics cover the board.
        gameStatsButton_ = new QPushButton(root);
        gameStatsButton_->setAccessibleName(QStringLiteral("Game stats"));
        gameStatsButton_->setToolTip(QStringLiteral("Game stats"));
        gameStatsButton_->setIcon(gameStatsIcon());
        gameStatsButton_->setIconSize(QSize(16, 16));
        gameStatsButton_->setMinimumSize(32, 32);
        gameStatsButton_->setFocusPolicy(Qt::StrongFocus);
        gameStatsButton_->setCheckable(true);
        gameStatsButton_->setEnabled(false);
        logToggleButton_ = new QPushButton("Log", root);
        logToggleButton_->setMinimumSize(kActionButtonWidth, 32);
        logActionRow->addStretch(1);
        logActionRow->addWidget(saveGameButton_);
        logActionRow->addWidget(loadGameButton_);
        logActionRow->addWidget(gameStatsButton_);
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

        engineAnnounceTimer_.setSingleShot(true);
        engineAnnounceTimer_.setInterval(kEngineAnnounceRetryMs);
        connect(&engineAnnounceTimer_, &QTimer::timeout, this, [this]() {
            announceEngine();
        });
        engineMoveTimer_.setSingleShot(true);
        connect(&engineMoveTimer_, &QTimer::timeout, this, [this]() {
            playEngineAnswer(engineAnswerFen_, engineAnswerMove_);
        });

        directConnectRetryTimer_ = new QTimer(this);
        directConnectRetryTimer_->setSingleShot(true);
        directConnectRetryTimer_->setInterval(kDirectConnectRetryIntervalMs);
        connect(directConnectRetryTimer_, &QTimer::timeout, this, [this]() {
            retryDirectConnection();
        });

        configureSessionControllerCallbacks();

        connectUiSignals();
        {
            QSettings stored;
            applyPreferences(ClientPreferences::load(stored));
        }

        setConnectedUi(false);
        updateClockLabels();
        refreshBoard();
        animateBoardPiecesIn();
    }

    ~MainWindowImpl() override
    {
        delete helpWindow_;
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
        connect(boardWidget_, &BoardWidget::flipRequested, this, [this]() {
            boardWidget_->setWhiteAtBottom(!boardWidget_->whiteAtBottom());
            refreshBoard();
        });
        connect(boardWidget_, &BoardWidget::squareActivated,
                this, &MainWindowImpl::squareClicked);
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
        engine_.onBestMove = [this](const QString &fen, const QString &move) {
            engineRestarts_ = 0;
            if (prefs_.engineDelay <= 0) {
                playEngineAnswer(fen, move);
                return;
            }
            // The pause leaves the opponent time to ask for a takeback. The
            // request stays pending meanwhile, so refreshes do not ask again.
            engineAnswerFen_ = fen;
            engineAnswerMove_ = move;
            engineMoveTimer_.start(prefs_.engineDelay * 1000);
        };
        engine_.onError = [this](const QString &error) {
            appendLog(QStringLiteral("ERROR: engine: %1").arg(error));
            forgetEngineRequest();
            // Never hand the side back to a human: the peer was told STOCKFISH
            // plays. One restart keeps that identity; if the engine dies again
            // before it has moved, the operator can resign or disconnect.
            if (botMode() && engineRestarts_ < kEngineRestartLimit) {
                ++engineRestarts_;
                QTimer::singleShot(0, this, [this]() { restartEngine(); });
                return;
            }
            setStatusText(QStringLiteral("Stockfish stopped: resign or disconnect"));
        };
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
                bool declined = false;
                if (!confirmLocalRequest(NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE,
                                         NETCHESSZX_UI_CONFIRM_RESTART_GAME,
                                         &declined)) {
                    if (declined) {
                        setStatusText(gameResult_);
                    }
                    return;
                }
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_RESTART_REQUESTED);
                    setConnectedUi(true);
                }
                return;
            }
            sendGameStart();
        });
        // The page reads the live sample, so it also works during a game and
        // is rebuilt after every move while it is open.
        showGameStatsPanel_ = [this]() {
            {
                BoardWidget::GameStatsView view;
                view.result = gameOver_ ? gameResult_
                                        : QStringLiteral("Game in progress");
                view.names[0] = moveHeaderWhite_;
                view.names[1] = moveHeaderBlack_;
                view.partial = gameStats_.bytes[GAME_STATS_FLAGS] & GAME_STATS_PARTIAL;
                const uint16_t ply = static_cast<uint16_t>(nextPly_ - 1);
                const auto duration = [](uint16_t seconds) {
                    return QTime(0, 0).addSecs(seconds).toString(seconds >= 3600 ? "h:mm:ss" : "m:ss");
                };
                // AT 255 means move 255 or later (common/chess/game_stats.h).
                const auto moveNumber = [](uint8_t at) {
                    return QString::number(at) + (at == GAME_STATS_AT_SATURATED ? "+" : "");
                };
                const uint8_t whiteShare = game_stats_white_share(&gameStats_);
                const bool anyLimit = gameStats_.bytes[GAME_STATS_FLAGS] &
                    (GAME_STATS_TOTAL_LIMIT_WHITE | (GAME_STATS_TOTAL_LIMIT_WHITE << 1));
                for (uint8_t side = 0; side < 2; ++side) {
                    const uint16_t moves = game_stats_moves(&gameStats_, ply, side);
                    const uint16_t total = game_stats_total(&gameStats_, side);
                    const bool limited = gameStats_.bytes[GAME_STATS_FLAGS] & (GAME_STATS_TOTAL_LIMIT_WHITE << side);
                    const uint16_t longest = game_stats_longest(&gameStats_, side);
                    const uint16_t fastest = game_stats_fastest(&gameStats_, side);
                    view.metrics[side] = QStringList{
                        duration(total) + (limited ? "+" : ""),
                        !moves || limited ? QStringLiteral("--") : duration(total / moves),
                        !moves ? QStringLiteral("--") : duration(longest) + (longest == 65535 && limited ? "+" : ""),
                        !moves ? QStringLiteral("--") : moveNumber(game_stats_at(&gameStats_, side)),
                        !moves ? QStringLiteral("--") : duration(fastest) + (fastest == 65535 && limited ? "+" : ""),
                        !moves ? QStringLiteral("--") : moveNumber(game_stats_fastest_at(&gameStats_, side))
                    };
                    if (whiteShare != 255 && !anyLimit) {
                        view.share[side] = QStringLiteral("%1%").arg(side ? 100 - whiteShare : whiteShare);
                    }
                    view.material[side] = QString::number(game_stats_material(&gameStats_, side));
                    for (uint8_t type = 5; type != 0; --type) {
                        char piece = " PNBRQ"[type];
                        if (side == 0) piece = static_cast<char>(piece + ('a' - 'A'));
                        view.captured[side].append(QString(game_stats_captured(&gameStats_, side, type), QChar(piece)));
                    }
                }
                boardWidget_->showGameStats(view);
            }
        };
        connect(gameStatsButton_, &QPushButton::clicked, this, [this]() {
            if (boardWidget_->gameStatsVisible()) {
                boardWidget_->hideGameStats();
            } else {
                showGameStatsPanel_();
            }
        });
        connect(boardWidget_, &BoardWidget::gameStatsVisibilityChanged,
                gameStatsButton_, &QPushButton::setChecked);
        connect(resetButton_, &QPushButton::clicked, this, [this]() {
            const bool connected = isConnected();
            if (connected) {
                if (restoreBusy()) {
                    setStatusText("Load in progress");
                    return;
                }
                if (!confirmLocalRequest(NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE,
                                         NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST)) {
                    return;
                }
                if (submitSessionLocalRequest(SESSION_REQUEST_RESET)) {
                    setStatusText(NETCHESSZX_UI_NOTICE_RESET_REQUESTED);
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
                [this](const QString &) {
            refreshChatButton();
        });
        connect(chatEdit_, &CommandInputWidget::submitRequested,
                this, &MainWindowImpl::sendChat);
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
        uint8_t capturedType = 0u;
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
        if (helpWindow_ != nullptr) {
            helpWindow_->close();
        }
        QMainWindow::closeEvent(event);
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
                    "href=\"https://github.com/IgnacioMonge/shatranj\">"
                    "github.com/IgnacioMonge/shatranj</a>"
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
        if (boardWidget_ != nullptr) {
            boardWidget_->setPosition(board_);
        }
        return true;
    }

    void refreshBoard()
    {
        boardWidget_->refresh();
    }

    void refreshChangedBoardSquares(const char previous[8][8])
    {
        boardWidget_->refreshChangedSquares(previous);
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
        return botMode() ? QString::fromLatin1(kBotName) : QStringLiteral("PLAYER");
    }

    QString opponentChatName() const
    {
        return QStringLiteral("OPPONENT");
    }

    bool syncBoardOrientationWithPcSide()
    {
        return boardWidget_->setWhiteAtBottom(pcPlaysWhite_);
    }

    void refreshBoardOrientationFromSide()
    {
        if (!syncBoardOrientationWithPcSide()) {
            return;
        }
        if (boardWidget_->piecesVisible() && !gameClockRunning_ && !gameOver_) {
            boardWidget_->setPiecesVisible(false);
            refreshBoard();
            animateBoardPiecesIn();
            return;
        }
        refreshBoard();
    }

    void animateBoardPiecesIn()
    {
        boardWidget_->animatePiecesIn();
    }

    void animateBoardPiecesMorph()
    {
        boardWidget_->animatePiecesMorph();
    }

    void flashPieceAt(int row, int col, std::function<void(bool)> done)
    {
        boardWidget_->flashPieceAt(row, col, std::move(done));
    }

    void refreshSelectionFootprint(const BoardWidget::SelectionState &previous)
    {
        boardWidget_->refreshSelectionFootprint(previous);
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
        lastAnswer_.at.invalidate();
        directSockets_.clear();
        transportCodec_.clear();
        directLinkUpSeen_.clear();
        directNextLinkId_ = 0u;
        directPrimaryLinkId_ = SESSION_LINK_NONE;
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
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
        lastAnswer_.at.invalidate();
        transportCodec_.clear();
        directSessionReady_ = false;
        directUiBusy_ = false;
        directStartTransitionApplied_ = false;
        directLocalResignPending_ = false;
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
        peerName_.clear();
        localMachSent_ = false;
        engineAnnounced_ = false;
        engineAnnounceAttempts_ = 0;
        engineAnnounceTimer_.stop();
        refreshMoveTableHeaders();
    }

    bool botMode() const
    {
        return prefs_.enginePlays;
    }

    // The name announced as MNAME: the reserved bot name while Stockfish
    // plays, otherwise the user's own (never the bot name).
    QString localPlayerName() const
    {
        return botMode() ? QString::fromLatin1(kBotName) : prefs_.playerName;
    }

    // Once per session, at the first game start or restore, the peer is told
    // in its own chat that an engine moves this side. The core refuses CHAT
    // while a move or restore is pending, so this retries for as long as the
    // game runs; the timer dies with the game clock or the announcement.
    void announceEngine()
    {
        if (!botMode() || !gameClockRunning_ || engineAnnounced_) {
            engineAnnounceTimer_.stop();
            return;
        }
        const QString engine = engine_.name().isEmpty() ? QString::fromLatin1(kBotName)
                                                        : engine_.name().toUpper();
        const QString text = QStringLiteral("%1 PLAYS %2 - LEVEL %3")
                                 .arg(engine, pcSideName())
                                 .arg(prefs_.engineLevel)
                                 .left(kChatTextMax);
        if (submitSessionLocalRequest(SESSION_REQUEST_CHAT, 0u, text.toLatin1())) {
            engineAnnounced_ = true;
            engineAnnounceTimer_.stop();
            return;
        }
        ++engineAnnounceAttempts_;
        engineAnnounceTimer_.start();
    }

    void setPeerName(const QByteArray &name)
    {
        /* The reducer already validated the slice; keep the display strict. */
        const QString peer = normalizedPlayerName(QString::fromLatin1(name));

        if (peer.isEmpty() || peer == peerName_) {
            return;
        }
        peerName_ = peer;
        appendLog("PEER NAME: " + peerName_);
        refreshMoveTableHeaders();
    }

    /* Column labels follow the colours: a player's name replaces its
       colour once known, and the colour stays as the fallback. */
    void refreshMoveTableHeaders()
    {
        QString white = QStringLiteral("WHITE");
        QString black = QStringLiteral("BLACK");

        if (directSessionReady_) {
            const QString local = localPlayerName();
            QString &mine = pcPlaysWhite_ ? white : black;
            QString &theirs = pcPlaysWhite_ ? black : white;

            if (!local.isEmpty()) {
                mine = local;
            }
            if (!peerName_.isEmpty()) {
                theirs = peerName_;
            }
        }
        if (moveTable_ == nullptr ||
            (white == moveHeaderWhite_ && black == moveHeaderBlack_)) {
            return;
        }
        moveHeaderWhite_ = white;
        moveHeaderBlack_ = black;
        moveTable_->setHorizontalHeaderLabels({QString(), white, black});
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

        // MNAME goes first: a Spectrum repaints its status bar on MACH, so the
        // name is already known there ("VS MAC: STOCKFISH" from macOS).
        const QString name = localPlayerName();
        if (!name.isEmpty() &&
            !sendAdvisoryPayload(localNamePayload(name), "MNAME")) {
            return;
        }
        const QByteArray payload = localMachPayload();
        if (payload.isEmpty()) {
            appendLog("NOTICE: local machine identity unavailable");
        } else if (!sendAdvisoryPayload(payload, "MACH")) {
            return;
        }
        refreshMoveTableHeaders();
    }

    /* Informational identity lines: no ACK, no retry, no session effect. */
    bool sendAdvisoryPayload(const QByteArray &payload, const char *label)
    {
        const QString what = QString::fromLatin1(label);

        if (payload.isEmpty()) {
            return false;
        }

        if (isMqttMode()) {
            const QByteArray suffix =
                sessionController_.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME);
            if (suffix.isEmpty() ||
                !mqttPublish(QString::fromLatin1(suffix),
                             QString::fromLatin1(payload), false)) {
                appendLog("NOTICE: " + what + " announcement not sent");
                return false;
            }
            return true;
        }

        const QPointer<QTcpSocket> sock =
            directSocketForLink(directPrimaryLinkId_);
        if (sock == nullptr ||
            sock->state() != QAbstractSocket::ConnectedState) {
            appendLog("NOTICE: " + what + " announcement link unavailable");
            return false;
        }

        QByteArray frame = payload;
        frame.append('\n');
        const qint64 written = sock->write(frame);
        if (written == frame.size()) {
            sock->flush();
            appendLog("TX: " + QString::fromLatin1(payload));
            return true;
        }

        appendLog(QString("NOTICE: %1 write %2/%3")
                      .arg(what)
                      .arg(written)
                      .arg(frame.size()));
        if (written > 0) {
            /* A partial TCP frame corrupts later line framing. */
            sock->abort();
        }
        return false;
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
        refreshBoardOrientationFromSide();
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
        refreshBoardOrientationFromSide();
        setConnectedUi(isConnected());
    }

    void handleDirectSessionChanged(uint8_t status, uint8_t endReason)
    {
        if (status == SESSION_CHANGED_READY) {
            cancelDirectConnectRetry();
            directSessionReady_ = true;
            directUiBusy_ = false;
            directEndStatus_.clear();
            setStatusText(QString());
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
                    NETCHESSZX_UI_PC_ERROR_CONNECTION_LOST);
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
            // CANCELLED: this side's request got no answer in time.
            // EXPIRED: the opponent withdrew its own request.
            const QString name = control == SESSION_REQUEST_TAKEBACK
                                     ? QStringLiteral("Takeback request") :
                                 control == SESSION_REQUEST_RESET
                                     ? QStringLiteral("Reset request")
                                     : QStringLiteral("Draw offer");
            const QString message = result == SESSION_CONTROL_CANCELLED
                                        ? name + QStringLiteral(" expired")
                                        : name + QStringLiteral(" withdrawn");

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
                noteEngineMoveRejected();
            }
            break;
        case SESSION_REQUEST_RESET:
            if (rejected) {
                directUiBusy_ = false;
                setStatusText(gameOver_ ? NETCHESSZX_UI_ERROR_RESTART_REJECTED
                                        : NETCHESSZX_UI_ERROR_RESET_REJECTED);
                if (gameOver_) {
                    // The opponent declined a new game: the session ends.
                    appendLog(NETCHESSZX_UI_ERROR_RESTART_REJECTED);
                    leaveAfterDeclinedRestart();
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
            directUiBusy_ = false;
            if (gameOver_) setStatusText(gameResult_);
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
                setStatusText(QStringLiteral("Load rejected by opponent"));
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
        case SESSION_DELIVER_PEER_NAME:
            setPeerName(payload);
            break;
        case SESSION_DELIVER_CONTROL:
            if (value == SESSION_REQUEST_RESET) {
                if (!replayingAnswer_) {
                    appendControlEvent(false, QStringLiteral("RESET"));
                }
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
        boardWidget_->stopAnimations();
        closeDirectDecisionPrompt();
        directUiBusy_ = local ? SESSION_REQUEST_RESIGN : 0u;
        appendControlEvent(local, result);
        endGameOver(result);
        setStatusText(local ? NETCHESSZX_UI_PC_NOTICE_RESIGNING : result);
    }

    void applyDirectStartTransition()
    {
        if (directStartTransitionApplied_ || replayingAnswer_) {
            return;
        }
        directStartTransitionApplied_ = true;
        directUiBusy_ = false;
        startGameFromAck();
        QTimer::singleShot(0, this, [this]() {
            directStartTransitionApplied_ = false;
        });
    }

    static QString declinedRequestText(uint8_t control)
    {
        switch (control) {
        case SESSION_REQUEST_DRAW:
            return QStringLiteral("Draw declined");
        case SESSION_REQUEST_TAKEBACK:
            return QStringLiteral("Takeback declined");
        case SESSION_REQUEST_RESTORE:
            return QStringLiteral("Load declined");
        default:
            return QStringLiteral("Reset declined");
        }
    }

    void showDirectDecision(uint8_t requestId, uint8_t control, uint16_t value)
    {
        // The requester resends every 2.5 s until our reply arrives, so a
        // retry can cross the reply we just gave. Answer it the same way
        // instead of asking again (wire contract: duplicates are idempotent).
        if (lastAnswer_.at.isValid() && lastAnswer_.at.elapsed() < kAnswerReplayMs &&
            lastAnswer_.control == control && lastAnswer_.value == value &&
            lastAnswer_.ply == nextPly_) {
            const uint8_t decision = lastAnswer_.decision;
            lastAnswer_.at.start();
            QTimer::singleShot(0, this, [this, requestId, decision]() {
                // The core delivers synchronously inside submit.
                replayingAnswer_ = true;
                submitSessionUserDecision(requestId, decision);
                replayingAnswer_ = false;
            });
            return;
        }
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
            setStatusText(QStringLiteral("Opponent asks to load a game"));
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
        connect(box, &QDialog::finished, this, [this, requestId, control, value](int result) {
            if (directDecisionRequestId_ != requestId ||
                directDecisionControl_ != control) {
                return;
            }
            directDecisionBox_.clear();
            directDecisionRequestId_ = 0u;
            directDecisionControl_ = 0u;
            const bool accepted = result == QMessageBox::Yes;
            // An accepted RESET restarts at ply 1; other answers keep the ply.
            lastAnswer_.control = control;
            lastAnswer_.value = value;
            lastAnswer_.decision = accepted ? SESSION_DECISION_ACCEPT
                                            : SESSION_DECISION_REJECT;
            lastAnswer_.ply = accepted && control == SESSION_REQUEST_RESET ? 1 : nextPly_;
            lastAnswer_.at.start();
            submitSessionUserDecision(requestId, accepted ? SESSION_DECISION_ACCEPT
                                                          : SESSION_DECISION_REJECT);
            if (!accepted && control == SESSION_REQUEST_RESET && gameOver_) {
                // Declining a new game after OVER ends the session.
                leaveAfterDeclinedRestart();
                return;
            }
            if (!accepted) {
                setStatusText(declinedRequestText(control));
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
        setStatusText(QString());
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
                boardWidget_->clearSelection();
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
        boardWidget_->clearSelection();
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

    // A declined restart after OVER ends the session on both clients. Leave
    // once the session core has handed off the NACK it is sending.
    void leaveAfterDeclinedRestart()
    {
        QTimer::singleShot(0, this, [this]() {
            if (gameOver_ && isConnected()) {
                disconnectToSetup();
            }
        });
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

    // The MQTT detail goes to the log; the user only sees the failure.
    void failMqttConnection(const QString &detail)
    {
        const QString status = QString::fromLatin1(NETCHESSZX_UI_PHASE_CONNECTION_FAILED);
        const bool linked = mqttSessionLinked_;
        clearMqttSubscriptionState();
        mqttSessionLinked_ = false;
        if (linked) {
            directEndStatus_ = status;
            (void)sessionController_.linkDown(kMqttLinkId);
        }
        ignoreNextDisconnect_ = true;
        appendLog("ERROR: " + detail);
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

    void showDestinationFeedback(int row, int col)
    {
        boardWidget_->showDestinationFeedback(row, col);
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

    void clearSelectionVisuals()
    {
        const BoardWidget::SelectionState previous = boardWidget_->selection();
        boardWidget_->clearSelection();
        refreshSelectionFootprint(previous);
    }

    void squareClicked(int row, int col)
    {
        const QString clicked = ChessHelpers::squareName(row, col);
        const BoardWidget::SelectionState previous = boardWidget_->selection();

        if (botMode()) {
            setStatusBarText(QStringLiteral("Stockfish plays this side"));
            return;
        }

        if (!canPcMove()) {
            boardWidget_->clearSelection();
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
            refreshSelectionFootprint(previous);
            return;
        }

        if (previous.selectedRow < 0) {
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

            const QStringList legalTargets = legalTargetsFrom(clicked);
            if (legalTargets.isEmpty()) {
                boardWidget_->clearSelection();
                selectedLabel_->setText(QString("Selected: none (%1 has no legal moves)").arg(clicked));
                setStatusText(QString("%1 has no legal moves").arg(clicked));
                appendLog(QString("CLICK: %1 no legal targets").arg(clicked));
                refreshSelectionFootprint(previous);
                return;
            }

            boardWidget_->setSelection(row, col, legalTargets);
            selectedLabel_->setText(QString("Selected: %1").arg(clicked));
            chatEdit_->clear();
            refreshChatButton();
            setStatusText(QString("Selected %1 (%2 legal)")
                                      .arg(clicked).arg(legalTargets.size()));
            appendLog(QString("CLICK: selected %1 targets %2")
                          .arg(clicked, legalTargets.join(',')));
            refreshSelectionFootprint(previous);
            return;
        }

        if (previous.selectedRow == row && previous.selectedCol == col) {
            boardWidget_->clearSelection();
            selectedLabel_->setText("Selected: none");
            setStatusText(NETCHESSZX_UI_NOTICE_SELECTION_CLEARED);
            appendLog(QString("CLICK: cleared %1").arg(clicked));
            refreshSelectionFootprint(previous);
            return;
        }

        if (isPcPiece(board_[row][col])) {
            const QStringList legalTargets = legalTargetsFrom(clicked);
            if (legalTargets.isEmpty()) {
                boardWidget_->clearSelection();
                selectedLabel_->setText(QString("Selected: none (%1 has no legal moves)").arg(clicked));
                setStatusText(QString("%1 has no legal moves").arg(clicked));
                appendLog(QString("CLICK: %1 no legal targets").arg(clicked));
                refreshSelectionFootprint(previous);
                return;
            }

            boardWidget_->setSelection(row, col, legalTargets);
            selectedLabel_->setText(QString("Selected: %1").arg(clicked));
            chatEdit_->clear();
            refreshChatButton();
            setStatusText(QString("Selected %1 (%2 legal)")
                                      .arg(clicked).arg(legalTargets.size()));
            appendLog(QString("CLICK: selected %1 targets %2")
                          .arg(clicked, legalTargets.join(',')));
            refreshSelectionFootprint(previous);
            return;
        }

        if (!boardWidget_->isLegalTarget(row, col)) {
            setStatusText(QString("Illegal target: %1").arg(clicked));
            appendLog(QString("CLICK: illegal target %1 for %2")
                          .arg(clicked, ChessHelpers::squareName(
                                            previous.selectedRow,
                                            previous.selectedCol)));
            return;
        }

        const char movingPiece = board_[previous.selectedRow][previous.selectedCol];
        QString promotionSuffix;
        if ((movingPiece == 'P' && row == 0) ||
            (movingPiece == 'p' && row == 7)) {
            const int promoGeneration = gameGeneration_;
            const int promoSelectedRow = previous.selectedRow;
            const int promoSelectedCol = previous.selectedCol;
            promotionSuffix = askPromotionPiece();
            if (promotionSuffix.isEmpty()) {
                return;
            }
            // exec() ran a nested event loop: session signals may have reset or
            // restored the game while the dialog was open.
            const BoardWidget::SelectionState current = boardWidget_->selection();
            if (gameGeneration_ != promoGeneration ||
                !canPcMove() ||
                current.selectedRow != promoSelectedRow ||
                current.selectedCol != promoSelectedCol ||
                board_[current.selectedRow][current.selectedCol] != movingPiece ||
                !boardWidget_->isLegalTarget(row, col)) {
                appendLog("CLICK: promotion aborted, game state changed");
                return;
            }
        }

        const BoardWidget::SelectionState current = boardWidget_->selection();
        const QString move = ChessHelpers::squareName(
                                 current.selectedRow, current.selectedCol) +
                             ChessHelpers::squareName(row, col) + promotionSuffix;
        chatEdit_->setText(move);
        boardWidget_->setTarget(row, col);
        selectedLabel_->setText(QString("Selected: %1 -> %2")
                                    .arg(ChessHelpers::squareName(
                                             current.selectedRow,
                                             current.selectedCol),
                                         clicked));
        setStatusBarText(NETCHESSZX_UI_NOTICE_MOVE_READY);
        refreshChatButton();
        appendLog(QString("CLICK: move %1").arg(move));
        refreshSelectionFootprint(previous);
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
        setStatusText(QString());
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
        boardWidget_->hideGameStats();
        game_stats_reset(&gameStats_, 0u, 0u);
        gameResult_.clear();
        boardWidget_->stopAnimations();
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
        boardWidget_->setPiecesVisible(false);
        boardWidget_->clearSelection();
        netchesszx_rules_reset();
        (void)syncBoardFromRules();
        nextPly_ = 1;
        chatEdit_->clear();
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
        const bool viewWhite = forPeer ? !pcPlaysWhite_ : boardWidget_->whiteAtBottom();
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
        return restorePeerReady() && boardWidget_->piecesVisible() && !restoreBusy();
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
        setStatusText(QStringLiteral("Waiting for opponent approval"));
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
                    savesDirectory, saveSlots[row].fileName))) {
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
                        savesDirectory, saveSlots[row].fileName))) {
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
            sendMove();
            return;
        }
        chatEdit_->rememberSubmission(text);
        if (botMode() && isSessionControlCommand(cmd)) {
            chatEdit_->clear();
            setStatusText(QString::fromLatin1(kBotControlsRefused));
            return;
        }
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
            setStatusText(NETCHESSZX_UI_PC_NOTICE_RESIGNING);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            gameOver_) {
            setStatusText(gameResult_);
            return;
        }
        if (isSessionControlCommand(cmd) &&
            directUiBusy_ != 0u &&
            !(cmd == QStringLiteral("/resign") &&
              resignCanPreemptBusy())) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
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
            if (!confirmLocalRequest(NETCHESSZX_UI_CONFIRM_PC_RESIGN_TITLE,
                                     NETCHESSZX_UI_CONFIRM_RESIGN)) {
                return;
            }
            directLocalResignPending_ = true;
            if (submitSessionLocalRequest(SESSION_REQUEST_RESIGN)) {
                setStatusText(NETCHESSZX_UI_PC_NOTICE_RESIGNING);
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

    // Yes/No prompt for a local request: true when confirmed and no session
    // event closed the prompt meanwhile; `declined` reports a plain No. The
    // engine is re-armed on close: an answer that arrived while the prompt
    // was open was dropped, so it must be asked for again.
    bool confirmLocalRequest(const QString &title, const QString &message,
                             bool *declined = nullptr)
    {
        resetPromptOpen_ = true;
        const QMessageBox::StandardButton answer =
            askQuestion(this, title, message, QMessageBox::Yes | QMessageBox::No);
        const bool interrupted = !resetPromptOpen_;
        resetPromptOpen_ = false;
        if (declined != nullptr) {
            *declined = answer != QMessageBox::Yes;
        }
        maybeEngineMove();
        return !interrupted && answer == QMessageBox::Yes;
    }

    bool requestDraw()
    {
        if (directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_PC_NOTICE_RESIGNING);
            return false;
        }
        if (gameOver_) {
            setStatusText(gameResult_);
            return false;
        }
        if (directUiBusy_ != 0u) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
            return false;
        }
        if (!gameClockRunning_ || restoreBusy()) {
            setStatusText(NETCHESSZX_UI_ERROR_CANNOT_OFFER_DRAW);
            return false;
        }
        if (!confirmLocalRequest(NETCHESSZX_UI_CONFIRM_PC_DRAW_TITLE,
                                 NETCHESSZX_UI_CONFIRM_DRAW)) {
            return false;
        }
        if (submitSessionLocalRequest(SESSION_REQUEST_DRAW)) {
            setStatusText(NETCHESSZX_UI_NOTICE_DRAW_PENDING);
            return true;
        }
        return false;
    }

    bool requestTakeback()
    {
        if (directLocalResignPending_) {
            setStatusText(NETCHESSZX_UI_PC_NOTICE_RESIGNING);
            return false;
        }
        if (gameOver_) {
            setStatusText(gameResult_);
            return false;
        }
        if (directUiBusy_ != 0u) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
            return false;
        }
        if (!canRequestTakeback()) {
            appendLog("ERROR: No move to take back");
            setStatusText("No move to take back");
            return false;
        }
        if (!confirmLocalRequest(NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_TITLE,
                                 NETCHESSZX_UI_CONFIRM_PC_TAKEBACK_REQUEST)) {
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
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_HOST_START);
            appendLog("START ignored: only host starts");
            return;
        }
        if (!directSessionReady_) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
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

    void sendMove()
    {
        if (botMode()) {
            setStatusBarText(QStringLiteral("Stockfish plays this side"));
            return;
        }
        if (submitLocalMove(chatEdit_->text().trimmed().toLower())) {
            chatEdit_->clear();
        }
    }

    bool submitLocalMove(const QString &move)
    {
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
            return false;
        }
        if (!ChessHelpers::isMoveSyntaxOk(move)) {
            appendLog("ERROR: move syntax must be e2e4 or e7e8q/r/b/n");
            return false;
        }
        const int fromCol = move[0].unicode() - 'a';
        const int fromRow = '8' - move[1].unicode();
        const int toCol = move[2].unicode() - 'a';
        const int toRow = '8' - move[3].unicode();
        if (!isPcPiece(board_[fromRow][fromCol])) {
            appendLog(QString("ERROR: you can only move %1 pieces: %2")
                          .arg(pcSideName(), move.left(2)));
            setStatusText(QString("You play %1").arg(pcSideName()));
            return false;
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
                return false;
            }
        }

        if (!submitSessionLocalRequest(SESSION_REQUEST_MOVE, 0u,
                                       moveBytes)) {
            return false;
        }
        const qint64 prepMs = sendPrepTimer.elapsed();
        if (prepMs > kMoveSendWarnMs) {
            appendLog(QString("WARN: move send prep took %1 ms").arg(prepMs));
        }

        clearSelectionVisuals();
        showDestinationFeedback(toRow, toCol);
        setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
        setConnectedUi(true);
        return true;
    }

    QString currentFen() const
    {
        CompactRulesState state = {};
        if (netchesszx_rules_save(&state, sizeof(state)) != NETCHESSZX_OK) {
            return QString();
        }
        return ChessHelpers::fenFromCompact(state, (nextPly_ - 1) / 2 + 1);
    }

    bool sessionLocked() const
    {
        return isConnected() || isConnecting() || isDirectListening();
    }

    // Main-window shortcut for Preferences > Engine > "Stockfish plays my side".
    void setEnginePlays(bool on)
    {
        if (sessionLocked()) {
            refreshEngineToggle();
            return;
        }
        if (on && !QFileInfo::exists(prefs_.enginePath)) {
            refreshEngineToggle();
            openPreferences(PreferencesDialog::EngineTab);
            return;
        }
        ClientPreferences next = prefs_;
        next.enginePlays = on;
        QSettings settings;
        next.save(settings);
        applyPreferences(ClientPreferences::load(settings));
    }

    void refreshEngineToggle()
    {
        const QSignalBlocker blocker(engineButton_);
        engineButton_->setChecked(botMode());
        engineButton_->setVisible(prefs_.engineShown);
        engineButton_->setEnabled(!sessionLocked());
        // The tooltip always states who plays, then what a click would do.
        const QString state =
            botMode() ? QStringLiteral("Stockfish plays your side (level %1)")
                            .arg(prefs_.engineLevel)
                      : QStringLiteral("You play your side");
        engineButton_->setToolTip(
            sessionLocked() ? state + QStringLiteral(": locked during a session.")
            : botMode()     ? state + QStringLiteral(" - click to play yourself.")
                            : state + QStringLiteral(" - click to let Stockfish play"
                                                     " (level %1).").arg(prefs_.engineLevel));
        refreshStatusBar();
    }

    void openPreferences(PreferencesDialog::Tab tab = PreferencesDialog::BoardTab)
    {
        const bool locked = sessionLocked();
        // Outside a session the engine may be updated or removed: release it.
        if (!locked) {
            engine_.stop();
        }
        PreferencesDialog dialog(prefs_, locked, this);
        dialog.setWindowModality(Qt::WindowModal);
        dialog.onHelp = [this](ManualSection section) { openHelp(section); };
        dialog.showTab(tab);
        if (dialog.exec() == QDialog::Accepted) {
            ClientPreferences next = dialog.preferences();
            if (locked) {
                // Identity and engine are fixed for the session.
                next.playerName = prefs_.playerName;
                next.enginePlays = prefs_.enginePlays;
                next.engineShown = prefs_.engineShown;
                next.engineLevel = prefs_.engineLevel;
                next.engineDelay = prefs_.engineDelay;
                next.enginePath = prefs_.enginePath;
            }
            QSettings settings;
            next.save(settings);
            applyPreferences(ClientPreferences::load(settings));
        } else if (!locked) {
            applyPreferences(prefs_);
        }
    }

    void openHelp()
    {
        if (helpWindow_ == nullptr) {
            // Top level without a parent, so a modal Preferences never blocks it.
            helpWindow_ = new HelpWindow(nullptr);
            helpWindow_->setStyleSheet(appStyleSheet() + helpWindow_->styleSheet());
        }
        helpWindow_->show();
        helpWindow_->raise();
        helpWindow_->activateWindow();
    }

    void openHelp(ManualSection section)
    {
        openHelp();
        helpWindow_->showSection(section);
    }

    // Applies stored preferences; startEngine false only for tests.
    void applyPreferences(const ClientPreferences &next, bool startEngine = true)
    {
        const bool first = !prefsApplied_;
        const ClientPreferences previous = prefs_;
        prefsApplied_ = true;
        prefs_ = next;

        if (first || next.boardTexture != previous.boardTexture) {
            PieceRenderer::setBoardTexture(next.boardTexture);
            boardWidget_->refreshFrameStyle();
        }
        if (first || next.pieceSet != previous.pieceSet) {
            PieceRenderer::setPieceSet(next.pieceSet);
            if (!first && boardWidget_->piecesVisible()) {
                animateBoardPiecesMorph();
            }
        }
        boardWidget_->setShowHints(next.showHints);

        if (!prefs_.enginePlays) {
            engine_.stop();
        } else if (startEngine && !engine_.running()) {
            engineRestarts_ = 0;
            if (!QFileInfo::exists(prefs_.enginePath) ||
                !engine_.start(prefs_.enginePath, prefs_.engineLevel)) {
                appendLog(QStringLiteral("ERROR: Stockfish not available: %1")
                              .arg(prefs_.enginePath));
                prefs_.enginePlays = false;
                QSettings settings;
                prefs_.save(settings);
                setStatusText(QStringLiteral("Stockfish not found: you play this side"));
            }
        } else {
            engine_.setSkill(prefs_.engineLevel);
        }

        refreshEngineToggle();
        boardWidget_->setBotMode(botMode());
        forgetEngineRequest();
        refreshMoveTableHeaders();
        refreshBoard();
        if (!first) {
            setConnectedUi(isConnected());
        }
    }

    // Called from every UI refresh; asks once per position while it is our turn.
    void maybeEngineMove()
    {
        if (!botMode() || !engine_.running() || gameOver_ || !canPcMove() ||
            engineHalted()) {
            forgetEngineRequest();
            return;
        }
        const QString fen = currentFen();
        if (fen.isEmpty() || fen == engineRequestedFen_) {
            return;
        }
        forgetEngineRequest();
        engineRequestedFen_ = fen;
        engine_.request(fen, kEngineMoveTimeMs);
    }

    // The search is answered, now or after the pause. If the answer cannot be
    // played (a local prompt, a takeback, a restore or a manual move overtook
    // it), the next refresh asks again instead of waiting forever.
    void playEngineAnswer(const QString &fen, const QString &move)
    {
        engineRequestedFen_.clear();
        if (botMode() && canPcMove() && fen == currentFen()) {
            (void)submitLocalMove(move);
        }
    }

    // Drops the search in flight and an answer still waiting out its pause.
    void forgetEngineRequest()
    {
        engineRequestedFen_.clear();
        engineMoveTimer_.stop();
    }

    // One attempt to bring a dead engine back with the same path and level,
    // so the STOCKFISH identity announced to the peer stays true.
    void restartEngine()
    {
        if (!botMode() || engine_.running()) {
            return;
        }
        if (QFileInfo::exists(prefs_.enginePath) &&
            engine_.start(prefs_.enginePath, prefs_.engineLevel)) {
            appendLog(QStringLiteral("Stockfish restarted"));
            setConnectedUi(isConnected());
            return;
        }
        setStatusText(QStringLiteral("Stockfish stopped: resign or disconnect"));
    }

    // The peer refused the engine's move for the current position often
    // enough: the boards disagree, and repeating the move would only loop.
    bool engineHalted() const
    {
        return engineMoveRejections_ >= kEngineRejectLimit &&
               engineRejectedFen_ == currentFen();
    }

    // A fresh position starts a new count, so a takeback, reset or restore
    // lets the engine try again.
    void noteEngineMoveRejected()
    {
        if (!botMode()) {
            return;
        }
        const QString fen = currentFen();
        if (fen != engineRejectedFen_) {
            engineRejectedFen_ = fen;
            engineMoveRejections_ = 0;
        }
        ++engineMoveRejections_;
        if (engineHalted()) {
            appendLog(QStringLiteral("Stockfish stopped: the opponent rejected its move %1 times")
                          .arg(engineMoveRejections_));
            setStatusText(QStringLiteral("Stockfish stopped: opponent rejected its move"));
        }
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

        if (engineButton_) {
            refreshEngineToggle();
        }
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
        setStatusText(NETCHESSZX_UI_NOTICE_MQTT_JOINING);
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
            setStatusText(NETCHESSZX_UI_NOTICE_MQTT_JOINING);
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
                setStatusText(NETCHESSZX_UI_NOTICE_MQTT_JOINING);
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
            refreshBoardOrientationFromSide();
            appendLog("MQTT SIDE READY");
            setConnectedUi(isConnected());
            sessionController_.pump();
            replayMqttBufferedPublishes();
            return;
        }

        mqttSideReady_ = pcIsHost_;
        setConnectedUi(true);
        setStatusText(QString());
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
            setStatusText(NETCHESSZX_UI_NOTICE_MQTT_JOINING);
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
        gameResult_ = message;
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
        boardWidget_->invalidatePieceFlash();
        boardWidget_->invalidateDestinationFeedback();
        hostPlaysWhite_ = (st.host_color == NETCHESSZX_SAVE_HOST_WHITE);
        pcPlaysWhite_ = pcIsHost_ ? hostPlaysWhite_ : !hostPlaysWhite_;
        clearTakebackState();
        closeControlPrompt();
        closeDirectDecisionPrompt();
        directUiBusy_ = false;
        nextPly_ = static_cast<int>(st.ply) + 1;
        boardWidget_->hideGameStats();
        game_stats_reset(&gameStats_, st.ply, 1u);
        const bool restoredActive = (st.flags & NETCHESSZX_SAVE_FLAG_ACTIVE) != 0u;
        gameOver_ = (st.flags & NETCHESSZX_SAVE_FLAG_GAME_OVER) != 0u;
        gameResult_ = gameOver_ ? QStringLiteral("Game over (loaded)") : QString();
        gameCheck_ = (st.flags & NETCHESSZX_SAVE_FLAG_CHECK) != 0u;
        pcTurn_ = restoredActive && !gameOver_ &&
            ((st.side == NETCHESSZX_SAVE_SIDE_WHITE) == pcPlaysWhite_);
        lastMove_.clear();
        clearMoveHistory();
        appendMoveRecord(static_cast<int>(st.ply),
                         QStringLiteral(NETCHESSZX_UI_EVENT_RESTORED),
                         QString());
        boardWidget_->setPiecesVisible(true);
        boardWidget_->clearSelection();
        chatEdit_->clear();
        selectedLabel_->setText("Selected: none");
        syncBoardOrientationWithPcSide();
        refreshBoard();
        if (restoredActive && !gameOver_) {
            startGameClock(elapsedFromSave(st.game_hour, st.game_minute, st.game_second),
                           elapsedFromSave(st.move_hour, st.move_minute, st.move_second));
        } else if (!restoredActive || gameOver_) {
            stopGameClock();
        }
        refreshTurnLabel();
        setConnectedUi(true);
        QTimer::singleShot(0, this, [this]() { announceEngine(); });
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
        game_stats_takeback(&gameStats_, static_cast<uint16_t>(nextPly_ - 1),
                            takebackSnapshot_.capturedType);
        pcTurn_ = takebackSnapshot_.pcTurn;
        gameCheck_ = takebackSnapshot_.gameCheck;
        gameOver_ = false;
        gameResult_.clear();
        boardWidget_->hideGameStats();
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
        if (!ChessHelpers::isMoveSyntaxOk(move)) return false;
        const int fromRow = '8' - move[1].toLatin1();
        const int fromCol = move[0].toLatin1() - 'a';
        const int toRow = '8' - move[3].toLatin1();
        const int toCol = move[2].toLatin1() - 'a';
        char captured = board_[toRow][toCol];
        if (captured == '.' && fromCol != toCol &&
            (board_[fromRow][fromCol] == 'P' || board_[fromRow][fromCol] == 'p')) {
            captured = board_[fromRow][toCol];
        }
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
        takebackSnapshot_.capturedType = game_stats_capture_type(captured);
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
                           bool nextPcTurn, bool stalemate = false)
    {
        lastMove_ = move;
        appendMoveRecord(ply, move, notation);
        nextPly_ = ply + 1;
        game_stats_record(&gameStats_, static_cast<uint16_t>(ply),
                          gameClockRunning_ ? static_cast<uint32_t>(
                              (moveTimerOffsetMs_ + moveTimer_.elapsed()) / 1000) : 0u,
                          takebackSnapshot_.capturedType);
        if (boardWidget_->gameStatsVisible()) {
            showGameStatsPanel_();
        }
        if (notation.contains('#')) {
            const QString message = nextPcTurn ? QString::fromLatin1(NETCHESSZX_UI_EVENT_CHECKMATE_LOST)
                                               : QString::fromLatin1(NETCHESSZX_UI_EVENT_CHECKMATE_WON);
            gameOver_ = true;
            gameResult_ = message;
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
            gameResult_ = QString::fromLatin1(NETCHESSZX_UI_EVENT_STALEMATE);
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
        // Turn label, move table and last-move highlight report the move.
        setStatusText(QString());
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
        finishAppliedMove(ply, move, notation, false, stalemate);
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
        if (!gameClockRunning_ || !boardWidget_->piecesVisible()) {
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
            finishAppliedMove(ply, move, notation, true, stalemate);
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
        boardWidget_->hideGameStats();
        game_stats_reset(&gameStats_, 0u, 0u);
        gameResult_.clear();
        const bool keepSetupPreview = boardWidget_->piecesVisible() && !gameOver_ &&
            !gameClockRunning_ && isStartingBoard();

        boardWidget_->invalidatePieceFlash();
        boardWidget_->invalidateDestinationFeedback();
        directLocalResignPending_ = false;
        netchesszx_rules_reset();
        (void)syncBoardFromRules();
        clearTakebackState();
        nextPly_ = 1;
        gameOver_ = false;
        gameCheck_ = false;
        pcTurn_ = pcPlaysWhite_;
        lastMove_.clear();
        clearMoveHistory();
        boardWidget_->clearSelection();
        chatEdit_->clear();
        selectedLabel_->setText("Selected: none");
        const bool orientationChanged = syncBoardOrientationWithPcSide();
        if (!keepSetupPreview || orientationChanged) {
            boardWidget_->setPiecesVisible(false);
            refreshBoard();
            animateBoardPiecesIn();
        }
        startGameClock();
        setStatusText(NETCHESSZX_UI_EVENT_GAME_STARTED);
        setConnectedUi(true);
        QTimer::singleShot(0, this, [this]() { announceEngine(); });
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
            text = isDirectListening() ? "LISTENING"
                                       : (isConnecting() ? "CONNECTING" : "OFFLINE");
            style = isConnecting() ? turnLabelStyle(SHZ_QUIET, SHZ_ACCENT)
                                   : turnLabelStyle(SHZ_DISABLED, SHZ_TEXT_DIM);
        } else if (directDecisionControl_ != 0u) {
            text = "YOUR ANSWER";
            style = turnLabelStyle(SHZ_WAIT, SHZ_WAIT_TEXT);
        } else if (gameOver_) {
            // The result stays put; later notices go to the status bar.
            text = (gameResult_.isEmpty() ? statusMessage_ : gameResult_).toUpper();
            style = turnLabelStyle(SHZ_ALERT, SHZ_ALERT_TEXT);
        } else if (directUiBusy_) {
            text = "WAITING FOR OPPONENT";
            style = turnLabelStyle(SHZ_WAIT, SHZ_WAIT_TEXT);
        } else if (!gameClockRunning_) {
            if (pcIsHost_ && !directSessionReady_) {
                text = "WAITING FOR OPPONENT";
            } else if (!pcIsHost_ && !directSessionReady_) {
                text = "WAITING FOR HOST";
            } else {
                text = pcIsHost_ ? "PRESS START GAME" : "WAITING FOR HOST TO START";
            }
            style = turnLabelStyle(SHZ_QUIET, SHZ_ACCENT);
        } else if (botMode() && pcTurn_ && (!engine_.running() || engineHalted())) {
            // The engine is gone or its move keeps being refused: the
            // operator resigns or disconnects.
            text = QString("STOCKFISH STOPPED - %1").arg(pcSideName());
            style = turnLabelStyle(SHZ_ALERT, SHZ_ALERT_TEXT);
        } else if (botMode() && pcTurn_) {
            text = gameCheck_ ? QStringLiteral("STOCKFISH IN CHECK")
                              : QString("STOCKFISH THINKING - %1").arg(pcSideName());
            style = turnLabelStyle(SHZ_BOT_FILL, "#ffffff");
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
        startGameButton_->setText(gameOver_ ? "Restart Game" : "Start Game");
        const bool startBaseReady = connected && !gameClockRunning_ &&
                                     !restoreBusy() && directSessionReady_;
        // Only the host starts a game, also a new one after OVER; the guest
        // waits for that request and answers it.
        const bool startReady = startBaseReady && pcIsHost_;
        startGameButton_->setEnabled(startReady);
        setWidgetStyle(startGameButton_, startButtonStyle(startReady));
        // Statistics stay available after OVER, except while this side's
        // restart request awaits its answer.
        const bool statsReady = gameOver_ ? directUiBusy_ != SESSION_REQUEST_RESET
                                          : gameClockRunning_;
        gameStatsButton_->setEnabled(statsReady);
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
        if (isDirectListening() ||
            (connected && statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED)) {
            setStatusText(QString());
        } else if (!connected && !connecting && statusMessage_.isEmpty()) {
            setStatusText(NETCHESSZX_UI_PHASE_DISCONNECTED);
        }
        refreshStatusBar();
        refreshTurnLabel();
        maybeEngineMove();
    }

    bool canStartConnection() const
    {
        const QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();

        // Never announce STOCKFISH before the engine can actually move.
        if (botMode() && !engine_.running()) {
            return false;
        }
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
        const BoardWidget::SelectionState selection = boardWidget_->selection();
        return selection.selectedRow >= 0 &&
               selection.selectedCol >= 0 &&
               selection.targetRow >= 0 &&
               selection.targetCol >= 0 &&
               boardWidget_->isLegalTarget(selection.targetRow, selection.targetCol);
    }

    bool pendingMoveCameFromSelection(const QString &move) const
    {
        if (!hasSelectedMoveTarget()) {
            return false;
        }

        const BoardWidget::SelectionState selection = boardWidget_->selection();
        const QString selectedMove =
            ChessHelpers::squareName(selection.selectedRow, selection.selectedCol) +
            ChessHelpers::squareName(selection.targetRow, selection.targetCol);
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
               (!isMqttMode() || (mqttSubscribed_ && mqttSideReady_));
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
        chatEdit_->setSubmissionEnabled(ready);
        setWidgetStyle(chatButton_, moveText
                                    ? moveButtonStyle(ready, ready && hasSelectedMoveTarget())
                                    : chatButtonStyle(ready));
    }

    void refreshSessionCommandButtons()
    {
        const bool inGame = isConnected() && gameClockRunning_ && !gameOver_;
        const bool canControl = inGame && !restoreBusy() &&
                                (directUiBusy_ == 0u || resignCanPreemptBusy());
        // While the engine plays, takeback and draw offers are not the
        // operator's to make: the buttons leave the window.
        if (drawButton_ != nullptr) {
            drawButton_->setVisible(!botMode());
            drawButton_->setEnabled(canControl);
        }
        if (takebackButton_ != nullptr) {
            takebackButton_->setVisible(!botMode());
            takebackButton_->setEnabled(canControl && canRequestTakeback());
        }
        if (resetIconButton_ != nullptr) {
            resetIconButton_->setEnabled(inGame && !restoreBusy());
        }
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
            boardWidget_ == nullptr || logToggleButton_ == nullptr) {
            return;
        }

        const int leftMargin = statusBarContents_->mapFromGlobal(
            boardWidget_->flipButtonGlobalPosition()).x();
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
            text == NETCHESSZX_UI_PC_EVENT_OPPONENT_LEFT ||
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
        if (directDecisionControl_ != 0u) {
            // The opponent's request awaits this side's answer.
            return statusMessage_;
        }
        if (gameOver_) {
            return gameResult_.isEmpty() ? statusMessage_ : gameResult_;
        }
        if (!gameClockRunning_) {
            if (!directSessionReady_) {
                return pcIsHost_ ? NETCHESSZX_UI_PHASE_WAITING_OPPONENT
                                 : NETCHESSZX_UI_PC_PHASE_WAITING_HOST;
            }
            return NETCHESSZX_UI_PHASE_OPPONENT_READY;
        }
        if (directUiBusy_) {
            return NETCHESSZX_UI_PHASE_WAITING_OPPONENT;
        }
        return QStringLiteral("LIVE");
    }

    // The pending notice, unless the state label already says it.
    QString statusNoticeText() const
    {
        const QString state = statusStateText();
        if (statusMessage_.isEmpty() ||
            statusMessage_.compare(state, Qt::CaseInsensitive) == 0 ||
            statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED ||
            statusMessage_ == NETCHESSZX_UI_PHASE_WAITING_OPPONENT) {
            return QString();
        }
        const QString prefix = state + QStringLiteral(" - ");
        return statusMessage_.startsWith(prefix) ? statusMessage_.mid(prefix.size())
                                                 : statusMessage_;
    }

    // Only what the state label, the turn label and the top controls do not
    // already show: the pending notice, the game-over action and the peer.
    QString statusContextText() const
    {
        QStringList parts;
        // Permanent while the engine plays this side.
        if (botMode()) {
            parts << QStringLiteral("STOCKFISH L%1").arg(prefs_.engineLevel);
        }
        const QString notice = statusNoticeText();
        if (!notice.isEmpty()) {
            parts << notice;
        }
        if (gameOver_ && isConnected() && directUiBusy_ == 0u &&
            directDecisionControl_ == 0u) {
            parts << QString(pcIsHost_ ? NETCHESSZX_UI_CONTEXT_PRESS_RESTART
                                       : NETCHESSZX_UI_PC_PHASE_WAITING_HOST);
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
        refreshMoveTableHeaders();
        const QString stateText = statusStateText();

        // A request awaiting this side's answer uses the turn label's wait colour.
        setWidgetStyle(statusStateLabel_,
                       statusBarLabelStyle(directDecisionControl_ != 0u
                                               ? StatusSeverity::Waiting
                                               : statusBarTextSeverity(stateText)));
        setLabelText(statusStateLabel_, stateText.toUpper());
        if (statusContextLabel_ != nullptr) {
            // Colour by the notice alone: the action hint and peer are neutral.
            const QString contextText = statusContextText();
            setWidgetStyle(statusContextLabel_,
                           statusBarLabelStyle(statusBarTextSeverity(statusNoticeText())));
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
    QPushButton *drawButton_ = nullptr;
    QPushButton *takebackButton_ = nullptr;
    QPushButton *resetIconButton_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QAction *directIpHistoryAction_ = nullptr;
    QMenu *directIpHistoryMenu_ = nullptr;
    QStringList directIpHistory_;
    QLineEdit *roomEdit_ = nullptr;
    ClientPreferences prefs_;
    bool prefsApplied_ = false;
    QToolButton *engineButton_ = nullptr;
    HelpWindow *helpWindow_ = nullptr;
    EnginePlayer engine_;
    QString engineRequestedFen_;
    // The answer waiting out Preferences > Engine > Pause before each move.
    QTimer engineMoveTimer_;
    QString engineAnswerFen_;
    QString engineAnswerMove_;
    int engineRestarts_ = 0;
    // Position the peer keeps rejecting and how many times it did.
    QString engineRejectedFen_;
    int engineMoveRejections_ = 0;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *startGameButton_ = nullptr;
    QPushButton *gameStatsButton_ = nullptr;
    std::function<void()> showGameStatsPanel_;
    QPushButton *resetButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    CommandInputWidget *chatEdit_ = nullptr;
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
    char board_[8][8] = {};
    QString mqttRoom_;
    QString statusMessage_;
    QString gameResult_;
    game_stats_t gameStats_ = {};
    QString lastSocketError_;
    QString lastMove_;
    QStringList logLines_;
    QVector<MoveRecord> moveHistoryRecords_;
    TakebackSnapshot takebackSnapshot_;
    QElapsedTimer gameTimer_;
    QElapsedTimer moveTimer_;
    QElapsedTimer linkWatch_;
    QElapsedTimer uiTickTimer_;
    qint64 gameTimerOffsetMs_ = 0;
    qint64 moveTimerOffsetMs_ = 0;
    int nextPly_ = 1;
    int moveTableFirstMoveNumber_ = 0;
    int gameGeneration_ = 0;
    int directConnectRetryCount_ = 0;
    uint8_t directNextLinkId_ = 0u;
    uint8_t directPrimaryLinkId_ = SESSION_LINK_NONE;
    uint8_t directDecisionRequestId_ = 0u;
    uint8_t directDecisionControl_ = 0u;
    uint8_t peerPlatform_ = NETCHESS_PLAT_UNKNOWN;
    QString peerName_;
    QString moveHeaderWhite_ = QStringLiteral("WHITE");
    QString moveHeaderBlack_ = QStringLiteral("BLACK");
    quint16 mqttSessionId_ = 0;
    quint64 mqttClientNonce_ = QRandomGenerator::global()->generate64();
    bool mqttSubscribed_ = false;
    bool mqttSideReady_ = false;
    bool mqttSessionLinked_ = false;
    bool directSessionReady_ = false;
    bool localMachSent_ = false;
    bool engineAnnounced_ = false;
    QTimer engineAnnounceTimer_;
    int engineAnnounceAttempts_ = 0;
    uint8_t directUiBusy_ = 0u;
    bool directStartTransitionApplied_ = false;
    bool directLocalResignPending_ = false;
    bool resetPromptOpen_ = false;
    // Last peer request answered here, replayed to its crossing retries for
    // longer than one 2.5 s retry period while the position is unchanged.
    struct AnsweredRequest {
        uint8_t control = 0u;
        uint16_t value = 0u;
        uint8_t decision = 0u;
        int ply = 0;
        QElapsedTimer at;
    };
    static constexpr qint64 kAnswerReplayMs = 3000;
    AnsweredRequest lastAnswer_;
    // Set while a replayed answer is submitted: a replayed accepted RESET
    // re-ACKs a retry of the RESET that already started this game (still at
    // ply 1), so the game is not restarted again.
    bool replayingAnswer_ = false;
    bool pcTurn_ = false;
    bool pcIsHost_ = false;
    bool ignoreNextDisconnect_ = false;
    bool localDisconnectPending_ = false;
    bool directConnectRetryActive_ = false;
    bool hostPlaysWhite_ = true;
    bool pcPlaysWhite_ = false;
    BoardWidget *boardWidget_ = nullptr;
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
QString MainWindow::testChatLogText() const { return impl_->testChatLogText(); }
QString MainWindow::testChatDraft() const { return impl_->testChatDraft(); }
void MainWindow::testSetChatDraft(const QString &text) { impl_->testSetChatDraft(text); }
QString MainWindow::testStatusContextText() const
{
    return impl_->testStatusContextText();
}
bool MainWindow::testStatusBarProjection() { return impl_->testStatusBarProjection(); }
bool MainWindow::testBotIdentity() { return impl_->testBotIdentity(); }
void MainWindow::testSetPlayerName(const QString &name)
{
    impl_->testSetPlayerName(name);
}
QStringList MainWindow::testMoveHeaderLabels() const
{
    return impl_->testMoveHeaderLabels();
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

bool MainWindow::testGameStatsProjection()
{
    return impl_->testGameStatsProjection();
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
bool MainWindow::testClickDrawButton()
{
    return impl_->testClickDrawButton();
}
QString MainWindow::testTurnLabelText() const
{
    return impl_->testTurnLabelText();
}
bool MainWindow::testEngineAnnounceKeepsRetrying()
{
    return impl_->testEngineAnnounceKeepsRetrying();
}
#endif
