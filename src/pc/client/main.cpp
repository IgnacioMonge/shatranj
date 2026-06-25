#include <QAbstractSocket>
#include <QApplication>
#include <QByteArray>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkInterface>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSettings>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <cstring>
#include <functional>

extern "C" {
#include "common/mqtt/mqtt.h"
#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/protocol/direct_session_protocol.h"
}
#include "common/chess/legal.h"
#include "common/ui_messages.h"

namespace {

constexpr int kBoardSquareSize = 52;
constexpr int kBoardCoordSize = 24;
constexpr int kPieceIconSize = 44;
constexpr int kActionButtonWidth = 100;
constexpr int kSidePanelWidth = 340;
constexpr int kChatTextMax = 42;
constexpr int kDirectPayloadTextMax = 47;
constexpr int kDirectChatTextMax = kChatTextMax;
constexpr int kSpectrumFrameMs = 20;
constexpr int kPieceRevealStepMs = 5 * kSpectrumFrameMs;
constexpr int kPieceRevealMiddlePauseMs = 3 * kSpectrumFrameMs;
#ifndef NETCHESSZX_APP_VERSION
#error "NETCHESSZX_APP_VERSION must be provided by the build system"
#endif
#define NETCHESSZX_STRINGIFY2(x) #x
#define NETCHESSZX_STRINGIFY(x) NETCHESSZX_STRINGIFY2(x)
constexpr const char *kAppVersion = NETCHESSZX_STRINGIFY(NETCHESSZX_APP_VERSION);
constexpr qint64 kUiStallWarnMs = 2500;
constexpr qint64 kMoveSendWarnMs = 250;

QIcon makeShatranjIcon()
{
    QIcon icon;
    for (int size : {16, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QFont font("Segoe UI Symbol");
        font.setPixelSize(static_cast<int>(size * 0.90));
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRectF(0, -size * 0.04, size, size * 1.08),
                         Qt::AlignCenter,
                         QString(QChar(0x265E)));
        icon.addPixmap(pixmap);
    }
    return icon;
}

class AppBanner final : public QWidget {
public:
    explicit AppBanner(QWidget *parent = nullptr)
        : QWidget(parent)
        , mosaicImage_(620, 70, QImage::Format_ARGB32_Premultiplied)
    {
        setFixedHeight(70);
        setCursor(Qt::PointingHandCursor);

        renderBridgeMosaic();
    }

    std::function<void()> clicked;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        pressed_ = event->button() == Qt::LeftButton;
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const bool activate = pressed_ &&
                              event->button() == Qt::LeftButton &&
                              rect().contains(event->pos());
        pressed_ = false;
        if (activate && clicked) {
            clicked();
        }
    }

private:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        painter.drawImage(rect(), mosaicImage_);

        painter.setPen(QColor(60, 60, 75));
        painter.drawLine(0, height() - 1, width(), height() - 1);

        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 26, QFont::Bold));
        painter.drawText(QRectF(18, 2, width() - 36, 44),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         "Shatranj");

        painter.setPen(QColor(0, 180, 220));
        painter.setFont(QFont("Segoe UI", 9));
        painter.drawText(QRectF(22, 43, width() - 44, 20),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         "Network Chess for ZX Spectrum");
    }

    struct MosaicPixel {
        int x;
        int y;
        int size;
        int r;
        int g;
        int b;
        int a;
    };

    void renderBridgeMosaic()
    {
        static const MosaicPixel bridgeMosaic[] = {
            {547, 36, 9, 255, 216, 0, 225},
            {481, 19, 6, 216, 0, 0, 139},
            {574, 41, 1, 0, 200, 0, 250},
            {574, 38, 8, 0, 180, 220, 250},
            {537, 36, 1, 0, 200, 0, 212},
            {576, -8, 18, 255, 216, 0, 247},
            {564, 45, 7, 255, 216, 0, 247},
            {456, 52, 4, 255, 216, 0, 107},
            {410, -3, 2, 216, 0, 0, 47},
            {569, 29, 4, 0, 180, 220, 254},
            {526, -6, 16, 255, 216, 0, 198},
            {514, -5, 2, 0, 180, 220, 182},
            {602, 42, 3, 0, 180, 220, 213},
            {562, 4, 6, 0, 200, 0, 245},
            {626, 57, 9, 0, 200, 0, 182},
            {401, 71, 6, 0, 200, 0, 45},
            {517, 8, 1, 216, 0, 0, 186},
            {610, 76, 3, 216, 0, 0, 203},
            {543, 2, 7, 255, 216, 0, 220},
            {492, 73, 1, 216, 0, 0, 154},
            {367, 70, 15, 255, 216, 0, 45},
            {492, 44, 18, 255, 216, 0, 154},
            {431, 37, 7, 0, 200, 0, 74},
            {580, 47, 7, 0, 180, 220, 242},
            {563, 73, 7, 0, 180, 220, 246},
            {520, 35, 3, 255, 216, 0, 190},
            {488, 59, 3, 216, 0, 0, 148},
            {594, 32, 7, 0, 200, 0, 224},
            {629, 22, 4, 0, 180, 220, 178},
            {507, 44, 14, 255, 216, 0, 173},
            {594, 18, 19, 216, 0, 0, 224},
            {597, 27, 13, 255, 216, 0, 220},
            {535, 36, 4, 0, 200, 0, 210},
            {523, 44, 8, 216, 0, 0, 194},
            {582, 17, 4, 0, 200, 0, 239},
            {523, 1, 2, 216, 0, 0, 194},
            {522, 48, 6, 255, 216, 0, 193},
            {458, 8, 11, 0, 200, 0, 109},
            {453, 48, 9, 255, 216, 0, 103},
            {492, 61, 6, 0, 180, 220, 154},
            {465, 77, 14, 255, 216, 0, 118},
            {456, 6, 4, 0, 180, 220, 107},
            {468, 18, 10, 216, 0, 0, 122},
            {602, 72, 7, 0, 180, 220, 213},
            {520, 47, 9, 216, 0, 0, 190},
            {605, 40, 4, 255, 216, 0, 210},
            {578, 63, 8, 0, 180, 220, 245},
            {489, 59, 13, 0, 200, 0, 150},
            {596, 22, 18, 0, 180, 220, 221},
            {612, 61, 3, 0, 180, 220, 200},
            {584, 8, 18, 0, 180, 220, 237},
            {502, 70, 3, 255, 216, 0, 167},
            {585, 25, 14, 0, 180, 220, 236},
            {496, 48, 1, 0, 200, 0, 159},
            {444, 40, 9, 0, 180, 220, 91},
            {474, 45, 1, 0, 180, 220, 130},
            {608, 67, 4, 255, 216, 0, 206},
            {579, 67, 16, 0, 180, 220, 243},
            {565, 68, 14, 0, 200, 0, 248},
            {514, 31, 6, 255, 216, 0, 182},
            {435, 32, 2, 0, 180, 220, 80},
            {435, 62, 5, 255, 216, 0, 80},
            {553, 43, 16, 0, 180, 220, 233},
            {492, 33, 2, 255, 216, 0, 154},
            {528, 15, 4, 216, 0, 0, 200},
            {548, 23, 2, 216, 0, 0, 226},
            {588, -5, 12, 0, 180, 220, 232},
            {588, -5, 19, 216, 0, 0, 232},
            {524, 45, 2, 0, 200, 0, 195},
            {595, 40, 6, 216, 0, 0, 222},
            {614, 17, 3, 0, 180, 220, 198},
            {573, 68, 15, 255, 216, 0, 251},
            {547, 34, 3, 255, 216, 0, 225},
            {542, -7, 6, 255, 216, 0, 219},
            {476, 75, 6, 0, 200, 0, 133},
            {582, -3, 2, 255, 216, 0, 239},
            {537, 19, 10, 216, 0, 0, 212},
            {574, -8, 9, 0, 200, 0, 250},
            {549, 42, 13, 216, 0, 0, 228},
            {483, 73, 9, 216, 0, 0, 142},
            {515, 38, 17, 255, 216, 0, 184},
            {517, 6, 16, 216, 0, 0, 186},
            {507, 73, 6, 216, 0, 0, 173},
            {601, 60, 19, 0, 200, 0, 215},
            {574, 12, 4, 0, 180, 220, 250},
            {556, 70, 18, 216, 0, 0, 237},
            {576, 2, 10, 216, 0, 0, 247},
            {459, 1, 4, 255, 216, 0, 111},
            {546, 77, 12, 0, 180, 220, 224},
            {340, 44, 5, 255, 216, 0, 45},
            {591, 37, 17, 216, 0, 0, 228},
            {543, 56, 15, 0, 180, 220, 220},
            {533, 10, 8, 0, 200, 0, 207},
            {529, 45, 3, 255, 216, 0, 202},
            {457, 65, 12, 216, 0, 0, 108},
            {576, 3, 11, 216, 0, 0, 247},
            {578, 5, 17, 255, 216, 0, 245},
            {531, 30, 4, 255, 216, 0, 204},
            {511, 16, 5, 216, 0, 0, 178},
            {585, 35, 7, 255, 216, 0, 236},
            {435, -1, 3, 0, 180, 220, 80},
            {436, 4, 1, 255, 216, 0, 81},
            {578, 29, 11, 0, 180, 220, 245},
            {570, 48, 2, 255, 216, 0, 255},
            {560, 71, 10, 255, 216, 0, 242},
            {455, 65, 3, 216, 0, 0, 106},
            {567, 34, 8, 0, 180, 220, 251},
            {600, 12, 7, 0, 200, 0, 216},
            {568, 4, 6, 216, 0, 0, 252},
            {451, -2, 17, 216, 0, 0, 100},
            {492, 40, 2, 0, 200, 0, 154},
            {560, 13, 4, 0, 200, 0, 242},
            {600, 12, 9, 216, 0, 0, 216},
            {523, 48, 7, 0, 180, 220, 194},
            {371, 68, 2, 255, 216, 0, 45},
            {595, -6, 4, 0, 200, 0, 222},
            {487, 35, 4, 255, 216, 0, 147},
            {469, 29, 16, 255, 216, 0, 124},
            {500, 5, 18, 255, 216, 0, 164},
            {488, 58, 12, 0, 180, 220, 148},
            {616, 61, 8, 216, 0, 0, 195},
            {582, 57, 6, 0, 180, 220, 239},
            {496, -8, 12, 0, 180, 220, 159},
            {564, 43, 7, 255, 216, 0, 247},
            {620, 77, 4, 255, 216, 0, 190},
            {525, 71, 7, 216, 0, 0, 196},
            {592, -6, 15, 0, 200, 0, 226},
            {455, 31, 9, 0, 200, 0, 106},
            {595, 67, 4, 0, 200, 0, 222},
            {602, 5, 8, 0, 200, 0, 213},
            {551, 45, 2, 216, 0, 0, 230},
            {523, 56, 15, 255, 216, 0, 194},
            {568, 12, 13, 0, 200, 0, 252},
            {397, 45, 9, 255, 216, 0, 45},
            {508, 18, 10, 0, 200, 0, 174},
            {627, 50, 17, 0, 180, 220, 181},
            {505, 27, 7, 0, 200, 0, 170},
            {511, 1, 1, 255, 216, 0, 178},
            {426, 36, 2, 216, 0, 0, 68},
            {608, -4, 9, 0, 180, 220, 206},
            {558, 61, 13, 255, 216, 0, 239},
            {617, 0, 2, 216, 0, 0, 194},
            {594, 30, 13, 216, 0, 0, 224},
            {584, 66, 10, 216, 0, 0, 237},
            {555, 13, 9, 0, 180, 220, 236},
            {561, 19, 4, 0, 180, 220, 243},
            {542, 34, 7, 216, 0, 0, 219},
            {438, 24, 3, 216, 0, 0, 83},
            {626, 36, 4, 0, 200, 0, 182},
            {369, 32, 2, 0, 180, 220, 45},
            {607, 54, 6, 0, 180, 220, 207},
            {489, 25, 6, 255, 216, 0, 150},
            {621, 27, 1, 0, 180, 220, 189},
            {560, 53, 8, 0, 200, 0, 242},
            {428, 3, 4, 0, 180, 220, 70},
            {557, 74, 2, 0, 180, 220, 238},
            {570, 55, 5, 216, 0, 0, 255},
            {498, -8, 4, 255, 216, 0, 161},
            {629, 25, 15, 0, 200, 0, 178},
            {477, 69, 4, 216, 0, 0, 134},
            {617, 36, 3, 0, 180, 220, 194},
            {582, 4, 4, 0, 180, 220, 239},
            {538, 57, 8, 216, 0, 0, 213},
            {367, 18, 1, 255, 216, 0, 45},
            {621, 43, 8, 216, 0, 0, 189},
            {448, 16, 12, 0, 180, 220, 96},
            {495, 5, 8, 216, 0, 0, 158},
            {377, 40, 13, 0, 180, 220, 45},
            {533, 2, 19, 0, 200, 0, 207},
            {563, 60, 7, 0, 180, 220, 246},
            {504, 2, 2, 216, 0, 0, 169},
            {546, 34, 6, 255, 216, 0, 224},
            {595, -7, 11, 255, 216, 0, 222},
            {488, 74, 12, 216, 0, 0, 148},
            {575, 41, 17, 255, 216, 0, 248},
            {565, 47, 2, 0, 180, 220, 248},
            {470, 29, 7, 0, 200, 0, 125},
            {518, 44, 9, 0, 200, 0, 187},
            {586, 4, 3, 0, 180, 220, 234},
            {358, 23, 14, 255, 216, 0, 45},
            {414, 40, 16, 216, 0, 0, 52},
            {554, 0, 15, 255, 216, 0, 234},
            {516, 3, 5, 216, 0, 0, 185},
            {512, 10, 9, 255, 216, 0, 180},
            {490, 27, 7, 255, 216, 0, 151},
            {621, -2, 12, 216, 0, 0, 189},
            {363, -8, 7, 216, 0, 0, 45},
            {599, 49, 5, 216, 0, 0, 217},
            {557, 10, 6, 0, 200, 0, 238},
            {624, 41, 8, 216, 0, 0, 185},
            {626, -6, 1, 216, 0, 0, 182},
            {555, 46, 16, 0, 180, 220, 236},
            {563, 10, 17, 216, 0, 0, 246},
            {503, 49, 5, 0, 200, 0, 168},
            {505, 60, 6, 255, 216, 0, 170},
            {546, 14, 4, 0, 180, 220, 224},
            {476, 10, 4, 0, 200, 0, 133},
            {500, 22, 4, 216, 0, 0, 164},
            {564, 6, 11, 0, 180, 220, 247},
            {466, 52, 16, 216, 0, 0, 120},
            {587, 60, 15, 0, 200, 0, 233},
            {584, 4, 17, 0, 180, 220, 237},
            {515, -5, 3, 216, 0, 0, 184},
            {476, 43, 3, 0, 200, 0, 133},
            {546, 69, 1, 255, 216, 0, 224},
            {549, 74, 7, 255, 216, 0, 228},
            {420, 63, 12, 0, 200, 0, 60},
            {576, 60, 8, 255, 216, 0, 247},
            {354, 60, 13, 216, 0, 0, 45},
            {533, 39, 10, 0, 180, 220, 207},
        };

        QPainter painter(&mosaicImage_);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient bg(QPointF(0, 0), QPointF(620, 0));
        bg.setColorAt(0.0, QColor(22, 22, 30));
        bg.setColorAt(1.0, QColor(32, 32, 42));
        painter.fillRect(0, 0, 620, 70, bg);

        for (const MosaicPixel &pixel : bridgeMosaic) {
            painter.fillRect(pixel.x, pixel.y, pixel.size, pixel.size,
                             QColor(pixel.r, pixel.g, pixel.b, pixel.a));
        }
    }

    QImage mosaicImage_;
    bool pressed_ = false;
};

static QString appStyleSheet()
{
    return QStringLiteral(
        "QMainWindow, QWidget { background:#1b1b25; color:#f1f3f6;"
        " font:10pt \"Segoe UI\"; }"
        "QLabel { color:#f1f3f6; background:transparent; }"
        "QLineEdit, QSpinBox, QPlainTextEdit, QTextEdit { background:#2b2b39; color:#f1f3f6;"
        " border:1px solid #555568; padding:3px 6px; selection-background-color:#00b7d8;"
        " selection-color:#101018; }"
        "QLineEdit:disabled, QSpinBox:disabled, QPlainTextEdit:disabled, QTextEdit:disabled {"
        " background:#252532; color:#88889a; border-color:#3a3a4a; }"
        "QPlainTextEdit, QTextEdit { padding:5px; }"
        "QPushButton { background:#3b465b; color:#f1f3f6; border:0;"
        " font:700 9pt \"Segoe UI\"; padding:3px 10px; min-height:18px; }"
        "QPushButton:hover { background:#46546d; }"
        "QPushButton:pressed { background:#31394b; }"
        "QPushButton:disabled { background:#303040; color:#8a8aa0; }"
        "QRadioButton { color:#f1f3f6; spacing:5px; }"
        "QRadioButton::indicator { width:10px; height:10px; border-radius:5px;"
        " border:1px solid #6a6a7e; background:#242432; }"
        "QRadioButton::indicator:checked { background:#00d7ff; border:1px solid #00d7ff; }"
        "QRadioButton::indicator:disabled { background:#303040; border-color:#454557; }"
        "QRadioButton::indicator:checked:disabled { background:#8a8aa0; border:1px solid #8a8aa0; }"
        "QRadioButton:checked:disabled { color:#c7c7d2; }"
        "QScrollBar:vertical { background:#242432; width:12px; margin:0; }"
        "QScrollBar::handle:vertical { background:#4c4c62; min-height:24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QStatusBar { background:#15151d; border-top:1px solid #343445; }"
        "QStatusBar QLabel { color:#00d7ff; font:700 11px \"Segoe UI\"; }");
}

static QLabel *captionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setStyleSheet(
        "QLabel { color:#00d7ff; font:700 10px \"Segoe UI\"; padding:0; }");
    return label;
}

static void configureActionButton(QPushButton *button)
{
    if (button != nullptr) {
        button->setAutoDefault(false);
        button->setDefault(false);
        button->setFixedSize(kActionButtonWidth, 26);
    }
}

static void setWidgetStyle(QWidget *widget, const QString &style)
{
    if (widget != nullptr && widget->styleSheet() != style) {
        widget->setStyleSheet(style);
    }
}

} // namespace

