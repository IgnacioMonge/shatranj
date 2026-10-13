#include "preferences_dialog.h"

#include "engine_player.h"
#include "piece_renderer.h"
#include "ui_theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("QLabel { color:" SHZ_TEXT_MUTED "; }"));
    return label;
}

QLabel *lockLabel(QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("Locked during a session."), parent);
    label->setStyleSheet(QStringLiteral("QLabel { color:" SHZ_WAIT_BRIGHT "; }"));
    return label;
}

} // namespace

PreferencesDialog::PreferencesDialog(const ClientPreferences &prefs, bool sessionLocked,
                                     QWidget *parent)
    : QDialog(parent), sessionLocked_(sessionLocked)
{
    setWindowTitle(QStringLiteral("Preferences"));
    setModal(true);

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildBoardTab(prefs), QStringLiteral("Board"));
    tabs_->addTab(buildPlayerTab(prefs), QStringLiteral("Player"));
    tabs_->addTab(buildEngineTab(prefs), QStringLiteral("Engine"));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this]() {
        if (onHelp) {
            onHelp(tabs_->currentIndex() == EngineTab ? ManualSection::Stockfish
                                                      : ManualSection::Preferences);
        }
    });

    // The dialog always takes the size its content needs, wrapped text
    // included: the install progress bar and long engine messages grow it
    // instead of clipping the notes. Tabs share the tallest page's height, so
    // switching them never moves the dialog.
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(tabs_);
    layout->addWidget(buttons);
    tabs_->setMinimumWidth(438);

    installer_.onReleaseFound = [this](const StockfishAsset &asset) {
        if (asset.tag == EngineInstaller::installedTag() &&
            EngineInstaller::isManaged(enginePath_)) {
            setBusy(false);
            refreshEngine(QStringLiteral("Already up to date (%1).").arg(asset.tag));
            return;
        }
        engineStatus_->setText(QStringLiteral("Downloading %1 (%2 MB)...")
                                   .arg(asset.tag)
                                   .arg(asset.size / 1048576));
        progress_->setValue(0);
        progress_->setVisible(true);
        installer_.install(asset);
    };
    installer_.onProgress = [this](qint64 received, qint64 total) {
        if (total > 0) {
            progress_->setValue(static_cast<int>(received * 100 / total));
        }
    };
    installer_.onInstalled = [this](const QString &path) {
        setBusy(false);
        adoptEngine(path);
    };
    installer_.onFailed = [this](const QString &error) {
        setBusy(false);
        refreshEngine(QStringLiteral("Download failed: %1").arg(error));
    };
    refreshEngine();
}

QWidget *PreferencesDialog::buildBoardTab(const ClientPreferences &prefs)
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    board_ = new QComboBox(page);
    board_->addItem(QStringLiteral("Default board"), QString());
    for (const QString &texture : PieceRenderer::boardTextures()) {
        board_->addItem(QFileInfo(texture).completeBaseName(), texture);
    }
    board_->setCurrentIndex(qMax(0, board_->findData(prefs.boardTexture)));

    pieces_ = new QComboBox(page);
    for (const QString &set : PieceRenderer::pieceSets()) {
        pieces_->addItem(set, set);
    }
    pieces_->setCurrentIndex(qMax(0, pieces_->findData(prefs.pieceSet)));

    hints_ = new QCheckBox(QStringLiteral("Highlight legal moves"), page);
    hints_->setChecked(prefs.showHints);

    form->addRow(QStringLiteral("Chessboard"), board_);
    form->addRow(QStringLiteral("Piece set"), pieces_);
    form->addRow(QStringLiteral("Hints"), hints_);
    return page;
}

QWidget *PreferencesDialog::buildPlayerTab(const ClientPreferences &prefs)
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    name_ = new QLineEdit(prefs.playerName, page);
    name_->setPlaceholderText(QStringLiteral("Optional"));
    name_->setMaxLength(10);
    name_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{0,10}")), name_));
    connect(name_, &QLineEdit::textEdited, this, [this](const QString &text) {
        const int cursor = name_->cursorPosition();
        name_->setText(text.toUpper());
        name_->setCursorPosition(cursor);
    });
    name_->setEnabled(!sessionLocked_);

    form->addRow(QStringLiteral("Name"), name_);
    form->addRow(QString(), noteLabel(QStringLiteral(
        "Shown to your opponent. Up to 10 letters, digits, - or _. "
        "STOCKFISH is reserved: it is used while the engine plays."), page));
    if (sessionLocked_) {
        form->addRow(QString(), lockLabel(page));
    }
    return page;
}

