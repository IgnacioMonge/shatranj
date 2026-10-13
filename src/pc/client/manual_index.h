#ifndef SHATRANJ_MANUAL_INDEX_H
#define SHATRANJ_MANUAL_INDEX_H

#include <QList>
#include <QString>

// Index of the bundled Markdown manual. Both languages keep the same
// sections in the same order, so a section is addressed by its position.
struct ManualHeading {
    int level = 0;   // 1 = title, 2 = section, 3 = subsection
    QString title;
};

enum class ManualSection {
    GettingStarted,
    Connecting,
    Playing,
    SavingLoading,
    Preferences,
    Stockfish,
    Spectrum,
    Troubleshooting,
    Count
};

namespace ManualIndex {

// ATX headings outside fenced code blocks, in document order.
QList<ManualHeading> headings(const QString &markdown);
// Title of the n-th level-2 heading, or empty.
QString sectionTitle(const QString &markdown, ManualSection section);

} // namespace ManualIndex

#endif // SHATRANJ_MANUAL_INDEX_H