class MainWindow final : public QMainWindow {
public:
    MainWindow()
    {
        setWindowTitle("Shatranj");
        setStyleSheet(appStyleSheet());
        resetBoard();
        prewarmPieceIcons();
        netchesszx_rules_reset();

        auto *root = new QWidget(this);
        auto *layout = new QVBoxLayout(root);
        layout->setContentsMargins(8, 8, 8, 4);
        layout->setSpacing(6);

        auto *banner = new AppBanner(root);
        banner->clicked = [this]() {
            showAboutDialog();
        };
        layout->addWidget(banner);

        auto *topRows = new QVBoxLayout();
        topRows->setContentsMargins(0, 0, 0, 0);
        topRows->setSpacing(14);
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
        hostEdit_->setText(savedHost);
        if (useMqtt) {
            mqttBrokerCache_ = savedHost;
        } else {
            directIpCache_ = savedHost;
        }
        hostEdit_->setClearButtonEnabled(true);
        hostEdit_->setFixedWidth(200);

        portSpin_ = new QSpinBox(root);
        portSpin_->setRange(1, 65535);
        portSpin_->setValue(settings.value("connection/port", useMqtt ? 1883 : 5000).toInt());
        portSpin_->setFixedWidth(76);

        roomEdit_ = new QLineEdit(root);
        roomEdit_->setPlaceholderText("Room");
        roomEdit_->setMaxLength(6);
        roomEdit_->setText(settings.value("connection/room", "DEVROOM").toString());
        roomEdit_->setClearButtonEnabled(true);
        roomEdit_->setFixedWidth(108);

        connectButton_ = new QPushButton("Connect", root);
        startGameButton_ = new QPushButton("Start Game", root);
        startGameButton_->setEnabled(false);
        resetButton_ = new QPushButton("Reset Game", root);
        restoreButton_ = new QPushButton("Restore Game", root);
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
        connectionWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *connectionControls = new QHBoxLayout(connectionWidget);
        connectionControls->setContentsMargins(0, 0, 0, 0);
        connectionControls->setSpacing(8);
        connectionControls->addWidget(directRadio_);
        connectionControls->addWidget(mqttRadio_);
        hostCaptionLabel_ = captionLabel("Host", root);
        roomCaptionLabel_ = captionLabel("Room", root);
        connectionControls->addWidget(hostCaptionLabel_);
        connectionControls->addWidget(hostEdit_);
        connectionControls->addWidget(captionLabel("Port", root));
        connectionControls->addWidget(portSpin_);
        connectionControls->addWidget(roomCaptionLabel_);
        connectionControls->addWidget(roomEdit_);
        connectionRow->addWidget(connectionWidget);
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
        roleRow->addWidget(captionLabel("Role", root));
        roleRow->addWidget(roleHostRadio_);
        roleRow->addWidget(roleGuestRadio_);
        roleRow->addSpacing(10);

        hostColorWidget_ = new QWidget(root);
        hostColorWidget_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *hostColorRow = new QHBoxLayout(hostColorWidget_);
        hostColorRow->setContentsMargins(0, 0, 0, 0);
        hostColorRow->setSpacing(8);
        hostColorLabel_ = captionLabel("Host Color", root);
        hostColorRow->addWidget(hostColorLabel_);
        hostColorRow->addWidget(hostWhiteRadio_);
        hostColorRow->addWidget(hostBlackRadio_);
        roleRow->addWidget(hostColorWidget_);
        sessionRow->addWidget(roleWidget);
        sessionRow->addStretch(1);
        sessionRow->addWidget(actionWidget);
        topRows->addLayout(connectionRow);
        topRows->addLayout(sessionRow);
        layout->addLayout(topRows);

        auto *mainRow = new QHBoxLayout();
        mainRow->setContentsMargins(0, 0, 0, 0);
        mainRow->setSpacing(10);

        auto *boardWidget = new QWidget(root);
        boardWidget->setObjectName("boardFrame");
        boardWidget->setFixedSize(kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2,
                                  kBoardCoordSize * 2 + kBoardSquareSize * 8 + 2);
        boardWidget->setStyleSheet(
            "QWidget#boardFrame { background:#101010; border:1px solid #e6e6e2; }");
        auto *boardLayout = new QGridLayout(boardWidget);
        boardLayout->setContentsMargins(1, 1, 1, 1);
        boardLayout->setSpacing(0);
        addBoardCoordinates(boardLayout, boardWidget);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                auto *button = new QPushButton(boardWidget);
                button->setFixedSize(kBoardSquareSize, kBoardSquareSize);
                button->setIconSize(QSize(kPieceIconSize, kPieceIconSize));
                button->setFocusPolicy(Qt::NoFocus);
                connect(button, &QPushButton::clicked, this, [this, row, col]() {
                    squareClicked(row, col);
                });
                squares_[row][col] = button;
                setSquareStyle(row, col, squareStyle(row, col, false, false, false, false));
                boardLayout->addWidget(button, row + 1, col + 1);
            }
        }
        flipBoardButton_ = new QPushButton(QString(QChar(0x21bb)), boardWidget);
        flipBoardButton_->setToolTip("Flip board");
        flipBoardButton_->setFixedSize(kBoardCoordSize - 4, kBoardCoordSize - 4);
        flipBoardButton_->setFocusPolicy(Qt::NoFocus);
        flipBoardButton_->setGeometry(2,
                                      boardWidget->height() - kBoardCoordSize + 2,
                                      kBoardCoordSize - 4,
                                      kBoardCoordSize - 4);
        flipBoardButton_->setStyleSheet(
            "QPushButton { background:#20202a; color:#00d7ff; border:1px solid #454557;"
            " font:700 11px \"Segoe UI\"; padding:0; }"
            "QPushButton:hover { background:#2b3442; color:#ffffff; }");
        flipBoardButton_->raise();

        auto *sideWidget = new QWidget(root);
        sideWidget->setFixedSize(kSidePanelWidth, boardWidget->height());
        auto *sidePanel = new QVBoxLayout(sideWidget);
        sidePanel->setContentsMargins(0, 0, 0, 0);
        sidePanel->setSpacing(6);

        sidePanel->addWidget(captionLabel("Status / Move", root));

        turnLabel_ = new QLabel("OFFLINE", root);
        turnLabel_->setAlignment(Qt::AlignCenter);
        turnLabel_->setFixedHeight(30);
        sidePanel->addWidget(turnLabel_);

        auto *moveRow = new QHBoxLayout();
        moveRow->setSpacing(6);

        moveEdit_ = new QLineEdit(root);
        moveEdit_->setPlaceholderText("e7e5");
        moveEdit_->setText("e7e5");
        moveEdit_->setClearButtonEnabled(true);
        moveEdit_->setFixedHeight(26);

        moveButton_ = new QPushButton("Send Move", root);
        moveButton_->setEnabled(false);
        configureActionButton(moveButton_);
        setWidgetStyle(moveButton_, moveButtonStyle(false, false));

        moveRow->addWidget(captionLabel("Move", root));
        moveRow->addWidget(moveEdit_, 1);
        moveRow->addWidget(moveButton_);
        sidePanel->addLayout(moveRow);

        auto *selectedRow = new QHBoxLayout();
        selectedRow->setContentsMargins(0, 0, 0, 0);
        selectedRow->setSpacing(6);

        selectedLabel_ = new QLabel("Selected: none", root);
        selectedLabel_->setMaximumHeight(24);
        selectedLabel_->setStyleSheet("QLabel { color:#c7c7d8; font:10px \"Segoe UI\"; }");

        showHintsCheck_ = new QCheckBox("Show Hints", root);
        showHintsCheck_->setChecked(settings.value("ui/showHints", true).toBool());
        showHintsCheck_->setStyleSheet(
            "QCheckBox { color:#c7c7d8; font:10px \"Segoe UI\"; spacing:5px; }"
            "QCheckBox::indicator { width:8px; height:8px; border:1px solid #6a6a7e; background:#242432; }"
            "QCheckBox::indicator:checked { background:#00d7ff; border:1px solid #00d7ff; }"
            "QCheckBox::indicator:disabled { background:#303040; border-color:#454557; }"
            "QCheckBox::indicator:checked:disabled { background:#8a8aa0; border:1px solid #8a8aa0; }"
            "QCheckBox:checked:disabled { color:#c7c7d2; }"
        );

        selectedRow->addWidget(selectedLabel_);
        selectedRow->addStretch(1);
        selectedRow->addWidget(showHintsCheck_);
        sidePanel->addLayout(selectedRow);

        sidePanel->addWidget(captionLabel("Chat", root));

        chatLogEdit_ = new QPlainTextEdit(root);
        chatLogEdit_->setReadOnly(true);
        chatLogEdit_->setFixedHeight(104);
        chatLogEdit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        chatLogEdit_->setStyleSheet(
            "QPlainTextEdit { background:#2b2b39; color:#e8eef6;"
            " font:10pt \"Segoe UI\"; border:1px solid #555568; padding:5px; }");
        chatLogEdit_->document()->setMaximumBlockCount(200);
        sidePanel->addWidget(chatLogEdit_);

        auto *chatControls = new QVBoxLayout();
        chatControls->setSpacing(4);

        chatEdit_ = new QLineEdit(root);
        chatEdit_->setPlaceholderText("Message");
        chatEdit_->setClearButtonEnabled(true);
        chatEdit_->setMaxLength(kChatTextMax);
        chatEdit_->setFixedHeight(26);
        chatEdit_->setStyleSheet(
            "QLineEdit { background:#202b35; color:#ffffff; border:1px solid #00d7ff;"
            " padding:3px 6px; selection-background-color:#00b7d8; selection-color:#101018; }"
            "QLineEdit:focus { background:#243444; border:1px solid #6fefff; }");
        chatButton_ = new QPushButton("Send Chat", root);
        chatButton_->setEnabled(false);
        configureActionButton(chatButton_);
        setWidgetStyle(chatButton_, chatButtonStyle(false));

        auto *chatActionRow = new QHBoxLayout();
        chatActionRow->setSpacing(6);
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
        moveTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        moveTable_->horizontalHeader()->resizeSection(0, 34);
        moveTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        moveTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        moveTable_->setStyleSheet(
            "QTableWidget { background:#2b2b39; color:#f1f0e8;"
            " alternate-background-color:#313142;"
            " font:9pt \"Cascadia Mono\"; border:1px solid #555568; }"
            "QTableWidget::item { padding:0 6px; border-bottom:1px solid #3a3a4a;"
            " border-right:1px solid #4c4c5d; }"
            "QHeaderView::section { background:#2b2b39; color:#f1f0e8;"
            " font:700 8pt \"Cascadia Mono\"; border:0;"
            " border-right:1px solid #4c4c5d; border-bottom:1px solid #5b5b70;"
            " padding:1px 6px; }");
        logStack_->addWidget(moveTable_);

        logEdit_ = new QTextEdit(logStack_);
        logEdit_->setReadOnly(true);
        logEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
        logEdit_->setStyleSheet(
            "QTextEdit { background:#2b2b39; color:#f1f0e8;"
            " font:9pt \"Cascadia Mono\"; border:1px solid #555568; padding:5px; }");
        logEdit_->document()->setDocumentMargin(0);
        logEdit_->document()->setMaximumBlockCount(400);
        logStack_->addWidget(logEdit_);
        sidePanel->addWidget(logStack_, 1);

        auto *logActionRow = new QHBoxLayout();
        logActionRow->setContentsMargins(0, 0, 0, 0);
        logActionRow->setSpacing(6);
        logToggleButton_ = new QPushButton("Log", root);
        logToggleButton_->setFixedSize(kActionButtonWidth, 26);
        logActionRow->addStretch(1);
        logActionRow->addWidget(logToggleButton_);
        sidePanel->addLayout(logActionRow);

        mainRow->addWidget(boardWidget, 0, Qt::AlignTop);
        mainRow->addWidget(sideWidget, 0, Qt::AlignTop);
        layout->addLayout(mainRow);
        setCentralWidget(root);

        socket_ = new QTcpSocket(this);
        directServer_ = new QTcpServer(this);
        statusStateLabel_ = new QLabel("DISCONNECTED", this);
        statusContextLabel_ = new QLabel(QString(), this);
        gameClockLabel_ = new QLabel("GAME --:--", this);
        moveClockLabel_ = new QLabel("MOVE --:--", this);
        statusStateLabel_->setMinimumWidth(150);
        statusContextLabel_->setMinimumWidth(420);
        gameClockLabel_->setMinimumWidth(96);
        moveClockLabel_->setMinimumWidth(96);
        statusStateLabel_->setStyleSheet("QLabel { color:#00d7ff; font:700 11px Segoe UI; }");
        statusContextLabel_->setStyleSheet("QLabel { color:#00d7ff; font:700 11px Segoe UI; }");
        gameClockLabel_->setStyleSheet("QLabel { color:#00d7ff; font:700 11px Segoe UI; }");
        moveClockLabel_->setStyleSheet("QLabel { color:#00d7ff; font:700 11px Segoe UI; }");
        statusContextLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        gameClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        moveClockLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusBar()->setSizeGripEnabled(false);
        statusBar()->addWidget(statusStateLabel_);
        statusBar()->addWidget(statusContextLabel_, 1);
        statusBar()->addPermanentWidget(gameClockLabel_);
        statusBar()->addPermanentWidget(moveClockLabel_);

        configureSessionFromUi();
        generateHostRoomIfNeeded();
        moveEdit_->setText(pcPlaysWhite_ ? "e2e4" : "e7e5");
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

        connect(connectButton_, &QPushButton::clicked, this, [this]() {
            if (isConnected()) {
                localDisconnectPending_ = true;
                (void)sendLine("BYE");
                publishMqttOfflineIfReady();
                socket_->disconnectFromHost();
                return;
            }
            if (isDirectListening()) {
                directServer_->close();
                setStatusText(NETCHESSZX_UI_NOTICE_LISTEN_CANCELLED);
                setConnectedUi(false);
                return;
            }
            if (isConnecting()) {
                if (socket_ != nullptr) {
                    socket_->abort();
                }
                setConnectedUi(false);
                return;
            }
            connectToOpponent();
        });
        connect(flipBoardButton_, &QPushButton::clicked, this, [this]() {
            boardOrientationManual_ = true;
            boardWhiteAtBottom_ = !boardWhiteAtBottom_;
            coordinatesInitialized_ = false;
            refreshBoard();
        });
        connect(directRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
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
                generateHostRoomIfNeeded();
                updateSessionControlsEnabled();
                updateConnectionModeUi();
                refreshStatusBar();
            }
        });
        connect(roleHostRadio_, &QRadioButton::toggled, this, [this](bool checked) {
            configureSessionFromUi();
            if (checked) {
                generateHostRoomIfNeeded();
            }
            updateConnectionModeUi();
            refreshBoard();
            setConnectedUi(isConnected());
        });
        connect(roleGuestRadio_, &QRadioButton::toggled, this, [this]() {
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
            setConnectedUi(isConnected());
            refreshStatusBar();
        });
        connect(hostEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (connectButton_->isEnabled()) {
                connectButton_->animateClick();
            }
        });
        connect(portSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
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
                setStatusText(NETCHESSZX_UI_CONFIRM_RESTART_GAME);
                resetPromptOpen_ = true;
                if (QMessageBox::question(this, NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE,
                                          NETCHESSZX_UI_CONFIRM_RESTART_GAME,
                                          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                    resetPromptOpen_ = false;
                    disconnectToSetup();
                    return;
                }
                resetPromptOpen_ = false;
                if (resetPending_) {
                    setStatusText(NETCHESSZX_UI_ERROR_RESTART_ALREADY_PENDING);
                    return;
                }
                if (sendLine("RESET")) {
                    resetPending_ = true;
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
                if (resetPending_) {
                    setStatusText(NETCHESSZX_UI_ERROR_RESET_ALREADY_PENDING);
                    return;
                }
                resetPromptOpen_ = true;
                if (QMessageBox::question(this, NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE,
                                          NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST,
                                          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                    resetPromptOpen_ = false;
                    return;
                }
                resetPromptOpen_ = false;
                if (sendLine("RESET")) {
                    resetPending_ = true;
                    setStatusText(NETCHESSZX_UI_NOTICE_RESET_REQUESTED_ACK);
                    setConnectedUi(true);
                }
                return;
            }
            resetGame("Game reset");
            stopGameClock();
            setConnectedUi(connected);
        });
        connect(moveButton_, &QPushButton::clicked, this, [this]() {
            sendMove(true);
        });
        connect(chatButton_, &QPushButton::clicked, this, [this]() {
            sendChat();
        });
        connect(chatEdit_, &QLineEdit::textChanged, this, [this]() {
            refreshChatButton();
        });
        connect(chatEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (chatButton_->isEnabled()) {
                sendChat();
            }
        });
        connect(logToggleButton_, &QPushButton::clicked, this, [this]() {
            toggleLogView();
        });
        connect(moveEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (moveButton_->isEnabled()) {
                setStatusText(NETCHESSZX_UI_NOTICE_MOVE_READY);
            }
        });
        connect(moveEdit_, &QLineEdit::textChanged, this, [this]() {
            refreshTurnLabel();
        });
        attachSocket(socket_);
        connect(directServer_, &QTcpServer::newConnection, this, [this]() {
            acceptDirectClient();
        });

        setConnectedUi(false);
        updateClockLabels();
        refreshBoard();
    }

