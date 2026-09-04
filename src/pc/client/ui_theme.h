#ifndef SHATRANJ_UI_THEME_H
#define SHATRANJ_UI_THEME_H

// Desktop theme for Shatranj: the original slate and cyan palette, restated as
// one token table and given a modern surface language (cards, soft radii,
// resolved fonts). Tokens are preprocessor literals so the style sheets stay
// compile-time concatenated strings with no runtime formatting.

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

// --- Surfaces ---------------------------------------------------------------
#define SHZ_INK          "#1b1b25"  // window ground
#define SHZ_SURFACE      "#2b2b39"  // inputs, wells, menus
#define SHZ_SURFACE_ALT  "#3b465b"  // raised: buttons
#define SHZ_HOVER        "#46546d"
#define SHZ_PRESSED      "#31394b"
#define SHZ_ROW_ALT      "#313142"  // alternate table rows
#define SHZ_CARD         "#21212d"  // grouped panel behind controls
#define SHZ_WELL         "#15151d"  // status bar
#define SHZ_QUIET        "#252532"  // neutral fill: disabled inputs, idle status
#define SHZ_BORDER       "#555568"  // visible hairline
#define SHZ_BORDER_SOFT  "#3a3a4a"  // internal separators
#define SHZ_DISABLED     "#303040"
#define SHZ_SCROLL       "#4c4c62"

// --- Ink --------------------------------------------------------------------
#define SHZ_TEXT         "#f1f3f6"
#define SHZ_TEXT_DIM     "#c7c7d8"
#define SHZ_TEXT_MUTED   "#8a8aa0"

// --- Accents ----------------------------------------------------------------
#define SHZ_ACCENT       "#00d7ff"  // captions, live state, links
#define SHZ_ACCENT_SOFT  "#6fefff"
#define SHZ_ACCENT_DEEP  "#00b7d8"  // selection fill, hover edge
#define SHZ_ACCENT_FILL  "#36556b"  // filled emphasis: your turn, primary action
#define SHZ_ACCENT_FILL_HOVER "#42657e"
#define SHZ_SEND         "#5aa7d8"  // chat send, ready
#define SHZ_SEND_HOVER   "#6db8e7"
#define SHZ_SEND_TEXT    "#08131b"
#define SHZ_GO           "#1f9d47"  // move ready to send
#define SHZ_GO_HOVER     "#28b956"
#define SHZ_GO_TEXT      "#f4fff8"
#define SHZ_ALERT        "#743838"  // failure fill
#define SHZ_ALERT_TEXT   "#fff0f0"
#define SHZ_ALERT_BRIGHT "#ff5a5a"
#define SHZ_WAIT         "#5f5534"  // waiting fill
#define SHZ_WAIT_TEXT    "#ffe99a"
#define SHZ_WAIT_BRIGHT  "#ffd166"
#define SHZ_CHECK        "#ffe15a"  // king in check

// --- Geometry ---------------------------------------------------------------
#define SHZ_R            "6px"      // inputs, buttons
#define SHZ_R_LG         "10px"     // cards, board frame
#define SHZ_R_SQ         8          // px: blunt vertex on the inner board block

// --- Board ------------------------------------------------------------------
#define SHZ_BOARD_WELL   "#101010"
#define SHZ_BOARD_EDGE   "#e6e6e2"
#define SHZ_SQ_LIGHT     "#f0f0ec"
#define SHZ_SQ_DARK      "#5f6870"
#define SHZ_SQ_EDGE      "#2c3034"
#define SHZ_SQ_SEL       "#c9b56b"
#define SHZ_SQ_SEL_EDGE  "#5d4b1d"
#define SHZ_SQ_TGT       "#9fb8d9"
#define SHZ_SQ_TGT_EDGE  "#2f5f9f"
#define SHZ_SQ_HIT       "#f2dc54"
#define SHZ_SQ_HIT_EDGE  "#1f7a8c"
#define SHZ_SQ_HINT_L    "#d5ebd5"
#define SHZ_SQ_HINT_D    "#486648"
#define SHZ_SQ_HINT_EDGE "#6fa86f"

namespace UiTheme {

// Proportional UI family, per platform, with a graceful fallback.
inline QString uiFamily()
{
    static const QString cached = []() {
        const QStringList families = QFontDatabase::families();
        for (const char *candidate : {"Segoe UI Variable Text", "Segoe UI",
                                      "SF Pro Text", "Helvetica Neue",
                                      "Inter", "Noto Sans"}) {
            const QString name = QString::fromLatin1(candidate);
            if (families.contains(name)) {
                return name;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    return cached;
}

// Tabular family for the move list and the raw log.
inline QString monoFamily()
{
    static const QString cached = []() {
        const QStringList families = QFontDatabase::families();
        for (const char *candidate : {"Cascadia Mono", "Consolas", "SF Mono",
                                      "Menlo", "DejaVu Sans Mono"}) {
            const QString name = QString::fromLatin1(candidate);
            if (families.contains(name)) {
                return name;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    return cached;
}

// Fusion plus an explicit palette so native chrome (menus, tooltips, combo
// popups, message boxes) matches the style sheet on every platform.
inline void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Override the system appearance so native title bars follow the dark
    // window ground on Windows and Linux. macOS still uses applyWindowChrome.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif

    QFont base(uiFamily());
    base.setPointSizeF(10.0);
    app.setFont(base);

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(SHZ_INK));
    pal.setColor(QPalette::WindowText, QColor(SHZ_TEXT));
    pal.setColor(QPalette::Base, QColor(SHZ_SURFACE));
    pal.setColor(QPalette::AlternateBase, QColor(SHZ_ROW_ALT));
    pal.setColor(QPalette::Text, QColor(SHZ_TEXT));
    pal.setColor(QPalette::Button, QColor(SHZ_SURFACE_ALT));
    pal.setColor(QPalette::ButtonText, QColor(SHZ_TEXT));
    pal.setColor(QPalette::BrightText, QColor(SHZ_ACCENT_SOFT));
    pal.setColor(QPalette::Highlight, QColor(SHZ_ACCENT_DEEP));
    pal.setColor(QPalette::HighlightedText, QColor(SHZ_WELL));
    pal.setColor(QPalette::Link, QColor(SHZ_ACCENT));
    pal.setColor(QPalette::ToolTipBase, QColor(SHZ_SURFACE));
    pal.setColor(QPalette::ToolTipText, QColor(SHZ_TEXT));
    pal.setColor(QPalette::PlaceholderText, QColor(SHZ_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(SHZ_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(SHZ_TEXT_MUTED));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(SHZ_TEXT_MUTED));
    app.setPalette(pal);
}

} // namespace UiTheme

#endif // SHATRANJ_UI_THEME_H
