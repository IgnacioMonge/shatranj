#ifndef ENGINE_INSTALLER_H
#define ENGINE_INSTALLER_H

#include "stockfish_release.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>

#include <functional>

class QNetworkReply;

// Downloads the latest official Stockfish build for this machine into the
// per-user data directory. Each step reports through one callback.
class EngineInstaller {
public:
    std::function<void(const StockfishAsset &)> onReleaseFound;
    std::function<void(qint64 received, qint64 total)> onProgress;
    std::function<void(const QString &binaryPath)> onInstalled;
    std::function<void(const QString &error)> onFailed;

    EngineInstaller();
    ~EngineInstaller();

    static QString installedBinary();
    // Release tag of the managed install (its folder name), or empty.
    static QString installedTag();
    // True when the binary lives in the managed engines folder.
    static bool isManaged(const QString &binaryPath);
    // Deletes the managed engines folder; the engine must not be running.
    static bool uninstall();
    void checkLatest();
    void install(const StockfishAsset &asset);
    void cancel();

private:
    void fail(const QString &error);
    void downloadFinished();
    void extractFinished(int exitCode);

    QNetworkAccessManager net_;
    QPointer<QNetworkReply> reply_;
    QProcess tar_;
    QFile archive_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    StockfishAsset asset_;
    QString unpackDir_;
};

#endif // ENGINE_INSTALLER_H
