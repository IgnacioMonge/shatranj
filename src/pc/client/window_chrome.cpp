#include "window_chrome.h"

#include <QColor>
#include <QEvent>
#include <QObject>
#include <QWidget>
#include <QtGlobal>

#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QGuiApplication>
#include <QLibrary>
#include <QtGui/qguiapplication_platform.h>
#include <QWindow>
#endif

#if defined(Q_OS_WIN)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

static COLORREF toColorRef(const QColor &color)
{
    return RGB(static_cast<BYTE>(color.red()),
               static_cast<BYTE>(color.green()),
               static_cast<BYTE>(color.blue()));
}

static QColor captionTextColor(const QColor &background, const QColor &foreground)
{
    if (foreground.isValid()) {
        return foreground;
    }
    const int luminance = (background.red() * 299 + background.green() * 587 +
                           background.blue() * 114) / 1000;
    return luminance < 128 ? QColor(241, 243, 246) : QColor(27, 27, 37);
}

void applyWindowChrome(QWidget *window, const QColor &background,
                       const QColor &foreground)
{
    if (window == nullptr || !background.isValid()) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd == nullptr) {
        return;
    }

    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
                          &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));

    const COLORREF caption = toColorRef(background);
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &caption, sizeof(caption));

    const COLORREF text = toColorRef(captionTextColor(background, foreground));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}

#elif defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)

static void applyLinuxGtkDarkVariant(QWidget *window)
{
    QWindow *handle = window->windowHandle();
    if (handle == nullptr) {
        handle = window->window()->windowHandle();
    }
    if (handle == nullptr) {
        return;
    }

    auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (x11App == nullptr) {
        return;
    }

    void *display = x11App->display();
    if (display == nullptr) {
        return;
    }

    QLibrary x11(QStringLiteral("X11"));
    if (!x11.load()) {
        x11.setFileName(QStringLiteral("libX11.so.6"));
        if (!x11.load()) {
            return;
        }
    }

    using XInternAtomFn = unsigned long (*)(void *, const char *, int);
    using XChangePropertyFn = int (*)(void *, unsigned long, unsigned long,
                                      unsigned long, int, int,
                                      const unsigned char *, int);
    auto internAtom = reinterpret_cast<XInternAtomFn>(x11.resolve("XInternAtom"));
    auto changeProperty =
        reinterpret_cast<XChangePropertyFn>(x11.resolve("XChangeProperty"));
    if (internAtom == nullptr || changeProperty == nullptr) {
        return;
    }

    const unsigned long variant = internAtom(display, "_GTK_THEME_VARIANT", 0);
    const unsigned long utf8 = internAtom(display, "UTF8_STRING", 0);
    static const unsigned char dark[] = "dark";
    const unsigned long xid = static_cast<unsigned long>(handle->winId());
    changeProperty(display, xid, variant, utf8, 8, 0, dark, 4);
}

void applyWindowChrome(QWidget *window, const QColor &background,
                       const QColor &foreground)
{
    Q_UNUSED(background);
    Q_UNUSED(foreground);
    if (window == nullptr) {
        return;
    }
    applyLinuxGtkDarkVariant(window);
}

#elif !defined(Q_OS_MACOS)

void applyWindowChrome(QWidget *window, const QColor &background,
                       const QColor &foreground)
{
    Q_UNUSED(window);
    Q_UNUSED(background);
    Q_UNUSED(foreground);
}

#endif

namespace {

class WindowChromeFilter final : public QObject {
public:
    WindowChromeFilter(const QColor &background, const QColor &foreground,
                       QObject *parent)
        : QObject(parent)
        , background_(background)
        , foreground_(foreground)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Show) {
            return false;
        }
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget == nullptr || !widget->isWindow()) {
            return false;
        }
        const Qt::WindowType type = widget->windowType();
        if (type != Qt::Window && type != Qt::Dialog) {
            return false;
        }
        applyWindowChrome(widget, background_, foreground_);
        return false;
    }

private:
    QColor background_;
    QColor foreground_;
};

} // namespace

void installWindowChrome(QObject *app, const QColor &background,
                         const QColor &foreground)
{
    if (app == nullptr) {
        return;
    }
    app->installEventFilter(
        new WindowChromeFilter(background, foreground, app));
}
