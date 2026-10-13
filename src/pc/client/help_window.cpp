#include "help_window.h"

#include "app_banner.h"
#include "piece_renderer.h"
#include "ui_theme.h"

#include <QAbstractTextDocumentLayout>
#include <QButtonGroup>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

namespace {

QString readManual(const QString &language)
{
    QFile file(PieceRenderer::assetPath(
        QStringLiteral("assets/pc-client/manual/manual.%1.md").arg(language)));
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
               ? QString::fromUtf8(file.readAll())
               : QStringLiteral("# Manual not found\n\nReinstall Shatranj.");
}

constexpr int kLevelRole = Qt::UserRole;
constexpr int kTitleRole = Qt::UserRole + 1;

QLabel *caption(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("caption"));
    return label;
}

// Gives the rendered Markdown the client's typographic hierarchy: cyan
// headings, readable body spacing, highlighted code that wraps.
void styleManual(QTextDocument *document)
{
    document->setDocumentMargin(24);
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        QTextCharFormat headingFormat;
        const int level = blockFormat.headingLevel();
        if (level == 1) {
            headingFormat.setForeground(QColor(SHZ_ACCENT));
            headingFormat.setFontPointSize(19);
            headingFormat.setFontWeight(QFont::Bold);
            blockFormat.setBottomMargin(12);
        } else if (level == 2) {
            headingFormat.setForeground(QColor(SHZ_ACCENT));
            headingFormat.setFontPointSize(14);
            headingFormat.setFontWeight(QFont::Bold);
            blockFormat.setTopMargin(26);
            blockFormat.setBottomMargin(8);
        } else if (level == 3) {
            headingFormat.setForeground(QColor(SHZ_TEXT));
            headingFormat.setFontPointSize(11.5);
            headingFormat.setFontWeight(QFont::DemiBold);
            blockFormat.setTopMargin(16);
            blockFormat.setBottomMargin(4);
        } else {
            blockFormat.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
            blockFormat.setBottomMargin(6);
        }
        if (blockFormat.nonBreakableLines()) {
            // Fenced code: a well, wrapped instead of a horizontal scrollbar.
            blockFormat.setNonBreakableLines(false);
            blockFormat.setBackground(QColor(SHZ_WELL));
            blockFormat.setLeftMargin(8);
            blockFormat.setRightMargin(8);
            blockFormat.setLineHeight(120, QTextBlockFormat::ProportionalHeight);
        }
        QTextCursor cursor(block);
        cursor.setBlockFormat(blockFormat);
        if (level != 0) {
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(headingFormat);
            continue;
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat format = fragment.charFormat();
            QTextCharFormat emphasis;
            if (format.fontFixedPitch()) {
                emphasis.setForeground(QColor(SHZ_ACCENT_SOFT));
                emphasis.setBackground(QColor(SHZ_QUIET));
            } else if (format.fontWeight() >= QFont::DemiBold) {
                emphasis.setForeground(QColor(SHZ_TEXT));
            } else {
                continue;
            }
            QTextCursor span(document);
            span.setPosition(fragment.position());
            span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            span.mergeCharFormat(emphasis);
        }
    }
}

} // namespace

