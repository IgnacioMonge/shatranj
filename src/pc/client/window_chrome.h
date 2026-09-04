#pragma once

#include <QColor>

class QObject;
class QWidget;

// Tint native window chrome so the title bar matches the window ground.
// macOS uses NSWindow; Windows 11 sets DWM caption/text/border colors;
// Linux relies on the application color scheme plus this hook.
void applyWindowChrome(QWidget *window, const QColor &background,
                       const QColor &foreground = QColor());
void installWindowChrome(QObject *app, const QColor &background,
                         const QColor &foreground);
