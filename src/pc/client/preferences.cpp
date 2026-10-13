#include "preferences.h"

extern "C" {
#include "common/protocol/platform_protocol.h"
}

#include <QSettings>
#include <QtGlobal>

namespace Preferences {

QString userName(const QString &text)
{
    const QByteArray name = text.trimmed().toUpper().toLatin1();
    if (!netchess_player_name_valid(name.constData()) || name == kBotName) {
        return QString();
    }
    return QString::fromLatin1(name);
}

} // namespace Preferences

ClientPreferences ClientPreferences::load(QSettings &settings)
{
    ClientPreferences prefs;
    prefs.boardTexture = settings.value("appearance/board").toString();
    const QString pieces = settings.value("appearance/pieces").toString();
    if (!pieces.isEmpty()) {
        prefs.pieceSet = pieces;
    }
    prefs.showHints = settings.value("ui/showHints", true).toBool();
    prefs.playerName = Preferences::userName(settings.value("player/name").toString());
    prefs.engineShown = settings.value("engine/shown", true).toBool();
    prefs.enginePlays = settings.value("engine/plays", false).toBool();
    prefs.engineLevel = qBound(0, settings.value("engine/level", 5).toInt(), 20);
    prefs.engineDelay = qBound(0, settings.value("engine/delay", 0).toInt(), 30);
    prefs.enginePath = settings.value("engine/path").toString();
    return prefs;
}

void ClientPreferences::save(QSettings &settings) const
{
    settings.setValue("appearance/board", boardTexture);
    settings.setValue("appearance/pieces", pieceSet);
    settings.setValue("ui/showHints", showHints);
    settings.setValue("player/name", Preferences::userName(playerName));
    // Without an engine there is nothing to show; a hidden option never plays.
    const bool shown = engineShown && !enginePath.isEmpty();
    settings.setValue("engine/shown", shown);
    settings.setValue("engine/plays", enginePlays && shown);
    settings.setValue("engine/level", qBound(0, engineLevel, 20));
    settings.setValue("engine/delay", qBound(0, engineDelay, 30));
    settings.setValue("engine/path", enginePath);
}
