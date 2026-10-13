#include "engine_installer.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QSysInfo>

namespace {

QString enginesRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           QStringLiteral("/engines");
}

QString hostOs()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString tarProgram()
{
#if defined(Q_OS_WIN)
    // System32 bsdtar reads .zip; a GNU tar earlier on PATH would not.
    return qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")) +
           QStringLiteral("\\System32\\tar.exe");
#else
    return QStringLiteral("tar");
#endif
}

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("shatranj-client"));
    return request;
}

} // namespace

EngineInstaller::EngineInstaller()
{
    QObject::connect(&tar_, &QProcess::finished, &tar_,
                     [this](int exitCode, QProcess::ExitStatus) { extractFinished(exitCode); });
    QObject::connect(&tar_, &QProcess::errorOccurred, &tar_,
                     [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("cannot run %1").arg(tarProgram()));
        }
    });
}

EngineInstaller::~EngineInstaller()
{
    cancel();
}

QString EngineInstaller::installedBinary()
{
    return StockfishRelease::findBinary(enginesRoot());
}

QString EngineInstaller::installedTag()
{
    const QString binary = installedBinary();
    if (binary.isEmpty()) {
        return QString();
    }
    const QString relative = QDir(enginesRoot()).relativeFilePath(binary);
    return relative.section(QLatin1Char('/'), 0, 0);
}

bool EngineInstaller::isManaged(const QString &binaryPath)
{
    return !binaryPath.isEmpty() &&
           QFileInfo(binaryPath).absoluteFilePath().startsWith(
               QDir(enginesRoot()).absolutePath() + QLatin1Char('/'));
}

bool EngineInstaller::uninstall()
{
    return QDir(enginesRoot()).removeRecursively();
}

void EngineInstaller::checkLatest()
{
    cancel();
    reply_ = net_.get(githubRequest(QUrl(QString::fromLatin1(StockfishRelease::kLatestUrl))));
    QObject::connect(reply_, &QNetworkReply::finished, reply_, [this]() {
        QNetworkReply *reply = reply_;
        reply_ = nullptr;
        reply->deleteLater();
        StockfishAsset asset;
        const QStringList prefixes = StockfishRelease::platformPrefixes(
            hostOs(), QSysInfo::currentCpuArchitecture(), QSysInfo::buildCpuArchitecture());
        if (reply->error() != QNetworkReply::NoError) {
            // Usually the API's per-IP limit (403): install the pinned release.
            if (!StockfishRelease::pickPinned(prefixes, &asset)) {
                fail(reply->errorString());
                return;
            }
        } else if (!StockfishRelease::pickAsset(reply->readAll(), prefixes, &asset)) {
            fail(QStringLiteral("no verified Stockfish build for %1").arg(prefixes.join(", ")));
            return;
        }
        if (onReleaseFound) onReleaseFound(asset);
    });
}

void EngineInstaller::install(const StockfishAsset &asset)
{
    cancel();
    asset_ = asset;
    QDir().mkpath(enginesRoot());
    archive_.setFileName(enginesRoot() + QLatin1Char('/') + asset.name);
    if (!archive_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(archive_.errorString());
        return;
    }
    hash_.reset();
    reply_ = net_.get(githubRequest(asset.url));
    QObject::connect(reply_, &QNetworkReply::readyRead, reply_, [this]() {
        const QByteArray chunk = reply_->readAll();
        hash_.addData(chunk);
        if (archive_.write(chunk) != chunk.size()) {
            fail(archive_.errorString());
        }
    });
    QObject::connect(reply_, &QNetworkReply::downloadProgress, reply_,
                     [this](qint64 received, qint64 total) {
        if (onProgress) onProgress(received, total > 0 ? total : asset_.size);
    });
    QObject::connect(reply_, &QNetworkReply::finished, reply_, [this]() { downloadFinished(); });
}

void EngineInstaller::cancel()
{
    if (reply_ != nullptr) {
        QNetworkReply *reply = reply_;
        reply_ = nullptr;
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }
    if (tar_.state() != QProcess::NotRunning) {
        tar_.kill();
        tar_.waitForFinished(1000);
    }
    if (archive_.isOpen()) {
        archive_.close();
        archive_.remove();
    }
}

void EngineInstaller::fail(const QString &error)
{
    cancel();
    if (onFailed) onFailed(error);
}

void EngineInstaller::downloadFinished()
{
    QNetworkReply *reply = reply_;
    reply_ = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(reply->errorString());
        return;
    }
    const QByteArray tail = reply->readAll();
    hash_.addData(tail);
    archive_.write(tail);
    archive_.close();
    if (hash_.result().toHex() != asset_.sha256) {
        archive_.remove();
        fail(QStringLiteral("SHA-256 mismatch for %1").arg(asset_.name));
        return;
    }
    unpackDir_ = enginesRoot() + QStringLiteral("/unpack");
    QDir(unpackDir_).removeRecursively();
    QDir().mkpath(unpackDir_);
    tar_.start(tarProgram(), { QStringLiteral("-xf"), archive_.fileName(),
                               QStringLiteral("-C"), unpackDir_ });
}

void EngineInstaller::extractFinished(int exitCode)
{
    archive_.remove();
    const QString root = enginesRoot();
    const QString target = root + QLatin1Char('/') + asset_.tag;
    if (exitCode != 0 || StockfishRelease::findBinary(unpackDir_).isEmpty()) {
        QDir(unpackDir_).removeRecursively();
        fail(QStringLiteral("cannot unpack %1").arg(asset_.name));
        return;
    }
    // Keep only the new release so installedBinary() finds one engine.
    for (const QString &old : QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (old != QStringLiteral("unpack")) {
            QDir(root + QLatin1Char('/') + old).removeRecursively();
        }
    }
    if (!QDir().rename(unpackDir_, target)) {
        fail(QStringLiteral("cannot install into %1").arg(target));
        return;
    }
    const QString binary = StockfishRelease::findBinary(target);
    QFile::setPermissions(binary, QFile::permissions(binary) |
                                  QFileDevice::ExeOwner | QFileDevice::ExeUser);
    if (onInstalled) onInstalled(binary);
}
