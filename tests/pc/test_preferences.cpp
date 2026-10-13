#include "pc/client/preferences.h"

#include <QSettings>
#include <QTemporaryDir>

#include <cstdio>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

int main()
{
    check(Preferences::userName(QStringLiteral(" ana-1 ")) == QStringLiteral("ANA-1"),
          "name is trimmed and upper-cased");
    check(Preferences::userName(QStringLiteral("stockfish")).isEmpty(),
          "engine name is reserved");
    check(Preferences::userName(QStringLiteral("TOO_LONG_NAME")).isEmpty() &&
              Preferences::userName(QStringLiteral("A B")).isEmpty(),
          "invalid names are dropped");

    QTemporaryDir dir;
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")), QSettings::IniFormat);

    const ClientPreferences defaults = ClientPreferences::load(settings);
    check(defaults.pieceSet == QStringLiteral("merida") && defaults.showHints &&
              defaults.engineShown && !defaults.enginePlays &&
              defaults.engineLevel == 5 && defaults.engineDelay == 0 &&
              defaults.playerName.isEmpty(),
          "defaults");

    ClientPreferences prefs;
    prefs.boardTexture = QStringLiteral("wood.png");
    prefs.pieceSet = QStringLiteral("alpha");
    prefs.showHints = false;
    prefs.playerName = QStringLiteral("TESTER");
    prefs.enginePlays = true;
    prefs.engineShown = true;
    prefs.engineLevel = 42;
    prefs.engineDelay = 99;
    prefs.enginePath = QStringLiteral("/engines/stockfish");
    prefs.save(settings);
    const ClientPreferences loaded = ClientPreferences::load(settings);
    check(loaded.boardTexture == prefs.boardTexture && loaded.pieceSet == prefs.pieceSet &&
              !loaded.showHints && loaded.playerName == QStringLiteral("TESTER") &&
              loaded.enginePlays && loaded.engineLevel == 20 &&
              loaded.engineDelay == 30 && loaded.enginePath == prefs.enginePath,
          "round trip clamps the level and the pause");

    prefs.playerName = QStringLiteral("STOCKFISH");
    prefs.enginePath.clear();
    prefs.save(settings);
    const ClientPreferences reserved = ClientPreferences::load(settings);
    check(reserved.playerName.isEmpty() && !reserved.enginePlays && !reserved.engineShown,
          "reserved name and engine without binary are not stored");

    prefs.enginePath = QStringLiteral("/engines/stockfish");
    prefs.engineShown = false;
    prefs.enginePlays = true;
    prefs.save(settings);
    const ClientPreferences hidden = ClientPreferences::load(settings);
    check(!hidden.engineShown && !hidden.enginePlays,
          "a hidden engine option never plays");

    settings.setValue("player/name", QStringLiteral("STOCKFISH"));
    check(ClientPreferences::load(settings).playerName.isEmpty(),
          "a hand-edited reserved name is ignored");

    if (failures != 0) {
        return 1;
    }
    std::printf("preferences tests ok\n");
    return 0;
}