QWidget *PreferencesDialog::buildEngineTab(const ClientPreferences &prefs)
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    enginePath_ = QFileInfo::exists(prefs.enginePath) ? prefs.enginePath
                                                      : EngineInstaller::installedBinary();
    engineName_ = enginePath_.isEmpty() ? QString() : EnginePlayer::probeName(enginePath_);

    engineStatus_ = new QLabel(page);
    engineStatus_->setWordWrap(true);
    layout->addWidget(engineStatus_);

    auto *actions = new QHBoxLayout();
    install_ = new QPushButton(page);
    uninstall_ = new QPushButton(page);
    locate_ = new QPushButton(QStringLiteral("Locate existing..."), page);
    for (QPushButton *button : {install_, uninstall_, locate_}) {
        button->setAutoDefault(false);
        actions->addWidget(button);
    }
    actions->addStretch(1);
    layout->addLayout(actions);
    connect(install_, &QPushButton::clicked, this, [this]() { installLatest(); });
    connect(uninstall_, &QPushButton::clicked, this, [this]() { uninstallEngine(); });
    connect(locate_, &QPushButton::clicked, this, [this]() { locateEngine(); });

    progress_ = new QProgressBar(page);
    progress_->setRange(0, 100);
    progress_->setVisible(false);
    layout->addWidget(progress_);

    enginePlays_ = prefs.enginePlays;
    shown_ = new QCheckBox(QStringLiteral("Show Stockfish in the main window"), page);
    shown_->setChecked(prefs.engineShown);
    level_ = new QSpinBox(page);
    level_->setRange(0, 20);
    level_->setValue(prefs.engineLevel);
    delay_ = new QSpinBox(page);
    delay_->setRange(0, 30);
    delay_->setSuffix(QStringLiteral(" s"));
    delay_->setValue(prefs.engineDelay);
    auto *engineRows = new QGridLayout();
    engineRows->addWidget(new QLabel(QStringLiteral("Level"), page), 0, 0);
    engineRows->addWidget(level_, 0, 1);
    engineRows->addWidget(noteLabel(QStringLiteral("0 weakest, 20 strongest"), page), 0, 2);
    engineRows->addWidget(new QLabel(QStringLiteral("Pause before each move"), page), 1, 0);
    engineRows->addWidget(delay_, 1, 1);
    engineRows->addWidget(noteLabel(QStringLiteral("0 for no pause"), page), 1, 2);
    engineRows->setColumnStretch(2, 1);
    layout->addSpacing(8);
    layout->addWidget(shown_);
    layout->addLayout(engineRows);
    layout->addWidget(noteLabel(QStringLiteral(
        "The \u265E button next to the gear lets Stockfish play your side. Your "
        "opponent then sees STOCKFISH instead of your name and is told the "
        "engine and level once per session."), page));
    if (sessionLocked_) {
        layout->addWidget(lockLabel(page));
    }

    auto *help = new QLabel(QStringLiteral("<a href=\"#\">How does it work?</a>"), page);
    connect(help, &QLabel::linkActivated, this, [this]() {
        if (onHelp) onHelp(ManualSection::Stockfish);
    });
    layout->addWidget(help);
    layout->addStretch(1);
    return page;
}

void PreferencesDialog::refreshEngine(const QString &message)
{
    const bool installed = !enginePath_.isEmpty() && QFileInfo::exists(enginePath_);
    const bool managed = EngineInstaller::isManaged(enginePath_);
    QString status = installed
        ? QStringLiteral("%1 is installed.").arg(engineName_.isEmpty()
                                                     ? QStringLiteral("Stockfish")
                                                     : engineName_)
        : QStringLiteral("Stockfish is not installed.");
    if (!message.isEmpty()) {
        status += QLatin1Char(' ') + message;
    }
    engineStatus_->setText(status);
    progress_->setVisible(busy_);

    const bool editable = !sessionLocked_ && !busy_;
    install_->setText(installed && managed ? QStringLiteral("Update to latest")
                                           : QStringLiteral("Install latest"));
    uninstall_->setText(installed && !managed ? QStringLiteral("Forget")
                                              : QStringLiteral("Uninstall"));
    install_->setEnabled(editable);
    uninstall_->setEnabled(editable && installed);
    locate_->setEnabled(editable);
    if (!installed) {
        shown_->setChecked(false);
    }
    shown_->setEnabled(editable && installed);
    level_->setEnabled(editable && installed);
    delay_->setEnabled(editable && installed);
}

void PreferencesDialog::setBusy(bool busy)
{
    busy_ = busy;
    refreshEngine();
}

void PreferencesDialog::installLatest()
{
    setBusy(true);
    engineStatus_->setText(QStringLiteral("Checking the latest Stockfish release..."));
    installer_.checkLatest();
}

void PreferencesDialog::uninstallEngine()
{
    const bool managed = EngineInstaller::isManaged(enginePath_);
    if (managed) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Uninstall Stockfish"),
            QStringLiteral("Remove the downloaded Stockfish from this computer?"));
        if (answer != QMessageBox::Yes) {
            return;
        }
        if (!EngineInstaller::uninstall()) {
            refreshEngine(QStringLiteral("Could not remove every file."));
            return;
        }
    }
    // A located engine is the user's own file: forget it, never delete it.
    enginePath_.clear();
    engineName_.clear();
    refreshEngine(managed ? QStringLiteral("Removed.") : QString());
}

void PreferencesDialog::locateEngine()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Locate Stockfish"));
    if (!path.isEmpty()) {
        adoptEngine(path);
    }
}

void PreferencesDialog::adoptEngine(const QString &path)
{
    const QString name = EnginePlayer::probeName(path);
    if (!name.startsWith(QStringLiteral("Stockfish"), Qt::CaseInsensitive)) {
        refreshEngine(QStringLiteral("That program is not a Stockfish engine."));
        return;
    }
    enginePath_ = path;
    engineName_ = name;
    shown_->setChecked(true);
    refreshEngine(QStringLiteral("Ready."));
}

ClientPreferences PreferencesDialog::preferences() const
{
    ClientPreferences prefs;
    prefs.boardTexture = board_->currentData().toString();
    prefs.pieceSet = pieces_->currentData().toString();
    prefs.showHints = hints_->isChecked();
    prefs.playerName = Preferences::userName(name_->text());
    prefs.enginePath = enginePath_;
    prefs.engineShown = shown_->isChecked() && !enginePath_.isEmpty();
    prefs.enginePlays = enginePlays_ && prefs.engineShown;
    prefs.engineLevel = level_->value();
    prefs.engineDelay = delay_->value();
    return prefs;
}

void PreferencesDialog::showTab(Tab tab)
{
    tabs_->setCurrentIndex(tab);
}

void PreferencesDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F1 && onHelp) {
        onHelp(tabs_->currentIndex() == EngineTab ? ManualSection::Stockfish
                                                  : ManualSection::Preferences);
        return;
    }
    QDialog::keyPressEvent(event);
}
