#ifndef STOCKFISH_RELEASE_H
#define STOCKFISH_RELEASE_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// Pure helpers for the on-demand Stockfish download (GitHub latest release).
struct StockfishAsset {
    QString tag;
    QString name;
    QUrl url;
    QByteArray sha256; // lowercase hex from the release "digest" field
    qint64 size = 0;
};

namespace StockfishRelease {

constexpr const char *kLatestUrl =
    "https://api.github.com/repos/official-stockfish/Stockfish/releases/latest";

// Asset name prefixes in preference order, e.g. "stockfish-windows-x86-64".
QStringList platformPrefixes(const QString &os, const QString &cpu, const QString &buildCpu);
bool pickAsset(const QByteArray &releaseJson, const QStringList &prefixes, StockfishAsset *out);
// Fallback when the API refuses the release query (it allows 60 unauthenticated
// requests an hour per public IP): a known release with its published digests.
// Release downloads are not under that limit.
bool pickPinned(const QStringList &prefixes, StockfishAsset *out);
// The engine is the largest file named stockfish* in the unpacked tree.
QString findBinary(const QString &dir);

} // namespace StockfishRelease

#endif // STOCKFISH_RELEASE_H