private:
    struct MoveRecord {
        int ply = 0;
        QString move;
        QString notation;
    };

    void closeEvent(QCloseEvent *event) override
    {
        if (isConnected()) {
            (void)sendLine("BYE");
        }
        publishMqttOfflineIfReady();
        if (directServer_ != nullptr) {
            directServer_->close();
        }
        QMainWindow::closeEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == chatEdit_ && event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
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
        return new AppBanner(parent);
    }

    void showAboutDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("About Shatranj");
        dialog.setModal(true);
        dialog.setFixedSize(440, 330);
        dialog.setStyleSheet(
            "QDialog { background:#171821; color:#f2f2f0; }"
            "QLabel { color:#f2f2f0; background:#171821; }"
            "QLabel#muted { color:#c4c7cf; }"
            "QLabel#link { color:#00d7ff; }"
            "QPushButton { background:#292a36; color:#f2f2f0; border:1px solid #444654;"
            " padding:6px 18px; }"
            "QPushButton:hover { background:#343646; }");

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(0, 0, 0, 16);
        layout->setSpacing(10);
        layout->addWidget(aboutHeader(&dialog));

        auto *version = new QLabel(
            QString("Version %1  -  Build %2").arg(QString::fromLatin1(kAppVersion),
                                                   buildStamp()),
            &dialog);
        version->setStyleSheet("QLabel { font:700 12px Segoe UI; padding-left:20px; }");
        layout->addWidget(version);

        auto *desc = new QLabel(
            "Qt desktop app for Shatranj.\n"
            "Connects this client to an opponent over direct TCP or MQTT.",
            &dialog);
        desc->setObjectName("muted");
        desc->setWordWrap(true);
        desc->setStyleSheet("QLabel#muted { color:#c4c7cf; padding-left:20px; padding-right:20px; }");
        layout->addWidget(desc);

        auto *protocol = new QLabel(
            "Protocol support: room setup, role selection, MOVE/ACK/NACK, CHAT, "
            "RESET and GAME START.",
            &dialog);
        protocol->setObjectName("muted");
        protocol->setWordWrap(true);
        protocol->setStyleSheet("QLabel#muted { color:#c4c7cf; padding-left:20px; padding-right:20px; }");
        layout->addWidget(protocol);

        auto *assets = new QLabel(
            "Chess pieces: Sashite western set, CC0 1.0 public domain.",
            &dialog);
        assets->setObjectName("muted");
        assets->setWordWrap(true);
        assets->setStyleSheet("QLabel#muted { color:#c4c7cf; padding-left:20px; padding-right:20px; }");
        layout->addWidget(assets);

        auto *copyright = new QLabel("(C) 2026 M. Ignacio Monge Garcia", &dialog);
        copyright->setStyleSheet("QLabel { font:700 12px Segoe UI; padding-left:20px; }");
        layout->addWidget(copyright);

        layout->addStretch(1);
        auto *buttonRow = new QHBoxLayout();
        buttonRow->setContentsMargins(0, 0, 20, 0);
        buttonRow->addStretch(1);
        auto *closeButton = new QPushButton("Close", &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttonRow->addWidget(closeButton);
        layout->addLayout(buttonRow);

        dialog.exec();
    }

    void resetBoard()
    {
        const char *rows[8] = {
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
                board_[row][col] = rows[row][col];
            }
        }
    }

    void refreshBoard()
    {
        refreshBoardCoordinates();
        for (int displayRow = 0; displayRow < 8; ++displayRow) {
            for (int displayCol = 0; displayCol < 8; ++displayCol) {
                const int boardRow = boardRowForDisplay(displayRow);
                const int boardCol = boardColForDisplay(displayCol);
                const bool selected = (boardRow == selectedRow_ && boardCol == selectedCol_);
                const bool target = (boardRow == targetRow_ && boardCol == targetCol_);
                const bool legalTarget = isLegalTarget(boardRow, boardCol);
                const bool feedback = isFeedbackSquare(boardRow, boardCol);
                squares_[displayRow][displayCol]->setText(QString());
                squares_[displayRow][displayCol]->setIcon(
                    boardPiecesVisible_ ? pieceIcon(board_[boardRow][boardCol]) : QIcon());
                const bool hasPiece = (board_[boardRow][boardCol] != '.');
                const bool showHints = showHintsCheck_ ? showHintsCheck_->isChecked() : true;
                setSquareStyle(displayRow, displayCol,
                               squareStyle(boardRow, boardCol, selected, target,
                                           legalTarget, feedback, hasPiece, showHints));
            }
        }
    }

    static QLabel *coordLabel(const QString &text, QWidget *parent, const QSize &size)
    {
        auto *label = new QLabel(text, parent);
        label->setAlignment(Qt::AlignCenter);
        label->setFixedSize(size);
        label->setStyleSheet(
            "QLabel { color:#7b828a; background:transparent;"
            " font:600 12px Segoe UI; padding:0; margin:0; }");
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

    static QPainterPath roundedRectPath(double x, double y, double w, double h, double r)
    {
        QPainterPath path;
        path.addRoundedRect(QRectF(x, y, w, h), r, r);
        return path;
    }

    static void drawBase(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        p.setPen(stroke);
        p.setBrush(fill);
        p.drawPath(roundedRectPath(13, 38, 26, 6, 2));
        p.drawPath(roundedRectPath(9, 44, 34, 5, 2));
    }

    static void drawPawn(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        p.setPen(stroke);
        p.setBrush(fill);
        p.drawEllipse(QPointF(26, 14), 8, 8);
        p.drawPath(roundedRectPath(20, 22, 12, 19, 5));
        drawBase(p, fill, stroke);
    }

    static void drawRook(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        QPainterPath crown;
        crown.moveTo(14, 10);
        crown.lineTo(19, 10);
        crown.lineTo(19, 15);
        crown.lineTo(24, 15);
        crown.lineTo(24, 10);
        crown.lineTo(29, 10);
        crown.lineTo(29, 15);
        crown.lineTo(34, 15);
        crown.lineTo(34, 10);
        crown.lineTo(39, 10);
        crown.lineTo(39, 20);
        crown.lineTo(14, 20);
        crown.closeSubpath();

        p.setPen(stroke);
        p.setBrush(fill);
        p.drawPath(crown);
        p.drawPath(roundedRectPath(17, 20, 18, 21, 2));
        drawBase(p, fill, stroke);
    }

    static void drawKnight(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        QPainterPath head;
        head.moveTo(17, 40);
        head.cubicTo(18, 31, 18, 23, 23, 16);
        head.cubicTo(27, 10, 34, 9, 39, 13);
        head.cubicTo(36, 16, 37, 19, 42, 23);
        head.lineTo(35, 26);
        head.cubicTo(38, 32, 35, 38, 30, 40);
        head.closeSubpath();

        p.setPen(stroke);
        p.setBrush(fill);
        p.drawPath(head);
        p.drawLine(28, 18, 24, 24);
        p.drawEllipse(QPointF(33, 16), 1.5, 1.5);
        drawBase(p, fill, stroke);
    }

    static void drawBishop(QPainter &p, const QBrush &fill, const QPen &stroke, const QPen &detail)
    {
        p.setPen(stroke);
        p.setBrush(fill);
        p.drawEllipse(QPointF(26, 13), 6, 6);
        p.drawPath(roundedRectPath(18, 19, 16, 22, 8));
        p.setPen(detail);
        p.drawLine(29, 21, 22, 33);
        drawBase(p, fill, stroke);
    }

    static void drawQueen(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        QPainterPath body;
        body.moveTo(15, 37);
        body.lineTo(20, 17);
        body.lineTo(26, 31);
        body.lineTo(32, 17);
        body.lineTo(37, 37);
        body.closeSubpath();

        p.setPen(stroke);
        p.setBrush(fill);
        p.drawEllipse(QPointF(20, 13), 4, 4);
        p.drawEllipse(QPointF(26, 10), 4, 4);
        p.drawEllipse(QPointF(32, 13), 4, 4);
        p.drawPath(body);
        drawBase(p, fill, stroke);
    }

    static void drawKing(QPainter &p, const QBrush &fill, const QPen &stroke)
    {
        p.setPen(stroke);
        p.setBrush(fill);
        p.drawLine(26, 7, 26, 18);
        p.drawLine(21, 12, 31, 12);
        p.drawEllipse(QPointF(26, 22), 8, 8);
        p.drawPath(roundedRectPath(18, 27, 16, 14, 5));
        drawBase(p, fill, stroke);
    }

    static QString pieceAssetName(char piece)
    {
        const bool white = piece >= 'A' && piece <= 'Z';
        const char lower = white ? static_cast<char>(piece + 32) : piece;
        char type = '\0';

        switch (lower) {
        case 'p':
            type = 'P';
            break;
        case 'r':
            type = 'R';
            break;
        case 'n':
            type = 'N';
            break;
        case 'b':
            type = 'B';
            break;
        case 'q':
            type = 'Q';
            break;
        case 'k':
            type = 'K';
            break;
        default:
            return QString();
        }

        return QString("%1%2.png").arg(white ? QLatin1Char('w') : QLatin1Char('b'))
                                  .arg(QLatin1Char(type));
    }

    static QString pieceAssetPath(char piece)
    {
        const QString name = pieceAssetName(piece);
        if (name.isEmpty()) {
            return QString();
        }

        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList dirs = {
            QDir::cleanPath(appDir + "/assets/pieces"),
            QDir::cleanPath(appDir + "/assets/pc-client/pieces"),
            QDir::cleanPath(appDir + "/../../assets/pc-client/pieces"),
            QDir::cleanPath(QDir::currentPath() + "/assets/pc-client/pieces")
        };

        for (const QString &dir : dirs) {
            const QString path = QDir(dir).filePath(name);
            if (QFileInfo::exists(path)) {
                return path;
            }
        }

        return QString();
    }

    static QIcon pieceIcon(char piece)
    {
        if (piece == '.') {
            return QIcon();
        }

        static QHash<QString, QIcon> iconCache;
        const QString cacheKey = QString(QChar(piece));
        if (iconCache.contains(cacheKey)) {
            return iconCache.value(cacheKey);
        }

        const QString path = pieceAssetPath(piece);
        if (!path.isEmpty()) {
            const QIcon icon(path);
            iconCache.insert(cacheKey, icon);
            return icon;
        }

        const bool white = piece >= 'A' && piece <= 'Z';
        const QColor fillColor = white ? QColor("#f7f0de") : QColor("#1d241f");
        const QColor strokeColor = white ? QColor("#222820") : QColor("#f7f0de");
        const QColor detailColor = white ? QColor("#5b604f") : QColor("#d5c7a8");

        QPixmap pixmap(52, 52);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.translate(0.5, 0.5);

        const QBrush fill(fillColor);
        const QPen stroke(strokeColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        const QPen detail(detailColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        switch (piece >= 'A' && piece <= 'Z' ? static_cast<char>(piece + 32) : piece) {
        case 'p':
            drawPawn(p, fill, stroke);
            break;
        case 'r':
            drawRook(p, fill, stroke);
            break;
        case 'n':
            drawKnight(p, fill, stroke);
            break;
        case 'b':
            drawBishop(p, fill, stroke, detail);
            break;
        case 'q':
            drawQueen(p, fill, stroke);
            break;
        case 'k':
            drawKing(p, fill, stroke);
            break;
        default:
            return QIcon();
        }

        const QIcon icon(pixmap);
        iconCache.insert(cacheKey, icon);
        return icon;
    }

    static void prewarmPieceIcons()
    {
        static bool warmed = false;
        if (warmed) {
            return;
        }
        warmed = true;

        for (const char piece : QByteArray("PNBRQKpnbrqk")) {
            (void)pieceIcon(piece);
        }
    }

    static QString squareName(int row, int col)
    {
        const QChar file('a' + col);
        const QChar rank('8' - row);
        return QString(file) + QString(rank);
    }

    static bool moveCoords(const QString &move, int *fromRow, int *fromCol,
                           int *toRow, int *toCol)
    {
        if (move.size() != 4 && move.size() != 5) {
            return false;
        }
        if (move[0] < 'a' || move[0] > 'h' ||
            move[2] < 'a' || move[2] > 'h' ||
            move[1] < '1' || move[1] > '8' ||
            move[3] < '1' || move[3] > '8') {
            return false;
        }

        *fromCol = move[0].unicode() - 'a';
        *fromRow = '8' - move[1].unicode();
        *toCol = move[2].unicode() - 'a';
        *toRow = '8' - move[3].unicode();
        return true;
    }

    static bool isWhitePiece(char piece)
    {
        return piece >= 'A' && piece <= 'Z';
    }

    static bool isBlackPiece(char piece)
    {
        return piece >= 'a' && piece <= 'z';
    }

    static bool sameColorPiece(char a, char b)
    {
        return (isWhitePiece(a) && isWhitePiece(b)) ||
               (isBlackPiece(a) && isBlackPiece(b));
    }

    static char lowerPiece(char piece)
    {
        return isWhitePiece(piece) ? static_cast<char>(piece + ('a' - 'A')) : piece;
    }

    static QString sanPieceLetter(char piece)
    {
        switch (lowerPiece(piece)) {
        case 'n':
            return QStringLiteral("N");
        case 'b':
            return QStringLiteral("B");
        case 'r':
            return QStringLiteral("R");
        case 'q':
            return QStringLiteral("Q");
        case 'k':
            return QStringLiteral("K");
        default:
            return QString();
        }
    }

    static bool pathClearOnBoard(const char board[8][8], int fromRow, int fromCol,
                                 int toRow, int toCol)
    {
        const int rowStep = (toRow > fromRow) ? 1 : (toRow < fromRow ? -1 : 0);
        const int colStep = (toCol > fromCol) ? 1 : (toCol < fromCol ? -1 : 0);
        int row = fromRow + rowStep;
        int col = fromCol + colStep;

        while (row != toRow || col != toCol) {
            if (board[row][col] != '.') {
                return false;
            }
            row += rowStep;
            col += colStep;
        }
        return true;
    }

    static bool pieceAttacksOnBoard(const char board[8][8], int fromRow, int fromCol,
                                    int toRow, int toCol)
    {
        const char piece = board[fromRow][fromCol];
        const int dr = toRow - fromRow;
        const int dc = toCol - fromCol;
        const int adr = dr < 0 ? -dr : dr;
        const int adc = dc < 0 ? -dc : dc;

        switch (lowerPiece(piece)) {
        case 'p':
            return isWhitePiece(piece) ? (dr == -1 && adc == 1)
                                       : (dr == 1 && adc == 1);
        case 'n':
            return (adr == 1 && adc == 2) || (adr == 2 && adc == 1);
        case 'b':
            return adr == adc && pathClearOnBoard(board, fromRow, fromCol, toRow, toCol);
        case 'r':
            return (dr == 0 || dc == 0) &&
                   pathClearOnBoard(board, fromRow, fromCol, toRow, toCol);
        case 'q':
            return ((adr == adc) || dr == 0 || dc == 0) &&
                   pathClearOnBoard(board, fromRow, fromCol, toRow, toCol);
        case 'k':
            return adr <= 1 && adc <= 1;
        default:
            return false;
        }
    }

    bool kingInCheck(bool whiteKing) const
    {
        const char king = whiteKing ? 'K' : 'k';
        int kingRow = -1;
        int kingCol = -1;

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (board_[row][col] == king) {
                    kingRow = row;
                    kingCol = col;
                    break;
                }
            }
            if (kingRow >= 0) {
                break;
            }
        }
        if (kingRow < 0) {
            return false;
        }

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                const char piece = board_[row][col];
                if (piece == '.' || sameColorPiece(piece, king)) {
                    continue;
                }
                if (pieceAttacksOnBoard(board_, row, col, kingRow, kingCol)) {
                    return true;
                }
            }
        }
        return false;
    }

    QString checkSuffixAfterMove(bool moverWhite) const
    {
        const bool opponentWhite = !moverWhite;
        if (!kingInCheck(opponentWhite)) {
            return QString();
        }
        return netchesszx_rules_has_legal_moves() ? QStringLiteral("+")
                                                  : QStringLiteral("#");
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
        return squareName(fromRow, fromCol);
    }

    QString moveNotationBase(const QString &move) const
    {
        int fromRow = 0;
        int fromCol = 0;
        int toRow = 0;
        int toCol = 0;
        if (!moveCoords(move, &fromRow, &fromCol, &toRow, &toCol)) {
            return move.toUpper();
        }

        const char piece = board_[fromRow][fromCol];
        if (piece == '.') {
            return move.toUpper();
        }

        const char kind = lowerPiece(piece);
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
            notation += sanPieceLetter(piece);
            notation += disambiguationForMove(piece, fromRow, fromCol, toRow, toCol);
        }
        if (capture) {
            notation += QStringLiteral("x");
        }
        notation += QChar('a' + toCol);
        notation += QChar('8' - toRow);
        if (move.size() == 5) {
            notation += QStringLiteral("=");
            notation += sanPieceLetter(move[4].toLatin1());
        }
        return notation;
    }

    static bool ackLineMatches(const QString &line, int ply)
    {
        if (!line.startsWith(QStringLiteral("ACK "))) {
            return false;
        }
        const int end = line.indexOf(' ', 4);
        bool ok = false;
        const int ackPly = (end < 0 ? line.mid(4) : line.mid(4, end - 4)).toInt(&ok);
        return ok && ackPly == ply;
    }

    static int nackLinePly(const QString &line, bool *ok)
    {
        if (!line.startsWith(QStringLiteral("NACK "))) {
            *ok = false;
            return 0;
        }
        const int end = line.indexOf(' ', 5);
        return (end < 0 ? line.mid(5) : line.mid(5, end - 5)).toInt(ok);
    }

    static bool isPingLine(const QString &line)
    {
        return line == "PING";
    }

    static bool isAckPingLine(const QString &line)
    {
        return line == "ACK PING";
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
        return QString(QChar('a' + boardColForDisplay(displayCol)));
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

    QString opponentSideLetter() const
    {
        return pcPlaysWhite_ ? "B" : "W";
    }

    QString pcChatName() const
    {
        return QStringLiteral("PLAYER");
    }

    QString opponentChatName() const
    {
        return QStringLiteral("OPPONENT");
    }

    QString opponentSideName() const
    {
        return pcPlaysWhite_ ? "BLACK" : "WHITE";
    }

    void syncBoardOrientationWithPcSide()
    {
        if (!boardOrientationManual_) {
            boardWhiteAtBottom_ = pcPlaysWhite_;
            coordinatesInitialized_ = false;
        }
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
        const QString square = squareName(row, col);

        for (const QString &target : legalTargets_) {
            if (target == square) {
                return true;
            }
        }

        return false;
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
                       squareStyle(row, col, selected, target, legalTarget, feedback, hasPiece, showHints));
    }

    void refreshBoardSquareIcon(int row, int col, bool visible)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8) {
            return;
        }

        const int displayRow = displayRowForBoard(row);
        const int displayCol = displayColForBoard(col);
        squares_[displayRow][displayCol]->setIcon(
            boardPiecesVisible_ && visible ? pieceIcon(board_[row][col]) : QIcon());
    }

    void revealBoardSquarePair(int generation, int rowA, int colA, int rowB, int colB)
    {
        if (generation != pieceRevealGeneration_) {
            return;
        }
        refreshBoardSquareIcon(rowA, colA, true);
        refreshBoardSquareIcon(rowB, colB, true);
    }

    void animateBoardPiecesIn()
    {
        const int generation = ++pieceRevealGeneration_;
        int delay = 0;

        boardPiecesVisible_ = true;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                refreshBoardSquareIcon(row, col, false);
            }
        }

        for (int i = 0; i < 8; ++i) {
            QTimer::singleShot(delay, this, [this, generation, i]() {
                revealBoardSquarePair(generation, 0, i, 7, 7 - i);
            });
            delay += kPieceRevealStepMs;
        }
        delay += kPieceRevealMiddlePauseMs;
        for (int i = 0; i < 8; ++i) {
            QTimer::singleShot(delay, this, [this, generation, i]() {
                revealBoardSquarePair(generation, 1, 7 - i, 6, i);
            });
            delay += kPieceRevealStepMs;
        }
    }

    void flashPieceAt(int row, int col, std::function<void()> done)
    {
        if (row < 0 || row >= 8 || col < 0 || col >= 8 ||
            board_[row][col] == '.' || !boardPiecesVisible_) {
            if (done) {
                done();
            }
            return;
        }

        const int generation = ++pieceFlashGeneration_;
        for (int step = 0; step <= 5; ++step) {
            QTimer::singleShot(step * 90, this, [this, generation, row, col, step, done]() {
                if (generation != pieceFlashGeneration_) {
                    return;
                }
                refreshBoardSquareIcon(row, col, (step % 2) != 0);
                if (step == 5) {
                    refreshBoardSquareIcon(row, col, true);
                    if (done) {
                        done();
                    }
                }
            });
        }
    }

    void refreshBoardSquareStyleByName(const QString &square)
    {
        if (square.size() < 2) {
            return;
        }
        refreshBoardSquareStyle('8' - square[1].unicode(),
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
        refreshBoardSquareStyle(oldSelectedRow, oldSelectedCol);
        refreshBoardSquareStyle(oldTargetRow, oldTargetCol);

        for (const QString &target : legalTargets_) {
            refreshBoardSquareStyleByName(target);
        }
        refreshBoardSquareStyle(selectedRow_, selectedCol_);
        refreshBoardSquareStyle(targetRow_, targetCol_);
        refreshTurnLabel();
    }

    void attachSocket(QTcpSocket *sock)
    {
        if (sock == nullptr) {
            return;
        }
        connect(sock, &QTcpSocket::connected, this, [this, sock]() {
            if (sock == socket_) {
                handleSocketConnected();
            }
        });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
            if (sock == socket_) {
                if (ignoreNextDisconnect_) {
                    ignoreNextDisconnect_ = false;
                    return;
                }
                handleSocketDisconnected();
            }
        });
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            if (sock == socket_) {
                consumeReadyRead();
            }
        });
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError) {
            if (sock == socket_) {
                handleSocketError();
            }
        });
