#pragma once

#include <QLineEdit>
#include <QStringList>

class QLabel;

class CommandInputWidget final : public QLineEdit {
    Q_OBJECT

public:
    explicit CommandInputWidget(int maximumLength, QWidget *parent = nullptr);

    void rememberSubmission(const QString &text);
    void setSubmissionEnabled(bool enabled);

signals:
    void submitRequested();

protected:
    bool event(QEvent *event) override;

private:
    void layoutCountLabel();

    QLabel *countLabel_ = nullptr;
    QStringList history_;
    int historyIndex_ = 0;
    bool submissionEnabled_ = false;
};
