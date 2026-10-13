#ifndef SHATRANJ_HELP_WINDOW_H
#define SHATRANJ_HELP_WINDOW_H

#include "manual_index.h"

#include <QWidget>

class QListWidget;
class QRadioButton;
class QTextBrowser;

// Non-modal manual viewer in the main window's style: banner, a card with the
// heading index and the Markdown text, ES/EN defaulting to the system language.
class HelpWindow final : public QWidget {
public:
    explicit HelpWindow(QWidget *parent = nullptr);

    void showSection(ManualSection section);

private:
    void loadLanguage(const QString &language);
    void scrollToHeading(int level, const QString &title);

    QListWidget *index_ = nullptr;
    QTextBrowser *text_ = nullptr;
    QRadioButton *spanish_ = nullptr;
    QRadioButton *english_ = nullptr;
    QString markdown_;
};

#endif // SHATRANJ_HELP_WINDOW_H