HelpWindow::HelpWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("helpWindow"));
    setWindowTitle(QStringLiteral("Shatranj Help"));
    resize(900, 680);
    setStyleSheet(QStringLiteral(
        "QWidget#helpWindow { background:" SHZ_INK "; }"
        "QWidget#helpCard { background:" SHZ_CARD "; border:1px solid " SHZ_BORDER_SOFT ";"
        " border-radius:" SHZ_R_LG "; }"
        "QListWidget#helpIndex { background:" SHZ_SURFACE "; border:1px solid " SHZ_BORDER ";"
        " border-radius:" SHZ_R "; padding:6px 4px; outline:0; }"
        "QListWidget#helpIndex::item { padding:4px 8px; border-radius:4px; }"
        "QListWidget#helpIndex::item:hover { background:" SHZ_HOVER "; }"
        "QListWidget#helpIndex::item:selected { background:" SHZ_ACCENT_FILL ";"
        " color:" SHZ_TEXT "; border-left:3px solid " SHZ_ACCENT "; }"
        "QTextBrowser#helpText { background:" SHZ_SURFACE "; color:" SHZ_TEXT_DIM ";"
        " border:1px solid " SHZ_BORDER "; border-radius:" SHZ_R "; padding:0; }"
        "QSplitter::handle { background:transparent; width:10px; }"));

    // Decorative here: About lives on the main window's banner.
    auto *banner = new AppBanner(this);
    banner->unsetCursor();
    banner->setToolTip(QString());
    banner->setFocusPolicy(Qt::NoFocus);
    banner->setAccessibleName(QStringLiteral("Shatranj"));

    spanish_ = new QRadioButton(QStringLiteral("ES"), this);
    english_ = new QRadioButton(QStringLiteral("EN"), this);
    spanish_->setToolTip(QStringLiteral("Español"));
    english_->setToolTip(QStringLiteral("English"));
    spanish_->setAccessibleName(QStringLiteral("Español"));
    english_->setAccessibleName(QStringLiteral("English"));
    auto *languages = new QButtonGroup(this);
    languages->addButton(spanish_);
    languages->addButton(english_);
    connect(spanish_, &QRadioButton::clicked, this, [this]() { loadLanguage("es"); });
    connect(english_, &QRadioButton::clicked, this, [this]() { loadLanguage("en"); });

    auto *card = new QWidget(this);
    card->setObjectName(QStringLiteral("helpCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 12);
    cardLayout->setSpacing(8);

    auto *top = new QHBoxLayout();
    top->setSpacing(10);
    top->addWidget(caption(QStringLiteral("Manual"), card));
    top->addStretch(1);
    top->addWidget(spanish_);
    top->addWidget(english_);
    cardLayout->addLayout(top);

    index_ = new QListWidget(card);
    index_->setObjectName(QStringLiteral("helpIndex"));
    index_->setMinimumWidth(230);
    text_ = new QTextBrowser(card);
    text_->setObjectName(QStringLiteral("helpText"));
    text_->setOpenExternalLinks(true);
    text_->setLineWrapMode(QTextEdit::WidgetWidth);
    text_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(index_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item) {
        if (item != nullptr) {
            scrollToHeading(item->data(kLevelRole).toInt(),
                            item->data(kTitleRole).toString());
        }
    });

    auto *splitter = new QSplitter(card);
    splitter->addWidget(index_);
    splitter->addWidget(text_);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 640});
    cardLayout->addWidget(splitter, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(banner);
    layout->addWidget(card, 1);

    const bool spanish = QLocale::system().language() == QLocale::Spanish;
    (spanish ? spanish_ : english_)->setChecked(true);
    loadLanguage(spanish ? QStringLiteral("es") : QStringLiteral("en"));
}

void HelpWindow::loadLanguage(const QString &language)
{
    // Keep the reader in the same section across languages.
    const int section = index_->currentRow();
    markdown_ = readManual(language);
    text_->setMarkdown(markdown_);
    styleManual(text_->document());

    const QSignalBlocker blocker(index_);
    index_->clear();
    for (const ManualHeading &heading : ManualIndex::headings(markdown_)) {
        if (heading.level < 2 || heading.level > 3) {
            continue;
        }
        auto *item = new QListWidgetItem(
            (heading.level == 3 ? QStringLiteral("      ") : QString()) + heading.title, index_);
        QFont font = index_->font();
        if (heading.level == 2) {
            font.setBold(true);
            item->setForeground(QColor(SHZ_TEXT));
        } else {
            item->setForeground(QColor(SHZ_TEXT_DIM));
        }
        item->setFont(font);
        item->setData(kLevelRole, heading.level);
        item->setData(kTitleRole, heading.title);
    }
    if (section >= 0 && section < index_->count()) {
        index_->setCurrentRow(section);
        scrollToHeading(index_->currentItem()->data(kLevelRole).toInt(),
                        index_->currentItem()->data(kTitleRole).toString());
    }
}

void HelpWindow::scrollToHeading(int level, const QString &title)
{
    QTextDocument *document = text_->document();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() == level && block.text() == title) {
            const QRectF rect = document->documentLayout()->blockBoundingRect(block);
            text_->verticalScrollBar()->setValue(static_cast<int>(rect.top()));
            return;
        }
    }
}

void HelpWindow::showSection(ManualSection section)
{
    const QString title = ManualIndex::sectionTitle(markdown_, section);
    for (int row = 0; row < index_->count(); ++row) {
        if (index_->item(row)->data(kLevelRole).toInt() == 2 &&
            index_->item(row)->data(kTitleRole).toString() == title) {
            index_->setCurrentRow(row);
            break;
        }
    }
    show();
    raise();
    activateWindow();
}
