#include "engine_player.h"
#include "chess_helpers.h"

EnginePlayer::EnginePlayer()
{
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(&process_, &QProcess::readyReadStandardOutput,
                     &process_, [this]() { readOutput(); });
    // A crash is followed by finished(), so every loss of a started engine
    // is reported there, once; a binary that never started has no finished().
    QObject::connect(&process_, &QProcess::errorOccurred,
                     &process_, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) reportLoss(process_.errorString());
    });
    QObject::connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     &process_, [this](int exitCode, QProcess::ExitStatus status) {
        reportLoss(status == QProcess::CrashExit
                       ? QStringLiteral("engine crashed")
                       : QStringLiteral("engine exited with code %1").arg(exitCode));
    });
}

void EnginePlayer::reportLoss(const QString &error)
{
    searching_ = false;
    queuedFen_.clear();
    if (!stopping_ && onError) onError(error);
}

EnginePlayer::~EnginePlayer()
{
    stop();
}

bool EnginePlayer::start(const QString &path, int skill)
{
    stop();
    name_.clear();
    process_.start(path, QStringList());
    if (!process_.waitForStarted(3000)) {
        return false;
    }
    write("uci");
    setSkill(skill);
    write("isready");
    return true;
}

void EnginePlayer::stop()
{
    searching_ = false;
    queuedFen_.clear();
    buffer_.clear();
    if (process_.state() == QProcess::NotRunning) {
        return;
    }
    stopping_ = true;
    write("quit");
    if (!process_.waitForFinished(500)) {
        process_.kill();
        process_.waitForFinished(500);
    }
    stopping_ = false;
}

QString EnginePlayer::probeName(const QString &path)
{
    QProcess probe;
    probe.start(path, QStringList());
    if (!probe.waitForStarted(3000)) {
        return QString();
    }
    probe.write("uci\nquit\n");
    probe.closeWriteChannel();
    if (!probe.waitForFinished(5000)) {
        probe.kill();
        probe.waitForFinished(1000);
    }
    for (const QByteArray &line : probe.readAllStandardOutput().split('\n')) {
        if (line.startsWith("id name ")) {
            return QString::fromLatin1(line.mid(8)).trimmed();
        }
    }
    return QString();
}

bool EnginePlayer::running() const
{
    return process_.state() == QProcess::Running;
}

void EnginePlayer::setSkill(int skill)
{
    write("setoption name Skill Level value " + QByteArray::number(qBound(0, skill, 20)));
}

void EnginePlayer::request(const QString &fen, int moveTimeMs)
{
    if (!running()) {
        return;
    }
    if (searching_) {
        queuedFen_ = fen;
        queuedMoveTimeMs_ = moveTimeMs;
        write("stop");
        return;
    }
    searching_ = true;
    searchFen_ = fen;
    write("position fen " + fen.toLatin1());
    write("go movetime " + QByteArray::number(moveTimeMs));
}

void EnginePlayer::write(const QByteArray &command)
{
    if (process_.state() != QProcess::NotRunning) {
        process_.write(command + '\n');
    }
}

void EnginePlayer::readOutput()
{
    buffer_ += process_.readAllStandardOutput();
    int end;
    while ((end = buffer_.indexOf('\n')) >= 0) {
        const QByteArray line = buffer_.left(end);
        buffer_.remove(0, end + 1);
        if (line.startsWith("id name ")) {
            name_ = QString::fromLatin1(line.mid(8)).trimmed();
            continue;
        }
        if (stopping_ || !line.startsWith("bestmove")) {
            continue;
        }
        searching_ = false;
        if (!queuedFen_.isEmpty()) {
            const QString next = queuedFen_;
            queuedFen_.clear();
            request(next, queuedMoveTimeMs_);
            continue;
        }
        const QString move = ChessHelpers::uciBestMove(line);
        if (!move.isEmpty() && onBestMove) {
            onBestMove(searchFen_, move);
        }
    }
}
