#include "pc/client/manual_index.h"

#include <QFile>

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

static QString readManual(const char *language)
{
    QFile file(QStringLiteral(SHATRANJ_MANUAL_DIR "/manual.%1.md").arg(language));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

static QList<int> levels(const QList<ManualHeading> &headings)
{
    QList<int> result;
    for (const ManualHeading &heading : headings) {
        result.append(heading.level);
    }
    return result;
}

int main()
{
    const QString sample = QStringLiteral(
        "# Title\n\n## One\ntext\n```text\n## not a heading\n```\n"
        "### Sub\n#nospace\n## Two\n");
    const QList<ManualHeading> parsed = ManualIndex::headings(sample);
    check(parsed.size() == 4 && parsed.at(0).level == 1 &&
              parsed.at(1).title == QStringLiteral("One") &&
              parsed.at(2).level == 3 && parsed.at(3).title == QStringLiteral("Two"),
          "headings skip code fences and malformed lines");
    check(ManualIndex::sectionTitle(sample, ManualSection::Connecting) == QStringLiteral("Two") &&
              ManualIndex::sectionTitle(sample, ManualSection::Playing).isEmpty(),
          "sections are addressed by position");

    const QString english = readManual("en");
    const QString spanish = readManual("es");
    check(!english.isEmpty() && !spanish.isEmpty(), "both manuals are bundled");
    const QList<ManualHeading> en = ManualIndex::headings(english);
    const QList<ManualHeading> es = ManualIndex::headings(spanish);
    check(levels(en) == levels(es), "both languages share the heading structure");
    for (int section = 0; section < static_cast<int>(ManualSection::Count); ++section) {
        check(!ManualIndex::sectionTitle(english, static_cast<ManualSection>(section)).isEmpty(),
              "every ManualSection exists");
    }
    check(ManualIndex::sectionTitle(english, ManualSection::Count).isEmpty(),
          "no extra top-level sections");
    check(ManualIndex::sectionTitle(english, ManualSection::Stockfish) ==
              QStringLiteral("Playing with Stockfish") &&
              ManualIndex::sectionTitle(spanish, ManualSection::Stockfish) ==
              QStringLiteral("Jugar con Stockfish"),
          "engine section is where the Preferences link expects it");

    if (failures != 0) {
        return 1;
    }
    std::printf("manual index tests ok\n");
    return 0;
}
