#ifndef ENGINE_PLAYER_H
#define ENGINE_PLAYER_H

#include <QByteArray>
#include <QProcess>
#include <QString>

#include <functional>

// Minimal UCI client: one external engine process, one search at a time.
class EnginePlayer {
public:
    std::function<void(const QString &fen, const QString &move)> onBestMove;
    std::function<void(const QString &error)> onError;

    EnginePlayer();
    ~EnginePlayer();

    bool start(const QString &path, int skill);
    // Runs the binary once and returns its UCI "id name", or empty.
    static QString probeName(const QString &path);
    void stop();
    bool running() const;
    // UCI "id name", e.g. "Stockfish 19"; empty until the engine reports it.
    QString name() const { return name_; }
    void setSkill(int skill);
    // A newer request supersedes the search in progress; its result is dropped.
    void request(const QString &fen, int moveTimeMs);

private:
    void write(const QByteArray &command);
    void readOutput();
    void reportLoss(const QString &error);

    QProcess process_;
    QByteArray buffer_;
    QString name_;
    QString searchFen_;
    QString queuedFen_;
    int queuedMoveTimeMs_ = 0;
    bool searching_ = false;
    bool stopping_ = false;
};

#endif // ENGINE_PLAYER_H
