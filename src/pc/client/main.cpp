#include <QApplication>

#include "main_window.h"
#include "ui_theme.h"
#include "window_chrome.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Shatranj");
    QApplication::setOrganizationName("Shatranj");
    QApplication::setDesktopFileName(
        QStringLiteral("io.github.ignaciomonge.shatranj"));
    UiTheme::apply(app);
    installWindowChrome(&app, QColor(QStringLiteral(SHZ_INK)),
                        QColor(QStringLiteral(SHZ_TEXT)));
    const QIcon appIcon = MainWindow::appIcon();
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.showNormal();

    return app.exec();
}
