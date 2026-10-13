#include "manual_index.h"

#include <QStringList>

namespace ManualIndex {

QList<ManualHeading> headings(const QString &markdown)
{
    QList<ManualHeading> result;
    bool fenced = false;
    for (const QString &raw : markdown.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QStringLiteral("```"))) {
            fenced = !fenced;
            continue;
        }
        if (fenced || !line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        int level = 0;
        while (level < line.size() && line.at(level) == QLatin1Char('#')) {
            ++level;
        }
        if (level > 6 || level >= line.size() || line.at(level) != QLatin1Char(' ')) {
            continue;
        }
        result.append({level, line.mid(level + 1).trimmed()});
    }
    return result;
}

QString sectionTitle(const QString &markdown, ManualSection section)
{
    int index = 0;
    for (const ManualHeading &heading : headings(markdown)) {
        if (heading.level == 2 && index++ == static_cast<int>(section)) {
            return heading.title;
        }
    }
    return QString();
}

} // namespace ManualIndex
