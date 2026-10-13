#include "stockfish_release.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace StockfishRelease {

static QString assetCpu(const QString &cpu)
{
    if (cpu == QStringLiteral("x86_64")) return QStringLiteral("x86-64");
    return cpu; // arm64, riscv64 match the release names already
}

QStringList platformPrefixes(const QString &os, const QString &cpu, const QString &buildCpu)
{
    if (os == QStringLiteral("macos")) {
        return { QStringLiteral("stockfish-macos-universal") };
    }
    QStringList prefixes;
    for (const QString &arch : { cpu, buildCpu }) {
        const QString prefix = QStringLiteral("stockfish-%1-%2").arg(os, assetCpu(arch));
        if (!prefixes.contains(prefix)) prefixes << prefix;
    }
    return prefixes;
}

bool pickAsset(const QByteArray &releaseJson, const QStringList &prefixes, StockfishAsset *out)
{
    const QJsonObject release = QJsonDocument::fromJson(releaseJson).object();
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QString &prefix : prefixes) {
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QString digest = asset.value(QStringLiteral("digest")).toString();
            const bool archive = name.endsWith(QStringLiteral(".zip")) ||
                                 name.endsWith(QStringLiteral(".tar.gz"));
            // Without a published digest the download cannot be verified.
            if (!name.startsWith(prefix) || !archive ||
                !digest.startsWith(QStringLiteral("sha256:"))) {
                continue;
            }
            out->tag = release.value(QStringLiteral("tag_name")).toString();
            out->name = name;
            out->url = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            out->sha256 = digest.mid(7).toLatin1().toLower();
            out->size = asset.value(QStringLiteral("size")).toInteger();
            return out->url.scheme() == QStringLiteral("https") && out->sha256.size() == 64;
        }
    }
    return false;
}

// sf_19 desktop assets as the API listed them (2026-10-11); refresh the tag,
// sizes and digests together when a newer Stockfish should be the fallback.
static const char kPinnedRelease[] = R"({
  "tag_name": "sf_19",
  "assets": [
    {"name": "stockfish-linux-arm64-universal.tar.gz", "size": 80181250,
     "digest": "sha256:fe26cfd1d9db4c8af3d21e24d9ff34cacb31c1f940085a7583da11796f2bac01",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-linux-arm64-universal.tar.gz"},
    {"name": "stockfish-linux-riscv64-universal.tar.gz", "size": 80241608,
     "digest": "sha256:d4b4d118c003bc669a939d9fb208e3ef6b367730fd79a70dbc278b18475df3d1",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-linux-riscv64-universal.tar.gz"},
    {"name": "stockfish-linux-x86-64-universal.tar.gz", "size": 81388977,
     "digest": "sha256:9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-linux-x86-64-universal.tar.gz"},
    {"name": "stockfish-macos-universal.tar.gz", "size": 82323876,
     "digest": "sha256:a1f0e3bcc5a6927a11fe6fc8e54a779754645f3c2bae2cf13420fd1957adaa77",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-macos-universal.tar.gz"},
    {"name": "stockfish-windows-arm64-universal.zip", "size": 80190536,
     "digest": "sha256:8372ad3f0d7276deb2c70f801f541ec7db463219fc6d9c7592864e542aa4f401",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-windows-arm64-universal.zip"},
    {"name": "stockfish-windows-x86-64-universal.zip", "size": 81431614,
     "digest": "sha256:3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7",
     "browser_download_url": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-windows-x86-64-universal.zip"}
  ]
})";

bool pickPinned(const QStringList &prefixes, StockfishAsset *out)
{
    return pickAsset(QByteArray(kPinnedRelease), prefixes, out);
}

QString findBinary(const QString &dir)
{
    QString best;
    qint64 bestSize = 0;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (info.fileName().startsWith(QStringLiteral("stockfish")) &&
            info.size() > bestSize) {
            best = info.absoluteFilePath();
            bestSize = info.size();
        }
    }
    return best;
}

} // namespace StockfishRelease
