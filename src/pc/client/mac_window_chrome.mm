#include "window_chrome.h"

#include <QColor>
#include <QGuiApplication>
#include <QWidget>

#import <Cocoa/Cocoa.h>

void applyWindowChrome(QWidget *window, const QColor &background,
                       const QColor &foreground)
{
    Q_UNUSED(foreground);
    if (window == nullptr ||
        QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return;
    }

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = view != nil ? view.window : nil;
    if (nativeWindow == nil) {
        return;
    }

    nativeWindow.titlebarAppearsTransparent = YES;
    nativeWindow.titleVisibility = NSWindowTitleVisible;
    if (@available(macOS 11.0, *)) {
        nativeWindow.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
    }
    nativeWindow.backgroundColor = [NSColor colorWithSRGBRed:background.redF()
                                                        green:background.greenF()
                                                         blue:background.blueF()
                                                        alpha:background.alphaF()];
    nativeWindow.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
}