#else
        connect(sock, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this, [this, sock](QAbstractSocket::SocketError) {
                    if (sock == socket_) {
                        handleSocketError();
                    }
                });
#endif
    }

    void handleSocketError()
    {
        lastSocketError_ = socket_->errorString();
        appendLog("ERROR: " + lastSocketError_);
        if (!isMqttMode() && !directReady_) {
            const QString status = lastSocketError_.contains("refused", Qt::CaseInsensitive) ?
                                       "Connection refused - opponent not ready" :
                                       "Connection failed - " + lastSocketError_;
            ignoreNextDisconnect_ = true;
            socket_->abort();
            resetGame(status);
            stopGameClock();
            clearChatLog();
            setStatusText(status);
            setConnectedUi(false);
            lastSocketError_.clear();
            return;
        }
        setStatusText(lastSocketError_);
        if (!isConnected()) {
            setConnectedUi(false);
        }
    }

    void handleSocketConnected()
    {
        linkWatch_.restart();
        directHelloRetries_ = 0;
        directPingOutstanding_ = false;
        directReady_ = false;
        pcTurn_ = false;
        clearSelection();
        chatLogEdit_->clear();
        if (isMqttMode()) {
            setConnectedUi(true);
            mqttHandshake();
            return;
        }
        setStatusText(NETCHESSZX_UI_NOTICE_WAITING_OPPONENT_APP);
        setConnectedUi(false);
        refreshBoard();
        appendLog("CONNECTED TCP");
        sendDirectHello();
    }

    void handleSocketDisconnected()
    {
        const bool directPending = !isMqttMode() && !directReady_;
        QString status;

        if (localDisconnectPending_) {
            status = NETCHESSZX_UI_PHASE_DISCONNECTED;
            localDisconnectPending_ = false;
        } else if (directPending && !lastSocketError_.isEmpty()) {
            status = lastSocketError_;
        } else if (directPending &&
                   statusMessage_.contains("conflict", Qt::CaseInsensitive)) {
            status = statusMessage_;
        } else {
            status = directPending ? NETCHESSZX_UI_ERROR_OPPONENT_APP_NOT_READY :
                                     NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED;
        }
        directReady_ = false;
        directPingOutstanding_ = false;
        resetGame(status);
        stopGameClock();
        clearChatLog();
        setConnectedUi(false);
        setStatusText(status);
        lastSocketError_.clear();
        appendLog("DISCONNECTED");
    }

    void disconnectToSetup()
    {
        if (isConnected()) {
            localDisconnectPending_ = true;
            (void)sendLine("BYE");
            publishMqttOfflineIfReady();
            socket_->disconnectFromHost();
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
        mqttConnected_ = false;
        mqttSubscribed_ = false;
        mqttSubacksPending_ = 0;
        mqttSideReady_ = false;
        mqttPeerReady_ = false;
        mqttPeerPingOutstanding_ = false;
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
        if (socket_ != nullptr && socket_->state() != QAbstractSocket::UnconnectedState) {
            socket_->abort();
        }
        if (socket_ != nullptr) {
            socket_->deleteLater();
        }
        socket_ = accepted;
        attachSocket(socket_);
        directServer_->close();
        appendLog(QString("ACCEPT %1:%2")
                      .arg(socket_->peerAddress().toString())
                      .arg(socket_->peerPort()));
        handleSocketConnected();
    }

    bool isFeedbackSquare(int row, int col) const
    {
        return feedbackOn_ && row == feedbackRow_ && col == feedbackCol_;
    }

    static QString squareStyle(int row, int col, bool selected, bool target,
                               bool legalTarget, bool feedback, bool hasPiece = false,
                               bool showHints = true)
    {
        const bool light = ((row + col) % 2) == 0;
        QString bg;
        QString border;

        if (feedback) {
            bg = "#f2dc54";
            border = "4px solid #1f7a8c";
        } else if (selected) {
            bg = "#c9b56b";
            border = "3px solid #5d4b1d";
        } else if (target) {
            if (legalTarget && showHints && !hasPiece) {
                const QString dotColor = "rgba(0, 0, 0, 0.28)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 #9fb8d9, stop:1.0 #9fb8d9)").arg(dotColor);
            } else {
                bg = "#9fb8d9";
            }
            border = "3px solid #2f5f9f";
        } else if (legalTarget && showHints) {
            if (hasPiece) {
                bg = light ? "#d5ebd5" : "#486648";
                border = "2px solid #6fa86f";
            } else {
                const QString baseBg = light ? "#f0f0ec" : "#5f6870";
                const QString dotColor = "rgba(0, 0, 0, 0.25)";
                bg = QString("qradialgradient(cx:0.5, cy:0.5, radius:0.12, fx:0.5, fy:0.5, stop:0 %1, stop:0.85 %1, stop:0.9 %2, stop:1.0 %2)").arg(dotColor, baseBg);
                border = "1px solid #2c3034";
            }
        } else {
            bg = light ? "#f0f0ec" : "#5f6870";
            border = "1px solid #2c3034";
        }

        const QString fg = light || selected || target || feedback ? "#1e1e1e" : "#ffffff";
        return QString("QPushButton { background:%1; color:%2; border:%3;"
                       " font:700 24px Consolas; padding:0; margin:0; }").arg(bg, fg, border);
    }

    void showDestinationFeedback(int row, int col)
    {
        ++feedbackGeneration_;
        feedbackRow_ = row;
        feedbackCol_ = col;
        feedbackOn_ = true;
        refreshBoardSquareStyle(row, col);

        const int generation = feedbackGeneration_;
        for (int step = 1; step <= 5; ++step) {
            QTimer::singleShot(step * 90, this, [this, generation, row, col, step]() {
                if (generation != feedbackGeneration_) {
                    return;
                }
                feedbackOn_ = (step % 2) == 0;
                refreshBoardSquareStyle(row, col);
                if (step == 5) {
                    feedbackRow_ = -1;
                    feedbackCol_ = -1;
                    feedbackOn_ = false;
                    refreshBoardSquareStyle(row, col);
                }
            });
        }
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

    void squareClicked(int displayRow, int displayCol)
    {
        const int row = boardRowForDisplay(displayRow);
        const int col = boardColForDisplay(displayCol);
        const QString clicked = squareName(row, col);
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
            } else if (pendingPly_ != 0) {
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
            moveEdit_->setText(clicked);
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
            moveEdit_->setText(clicked);
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
                          .arg(clicked, squareName(selectedRow_, selectedCol_)));
            return;
        }

        const char movingPiece = board_[selectedRow_][selectedCol_];
        QString promotionSuffix = "";
        if (movingPiece == 'P' && row == 0) {
            promotionSuffix = "q";
        } else if (movingPiece == 'p' && row == 7) {
            promotionSuffix = "q";
        }

        const QString move = squareName(selectedRow_, selectedCol_) + squareName(row, col) + promotionSuffix;
        moveEdit_->setText(move);
        targetRow_ = row;
        targetCol_ = col;
        selectedLabel_->setText(QString("Selected: %1 -> %2")
                                    .arg(squareName(selectedRow_, selectedCol_), clicked));
        setStatusText(QString("Move ready: %1 - press Send Move").arg(move));
        appendLog(QString("CLICK: move %1").arg(move));
        refreshSelectionFootprint(oldSelectedRow, oldSelectedCol,
                                  oldTargetRow, oldTargetCol,
                                  oldLegalTargets);
        refreshTurnLabel();
    }

    void connectToOpponent()
    {
        const QString host = hostEdit_->text().trimmed();
        const quint16 port = static_cast<quint16>(portSpin_->value());
        const QString room = roomEdit_->text().trimmed().toUpper();

        configureSessionFromUi();

        if (host.isEmpty() && (isMqttMode() || !pcIsHost_)) {
            appendLog("ERROR: empty host");
            setStatusText(isMqttMode() ? NETCHESSZX_UI_ERROR_EMPTY_BROKER : NETCHESSZX_UI_ERROR_INVALID_IP);
            return;
        }
        if (!isMqttMode() && !pcIsHost_ && !isDirectIpSyntaxOk(host)) {
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
        if (isMqttMode() && !isMqttRoomSyntaxOk(room)) {
            appendLog("ERROR: MQTT room must be A-Z or 0-9, max 6 chars");
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

        if (isMqttMode()) {
            mqttBrokerCache_ = host;
        } else if (!pcIsHost_) {
            directIpCache_ = host;
        }
        const QString savedConnectionHost =
            (!isMqttMode() && pcIsHost_) ?
                (directIpCache_.isEmpty() ? QStringLiteral("192.168.0.") : directIpCache_) :
                host;

        QSettings settings;
        settings.setValue("connection/host", savedConnectionHost);
        settings.setValue("connection/port", port);
        settings.setValue("connection/mqtt", isMqttMode());
        settings.setValue("connection/room", room);
        settings.setValue("connection/pcHost", pcIsHost_);
        settings.setValue("connection/hostWhite", hostPlaysWhite_);

        rxBuffer_.clear();
        mqttParser_.clear();
        mqttConnected_ = false;
        mqttSubscribed_ = false;
        mqttSubacksPending_ = 0;
        mqttSetupAnnounces_ = 0;
        mqttPeerPingOutstanding_ = false;
        directHelloRetries_ = 0;
        directPingOutstanding_ = false;
        directReady_ = false;
        mqttPeerReady_ = false;
        mqttSideReady_ = false;
        mqttSessionId_ = (isMqttMode() && pcIsHost_) ? newMqttSessionId() : 0;
        startPending_ = false;
        ignoreNextDisconnect_ = false;
        lastSocketError_.clear();
        mqttNextPacketId_ = 1;
        mqttRoom_ = room;

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
        socket_->connectToHost(host, port);
        setConnectedUi(false);
    }

    void resetGame(const QString &status, bool keepDirectLink = false)
    {
        ++pieceFlashGeneration_;
        ++pieceRevealGeneration_;
        ++feedbackGeneration_;
        stopGameClock();
        pendingPly_ = 0;
        pendingMove_.clear();
        startPending_ = false;
        resetPending_ = false;
        drawPending_ = false;
        resetPromptOpen_ = false;
        gameOver_ = false;
        gameCheck_ = false;
        rxBuffer_.clear();
        if (!isMqttMode()) {
            mqttParser_.clear();
            mqttConnected_ = false;
            mqttSubscribed_ = false;
            mqttSubacksPending_ = 0;
            mqttSetupAnnounces_ = 0;
            mqttPeerPingOutstanding_ = false;
            // A RESET/rematch happens on the SAME live TCP connection (ping
            // still flowing), so the direct handshake must persist. Only tear
            // it down on a real disconnect (keepDirectLink == false), otherwise
            // the host's follow-up GAME START is dropped as "pre-HELLO".
            if (!keepDirectLink) {
                directHelloRetries_ = 0;
                directPingOutstanding_ = false;
                directReady_ = false;
            }
            mqttPeerReady_ = false;
            mqttSideReady_ = false;
        }
        pcTurn_ = false;
        lastMove_.clear();
        clearMoveHistory();
        boardPiecesVisible_ = false;
        clearSelection();
        resetBoard();
        netchesszx_rules_reset();
        nextPly_ = 1;
        moveEdit_->setText(pcPlaysWhite_ ? "e2e4" : "e7e5");
        selectedLabel_->setText("Selected: none");
        setStatusText(status);
        refreshBoard();
        refreshTurnLabel();
    }

    QString directHelloLine() const
    {
        return QString::fromLatin1(
            netchess_direct_hello(pcIsHost_ ? 1u : 0u,
                                  pcPlaysWhite_ ? 1u : 0u));
    }

    void sendDirectHello()
    {
        if (!isMqttMode() && socket_->state() == QAbstractSocket::ConnectedState) {
            (void)sendLine(directHelloLine());
        }
    }

    void sendChat()
    {
        QString text = chatEdit_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }

        text.replace('\r', ' ');
        text.replace('\n', ' ');
        const int maxText = isMqttMode() ? kChatTextMax : kDirectChatTextMax;
        if (text.size() > maxText) {
            text = text.left(maxText);
        }
        const QString cmd = text.toLower();
        if (cmd == "/resign") {
            if (!gameClockRunning_) {
                setStatusText(NETCHESSZX_UI_NOTICE_GAME_NOT_STARTED);
                return;
            }
            if (QMessageBox::question(this, NETCHESSZX_UI_CONFIRM_PC_RESIGN_TITLE,
                                      NETCHESSZX_UI_CONFIRM_RESIGN,
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
            if (sendLine("RESIGN")) {
                chatEdit_->clear();
                endGameOver("RESIGN");
            }
            return;
        }
        if (cmd == "/draw") {
            if (!gameClockRunning_ || resetPending_ || drawPending_) {
                setStatusText(NETCHESSZX_UI_ERROR_CANNOT_OFFER_DRAW);
                return;
            }
            if (QMessageBox::question(this, NETCHESSZX_UI_CONFIRM_PC_DRAW_TITLE,
                                      NETCHESSZX_UI_CONFIRM_DRAW,
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
            if (sendLine("DRAW")) {
                drawPending_ = true;
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_DRAW_ACK);
                chatEdit_->clear();
            }
            return;
        }
        if (!sendLine("CHAT " + text)) {
            return;
        }
        appendChat(pcChatName(), text);
        chatEdit_->clear();
        refreshChatButton();
    }

    void sendGameStart()
    {
        if (socket_->state() != QAbstractSocket::ConnectedState) {
            setStatusText(NETCHESSZX_UI_ERROR_NOT_CONNECTED);
            return;
        }
        if (startPending_) {
            setStatusText(NETCHESSZX_UI_ERROR_START_ALREADY_PENDING);
            return;
        }
        if (resetPending_) {
            setStatusText(NETCHESSZX_UI_ERROR_RESET_PENDING);
            return;
        }
        if (gameClockRunning_) {
            setStatusText(NETCHESSZX_UI_ERROR_GAME_ALREADY_RUNNING);
            return;
        }
        if (!pcIsHost_) {
            setStatusText(isMqttMode() ? NETCHESSZX_UI_PHASE_WAITING_HOST_START :
                                         NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            appendLog("START ignored: only host starts");
            return;
        }
        if (isMqttMode()) {
            if (!mqttPeerReady_) {
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT);
                appendLog("START ignored: peer not ready");
                return;
            }
        } else if (!directReady_) {
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT);
            appendLog("START ignored: direct peer not ready");
            return;
        }
        if (sendLine(isMqttMode() ? QString("GAME START")
                                  : QString::fromLatin1(
                                        pcPlaysWhite_ ? "GAME START WHITE=HOST"
                                                      : "GAME START WHITE=GUEST"))) {
            gameOver_ = false;
            startPending_ = true;
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
            } else if (pendingPly_ != 0) {
                appendLog(QString("ERROR: waiting ACK for ply %1").arg(pendingPly_));
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
            } else {
                appendLog("ERROR: opponent moves first; local side waits");
                setStatusBarText("Not your turn");
            }
            return;
        }
        if (!isMoveSyntaxOk(move)) {
            appendLog("ERROR: move syntax must be e2e4 or e7e8q");
            return;
        }
        if (move.size() == 5 && move[4] != 'q') {
            appendLog("ERROR: mcu-max supports queen promotion only for now");
            setStatusText(NETCHESSZX_UI_ERROR_ONLY_QUEEN_PROMOTION);
            return;
        }
        if (pendingPly_ != 0) {
            appendLog(QString("ERROR: waiting ACK for ply %1").arg(pendingPly_));
            setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
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

        const int ply = nextPly_;
        QString payload = QString("MOVE %1 %2").arg(ply).arg(move);
        if (!sendLine(payload)) {
            return;
        }
        const qint64 prepMs = sendPrepTimer.elapsed();
        if (prepMs > kMoveSendWarnMs) {
            appendLog(QString("WARN: move send prep took %1 ms").arg(prepMs));
        }

        pendingPly_ = ply;
        pendingMove_ = move;
        clearSelection();
        moveEdit_->clear();
        refreshBoard();
        showDestinationFeedback(toRow, toCol);
        setStatusText(NETCHESSZX_UI_PHASE_WAITING_OPPONENT);
        setConnectedUi(true);
    }

    bool sendLine(const QString &text)
    {
        if (socket_->state() != QAbstractSocket::ConnectedState) {
            appendLog("ERROR: not connected");
            return false;
        }

        if (isMqttMode()) {
            return sendMqttText(text);
        }

        QByteArray data = text.toLatin1();
        if (data.size() > kDirectPayloadTextMax) {
            appendLog("ERROR: direct line too long");
            return false;
        }
        data.append('\n');

        const qint64 written = socket_->write(data);
        if (written < 0) {
            appendLog("ERROR: write failed: " + socket_->errorString());
            return false;
        }
        if (written != data.size()) {
            appendLog(QString("ERROR: partial write %1/%2")
                          .arg(written)
                          .arg(data.size()));
            return false;
        }

        socket_->flush();
        appendLog("TX: " + text);
        return true;
    }

    void consumeReadyRead()
    {
        const QByteArray data = socket_->readAll();
        if (!data.isEmpty() && isMqttMode()) {
            linkWatch_.restart();
        }
        if (isMqttMode()) {
            consumeMqttBytes(data);
            return;
        }
        rxBuffer_.append(data);

        while (true) {
            const qsizetype eol = rxBuffer_.indexOf('\n');
            if (eol < 0) {
                break;
            }

            QByteArray lineBytes = rxBuffer_.left(eol);
            rxBuffer_.remove(0, eol + 1);
            if (lineBytes.endsWith('\r')) {
                lineBytes.chop(1);
            }

            const QString line = QString::fromLatin1(lineBytes).trimmed();
            if (!line.isEmpty()) {
                handleRxLine(line);
            }
        }

        if (rxBuffer_.size() > 4096) {
            appendLog("ERROR: RX buffer overflow, clearing");
            rxBuffer_.clear();
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
        syncBoardOrientationWithPcSide();
        if (moveEdit_ != nullptr && !gameClockRunning_) {
            moveEdit_->setText(pcPlaysWhite_ ? "e2e4" : "e7e5");
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
        if (hostColorWidget_) {
            hostColorWidget_->setVisible(colorVisible);
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

    static QStringList splitWords(const QString &text)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return text.split(' ', Qt::SkipEmptyParts);
#else
        return text.split(' ', QString::SkipEmptyParts);
#endif
    }

    static bool parseUInt16Token(const QString &text, quint16 *out)
    {
        bool ok = false;
        const uint value = text.toUInt(&ok);

        if (!ok || value == 0 || value > 65535u) {
            return false;
        }
        *out = static_cast<quint16>(value);
        return true;
    }

    static quint16 newMqttSessionId()
    {
        return static_cast<quint16>(
            QRandomGenerator::global()->bounded(1, 65536));
    }

    bool mqttSessionMatches(quint16 sessionId) const
    {
        return sessionId != 0 && mqttSessionId_ != 0 &&
               sessionId == mqttSessionId_;
    }

    bool parseMqttHostPayload(const QString &payload,
                               QString *side,
                               quint16 *sessionId) const
    {
        QByteArray payloadBytes = payload.toLatin1();
        uint8_t hostColor = 0u;
        uint16_t parsedSessionId = 0u;

        if (!netchess_mqtt_session_parse_host(payloadBytes.constData(),
                                               &hostColor,
                                               &parsedSessionId)) {
            return false;
        }
        *side = QString(QChar(netchess_mqtt_session_color_char(hostColor)));
        *sessionId = static_cast<quint16>(parsedSessionId);
        return true;
    }

    bool parseMqttJoinPayload(const QString &payload, quint16 *sessionId) const
    {
        QByteArray payloadBytes = payload.toLatin1();
        uint16_t parsedSessionId = 0u;

        if (!netchess_mqtt_session_parse_join(payloadBytes.constData(),
                                               &parsedSessionId)) {
            return false;
        }
        *sessionId = static_cast<quint16>(parsedSessionId);
        return true;
    }

    bool parseMqttSideSessionPayload(const QString &payload,
                                     const QString &verb,
                                     QString *side,
                                     quint16 *sessionId) const
    {
        QByteArray payloadBytes = payload.toLatin1();
        char sideChar = '\0';
        uint16_t parsedSessionId = 0u;
        uint8_t hasSessionId = 0u;
        const char verbChar = verb.isEmpty() ? '\0' : verb.at(0).toLatin1();

        if (!netchess_mqtt_session_parse_side(payloadBytes.constData(),
                                              verbChar,
                                              &sideChar,
                                              &parsedSessionId,
                                              &hasSessionId)) {
            return false;
        }
        *side = QString(QChar(sideChar));
        *sessionId = hasSessionId ? static_cast<quint16>(parsedSessionId) : 0;
        return true;
    }

    QString mqttSetupPayload() const
    {
        char payload[32];

        if (pcIsHost_) {
            const uint8_t hostColor = hostPlaysWhite_
                ? NETCHESS_MQTT_SESSION_COLOR_WHITE
                : NETCHESS_MQTT_SESSION_COLOR_BLACK;

            if (netchess_mqtt_session_format_host(
                    payload,
                    sizeof(payload),
                    hostColor,
                    mqttSessionId_)) {
                return QString::fromLatin1(payload);
            }
            return QString();
        }
        if (netchess_mqtt_session_format_join(
                payload,
                sizeof(payload),
                mqttSessionId_)) {
            return QString::fromLatin1(payload);
        }
        return QString();
    }

    QString topicFor(const QString &suffix) const
    {
        return QString("netchesszx/v1/%1/%2").arg(mqttRoom_, suffix);
    }

    QString mqttOutSuffix() const
    {
        return pcPlaysWhite_ ? "w2b" : "b2w";
    }

    QString mqttInSuffix() const
    {
        return pcPlaysWhite_ ? "b2w" : "w2b";
    }

    QString mqttOutAckSuffix() const
    {
        return pcPlaysWhite_ ? "ack_b" : "ack_w";
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

    void activateMqttSide()
    {
        if (!mqttConnected_ || !mqttSubscribed_) {
            return;
        }
        if (!mqttSideReady_) {
            if (!mqttSubscribe(mqttInSuffix()) ||
                !mqttSubscribe(mqttInAckSuffix()) ||
                !mqttSubscribe(mqttPeerPresenceSuffix())) {
                appendLog("ERROR: MQTT side subscribe failed");
                socket_->abort();
                return;
            }
            mqttSideReady_ = true;
            mqttPublish(mqttPresenceSuffix(),
                        QString("O %1 %2").arg(pcSideLetter()).arg(mqttSessionId_),
                        true);
            announceMqttSetup();
        }
    }

    uint16_t mqttPacketId()
    {
        if (mqttNextPacketId_ == 0) {
            mqttNextPacketId_ = 1;
        }
        return mqttNextPacketId_++;
    }

    bool writeMqttPacket(const uint8_t *data, size_t len, const QString &label)
    {
        if (len == 0) {
            appendLog("ERROR: MQTT encode failed: " + label);
            return false;
        }
        const auto written = socket_->write(reinterpret_cast<const char *>(data),
                                           static_cast<qint64>(len));
        if (written < 0) {
            appendLog("ERROR: MQTT write failed: " + socket_->errorString());
            return false;
        }
        if (written != static_cast<qint64>(len)) {
            appendLog(QString("ERROR: MQTT partial write %1/%2")
                          .arg(written)
                          .arg(static_cast<qint64>(len)));
            return false;
        }
        socket_->flush();
        appendLog("MQTT TX: " + label);
        return true;
    }

    void mqttHandshake()
    {
        uint8_t packet[NETCHESSZX_MQTT_PACKET_MAX];
        const QByteArray clientId =
            QString("SHATRANJ-CLIENT-%1-%2").arg(mqttRoom_, pcSideLetter()).toLatin1();
        QByteArray willTopic;
        QByteArray willPayload;
        const bool useWill = pcIsHost_;
        const bool willRetain = pcIsHost_;

        if (pcIsHost_) {
            willTopic = topicFor(mqttPresenceSuffix()).toLatin1();
            willPayload = QString("F %1 %2")
                              .arg(pcSideLetter())
                              .arg(mqttSessionId_)
                              .toLatin1();
        }
        const size_t len = netchess_mqtt_encode_connect(packet,
                                                        sizeof(packet),
                                                        clientId.constData(),
                                                        45,
                                                        useWill ? willTopic.constData() : nullptr,
                                                        useWill ? willPayload.constData() : nullptr,
                                                        willRetain ? 1 : 0);
        appendLog("CONNECTED TCP MQTT");
        setStatusText(NETCHESSZX_UI_NOTICE_MQTT_CONNECTING);
        (void)writeMqttPacket(packet, len, "CONNECT");
    }

    bool mqttSubscribe(const QString &suffix)
    {
        uint8_t packet[NETCHESSZX_MQTT_PACKET_MAX];
        const QByteArray topic = topicFor(suffix).toLatin1();
        const uint16_t id = mqttPacketId();
        const size_t len = netchess_mqtt_encode_subscribe(packet,
                                                          sizeof(packet),
                                                          id,
                                                          topic.constData(),
                                                          1);
        return writeMqttPacket(packet, len, "SUB " + topicFor(suffix));
    }

    bool mqttPublish(const QString &suffix, const QString &payload, bool retain = false)
    {
        uint8_t packet[NETCHESSZX_MQTT_PACKET_MAX];
        const QByteArray topic = topicFor(suffix).toLatin1();
        const QByteArray body = payload.toLatin1();
        const uint16_t id = mqttPacketId();
        const size_t len = netchess_mqtt_encode_publish(packet,
                                                        sizeof(packet),
                                                        id,
                                                        topic.constData(),
                                                        reinterpret_cast<const uint8_t *>(body.constData()),
                                                        static_cast<uint16_t>(body.size()),
                                                        1,
                                                        retain ? 1 : 0);
        return writeMqttPacket(packet, len, "PUB " + suffix + " " + payload);
    }

    void publishMqttOfflineIfReady()
    {
        bool wrote = false;

        if (!isMqttMode() || !mqttConnected_ || !mqttSubscribed_) {
            return;
        }
        if (mqttSideReady_) {
            wrote = mqttPublish(mqttPresenceSuffix(),
                                QString("F %1 %2")
                                    .arg(pcSideLetter())
                                    .arg(mqttSessionId_),
                                true) || wrote;
        }
        if (pcIsHost_) {
            wrote = mqttPublish("meta", QString(), true) || wrote;
        }
        if (wrote) {
            socket_->flush();
            (void)socket_->waitForBytesWritten(250);
        }
    }

    bool sendMqttText(const QString &text)
    {
        if (!mqttConnected_ || !mqttSubscribed_ || !mqttSideReady_) {
            appendLog("ERROR: MQTT not ready");
            return false;
        }
        if (text == "ACK GAME START") {
            return mqttPublish("meta", text, false);
        }
        if (text == "BYE" || text == "RESET" || text == "ACK RESET" || text.startsWith("NACK RESET")) {
            return mqttPublish(mqttOutSuffix(), text, false);
        }
        if (text.startsWith("ACK ")) {
            return mqttPublish(mqttOutAckSuffix(), text);
        }
        if (text == "GAME START" || text.startsWith("GAME START ")) {
            return mqttPublish("meta", text, false);
        }
        return mqttPublish(mqttOutSuffix(), text);
    }

    static QString topicSuffix(const QString &topic)
    {
        const int slash = topic.lastIndexOf('/');
        return slash >= 0 ? topic.mid(slash + 1) : topic;
    }

    void consumeMqttBytes(const QByteArray &data)
    {
        mqttParser_.append(data);
        while (true) {
            const int packetLen = mqttAvailablePacketLength();
            if (packetLen == 0) {
                return;
            }
            if (packetLen < 0) {
                appendLog("ERROR: malformed MQTT packet");
                socket_->abort();
                return;
            }
            const QByteArray packet = mqttParser_.left(packetLen);
            mqttParser_.remove(0, packetLen);
            handleMqttPacket(packet);
        }
    }

    int mqttAvailablePacketLength() const
    {
        if (mqttParser_.size() < 2) {
            return 0;
        }
        int multiplier = 1;
        int remaining = 0;
        int used = 0;
        for (int i = 1; i < mqttParser_.size() && i < 5; ++i) {
            const auto encoded = static_cast<unsigned char>(mqttParser_.at(i));
            remaining += (encoded & 0x7f) * multiplier;
            multiplier *= 128;
            used = i;
            if ((encoded & 0x80) == 0) {
                const int total = used + 1 + remaining;
                if (total > static_cast<int>(NETCHESSZX_MQTT_PACKET_MAX)) {
                    return -1;
                }
                return mqttParser_.size() >= total ? total : 0;
            }
        }
        return mqttParser_.size() >= 5 ? -1 : 0;
    }

    void handleMqttPacket(const QByteArray &raw)
    {
        netchess_mqtt_packet_t packet{};
        netchess_mqtt_parser_t parser{};
        netchess_mqtt_parser_init(&parser);
        for (unsigned char byte : raw) {
            const int rc = netchess_mqtt_parser_feed(&parser, byte, &packet);
            if (rc == NETCHESSZX_MQTT_ERROR) {
                appendLog("ERROR: MQTT parse failed");
                return;
            }
            if (rc == NETCHESSZX_MQTT_NEED_MORE) {
                continue;
            }
        }

        if (packet.type == NETCHESS_MQTT_CONNACK) {
            if (packet.return_code != 0) {
                failMqttConnection(QString("MQTT CONNACK %1").arg(packet.return_code));
                return;
            }
            mqttConnected_ = true;
            mqttSubscribed_ = false;
            mqttSideReady_ = false;
            mqttSubacksPending_ = pcIsHost_ ? 4 : 1;
            if ((pcIsHost_ &&
                 (!mqttSubscribe(mqttInSuffix()) ||
                  !mqttSubscribe(mqttInAckSuffix()) ||
                  !mqttSubscribe(mqttPeerPresenceSuffix()))) ||
                !mqttSubscribe("meta")) {
                failMqttConnection("MQTT subscribe send failed");
                return;
            }
            setStatusText(NETCHESSZX_UI_NOTICE_MQTT_SUBSCRIBING);
            return;
        }

        if (packet.type == NETCHESS_MQTT_SUBACK) {
            if (packet.return_code == 0x80) {
                failMqttConnection(QString("MQTT SUBACK failed id=%1").arg(packet.packet_id));
                return;
            }
            if (mqttSubacksPending_ > 0) {
                --mqttSubacksPending_;
            }
            if (mqttSubacksPending_ != 0) {
                setStatusText(QString("MQTT subscribing... %1").arg(mqttSubacksPending_));
                return;
            }
            if (mqttSubscribed_) {
                return;
            }
            mqttSubscribed_ = true;
            mqttSideReady_ = pcIsHost_;
            peerActivity_.restart();
            mqttSetupActivity_.restart();
            setConnectedUi(true);
            setStatusText(pcIsHost_ ? "MQTT link established - waiting opponent"
                                    : "MQTT link established - waiting host");
            appendLog("MQTT READY");
            if (pcIsHost_) {
                mqttPublish(mqttPresenceSuffix(),
                            QString("O %1 %2")
                                .arg(pcSideLetter())
                                .arg(mqttSessionId_),
                            true);
                announceMqttSetup();
            } else {
                announceMqttSetup();
            }
            return;
        }

        if (packet.type == NETCHESS_MQTT_PUBLISH) {
            const QString topic = QString::fromLatin1(packet.topic);
            const QString suffix = topicSuffix(topic);
            const QString payload = QString::fromLatin1(
                reinterpret_cast<const char *>(packet.payload),
                packet.payload_len).trimmed();
            appendLog(QString("MQTT RX %1%2: %3")
                          .arg(topic,
                               packet.retain ? QString(" [retained]") : QString(),
                               payload));
            if (packet.packet_id != 0) {
                uint8_t ack[4];
                writeMqttPacket(ack,
                                netchess_mqtt_encode_puback(ack, sizeof(ack), packet.packet_id),
                                QString("PUBACK %1").arg(packet.packet_id));
            }
            const bool peerTopic = suffix == mqttInSuffix() ||
                                   suffix == mqttPeerPresenceSuffix();
            if (peerTopic && packet.retain == 0u) {
                peerActivity_.restart();
                mqttPeerPingOutstanding_ = false;
            }
            if (suffix == mqttOutSuffix() ||
                suffix == mqttOutAckSuffix()) {
                appendLog("DROP own-topic echo: " + payload);
                return;
            }
            handleMqttPayload(suffix, payload, packet.retain != 0u);
            return;
        }
    }

    void handleMqttPayload(const QString &suffix,
                           const QString &payload,
                           bool retained = false)
    {
        if (suffix == mqttPeerPresenceSuffix()) {
            if (!handleMqttSessionPayload(payload, retained)) {
                appendLog("IGNORE non-presence payload: " + payload);
            }
            return;
        }

        if (suffix == "meta") {
            if (handleMqttSessionPayload(payload, retained)) {
                return;
            }
            if (payload == "GAME START") {
                if (retained) {
                    appendLog("IGNORE retained GAME START");
                    return;
                }
                if (pcIsHost_) {
                    appendLog("IGNORE own GAME START echo");
                    return;
                }
                if (!mqttPeerReady_ || !mqttSideReady_ || mqttSessionId_ == 0) {
                    appendLog("DROP GAME START before MQTT host setup");
                    setStatusText(NETCHESSZX_UI_PHASE_WAITING_HOST);
                    return;
                }
                peerActivity_.restart();
                mqttPeerPingOutstanding_ = false;
                handleRxLine(payload);
                return;
            }
            if (payload == "ACK GAME START") {
                if (!retained) {
                    handleRxLine(payload);
                }
                return;
            }
            if (retained) {
                appendLog("IGNORE retained meta: " + payload);
                return;
            }
            appendLog("IGNORE meta payload: " + payload);
            return;
        }

        if (suffix != mqttInSuffix() && suffix != mqttInAckSuffix()) {
            appendLog("IGNORE MQTT topic " + suffix + ": " + payload);
            return;
        }

        if (handleMqttSessionPayload(payload, retained)) {
            return;
        }
        if (retained) {
            appendLog("IGNORE retained control: " + payload);
            return;
        }
        handleRxLine(payload);
    }

    bool handleMqttSessionPayload(const QString &payload, bool retained)
    {
        if (payload.isEmpty()) {
            return true;
        }
        if (payload.startsWith("O ")) {
            QString side;
            quint16 sessionId = 0;

            if (parseMqttSideSessionPayload(payload, "O", &side, &sessionId) &&
                side == opponentSideLetter() &&
                (sessionId == 0 || mqttSessionMatches(sessionId))) {
                if (!mqttPeerReady_) {
                    setStatusText(pcIsHost_ ? "Opponent online - waiting join"
                                            : "Host online - waiting host");
                }
                setConnectedUi(true);
            }
            return true;
        }
        if (payload.startsWith("F ")) {
            QString side;
            quint16 sessionId = 0;

            if (retained) {
                return true;
            }
            if (parseMqttSideSessionPayload(payload, "F", &side, &sessionId) &&
                side == opponentSideLetter() &&
                (sessionId == 0 || mqttSessionMatches(sessionId))) {
                mqttPeerReady_ = false;
                mqttPeerPingOutstanding_ = false;
                resetGame(NETCHESSZX_UI_ERROR_CONNECTION_LOST);
                clearChatLog();
                appendLog(NETCHESSZX_UI_ERROR_CONNECTION_LOST);
                setConnectedUi(true);
            }
            return true;
        }
        if (payload.startsWith("H ")) {
            QString side;
            quint16 sessionId = 0;

            if (!parseMqttHostPayload(payload, &side, &sessionId)) {
                appendLog("IGNORE bad HOST: " + payload);
                return true;
            }
            if (retained) {
                appendLog("IGNORE retained HOST setup: " + payload);
                if (!pcIsHost_) {
                    setStatusText(NETCHESSZX_UI_PHASE_WAITING_HOST);
                    setConnectedUi(true);
                }
                return true;
            }
            if (pcIsHost_) {
                /* A HOST setup from a different session means another player is
                   hosting; our own echo carries our own session. */
                if (!mqttSessionMatches(sessionId)) {
                    mqttPeerReady_ = false;
                    setStatusText(retained ? "Room already has host" :
                                             "Host conflict - disconnect, then select Guest");
                    appendLog(QString("HOST CONFLICT%1: %2")
                                  .arg(retained ? " [retained]" : "", payload));
                    setConnectedUi(true);
                }
            } else {
                if (mqttSessionId_ != 0 && !mqttSessionMatches(sessionId)) {
                    if (gameClockRunning_) {
                        appendLog("IGNORE stale HOST session: " + payload);
                        return true;
                    }
                    mqttPeerReady_ = false;
                }
                mqttSessionId_ = sessionId;
                if (side == "W" || side == "B") {
                    const bool oldPcWhite = pcPlaysWhite_;

                    hostPlaysWhite_ = side == "W";
                    pcPlaysWhite_ = !hostPlaysWhite_;
                    if (moveEdit_ != nullptr && !gameClockRunning_) {
                        moveEdit_->setText(pcPlaysWhite_ ? "e2e4" : "e7e5");
                    }
                    syncBoardOrientationWithPcSide();
                    if (oldPcWhite != pcPlaysWhite_) {
                        mqttSideReady_ = false;
                        refreshBoard();
                    }
                    activateMqttSide();
                } else {
                    setStatusText(NETCHESSZX_UI_ERROR_INVALID_HOST_COLOR);
                    appendLog("ERROR: invalid host color in " + payload);
                    return true;
                }
                mqttPeerReady_ = true;
                peerActivity_.restart();
                mqttPeerPingOutstanding_ = false;
                setStatusText(QString("Host ready (%1) - waiting start").arg(side));
                setConnectedUi(true);
            }
            return true;
        }
        if (payload.startsWith("J ")) {
            if (pcIsHost_) {
                quint16 sessionId = 0;

                if (retained) {
                    appendLog("IGNORE retained join: " + payload);
                    return true;
                }
                if (!parseMqttJoinPayload(payload, &sessionId)) {
                    appendLog("IGNORE bad join: " + payload);
                    return true;
                }
                if (!mqttSessionMatches(sessionId)) {
                    appendLog("IGNORE stale JOIN session: " + payload);
                    return true;
                }
                if (gameClockRunning_) {
                    (void)sendLine("BYE");
                    setStatusText(NETCHESSZX_UI_ERROR_GAME_ALREADY_ACTIVE);
                    return true;
                }
                mqttPublish("meta", mqttSetupPayload(), false);
                mqttPeerReady_ = true;
                peerActivity_.restart();
                mqttPeerPingOutstanding_ = false;
                setStatusText(NETCHESSZX_UI_NOTICE_OPPONENT_JOINED_START);
                setConnectedUi(true);
            }
            return true;
        }
        if (payload.startsWith("META ")) {
            return true;
        }
        return false;
    }

    void handleRxLine(const QString &line)
    {
        if (!isMqttMode()) {
            linkWatch_.restart();
        }

        if (isPingLine(line)) {
            if (!isMqttMode()) {
                directPingOutstanding_ = false;
            }
            (void)sendLine("ACK PING");
            appendLog("RX: " + line);
            return;
        }

        if (isAckPingLine(line)) {
            appendLog("RX: " + line);
            if (isMqttMode()) {
                if (mqttPeerPingOutstanding_) {
                    mqttPeerPingOutstanding_ = false;
                    peerActivity_.restart();
                } else {
                    appendLog("DROP stale MQTT ACK PING");
                }
            } else if (directPingOutstanding_) {
                directPingOutstanding_ = false;
            } else {
                appendLog("DROP stale ACK PING");
            }
            return;
        }

        appendLog("RX: " + line);

        if (!isMqttMode() && line.startsWith("HELLO DIRECT ")) {
            handleDirectHello(line);
            return;
        }
        if (!isMqttMode() && !directReady_) {
            if (isPingLine(line)) {
                (void)sendLine("ACK PING");
                return;
            }
            appendLog("DROP pre-HELLO direct payload: " + line);
            return;
        }

        if (line == "BYE") {
            appendLog("RX: opponent disconnected");
            if (isMqttMode()) {
                mqttPeerReady_ = false;
                mqttPeerPingOutstanding_ = false;
                resetGame(NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED);
                clearChatLog();
                setConnectedUi(true);
                return;
            }
            resetGame(NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED);
            clearChatLog();
            if (socket_ != nullptr) {
                ignoreNextDisconnect_ = true;
                socket_->abort();
                if (socket_->state() == QAbstractSocket::UnconnectedState) {
                    ignoreNextDisconnect_ = false;
                }
                setConnectedUi(false);
            }
            return;
        }

        if (line == "RESIGN") {
            if (!gameOver_) {
                endGameOver("RESIGN");
            }
            return;
        }

        if (line == "DRAW") {
            if (!gameClockRunning_ || resetPending_ || resetPromptOpen_) {
                (void)sendLine("NACK DRAW");
            } else if (drawPending_) {
                (void)sendLine("ACK DRAW");
                beginDrawRematch(true);
            } else {
                resetPromptOpen_ = true;
                setStatusText(NETCHESSZX_UI_EVENT_DRAW);
                QTimer::singleShot(0, this, [this]() {
                    const QMessageBox::StandardButton answer =
                        QMessageBox::question(this,
                                              NETCHESSZX_UI_CONFIRM_PC_DRAW_TITLE,
                                              NETCHESSZX_UI_CONFIRM_PC_ACCEPT_DRAW,
                                              QMessageBox::Yes | QMessageBox::No);
                    if (!resetPromptOpen_) {
                        return;
                    }
                    resetPromptOpen_ = false;
                    if (answer == QMessageBox::Yes) {
                        (void)sendLine("ACK DRAW");
                        beginDrawRematch(false);
                    } else {
                        (void)sendLine("NACK DRAW");
                        setConnectedUi(true);
                    }
                });
            }
            return;
        }

        if (line == "RESET") {
            const bool rematch = gameOver_;
            const QString previousStatus = statusMessage_;

            if (resetPending_) {
                if (rematch) {
                    resetPending_ = false;
                    (void)sendLine("ACK RESET");
                    startGameFromAck();
                } else {
                    appendLog("DROP RESET: reset already pending locally");
                    (void)sendLine("NACK RESET BUSY");
                }
                return;
            }
            if (resetPromptOpen_) {
                appendLog("DROP RESET: reset prompt already open");
                (void)sendLine("NACK RESET BUSY");
                return;
            }
            if (!gameClockRunning_ && !gameOver_) {
                appendLog("DROP RESET before game start");
                (void)sendLine("NACK RESET START");
                return;
            }
            resetPromptOpen_ = true;
            setStatusText(rematch ? NETCHESSZX_UI_CONFIRM_RESTART_REQUEST :
                                    NETCHESSZX_UI_CONFIRM_RESET_REQUEST);
            setConnectedUi(true);
            QTimer::singleShot(0, this, [this, rematch, previousStatus]() {
                const QMessageBox::StandardButton answer =
                    QMessageBox::question(this,
                                          rematch ? NETCHESSZX_UI_CONFIRM_PC_RESTART_TITLE :
                                                    NETCHESSZX_UI_CONFIRM_PC_RESET_TITLE,
                                          rematch ? NETCHESSZX_UI_CONFIRM_PC_RESTART_REQUEST :
                                                    NETCHESSZX_UI_CONFIRM_PC_RESET_REQUEST_LONG,
                                          QMessageBox::Yes | QMessageBox::No);
                if (!resetPromptOpen_) {
                    return;
                }
                resetPromptOpen_ = false;
                if (answer == QMessageBox::Yes) {
                    (void)sendLine("ACK RESET");
                    if (rematch) {
                        startGameFromAck();
                    } else {
                        resetGame("Game reset by opponent", true);
                    }
                } else {
                    (void)sendLine("NACK RESET");
                    setStatusText(rematch ? previousStatus : "RESET rejected");
                    setConnectedUi(true);
                }
            });
            return;
        }

        if (pendingPly_ != 0 && ackLineMatches(line, pendingPly_)) {
            applyPendingMove();
            return;
        }

        if (line.startsWith("NACK ")) {
            if (line.startsWith("NACK DRAW")) {
                if (drawPending_) {
                    drawPending_ = false;
                    setStatusText(NETCHESSZX_UI_ERROR_DRAW_REJECTED);
                    setConnectedUi(true);
                }
                return;
            }
            if (line.startsWith("NACK RESET")) {
                appendLog("ERROR: reset rejected by opponent: " + line);
                if (resetPending_) {
                    resetPending_ = false;
                    setStatusText(gameOver_
                        ? NETCHESSZX_UI_ERROR_RESTART_REJECTED
                        : NETCHESSZX_UI_ERROR_RESET_REJECTED);
                    setConnectedUi(true);
                }
                return;
            }
            if (line.startsWith("NACK GAME START")) {
                appendLog("ERROR: start rejected by opponent: " + line);
                startPending_ = false;
                setStatusText(NETCHESSZX_UI_ERROR_START_REJECTED_BY_OPPONENT);
                setConnectedUi(true);
                return;
            }
            handleNack(line);
            return;
        }

        if (line.startsWith("MOVE ")) {
            handleRemoteMove(line);
            return;
        }

        if ((isMqttMode() && line == "GAME START") ||
            (!isMqttMode() && line.startsWith("GAME START"))) {
            startGameFromOpponent(line);
            return;
        }

        if (line.startsWith("CHAT ")) {
            QByteArray lineBytes = line.toLatin1();
            char chat[NETCHESSZX_MQTT_PAYLOAD_MAX];

            if (netchess_proto_parse_chat(lineBytes.constData(),
                                          chat,
                                          sizeof(chat))) {
                appendChat(opponentChatName(), QString::fromLatin1(chat));
            } else {
                appendLog("ERROR: bad CHAT line");
            }
            return;
        }

        if (line == "ACK GAME START") {
            if (!isMqttMode() && (!directReady_ || !startPending_ || !pcIsHost_)) {
                appendLog("IGNORE ACK GAME START without pending direct start");
            } else if (!gameClockRunning_ && startPending_) {
                startGameFromAck();
            } else if (!startPending_) {
                appendLog("IGNORE ACK GAME START without pending start");
            }
            return;
        }

        if (line == "ACK RESET") {
            if (resetPending_) {
                if (gameOver_) {
                    resetPending_ = false;
                    startGameFromAck();
                } else {
                    resetGame(pcIsHost_ ? "Reset confirmed - press Start Game" :
                                          "Reset confirmed - waiting start",
                              true);
                }
                setConnectedUi(true);
            } else {
                appendLog("IGNORE ACK RESET without pending reset");
            }
        } else if (line == "ACK DRAW") {
            if (drawPending_) {
                beginDrawRematch(true);
            }
        } else if (line.startsWith("ACK ")) {
            setStatusText(NETCHESSZX_UI_NOTICE_REPLY_RECEIVED);
        }
    }

    void endGameOver(const QString &message)
    {
        drawPending_ = false;
        resetPending_ = false;
        resetPromptOpen_ = false;
        startPending_ = false;
        pendingPly_ = 0;
        pendingMove_.clear();
        gameOver_ = true;
        pcTurn_ = false;
        stopGameClock();
        appendLog(message);
        setStatusText(message);
        setConnectedUi(true);
    }

    void beginDrawRematch(bool sendReset)
    {
        endGameOver("DRAW");
        resetPending_ = true;
        if (sendReset && !sendLine("RESET")) {
            resetPending_ = false;
        }
        setConnectedUi(true);
    }

    void handleDirectHello(const QString &line)
    {
        const QByteArray bytes = line.toLatin1();
        const char *payload = bytes.constData();

        if (netchess_direct_parse_guest_hello(payload)) {
            if (!pcIsHost_) {
                appendLog("ERROR: direct guest conflict");
                setStatusText(NETCHESSZX_UI_ERROR_GUEST_CONFLICT);
                socket_->abort();
                return;
            }
            directReady_ = true;
            directHelloRetries_ = 0;
            directPingOutstanding_ = false;
            linkWatch_.restart();
            // Spectrum guest can miss the host HELLO sent at TCP connect (UART
            // contention during its connect-time GUI work). Echo the host HELLO
            // on EVERY guest HELLO: the guest re-announces on a timer only while
            // not ready, so this is bounded retransmission, not a ping-pong.
            // Once the guest is ready it stops sending HELLO (it replies only on
            // the ready transition), so these echoes stop on their own.
            sendDirectHello();
            if (!gameClockRunning_ && !startPending_) {
                setStatusText(NETCHESSZX_UI_NOTICE_OPPONENT_READY_START);
                setConnectedUi(true);
            }
            return;
        }

        uint8_t whiteOwner = 0u;
        if (netchess_direct_parse_host_hello(payload, &whiteOwner)) {
            if (pcIsHost_) {
                appendLog("ERROR: direct host conflict");
                setStatusText(NETCHESSZX_UI_ERROR_HOST_CONFLICT_OPPONENT_HOST);
                socket_->abort();
                return;
            }
            pcPlaysWhite_ = (whiteOwner == NETCHESS_DIRECT_WHITE_OWNER_GUEST);
            syncBoardOrientationWithPcSide();
            directReady_ = true;
            directHelloRetries_ = 0;
            directPingOutstanding_ = false;
            linkWatch_.restart();
            // Echo our guest HELLO on every host HELLO. The peer host re-announces
            // on a timer only while not ready and replies only on its ready
            // transition, so this is bounded retransmission, not a ping-pong.
            sendDirectHello();
            if (!gameClockRunning_ && !startPending_) {
                setStatusText(NETCHESSZX_UI_NOTICE_OPPONENT_READY_WAIT_START);
                setConnectedUi(true);
            }
            return;
        }

        appendLog("ERROR: bad direct HELLO");
        setStatusText(NETCHESSZX_UI_ERROR_BAD_DIRECT_HELLO);
        (void)sendLine("BYE");
        socket_->abort();
    }

    void handleNack(const QString &line)
    {
        bool ok = false;
        const int ply = nackLinePly(line, &ok);
        const QString reason = line.section(' ', 2);

        if (!ok) {
            appendLog("ERROR: bad NACK line");
            return;
        }
        if (pendingPly_ != 0 && ply == pendingPly_) {
            appendLog(QString("DROP pending MOVE %1 after NACK %2")
                          .arg(pendingMove_, reason));
            pendingPly_ = 0;
            pendingMove_.clear();
            pcTurn_ = true;
            setStatusText(reason.isEmpty() ? "Move rejected by opponent"
                                           : QString("Move rejected: %1").arg(reason));
            setConnectedUi(true);
            return;
        }
        appendLog(QString("IGNORE NACK %1 without matching pending move").arg(ply));
    }

    bool applyMoveToBoardCells(const QString &move, bool *moverWhite = nullptr)
    {
        const int fromCol = move[0].unicode() - 'a';
        const int fromRow = '8' - move[1].unicode();
        const int toCol = move[2].unicode() - 'a';
        const int toRow = '8' - move[3].unicode();

        char piece = board_[fromRow][fromCol];
        if (piece == '.') {
            return false;
        }
        if (moverWhite != nullptr) {
            *moverWhite = isWhitePiece(piece);
        }
        const bool castle = lowerPiece(piece) == 'k' &&
                            fromCol == 4 && (toCol == 6 || toCol == 2);
        const bool enPassant = lowerPiece(piece) == 'p' &&
                               fromCol != toCol &&
                               board_[toRow][toCol] == '.';
        if (move.size() == 5) {
            const char promo = move[4].toLatin1();
            piece = (piece >= 'A' && piece <= 'Z') ?
                static_cast<char>(promo - 'a' + 'A') : promo;
        }

        board_[toRow][toCol] = piece;
        board_[fromRow][fromCol] = '.';
        if (enPassant) {
            board_[fromRow][toCol] = '.';
        }
        if (castle) {
            if (toCol == 6) {
                board_[fromRow][5] = board_[fromRow][7];
                board_[fromRow][7] = '.';
            } else {
                board_[fromRow][3] = board_[fromRow][0];
                board_[fromRow][0] = '.';
            }
        }
        return true;
    }

    bool applyMoveToBoard(const QString &move, QString *checkSuffix = nullptr)
    {
        const QByteArray moveBytes = move.toLatin1();
        const int legalRc = netchesszx_rules_play(moveBytes.constData());
        if (legalRc != NETCHESSZX_OK) {
            appendLog(QString("ERROR: move rejected by rules: %1 (%2)")
                          .arg(move, QString::fromLatin1(netchesszx_error_string(legalRc))));
            setStatusText(NETCHESSZX_UI_ERROR_RULES_REJECTED_MOVE);
            return false;
        }

        bool moverWhite = false;
        if (!applyMoveToBoardCells(move, &moverWhite)) {
            setStatusText(NETCHESSZX_UI_ERROR_BOARD_REJECTED_MOVE);
            return false;
        }
        if (checkSuffix != nullptr) {
            *checkSuffix = checkSuffixAfterMove(moverWhite);
        }
        clearSelection();
        selectedLabel_->setText("Selected: none");
        refreshBoard();
        return true;
    }

    QString previewNotationForMove(const QString &move)
    {
        const QString notationBase = moveNotationBase(move);
        QByteArray rulesState;
        char boardState[8][8];
        QString suffix;
        bool moverWhite = false;

        rulesState.resize(static_cast<int>(netchesszx_rules_state_size()));
        if (rulesState.isEmpty()) {
            return notationBase;
        }
        if (netchesszx_rules_save(rulesState.data(),
                                  static_cast<size_t>(rulesState.size())) != NETCHESSZX_OK) {
            return notationBase;
        }
        std::memcpy(boardState, board_, sizeof(boardState));

        const QByteArray moveBytes = move.toLatin1();
        if (netchesszx_rules_play(moveBytes.constData()) == NETCHESSZX_OK &&
            applyMoveToBoardCells(move, &moverWhite)) {
            suffix = checkSuffixAfterMove(moverWhite);
        }

        std::memcpy(board_, boardState, sizeof(board_));
        (void)netchesszx_rules_restore(rulesState.constData(),
                                       static_cast<size_t>(rulesState.size()));
        return notationBase + suffix;
    }

    void finishAppliedMove(int ply, const QString &move, const QString &notation,
                           bool nextPcTurn, const QString &normalStatus)
    {
        lastMove_ = move;
        appendMoveRecord(ply, move, notation);
        pendingPly_ = 0;
        pendingMove_.clear();
        nextPly_ = ply + 1;
        if (notation.contains('#')) {
            const QString message = nextPcTurn ? QStringLiteral("CHECK MATE: YOU LOST")
                                               : QStringLiteral("CHECK MATE: YOU WON");
            gameOver_ = true;
            gameCheck_ = false;
            pcTurn_ = false;
            stopGameClock();
            appendLog(message + QString(" (%1)").arg(notation));
            setStatusText(message);
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

    void applyRemoteMoveAnimated(int ply, const QString &move)
    {
        const int fromCol = move[0].unicode() - 'a';
        const int fromRow = '8' - move[1].unicode();
        const int toCol = move[2].unicode() - 'a';
        const int toRow = '8' - move[3].unicode();
        const QByteArray moveBytes = move.toLatin1();
        const int legalRc = netchesszx_rules_can_play(moveBytes.constData());
        const QString notationBase = moveNotationBase(move);

        if (legalRc != NETCHESSZX_OK) {
            appendLog(QString("ERROR: move rejected by rules: %1 (%2)")
                          .arg(move, QString::fromLatin1(netchesszx_error_string(legalRc))));
            setStatusText(NETCHESSZX_UI_ERROR_RULES_REJECTED_MOVE);
            (void)sendLine(QString("NACK %1 ILLEGAL").arg(ply));
            return;
        }

        ++pieceFlashGeneration_;
        flashPieceAt(fromRow, fromCol, [this, ply, move, toRow, toCol, notationBase]() {
            QString checkSuffix;
            if (!applyMoveToBoard(move, &checkSuffix)) {
                (void)sendLine(QString("NACK %1 ILLEGAL").arg(ply));
                return;
            }

            const QString notation = notationBase + checkSuffix;
            finishAppliedMove(ply, move, notation, true,
                              QString("Opponent move %1 - your move").arg(notation));

            flashPieceAt(toRow, toCol, [this, ply, notation]() {
                (void)sendLine(QString("ACK %1 %2").arg(ply).arg(notation));
            });
        });
    }

    void applyPendingMove()
    {
        const QString notationBase = moveNotationBase(pendingMove_);
        const QString move = pendingMove_;
        QString checkSuffix;

        if (!applyMoveToBoard(move, &checkSuffix)) {
            pendingPly_ = 0;
            pendingMove_.clear();
            pcTurn_ = false;
            setConnectedUi(true);
            return;
        }
        const int ackPly = pendingPly_;
        const QString notation = notationBase + checkSuffix;
        finishAppliedMove(ackPly, move, notation, false,
                          QString("Move %1 confirmed - opponent to move").arg(notation));
    }

    void startGameFromAck()
    {
        ++pieceFlashGeneration_;
        ++feedbackGeneration_;
        resetBoard();
        netchesszx_rules_reset();
        pendingPly_ = 0;
        pendingMove_.clear();
        startPending_ = false;
        nextPly_ = 1;
        gameOver_ = false;
        gameCheck_ = false;
        pcTurn_ = pcPlaysWhite_;
        lastMove_.clear();
        clearMoveHistory();
        boardPiecesVisible_ = false;
        clearSelection();
        moveEdit_->setText(pcPlaysWhite_ ? "e2e4" : "e7e5");
        selectedLabel_->setText("Selected: none");
        refreshBoard();
        startGameClock();
        animateBoardPiecesIn();
        setStatusText(NETCHESSZX_UI_NOTICE_GAME_STARTED_WHITE);
        setConnectedUi(true);
    }

    void startGameFromOpponent(const QString &line)
    {
        if (!isMqttMode()) {
            if (pcIsHost_) {
                appendLog("DROP GAME START from guest");
                setStatusText(NETCHESSZX_UI_ERROR_START_IGNORED_LOCAL_HOST);
                (void)sendLine("NACK GAME START HOST");
                return;
            }
            if (!directReady_) {
                appendLog("DROP GAME START before direct HELLO");
                setStatusText(NETCHESSZX_UI_NOTICE_WAITING_OPPONENT_APP);
                (void)sendLine("NACK GAME START HELLO");
                return;
            }
            if (line == QStringLiteral("GAME START WHITE=GUEST")) {
                pcPlaysWhite_ = true;
            } else if (line == QStringLiteral("GAME START WHITE=HOST")) {
                pcPlaysWhite_ = false;
            } else {
                appendLog("ERROR: bad GAME START payload");
                setStatusText(NETCHESSZX_UI_ERROR_BAD_GAME_START);
                (void)sendLine("NACK GAME START BAD");
                return;
            }
            syncBoardOrientationWithPcSide();
        }
        if (isMqttMode()) {
            if (pcIsHost_) {
                appendLog("DROP GAME START from guest");
                setStatusText(NETCHESSZX_UI_ERROR_START_IGNORED_LOCAL_HOST);
                return;
            }
            if (!mqttPeerReady_ || !mqttSideReady_ || mqttSessionId_ == 0) {
                appendLog("DROP GAME START before MQTT host setup");
                setStatusText(NETCHESSZX_UI_PHASE_WAITING_HOST);
                return;
            }
        }
        if (!gameClockRunning_) {
            startGameFromAck();
            setStatusText(NETCHESSZX_UI_NOTICE_GAME_STARTED_BY_OPPONENT_WHITE);
        }
        (void)sendLine("ACK GAME START");
    }

    void handleRemoteMove(const QString &line)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
#else
        const QStringList parts = line.split(' ', QString::SkipEmptyParts);
#endif
        QByteArray lineBytes = line.toLatin1();
        char plyText[8];
        char moveText[8];

        if (!netchess_proto_parse_move(lineBytes.constData(),
                                       plyText,
                                       sizeof(plyText),
                                       moveText,
                                       sizeof(moveText),
                                       nullptr,
                                       0u)) {
            appendLog("ERROR: bad MOVE line");
            if (parts.size() >= 2) {
                (void)sendLine(QString("NACK %1 BAD").arg(parts[1]));
            }
            return;
        }

        bool okPly = false;
        const int ply = QString::fromLatin1(plyText).toInt(&okPly);
        const QString move = QString::fromLatin1(moveText).toLower();
        const bool okSyntax = isMoveSyntaxOk(move);

        if (!gameClockRunning_) {
            appendLog("ERROR: remote MOVE before game start");
            (void)sendLine(QString("NACK %1 START").arg(plyText));
            return;
        }

        if (!okPly || ply <= 0) {
            appendLog("ERROR: bad remote MOVE payload");
            return;
        }
        if (!okSyntax) {
            appendLog("ERROR: bad remote MOVE syntax");
            (void)sendLine(QString("NACK %1 SYNTAX").arg(ply));
            return;
        }
        if (resetPending_ || drawPending_ || resetPromptOpen_) {
            appendLog(QString("ERROR: remote MOVE %1 during reset handshake").arg(ply));
            (void)sendLine(QString("NACK %1 BUSY").arg(ply));
            return;
        }

        if (ply < nextPly_) {
            appendLog(QString("DROP: stale remote MOVE ply %1, expected %2")
                          .arg(ply).arg(nextPly_));
            (void)sendLine(QString("ACK %1").arg(ply));
            return;
        }

        if (pendingPly_ != 0) {
            if (ply == pendingPly_ + 1) {
                appendLog(QString("RECOVER: MOVE %1 implies ACK %2")
                              .arg(ply).arg(pendingPly_));
                applyPendingMove();
            }
        }

        if (pendingPly_ != 0) {
            appendLog(QString("ERROR: remote MOVE %1 while waiting ACK %2")
                          .arg(ply).arg(pendingPly_));
            (void)sendLine(QString("NACK %1 BUSY").arg(ply));
            return;
        }

        if (pcTurn_) {
            appendLog(QString("ERROR: remote MOVE %1 while local turn").arg(ply));
            (void)sendLine(QString("NACK %1 TURN").arg(ply));
            return;
        }

        if (ply != nextPly_) {
            appendLog(QString("ERROR: remote MOVE ply %1, expected %2")
                          .arg(ply).arg(nextPly_));
            (void)sendLine(QString("NACK %1 SYNC").arg(ply));
            return;
        }

        applyRemoteMoveAnimated(ply, move);
    }

    static bool isMoveSyntaxOk(const QString &move)
    {
        if (move.size() != 4 && move.size() != 5) {
            return false;
        }
        if (move[0] < 'a' || move[0] > 'h' ||
            move[2] < 'a' || move[2] > 'h') {
            return false;
        }
        if (move[1] < '1' || move[1] > '8' ||
            move[3] < '1' || move[3] > '8') {
            return false;
        }
        if (move.size() == 4) {
            return true;
        }
        return move[4] == 'q' || move[4] == 'r' ||
               move[4] == 'b' || move[4] == 'n';
    }

    static bool isMqttRoomSyntaxOk(const QString &room)
    {
        if (room.isEmpty() || room.size() > 6) {
            return false;
        }
        for (const QChar ch : room) {
            const ushort c = ch.unicode();
            if ((c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9')) {
                continue;
            }
            return false;
        }
        return true;
    }

    static bool isDirectIpSyntaxOk(const QString &host)
    {
        QHostAddress address;

        return address.setAddress(host) &&
               address.protocol() == QAbstractSocket::IPv4Protocol &&
               !address.isNull();
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

    void generateHostRoomIfNeeded()
    {
        if (!roomEdit_ || !isMqttMode() || !pcIsHost_ ||
            isConnected() || isConnecting()) {
            return;
        }
        roomEdit_->setText(generateMqttRoomCode());
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

    static constexpr qint64 kMqttPeerPingIdleMs = 5000;
    static constexpr qint64 kMqttPeerTimeoutMs = 12000;
    static constexpr qint64 kDirectHandshakeRetryMs = 3000;
    static constexpr qint64 kDirectPingIdleMs = 3000;
    static constexpr qint64 kDirectPingTimeoutMs = 15000;

    void startGameClock()
    {
        gameClockRunning_ = true;
        gameTimer_.restart();
        moveTimer_.restart();
        updateClockLabels();
    }

    void stopGameClock()
    {
        gameClockRunning_ = false;
        updateClockLabels();
    }

    void restartMoveClock()
    {
        if (gameClockRunning_) {
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
        setLabelText(gameClockLabel_, "GAME " + formatElapsed(gameTimer_.elapsed()));
        setLabelText(moveClockLabel_, "MOVE " + formatElapsed(moveTimer_.elapsed()));
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
        if (socket_ == nullptr ||
            socket_->state() != QAbstractSocket::ConnectedState ||
            !linkWatch_.isValid()) {
            return;
        }
        if (!isMqttMode() && socket_->bytesAvailable() > 0) {
            consumeReadyRead();
            if (socket_->state() != QAbstractSocket::ConnectedState) {
                return;
            }
        }

        if (isMqttMode() && mqttSubscribed_ && !gameClockRunning_ &&
            (!mqttPeerReady_ || (!pcIsHost_ && mqttSideReady_)) &&
            mqttSetupActivity_.isValid() && mqttSetupActivity_.elapsed() > 5000) {
            announceMqttSetup();
            mqttSetupActivity_.restart();
        }

        if (isMqttMode() && mqttSubscribed_ && mqttPeerReady_ &&
            peerActivity_.isValid()) {
            const qint64 idle = peerActivity_.elapsed();

            if (mqttPeerPingOutstanding_ && idle > kMqttPeerTimeoutMs) {
                mqttPeerPingOutstanding_ = false;
                mqttPeerReady_ = false;
                appendLog("ERROR: MQTT peer timeout");
                resetGame(NETCHESSZX_UI_ERROR_CONNECTION_LOST);
                clearChatLog();
                setConnectedUi(true);
                return;
            }
            if (!mqttPeerPingOutstanding_ && idle > kMqttPeerPingIdleMs) {
                if (mqttPublish(mqttOutSuffix(), QStringLiteral("PING"), false)) {
                    mqttPeerPingOutstanding_ = true;
                }
            }
        }

        if (!isMqttMode()) {
            if (!directReady_) {
                if (linkWatch_.elapsed() > kDirectHandshakeRetryMs) {
                    if (directHelloRetries_ >= 5) {
                        appendLog("ERROR: opponent app handshake timeout");
                        setStatusText(NETCHESSZX_UI_ERROR_OPPONENT_APP_NOT_READY);
                        socket_->abort();
                        return;
                    }
                    sendDirectHello();
                    ++directHelloRetries_;
                    linkWatch_.restart();
                }
                return;
            }
            if (directPingOutstanding_) {
                if (linkWatch_.elapsed() > kDirectPingTimeoutMs) {
                    directPingOutstanding_ = false;
                    appendLog("ERROR: opponent link timeout");
                    setStatusText(NETCHESSZX_UI_ERROR_OPPONENT_LINK_TIMEOUT);
                    socket_->abort();
                }
                return;
            }
            if (linkWatch_.elapsed() > kDirectPingIdleMs) {
                if (sendLine(QStringLiteral("PING"))) {
                    directPingOutstanding_ = true;
                    linkWatch_.restart();
                } else {
                    appendLog("ERROR: direct ping failed");
                    setStatusText(NETCHESSZX_UI_ERROR_OPPONENT_LINK_TIMEOUT);
                    socket_->abort();
                }
            }
            return;
        }

        if (linkWatch_.elapsed() <= 45000) {
            return;
        }

        uint8_t packet[4];
        (void)writeMqttPacket(packet,
                              netchess_mqtt_encode_pingreq(packet, sizeof(packet)),
                              "PINGREQ");
        linkWatch_.restart();
    }

    void announceMqttSetup()
    {
        if (mqttConnected_ && mqttSubscribed_ && !gameClockRunning_) {
            if (!pcIsHost_ && mqttSessionId_ == 0) {
                return;
            }
            const QString payload = mqttSetupPayload();

            mqttPublish("meta", payload, pcIsHost_);
            if (!pcIsHost_ && mqttSideReady_) {
                mqttPublish(mqttOutSuffix(), payload, false);
            }
            ++mqttSetupAnnounces_;
        }
    }

    void refreshTurnLabel()
    {
        if (!turnLabel_ || !moveButton_) {
            return;
        }

        const bool connected = socket_ != nullptr &&
                               socket_->state() == QAbstractSocket::ConnectedState;
        QString text;
        QString style;
        if (!connected && isConnectionErrorStatus()) {
            text = "CONNECT FAILED";
            style = "QLabel { background:#743838; color:#fff0f0;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (!connected) {
            text = isConnecting() ? "CONNECTING" : "OFFLINE";
            style = isConnecting() ?
                    "QLabel { background:#252532; color:#00d7ff;"
                    " font:700 14px Segoe UI; padding:6px; }" :
                    "QLabel { background:#303040; color:#c7c7d8;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (statusMessage_.startsWith(NETCHESSZX_UI_ERROR_HOST_CONFLICT)) {
            text = "HOST CONFLICT";
            style = "QLabel { background:#743838; color:#fff0f0;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (gameOver_) {
            text = statusMessage_;
            style = "QLabel { background:#743838; color:#fff0f0;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (!gameClockRunning_) {
            if (startPending_) {
                text = "STARTING";
            } else if (resetPending_) {
                text = "RESET PENDING";
            } else if (isMqttMode() && pcIsHost_ && !mqttPeerReady_) {
                text = "WAITING OPPONENT";
            } else if (isMqttMode() && !pcIsHost_) {
                text = mqttPeerReady_ ? "WAITING HOST START" : "WAITING HOST";
            } else if (!isMqttMode() && pcIsHost_ && !directReady_) {
                text = "WAITING OPPONENT";
            } else {
                text = pcIsHost_ ? "PRESS START GAME" : "WAITING OPPONENT START";
            }
            style = "QLabel { background:#252532; color:#00d7ff;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (pendingPly_ != 0) {
            text = "MOVE SENT";
            style = "QLabel { background:#5f5534; color:#ffe99a;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else if (gameCheck_) {
            text = pcTurn_ ? "YOUR KING IN CHECK" : "OPPONENT IN CHECK";
            style = pcTurn_
                    ? "QLabel { background:#252532; color:#ffe15a;"
                      " font:700 14px Segoe UI; padding:6px; }"
                    : "QLabel { background:#ffe15a; color:#101010;"
                      " font:700 14px Segoe UI; padding:6px; }";
        } else if (pcTurn_) {
            text = QString("YOUR TURN - %1").arg(pcSideName());
            style = "QLabel { background:#36556b; color:#ffffff;"
                    " font:700 14px Segoe UI; padding:6px; }";
        } else {
            text = QString("%1 TO MOVE").arg(pcPlaysWhite_ ? "BLACK" : "WHITE");
            style = "QLabel { background:#252532; color:#00d7ff;"
                    " font:700 14px Segoe UI; padding:6px; }";
        }
        setLabelText(turnLabel_, text);
        setWidgetStyle(turnLabel_, style);

        const bool moveReady = canPcMove();
        const bool typedMoveReady = moveEdit_ != nullptr &&
                                    isMoveSyntaxOk(moveEdit_->text().trimmed().toLower());
        const bool destinationReady = moveReady && hasSelectedMoveTarget();
        const bool sendReady = moveReady && (typedMoveReady || destinationReady);
        moveButton_->setEnabled(sendReady);
        setWidgetStyle(moveButton_, moveButtonStyle(sendReady, destinationReady));
    }

    static QString moveButtonStyle(bool ready, bool destinationReady)
    {
        if (destinationReady) {
            return "QPushButton { background:#1f9d47; color:#f4fff8;"
                   " font:700 9pt Segoe UI; border:0; padding:3px 10px; }"
                   " QPushButton:hover { background:#28b956; }";
        }
        if (ready) {
            return "QPushButton { background:#36556b; color:#ffffff;"
                   " font:700 9pt Segoe UI; border:0; padding:3px 10px; }"
                   " QPushButton:hover { background:#42657e; }";
        }
        return "QPushButton { background:#303040; color:#8a8aa0;"
               " font:700 9pt Segoe UI; border:0; padding:3px 10px; }";
    }

    static QString chatButtonStyle(bool ready)
    {
        return ready
            ? "QPushButton { background:#5aa7d8; color:#08131b;"
              " font:700 9pt Segoe UI; border:0; padding:3px 10px; }"
              " QPushButton:hover { background:#6db8e7; }"
            : "QPushButton { background:#303040; color:#8a8aa0;"
              " font:700 9pt Segoe UI; border:0; padding:3px 10px; }";
    }

    static QString startButtonStyle(bool ready)
    {
        return ready
            ? "QPushButton { background:#36556b; color:#ffffff;"
              " font:700 9pt Segoe UI; border:0; padding:3px 10px; }"
              " QPushButton:hover { background:#42657e; }"
            : "QPushButton { background:#303040; color:#8a8aa0;"
              " font:700 9pt Segoe UI; border:0; padding:3px 10px; }";
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
                                     pendingPly_ == 0 &&
                                     !startPending_ &&
                                      !resetPending_ &&
                                      !drawPending_ &&
                                      !resetPromptOpen_ &&
                                     (isMqttMode() ? mqttPeerReady_ : directReady_);
        const bool startReady = startBaseReady && pcIsHost_;
        startGameButton_->setEnabled(startReady);
        setWidgetStyle(startGameButton_, startButtonStyle(startReady));
        const bool resetReady = connected && gameClockRunning_ &&
                                !startPending_ && !resetPending_ &&
                                !drawPending_ &&
                                !resetPromptOpen_ &&
                                pendingPly_ == 0;
        resetButton_->setEnabled(resetReady);
        setWidgetStyle(resetButton_, startButtonStyle(resetReady));
        if (restoreButton_ != nullptr) {
            restoreButton_->setEnabled(false);
            setWidgetStyle(restoreButton_, startButtonStyle(false));
        }
        refreshChatButton();
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
        } else if (connected && pendingPly_ == 0 && statusMessage_ == NETCHESSZX_UI_PHASE_DISCONNECTED) {
            setStatusText(pcTurn_ ? "Connected - your move" :
                                    "Connected - opponent to move");
        }
        refreshTurnLabel();
    }

    bool canStartConnection() const
    {
        const QString host = hostEdit_ ? hostEdit_->text().trimmed() : QString();

        if (isMqttMode()) {
            const QString room = roomEdit_ ? roomEdit_->text().trimmed().toUpper() :
                                             QString();
            return !host.isEmpty() && isMqttRoomSyntaxOk(room);
        }
        return pcIsHost_ || isDirectIpSyntaxOk(host);
    }

    bool canPcMove() const
    {
        return socket_->state() == QAbstractSocket::ConnectedState &&
               (!isMqttMode() || mqttSubscribed_) &&
               gameClockRunning_ &&
               pcTurn_ &&
               pendingPly_ == 0 &&
               !resetPending_ &&
               !drawPending_ &&
               !resetPromptOpen_;
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

        const QString selectedMove = squareName(selectedRow_, selectedCol_) +
                                     squareName(targetRow_, targetCol_);
        return move == selectedMove || move == selectedMove + "q";
    }

    bool canSendChat() const
    {
        if (!isConnected() ||
            chatEdit_ == nullptr ||
            chatEdit_->text().trimmed().isEmpty() ||
            resetPending_ ||
            drawPending_ ||
            resetPromptOpen_) {
            return false;
        }

        if (isMqttMode()) {
            return mqttSubscribed_ &&
                   mqttPeerReady_ &&
                   !statusMessage_.startsWith(NETCHESSZX_UI_ERROR_HOST_CONFLICT) &&
                   statusMessage_ != NETCHESSZX_UI_ERROR_CONNECTION_LOST;
        }

        return true;
    }

    void refreshChatButton()
    {
        if (chatButton_ == nullptr) {
            return;
        }
        const bool ready = canSendChat();
        chatButton_->setEnabled(ready);
        setWidgetStyle(chatButton_, chatButtonStyle(ready));
    }

    void resizeToContent()
    {
        if (QWidget *root = centralWidget()) {
            root->layout()->activate();
            const QSize contentSize = root->sizeHint();
            const QSize chromeSize(0, statusBar()->sizeHint().height());
            setFixedSize(contentSize + chromeSize);
        }
    }

    void updateConnectionModeUi()
    {
        const bool mqtt = isMqttMode();
        const bool directHost = !mqtt && pcIsHost_;

        if (hostCaptionLabel_ != nullptr) {
            hostCaptionLabel_->setText(directHost ? "LOCAL IP" : "HOST");
        }
        if (roomCaptionLabel_ != nullptr) {
            roomCaptionLabel_->setVisible(mqtt);
        }
        if (roomEdit_ != nullptr) {
            roomEdit_->setVisible(mqtt);
        }
        if (chatEdit_ != nullptr) {
            chatEdit_->setMaxLength(mqtt ? kChatTextMax : kDirectChatTextMax);
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

        resizeToContent();
    }

    static bool looksLikeMqttHost(const QString &host)
    {
        return host.contains("broker", Qt::CaseInsensitive) ||
               host.contains("mqtt", Qt::CaseInsensitive) ||
               host.contains("hivemq", Qt::CaseInsensitive) ||
               host.contains("mosquitto", Qt::CaseInsensitive);
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
        return isMqttMode() || directReady_;
    }

    bool isDirectListening() const
    {
        return directServer_ != nullptr && directServer_->isListening();
    }

    bool isConnectionErrorStatus() const
    {
        return statusMessage_.startsWith("Connection refused") ||
               statusMessage_.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
               statusMessage_ == NETCHESSZX_UI_ERROR_INVALID_IP ||
               statusMessage_ == NETCHESSZX_UI_ERROR_OPPONENT_APP_NOT_READY;
    }

    static bool statusBarTextIsError(const QString &text)
    {
        return text == NETCHESSZX_UI_PHASE_DISCONNECTED ||
               text == NETCHESSZX_UI_PHASE_CONNECTION_FAILED ||
               text == NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED ||
               text == NETCHESSZX_UI_ERROR_HOST_CONFLICT ||
               text.startsWith("CHECK MATE") ||
               text.startsWith("RESTART GAME") ||
               text.startsWith("RESTART rejected") ||
               text.startsWith("Opponent restart") ||
               text.startsWith("Waiting restart") ||
               text.startsWith("Connection refused") ||
               text.startsWith(NETCHESSZX_UI_PHASE_CONNECTION_FAILED) ||
               text.contains("disconnected", Qt::CaseInsensitive) ||
               text.contains(NETCHESSZX_UI_ERROR_INVALID_IP, Qt::CaseInsensitive) ||
               text.contains("not ready", Qt::CaseInsensitive);
    }

    static QString statusBarLabelStyle(bool error)
    {
        return error
            ? "QLabel { color:#ff5a5a; font:700 11px Segoe UI; }"
            : "QLabel { color:#00d7ff; font:700 11px Segoe UI; }";
    }

    bool isConnecting() const
    {
        if (isDirectListening()) {
            return true;
        }
        if (socket_ == nullptr) {
            return false;
        }
        if (!isMqttMode() &&
            socket_->state() == QAbstractSocket::ConnectedState &&
            !directReady_) {
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
            if (statusMessage_.startsWith(NETCHESSZX_UI_ERROR_HOST_CONFLICT)) {
                return NETCHESSZX_UI_ERROR_HOST_CONFLICT;
            }
            if (isMqttMode()) {
                if (pcIsHost_) {
                    if (!mqttPeerReady_) {
                        return NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT;
                    }
                } else {
                    return mqttPeerReady_ ? NETCHESSZX_UI_PHASE_WAITING_HOST_START : NETCHESSZX_UI_PHASE_WAITING_HOST;
                }
            }
            return statusMessage_.isEmpty() ? NETCHESSZX_UI_PHASE_OPPONENT_LINKED :
                                             statusMessage_;
        }
        if (pendingPly_ != 0) {
            return NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT;
        }
        if (pcTurn_) {
            return NETCHESSZX_UI_PHASE_YOUR_TURN;
        }
        if (statusMessage_ == NETCHESSZX_UI_PHASE_OPPONENT_LINKED ||
            statusMessage_ == NETCHESSZX_UI_PHASE_OPPONENT_LINKED) {
            return statusMessage_;
        }
        return NETCHESSZX_UI_PHASE_OPPONENT_TURN;
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
                    !isDirectIpSyntaxOk(hostEdit_ ? hostEdit_->text().trimmed() :
                                                    QString())) {
                    return endpointText() + " | Invalid IP";
                }
                return endpointText();
            }
            return endpointText() + " | " + statusMessage_;
        }

        if (!gameClockRunning_) {
            if (gameOver_) {
                return QString("Side %1 | %2 | %3")
                    .arg(pcSideName(), QString(NETCHESSZX_UI_CONTEXT_PRESS_RESTART), endpointText());
            }
            if (statusMessage_.startsWith(NETCHESSZX_UI_ERROR_HOST_CONFLICT)) {
                return QString(NETCHESSZX_UI_CONTEXT_DISCONNECT_SELECT_GUEST " | %1").arg(endpointText());
            }
            QString action = pcIsHost_ ? QString(NETCHESSZX_UI_CONTEXT_PRESS_START) :
                                         QString(NETCHESSZX_UI_PHASE_WAITING_OPPONENT_START);
            if (isMqttMode() && pcIsHost_ && !mqttPeerReady_) {
                action = NETCHESSZX_UI_PHASE_WAITING_OPPONENT_SHORT;
            } else if (isMqttMode() && !pcIsHost_) {
                action = mqttPeerReady_ ? QString(NETCHESSZX_UI_PHASE_WAITING_HOST_START) :
                                          QString(NETCHESSZX_UI_PHASE_WAITING_HOST);
            }

            QString text = QString("Side %1 | %2 | %3")
                               .arg(pcSideName(), action, endpointText());
            if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText()) {
                text += " | " + statusMessage_;
            }
            return text;
        }

        QStringList parts;
        parts << QString("Side %1").arg(pcSideName());
        parts << QString("Ply %1").arg(nextPly_);
        if (!lastMove_.isEmpty()) {
            parts << QString("Last %1").arg(lastMove_.toUpper());
        }
        if (!statusMessage_.isEmpty() && statusMessage_ != statusStateText() &&
            statusMessage_ != NETCHESSZX_UI_PHASE_WAITING_OPPONENT) {
            parts << statusMessage_;
        }
        return parts.join(" | ");
    }

    void refreshStatusBar()
    {
        const QString stateText = statusStateText();

        setWidgetStyle(statusStateLabel_,
                       statusBarLabelStyle(statusBarTextIsError(stateText)));
        setLabelText(statusStateLabel_, stateText.toUpper());
        if (statusContextLabel_ != nullptr) {
            const QString contextText = statusContextText();
            setWidgetStyle(statusContextLabel_,
                           statusBarLabelStyle(statusBarTextIsError(contextText)));
            setLabelText(statusContextLabel_, contextText.toUpper());
        }
    }

    static QString sideStatusText(const QString &text)
    {
        if (text.isEmpty() || text == NETCHESSZX_UI_PHASE_DISCONNECTED ||
            text.startsWith(NETCHESSZX_UI_PHASE_DISCONNECTED)) {
            return NETCHESSZX_UI_SIDE_CONNECT_READY;
        }
        if (text == NETCHESSZX_UI_PHASE_OPPONENT_LINKED) {
            return NETCHESSZX_UI_SIDE_LINK_OK;
        }

        QString compact = text.toUpper();
        compact.replace(" - ", " | ");
        constexpr int kMaxSideStatusChars = 72;
        if (compact.size() > kMaxSideStatusChars) {
            compact = compact.left(kMaxSideStatusChars - 3) + "...";
        }
        return compact;
    }

    void setStatusText(const QString &text)
    {
        statusMessage_ = text;
        if (statusLabel_) {
            setLabelText(statusLabel_, sideStatusText(text));
        }
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
        const QString now = QDateTime::currentDateTime().toString("HH:mm:ss");
        const QString line = QString("[%1] %2").arg(now, text);
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

    static void trimMoveRecords(QVector<MoveRecord> &records)
    {
        constexpr int kMaxRecords = 400;
        while (records.size() > kMaxRecords) {
            records.removeFirst();
        }
    }

    void appendMoveRecord(int ply, const QString &move, const QString &notation)
    {
        moveHistoryRecords_.append(MoveRecord{ply, move, notation});
        trimMoveRecords(moveHistoryRecords_);
        if (showingMoveHistory_ && logEdit_ != nullptr) {
            renderLogView();
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

    void renderMoveHistoryTable()
    {
        if (moveTable_ == nullptr) {
            return;
        }
        if (moveHistoryRecords_.isEmpty()) {
            moveTable_->setRowCount(0);
            return;
        }

        QHash<int, MoveRecord> whiteMoves;
        QHash<int, MoveRecord> blackMoves;
        int lastMoveNumber = 0;

        for (const MoveRecord &record : moveHistoryRecords_) {
            const int moveNumber = (record.ply + 1) / 2;
            if (moveNumber > lastMoveNumber) {
                lastMoveNumber = moveNumber;
            }
            if ((record.ply % 2) == 1) {
                whiteMoves.insert(moveNumber, record);
            } else {
                blackMoves.insert(moveNumber, record);
            }
        }

        moveTable_->setRowCount(lastMoveNumber);
        const auto setItem = [this](int row, int col, const QString &text, Qt::Alignment align) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(align);
            moveTable_->setItem(row, col, item);
        };

        for (int moveNumber = 1; moveNumber <= lastMoveNumber; ++moveNumber) {
            const MoveRecord whiteMove = whiteMoves.value(moveNumber);
            const MoveRecord blackMove = blackMoves.value(moveNumber);
            const int row = moveNumber - 1;
            setItem(row, 0, QStringLiteral("%1.").arg(moveNumber), Qt::AlignRight | Qt::AlignVCenter);
            setItem(row, 1, moveCellText(whiteMove), Qt::AlignLeft | Qt::AlignVCenter);
            setItem(row, 2, moveCellText(blackMove), Qt::AlignLeft | Qt::AlignVCenter);
            moveTable_->setRowHeight(row, 18);
        }
        if (QScrollBar *bar = moveTable_->verticalScrollBar()) {
            bar->setValue(bar->maximum());
        }
    }

    void appendChat(const QString &sender, const QString &text)
    {
        const QString now = QDateTime::currentDateTime().toString("HH:mm");
        chatLogEdit_->appendPlainText(QString("%1 %2: %3")
                                          .arg(now, sender, text));
    }

    void clearChatLog()
    {
        if (chatLogEdit_ != nullptr) {
            chatLogEdit_->clear();
        }
    }

    QTcpSocket *socket_ = nullptr;
    QTcpServer *directServer_ = nullptr;
    QString mqttBrokerCache_;
    QString directIpCache_;
    bool directShowingLocalHost_ = false;
    QRadioButton *directRadio_ = nullptr;
    QRadioButton *mqttRadio_ = nullptr;
    QRadioButton *roleHostRadio_ = nullptr;
    QRadioButton *roleGuestRadio_ = nullptr;
    QWidget *hostColorWidget_ = nullptr;
    QLabel *hostColorLabel_ = nullptr;
    QRadioButton *hostWhiteRadio_ = nullptr;
    QRadioButton *hostBlackRadio_ = nullptr;
    QLabel *hostCaptionLabel_ = nullptr;
    QLabel *roomCaptionLabel_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QLineEdit *roomEdit_ = nullptr;
    QCheckBox *showHintsCheck_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *startGameButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QPushButton *restoreButton_ = nullptr;
    QLineEdit *moveEdit_ = nullptr;
    QPushButton *moveButton_ = nullptr;
    QPushButton *flipBoardButton_ = nullptr;
    QLineEdit *chatEdit_ = nullptr;
    QPushButton *chatButton_ = nullptr;
    QPlainTextEdit *chatLogEdit_ = nullptr;
    QLabel *turnLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *selectedLabel_ = nullptr;
    QLabel *statusStateLabel_ = nullptr;
    QLabel *statusContextLabel_ = nullptr;
    QLabel *gameClockLabel_ = nullptr;
    QLabel *moveClockLabel_ = nullptr;
    QLabel *logTitleLabel_ = nullptr;
    QStackedWidget *logStack_ = nullptr;
    QTableWidget *moveTable_ = nullptr;
    QPushButton *logToggleButton_ = nullptr;
    QTextEdit *logEdit_ = nullptr;
    QTimer *clockTimer_ = nullptr;
    QLabel *fileLabelsTop_[8] = {};
    QLabel *fileLabelsBottom_[8] = {};
    QLabel *rankLabelsLeft_[8] = {};
    QLabel *rankLabelsRight_[8] = {};
    QPushButton *squares_[8][8] = {};
    QString squareStyleCache_[8][8];
    char board_[8][8] = {};
    QByteArray rxBuffer_;
    QByteArray mqttParser_;
    QString mqttRoom_;
    QString pendingMove_;
    QString statusMessage_;
    QString lastSocketError_;
    QString lastMove_;
    QStringList logLines_;
    QVector<MoveRecord> moveHistoryRecords_;
    QStringList legalTargets_;
    QElapsedTimer gameTimer_;
    QElapsedTimer moveTimer_;
    QElapsedTimer linkWatch_;
    QElapsedTimer uiTickTimer_;
    QElapsedTimer peerActivity_;
    QElapsedTimer mqttSetupActivity_;
    int pendingPly_ = 0;
    int nextPly_ = 1;
    int selectedRow_ = -1;
    int selectedCol_ = -1;
    int targetRow_ = -1;
    int targetCol_ = -1;
    int feedbackRow_ = -1;
    int feedbackCol_ = -1;
    int feedbackGeneration_ = 0;
    int pieceFlashGeneration_ = 0;
    int pieceRevealGeneration_ = 0;
    int mqttSetupAnnounces_ = 0;
    int directHelloRetries_ = 0;
    quint16 mqttSessionId_ = 0;
    bool feedbackOn_ = false;
    bool mqttConnected_ = false;
    bool mqttSubscribed_ = false;
    bool mqttPeerReady_ = false;
    bool mqttSideReady_ = false;
    bool mqttPeerPingOutstanding_ = false;
    bool directPingOutstanding_ = false;
    bool directReady_ = false;
    bool startPending_ = false;
    bool resetPending_ = false;
    bool drawPending_ = false;
    bool resetPromptOpen_ = false;
    bool pcTurn_ = false;
    bool pcIsHost_ = false;
    bool ignoreNextDisconnect_ = false;
    bool localDisconnectPending_ = false;
    bool hostPlaysWhite_ = true;
    bool pcPlaysWhite_ = false;
    bool coordinatesInitialized_ = false;
    bool lastCoordinatePcWhite_ = false;
    bool boardWhiteAtBottom_ = false;
    bool boardOrientationManual_ = false;
    bool boardPiecesVisible_ = false;
    bool showingMoveHistory_ = true;
    bool gameOver_ = false;
    bool gameCheck_ = false;
    bool gameClockRunning_ = false;
    int mqttSubacksPending_ = 0;
    uint16_t mqttNextPacketId_ = 1;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Shatranj");
    QApplication::setOrganizationName("Shatranj");
    const QIcon appIcon = makeShatranjIcon();
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();

    return app.exec();
}
