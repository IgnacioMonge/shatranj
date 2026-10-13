#include "command_input_widget.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QtGlobal>

#include "ui_theme.h"

CommandInputWidget::CommandInputWidget(int maximumLength, QWidget *parent)
    : QLineEdit(parent)
{
    setAccessibleName(QStringLiteral("Chat message or move"));
    setPlaceholderText(QStringLiteral("Message or move (e2e4)"));
    setClearButtonEnabled(true);
    setMaxLength(maximumLength);
    setMinimumHeight(32);
    setStyleSheet(
        "QLineEdit { background:#202b35; color:#ffffff;"
        " border:1px solid " SHZ_ACCENT "; border-radius:" SHZ_R ";"
        " padding:4px 8px; selection-background-color:" SHZ_ACCENT_DEEP ";"
        " selection-color:" SHZ_WELL "; }"
        "QLineEdit:focus { background:#243444;"
        " border:1px solid " SHZ_ACCENT_SOFT "; }");

    countLabel_ = new QLabel(this);
    countLabel_->setAccessibleName(QStringLiteral("Chat character count"));
    countLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    countLabel_->setStyleSheet(
        "QLabel { color:" SHZ_TEXT_MUTED "; font-size:9px;"
        " background:transparent; padding:0; }");
    countLabel_->setText(QStringLiteral("0/%1").arg(maximumLength));

    connect(this, &QLineEdit::textChanged, this, [this](const QString &text) {
        historyIndex_ = static_cast<int>(history_.size());
        countLabel_->setText(QStringLiteral("%1/%2").arg(text.size()).arg(maxLength()));
        layoutCountLabel();
    });
    connect(this, &QLineEdit::returnPressed, this, [this]() {
        if (submissionEnabled_) {
            emit submitRequested();
        }
    });
}

void CommandInputWidget::rememberSubmission(const QString &text)
{
    history_.append(text);
    while (history_.size() > 5) {
        history_.removeFirst();
    }
    historyIndex_ = static_cast<int>(history_.size());
}

void CommandInputWidget::setSubmissionEnabled(bool enabled)
{
    submissionEnabled_ = enabled;
}

bool CommandInputWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        layoutCountLabel();
    } else if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) &&
            !history_.isEmpty()) {
            const int delta = keyEvent->key() == Qt::Key_Up ? -1 : 1;
            const int historySize = static_cast<int>(history_.size());
            const int target = qBound(0, historyIndex_ + delta, historySize);
            setText(target == historySize ? QString() : history_.at(target));
            historyIndex_ = target;
            selectAll();
            return true;
        }
        const bool isReturn = keyEvent->key() == Qt::Key_Return ||
                              keyEvent->key() == Qt::Key_Enter;
        if (isReturn && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            if (submissionEnabled_) {
                emit submitRequested();
            }
            return true;
        }
    }
    return QLineEdit::event(event);
}

void CommandInputWidget::layoutCountLabel()
{
    countLabel_->adjustSize();
    constexpr int clearGutter = 22;
    constexpr int pad = 8;
    const QSize hint = countLabel_->sizeHint();
    const int x = width() - hint.width() - clearGutter;
    const int y = (height() - hint.height()) / 2;
    countLabel_->move(qMax(pad, x), qMax(0, y));
    const QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(text());
    const int textRight = textMargins().left() + pad + textWidth;
    countLabel_->setVisible(textRight + 6 < countLabel_->x());
}
