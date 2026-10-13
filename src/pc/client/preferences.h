#ifndef SHATRANJ_PREFERENCES_H
#define SHATRANJ_PREFERENCES_H

#include <QString>

class QSettings;

// Everything the Preferences dialog edits, persisted in QSettings.
struct ClientPreferences {
    QString boardTexture;                     // empty: default board
    QString pieceSet = QStringLiteral("merida");
    bool showHints = true;
    QString playerName;                       // valid user name or empty
    bool engineShown = true;                  // Player: You/Stockfish in main window
    bool enginePlays = false;                 // Stockfish moves this side
    int engineLevel = 5;                      // UCI Skill Level 0..20
    int engineDelay = 0;                      // seconds before each engine move, 0..30
    QString enginePath;                       // installed or located binary

    static ClientPreferences load(QSettings &settings);
    void save(QSettings &settings) const;
};

namespace Preferences {

// The engine's reserved MNAME; never accepted as a user name.
constexpr const char *kBotName = "STOCKFISH";

// Upper-cased MNAME-valid name, or empty when invalid or reserved.
QString userName(const QString &text);

} // namespace Preferences

#endif // SHATRANJ_PREFERENCES_H
