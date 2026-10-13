// Game flow with Stockfish playing this side, end to end: a scripted DIRECT
// peer on a loopback socket and the fake UCI engine built next to this test
// (argv[1]), so no Stockfish download is involved.

#include <QAbstractButton>
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "pc/client/main_window.h"

#include <cstdio>
#include <functional>
#include <memory>

namespace {

bool waitUntil(const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer wait;
    wait.start();
    while (!done() && wait.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return done();
}

void pump(int ms)
{
    (void)waitUntil([]() { return false; }, ms);
}

struct EngineGame {
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    std::unique_ptr<MainWindow> window;
    QByteArray received;

    // Appends what the window sent so far and tells whether `needle` arrived.
    bool waitForLine(const QByteArray &needle, int timeoutMs)
    {
        return waitUntil([this, &needle]() {
            received.append(peer->readAll());
            return received.contains(needle);
        }, timeoutMs);
    }

    bool send(const QByteArray &line)
    {
        return peer->write(line) == line.size() && peer->waitForBytesWritten(2000);
    }

    void finish()
    {
        if (window) {
            window->testSocket()->abort();
        }
        if (peer) {
            peer->abort();
        }
        QCoreApplication::processEvents();
        window.reset();
    }
};

// The next MainWindow starts the fake engine from these stored preferences.
void storeEnginePreferences(const QString &enginePath, int pauseSeconds = 0)
{
    QSettings settings;
    settings.setValue(QStringLiteral("engine/path"), enginePath);
    settings.setValue(QStringLiteral("engine/shown"), true);
    settings.setValue(QStringLiteral("engine/plays"), true);
    settings.setValue(QStringLiteral("engine/level"), 7);
    settings.setValue(QStringLiteral("engine/delay"), pauseSeconds);
    settings.remove(QStringLiteral("player/name"));
    settings.sync();
}

void clearEnginePreferences()
{
    QSettings settings;
    settings.remove(QStringLiteral("engine"));
    settings.sync();
}

void setEnvOrUnset(const char *name, const QByteArray &value)
{
    if (value.isEmpty()) {
        qunsetenv(name);
    } else {
        qputenv(name, value);
    }
}

// Fake engine behaviour for the processes started from now on.
void setFakeEngineMode(const QByteArray &delayMs, const QString &crashOnceFile,
                       bool crashAlways, const QString &startLog,
                       const QByteArray &moves = QByteArray())
{
    setEnvOrUnset("SHATRANJ_FAKE_UCI_MOVES", moves);
    setEnvOrUnset("SHATRANJ_FAKE_UCI_DELAY_MS", delayMs);
    setEnvOrUnset("SHATRANJ_FAKE_UCI_CRASH_FILE", crashOnceFile.toLocal8Bit());
    setEnvOrUnset("SHATRANJ_FAKE_UCI_CRASH_ALWAYS", crashAlways ? QByteArray("1") : QByteArray());
    setEnvOrUnset("SHATRANJ_FAKE_UCI_START_LOG", startLog.toLocal8Bit());
}

int countLines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    return static_cast<int>(file.readAll().count('\n'));
}

// Window as DIRECT guest with the white pieces, STOCKFISH announced, game
// started: when this returns true the engine has been asked for move 1.
bool startEngineGame(EngineGame &game, const char *label)
{
    if (!game.server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "FAIL: %s: listen\n", label);
        return false;
    }
    game.window = std::make_unique<MainWindow>();
    game.window->testStartDirectGuestConnection(QStringLiteral("127.0.0.1"),
                                                game.server.serverPort());
    if (!waitUntil([&game]() { return game.server.hasPendingConnections(); }, 2000)) {
        std::fprintf(stderr, "FAIL: %s: guest did not connect\n", label);
        return false;
    }
    game.peer.reset(game.server.nextPendingConnection());
    if (!game.waitForLine(QByteArrayLiteral("HELLO DIRECT GUEST\n"), 2000)) {
        std::fprintf(stderr, "FAIL: %s: guest HELLO\n", label);
        return false;
    }
    game.received.clear();
    if (!game.send(QByteArrayLiteral("HELLO DIRECT HOST WHITE=GUEST\n")) ||
        !waitUntil([&game]() { return game.window->testSessionReady(); }, 2000) ||
        !game.waitForLine(QByteArrayLiteral("MNAME STOCKFISH\n"), 2000)) {
        std::fprintf(stderr, "FAIL: %s: session ready with STOCKFISH identity\n", label);
        return false;
    }
    if (!game.send(QByteArrayLiteral("GAME START WHITE=GUEST\n")) ||
        !game.waitForLine(QByteArrayLiteral("ACK GAME START\n"), 2000)) {
        std::fprintf(stderr, "FAIL: %s: game start\n", label);
        return false;
    }
    return true;
}

void answerActiveQuestionNo()
{
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (box == nullptr) {
        return;
    }
    if (QAbstractButton *no = box->button(QMessageBox::No)) {
        no->click();
    } else {
        box->reject();
    }
}

// 1. The engine answers while the operator's own Draw prompt is open. That
//    answer is dropped, so closing the prompt with No must ask again.
bool runPromptDropCase(const QString &enginePath)
{
    setFakeEngineMode("300", QString(), false, QString());
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "prompt drop")) {
        return false;
    }
    // bestmove lands at ~300 ms; the prompt stays open until 1200 ms.
    QTimer::singleShot(1200, &answerActiveQuestionNo);
    const bool prompted = game.window->testClickDrawButton();
    game.received.append(game.peer->readAll());
    const bool droppedWhileOpen = !game.received.contains("MOVE 1");
    const bool moved = game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000);
    if (moved) {
        (void)game.send(QByteArrayLiteral("ACK 1 e4\n"));
    }
    game.finish();
    const bool ok = prompted && droppedWhileOpen && moved;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: prompt drop: prompted=%d dropped=%d moved=%d\n",
                     prompted ? 1 : 0, droppedWhileOpen ? 1 : 0, moved ? 1 : 0);
    }
    return ok;
}

// 2. The engine dies before answering. The client restarts it once, keeps the
//    STOCKFISH identity and its single announcement, and the move arrives.
bool runEngineLossRestartCase(const QString &enginePath, const QTemporaryDir &dir)
{
    const QString startLog = dir.filePath(QStringLiteral("restart-starts.log"));
    setFakeEngineMode("0", dir.filePath(QStringLiteral("crash-once")), false, startLog);
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "engine loss")) {
        return false;
    }
    const bool moved = game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000);
    if (moved) {
        (void)game.send(QByteArrayLiteral("ACK 1 e4\n"));
    }
    pump(200);
    game.received.append(game.peer->readAll());
    const int starts = countLines(startLog);
    const bool identityKept = game.received.count("MNAME ") == 1 &&
                              game.received.count("CHAT ") == 1 &&
                              game.received.contains("PLAYS WHITE - LEVEL 7\n");
    game.finish();
    const bool ok = moved && starts == 2 && identityKept;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: engine loss: moved=%d starts=%d identity=%d\n",
                     moved ? 1 : 0, starts, identityKept ? 1 : 0);
    }
    return ok;
}

// 3. The engine dies again right after the restart. The client gives up: the
//    turn label says so instead of "thinking", nothing else is restarted, and
//    the session stays up for the operator to resign or disconnect.
bool runEngineLossGiveUpCase(const QString &enginePath, const QTemporaryDir &dir)
{
    const QString startLog = dir.filePath(QStringLiteral("giveup-starts.log"));
    setFakeEngineMode("0", QString(), true, startLog);
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "engine give-up")) {
        return false;
    }
    const bool stopped = waitUntil([&game]() {
        return game.window->testTurnLabelText().startsWith(QStringLiteral("STOCKFISH STOPPED"));
    }, 3000);
    pump(500);
    game.received.append(game.peer->readAll());
    const int starts = countLines(startLog);
    const bool quiet = !game.received.contains("MOVE 1");
    const bool sessionUp = game.window->testSessionReady();
    const QString label = game.window->testTurnLabelText();
    game.finish();
    const bool ok = stopped && starts == 2 && quiet && sessionUp;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: engine give-up: label=\"%s\" starts=%d quiet=%d session=%d\n",
                     label.toUtf8().constData(), starts, quiet ? 1 : 0, sessionUp ? 1 : 0);
    }
    return ok;
}

QMessageBox *activeQuestion()
{
    return qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
}

// 4. The peer takes back its last move while the engine is thinking and the
//    operator accepts: the stale answer is dropped, the position goes back,
//    the peer's new move is accepted again and the engine answers that one.
bool runPeerTakebackWhileThinkingCase(const QString &enginePath, bool withdraw)
{
    setFakeEngineMode("300", QString(), false, QString(), "e2e4,g1f3");
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "peer takeback")) {
        return false;
    }
    if (!game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000) ||
        !game.send(QByteArrayLiteral("ACK 1 e4\n")) ||
        !game.send(QByteArrayLiteral("MOVE 2 e7e5\n")) ||
        !game.waitForLine(QByteArrayLiteral("ACK 2"), 3000)) {
        std::fputs("FAIL: peer takeback: opening exchange\n", stderr);
        game.finish();
        return false;
    }
    // The engine is asked now; its answer lands at ~300 ms, inside the prompt.
    QElapsedTimer sinceAsk;
    sinceAsk.start();
    if (!game.send(QByteArrayLiteral("TAKEBACK 2\n")) ||
        !waitUntil([]() { return activeQuestion() != nullptr; }, 2000)) {
        std::fputs("FAIL: peer takeback: no decision prompt\n", stderr);
        game.finish();
        return false;
    }
    (void)waitUntil([&sinceAsk]() { return sinceAsk.elapsed() >= 700; }, 1000);
    game.received.append(game.peer->readAll());
    const bool heldWhileAsked = !game.received.contains("MOVE 3");
    if (withdraw) {
        const bool cancelled = game.send(QByteArrayLiteral("CANCEL TAKEBACK 2\n")) &&
            game.waitForLine(QByteArrayLiteral("NACK 2\n"), 2000) &&
            waitUntil([]() { return activeQuestion() == nullptr; }, 1000);
        const bool answered = game.waitForLine(QByteArrayLiteral("MOVE 3 g1f3\n"), 3000);
        const bool unchanged = game.received.count("\nACK 2") == 1 &&
                               game.window->testSessionReady();
        if (answered) {
            (void)game.send(QByteArrayLiteral("ACK 3 Nf3\n"));
        }
        game.finish();
        const bool ok = heldWhileAsked && cancelled && answered && unchanged;
        if (!ok) {
            std::fprintf(stderr,
                         "FAIL: withdrawn takeback: held=%d cancelled=%d answered=%d unchanged=%d\n",
                         heldWhileAsked, cancelled, answered, unchanged);
        }
        return ok;
    }
    if (QMessageBox *box = activeQuestion()) {
        box->button(QMessageBox::Yes)->click();
    }
    const bool takebackAcked = waitUntil([&game]() {
        game.received.append(game.peer->readAll());
        return game.received.count("ACK 2") >= 2;
    }, 2000);
    const bool heldAfterTakeback = !game.received.contains("MOVE 3");
    const bool replayed = game.send(QByteArrayLiteral("MOVE 2 d7d5\n")) &&
                          waitUntil([&game]() {
                              game.received.append(game.peer->readAll());
                              return game.received.count("ACK 2") >= 3;
                          }, 3000);
    const bool answered = game.waitForLine(QByteArrayLiteral("MOVE 3 g1f3\n"), 3000);
    if (answered) {
        (void)game.send(QByteArrayLiteral("ACK 3 Nf3\n"));
    }
    game.finish();
    const bool ok = heldWhileAsked && takebackAcked && heldAfterTakeback &&
                    replayed && answered;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: peer takeback: held=%d acked=%d heldAfter=%d "
                     "replayed=%d answered=%d\n%s\n",
                     heldWhileAsked ? 1 : 0, takebackAcked ? 1 : 0,
                     heldAfterTakeback ? 1 : 0, replayed ? 1 : 0,
                     answered ? 1 : 0, game.received.constData());
    }
    return ok;
}

// 5. The peer rejects the engine's move again and again (boards out of sync):
//    the engine is asked a bounded number of times, then the client stops and
//    says so, leaving the session up for the operator.
bool runPeerRejectsEngineMoveCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString());
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "peer rejects")) {
        return false;
    }
    int rejected = 0;
    while (rejected < 3) {
        if (!waitUntil([&game, rejected]() {
                game.received.append(game.peer->readAll());
                return game.received.count("MOVE 1 e2e4\n") > rejected;
            }, 3000) ||
            !game.send(QByteArrayLiteral("NACK 1 SYNC\n"))) {
            std::fprintf(stderr, "FAIL: peer rejects: move %d not resent\n", rejected + 1);
            game.finish();
            return false;
        }
        ++rejected;
    }
    const bool stopped = waitUntil([&game]() {
        return game.window->testTurnLabelText().startsWith(QStringLiteral("STOCKFISH STOPPED"));
    }, 2000);
    pump(1500);
    game.received.append(game.peer->readAll());
    const int sent = static_cast<int>(game.received.count("MOVE 1 e2e4\n"));
    const bool sessionUp = game.window->testSessionReady();
    const QString label = game.window->testTurnLabelText();
    game.finish();
    const bool ok = stopped && sent == 3 && sessionUp;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: peer rejects: label=\"%s\" sent=%d session=%d\n",
                     label.toUtf8().constData(), sent, sessionUp ? 1 : 0);
    }
    return ok;
}

// 6. The chat announcement keeps retrying while the game runs instead of
//    giving up after a few refusals.
bool runAnnouncementRetryCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString());
    storeEnginePreferences(enginePath);
    MainWindow window;
    const bool ok = window.testEngineAnnounceKeepsRetrying();
    if (!ok) {
        std::fputs("FAIL: engine announcement gave up while the game was running\n", stderr);
    }
    return ok;
}

// 7. Stockfish plays this side, as White. After an accepted RESET it moves at
//    once, so the peer's RESET retry that crossed our ACK arrives after move 1
//    of the new game. That retry must neither ask again nor restart the game
//    the engine already began: the peer has that move.
bool runResetRetryAfterEngineMoveCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString());
    storeEnginePreferences(enginePath);
    EngineGame game;
    if (!startEngineGame(game, "reset retry after engine move")) {
        return false;
    }
    const auto question = []() { return activeQuestion() != nullptr; };
    bool ok = game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000) &&
              game.send(QByteArrayLiteral("ACK 1 e4\n"));
    game.received.clear();
    ok = ok && game.send(QByteArrayLiteral("RESET\n")) && waitUntil(question, 2000);
    if (QMessageBox *box = activeQuestion()) {
        box->button(QMessageBox::Yes)->click();
    }
    ok = ok && game.waitForLine(QByteArrayLiteral("ACK RESET\n"), 2000) &&
         game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000);
    if (!ok) {
        std::fputs("FAIL: reset retry after engine move: accepted reset and move 1\n", stderr);
        game.finish();
        return false;
    }
    const QString log = game.window->testChatLogText();
    game.received.clear();
    const bool again = game.send(QByteArrayLiteral("RESET\n")) && waitUntil(question, 800);
    if (QMessageBox *box = activeQuestion()) {
        // What a player does with the same question asked twice.
        box->button(QMessageBox::Yes)->click();
    }
    pump(500);
    game.received.append(game.peer->readAll());
    const bool restarted = game.received.contains("MOVE 1") ||
                           game.window->testChatLogText() != log;
    game.finish();
    if (again || restarted) {
        std::fprintf(stderr,
                     "FAIL: reset retry after engine move: prompted_again=%d "
                     "restarted=%d\n%s\n",
                     again, restarted, game.received.constData());
        return false;
    }
    return true;
}

// 8. A requester resends its request every 2.5 s until our reply arrives, so
//    a retry can cross the reply. The same request answered moments ago gets
//    the same reply again, never a second prompt; a genuine request after a
//    move or after the replay window still asks.
bool runRetryCrossesAnswerCase()
{
    clearEnginePreferences();
    EngineGame game;
    if (!game.server.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: retry crosses answer: listen\n", stderr);
        return false;
    }
    game.window = std::make_unique<MainWindow>();
    game.window->testStartDirectGuestConnection(QStringLiteral("127.0.0.1"),
                                                game.server.serverPort());
    if (!waitUntil([&game]() { return game.server.hasPendingConnections(); }, 2000)) {
        std::fputs("FAIL: retry crosses answer: guest did not connect\n", stderr);
        return false;
    }
    game.peer.reset(game.server.nextPendingConnection());
    const auto prompted = [](int ms) {
        return waitUntil([]() { return activeQuestion() != nullptr; }, ms);
    };
    const auto answer = [](QMessageBox::StandardButton button) {
        if (QMessageBox *box = activeQuestion()) {
            box->button(button)->click();
        }
    };
    // Sends `request`, answers its prompt, then resends it: the retry must be
    // answered with exactly `reply` again and open no prompt.
    const auto crossed = [&](const char *label, const QByteArray &request,
                             QMessageBox::StandardButton button,
                             const QByteArray &reply) {
        game.received.clear();
        const bool asked = game.send(request) && prompted(2000);
        answer(button);
        const bool replied = asked && game.waitForLine(reply, 2000);
        // The retry leaves the game as the answer left it: no second RESET
        // in the log and no restart clearing the player's chat draft.
        const QString log = game.window->testChatLogText();
        game.window->testSetChatDraft(QStringLiteral("draft"));
        game.received.clear();
        const bool resent = replied && game.send(request);
        const bool again = resent && prompted(800);
        if (again) {
            answer(QMessageBox::No);
        }
        const bool same = resent && game.waitForLine(reply, 2000) &&
                          (reply.startsWith("NACK") || !game.received.contains("NACK"));
        const bool kept = game.window->testChatLogText() == log &&
                          game.window->testChatDraft() == QStringLiteral("draft");
        game.window->testSetChatDraft(QString());
        if (!asked || !replied || again || !same || !kept) {
            std::fprintf(stderr,
                         "FAIL: retry crosses answer: %s: asked=%d replied=%d "
                         "prompted_again=%d same_reply=%d game_kept=%d\n%s\n",
                         label, asked, replied, again, same, kept,
                         game.received.constData());
            return false;
        }
        return true;
    };
    bool ok = game.waitForLine(QByteArrayLiteral("HELLO DIRECT GUEST\n"), 2000) &&
        game.send(QByteArrayLiteral("HELLO DIRECT HOST WHITE=HOST\n")) &&
        waitUntil([&game]() { return game.window->testSessionReady(); }, 2000) &&
        game.send(QByteArrayLiteral("GAME START WHITE=HOST\n")) &&
        game.waitForLine(QByteArrayLiteral("ACK GAME START\n"), 2000);
    if (!ok) {
        std::fputs("FAIL: retry crosses answer: game start\n", stderr);
    }
    ok = ok && crossed("declined RESET", QByteArrayLiteral("RESET\n"),
                       QMessageBox::No, QByteArrayLiteral("NACK RESET\n"));
    ok = ok && crossed("declined LOAD", QByteArrayLiteral("RQ\n"),
                       QMessageBox::No, QByteArrayLiteral("RN\n"));
    if (ok) {
        // Past the replay window the same request is a new one again.
        pump(3200);
        game.received.clear();
        const bool asked = game.send(QByteArrayLiteral("RQ\n")) && prompted(2000);
        answer(QMessageBox::No);
        ok = asked && game.waitForLine(QByteArrayLiteral("RN\n"), 2000);
        if (!ok) {
            std::fputs("FAIL: retry crosses answer: LOAD after the window asks again\n", stderr);
        }
    }
    ok = ok && game.send(QByteArrayLiteral("MOVE 1 e2e4\n")) &&
         game.waitForLine(QByteArrayLiteral("ACK 1"), 2000);
    ok = ok && crossed("declined TAKEBACK", QByteArrayLiteral("TAKEBACK 1\n"),
                       QMessageBox::No, QByteArrayLiteral("NACK 1"));
    if (ok) {
        game.received.clear();
        ok = game.send(QByteArrayLiteral("DRAW\n")) && prompted(2000);
        answer(QMessageBox::Yes);
        ok = ok && game.waitForLine(QByteArrayLiteral("ACK DRAW\n"), 2000);
        if (!ok) {
            std::fputs("FAIL: retry crosses answer: draw\n", stderr);
        }
    }
    ok = ok && crossed("accepted RESET after DRAW", QByteArrayLiteral("RESET\n"),
                       QMessageBox::Yes, QByteArrayLiteral("ACK RESET\n"));
    if (ok) {
        // A move ends the replay: the next RESET is a genuine request.
        game.received.clear();
        const bool asked = game.send(QByteArrayLiteral("MOVE 1 e2e4\n")) &&
            game.waitForLine(QByteArrayLiteral("ACK 1"), 2000) &&
            game.send(QByteArrayLiteral("RESET\n")) && prompted(2000);
        answer(QMessageBox::No);
        ok = asked;
        if (!ok) {
            std::fputs("FAIL: retry crosses answer: RESET after a move asks again\n", stderr);
        }
    }
    game.finish();
    return ok;
}

// 9. With a one-second pause the engine's answer is held that long, while the
//    turn label keeps saying Stockfish is thinking, and then sent as usual.
bool runPausedMoveCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString());
    storeEnginePreferences(enginePath, 1);
    EngineGame game;
    if (!startEngineGame(game, "paused move")) {
        return false;
    }
    QElapsedTimer sinceAsk;
    sinceAsk.start();
    pump(600);
    game.received.append(game.peer->readAll());
    const bool held = !game.received.contains("MOVE 1");
    const QString label = game.window->testTurnLabelText();
    const bool thinking = label.startsWith(QStringLiteral("STOCKFISH THINKING"));
    const bool moved = game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000);
    const qint64 elapsed = sinceAsk.elapsed();
    if (moved) {
        (void)game.send(QByteArrayLiteral("ACK 1 e4\n"));
    }
    game.finish();
    // A coarse QTimer may fire a few percent early.
    const bool ok = held && thinking && moved && elapsed >= 900;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: paused move: held=%d label=\"%s\" moved=%d after=%lld ms\n",
                     held ? 1 : 0, label.toUtf8().constData(), moved ? 1 : 0,
                     static_cast<long long>(elapsed));
    }
    return ok;
}

// 10. The peer takes back its move while the engine's answer waits out the
//     pause and the operator accepts: that answer is never sent, and after
//     the peer's new move the engine answers the new position, again paused.
bool runTakebackDuringPauseCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString(), "e2e4,g1f3");
    storeEnginePreferences(enginePath, 1);
    EngineGame game;
    if (!startEngineGame(game, "takeback during pause")) {
        return false;
    }
    if (!game.waitForLine(QByteArrayLiteral("MOVE 1 e2e4\n"), 3000) ||
        !game.send(QByteArrayLiteral("ACK 1 e4\n")) ||
        !game.send(QByteArrayLiteral("MOVE 2 e7e5\n")) ||
        !game.waitForLine(QByteArrayLiteral("ACK 2"), 3000)) {
        std::fputs("FAIL: takeback during pause: opening exchange\n", stderr);
        game.finish();
        return false;
    }
    // The engine answers g1f3 at once; that answer now waits out the pause.
    pump(200);
    if (!game.send(QByteArrayLiteral("TAKEBACK 2\n")) ||
        !waitUntil([]() { return activeQuestion() != nullptr; }, 2000)) {
        std::fputs("FAIL: takeback during pause: no decision prompt\n", stderr);
        game.finish();
        return false;
    }
    if (QMessageBox *box = activeQuestion()) {
        box->button(QMessageBox::Yes)->click();
    }
    const bool takebackAcked = waitUntil([&game]() {
        game.received.append(game.peer->readAll());
        return game.received.count("ACK 2") >= 2;
    }, 2000);
    // Well past the pause: the dropped answer never surfaces.
    pump(1300);
    game.received.append(game.peer->readAll());
    const bool dropped = !game.received.contains("MOVE 3");
    const bool replayed = game.send(QByteArrayLiteral("MOVE 2 d7d5\n")) &&
                          waitUntil([&game]() {
                              game.received.append(game.peer->readAll());
                              return game.received.count("ACK 2") >= 3;
                          }, 3000);
    QElapsedTimer sinceReplay;
    sinceReplay.start();
    const bool answered = game.waitForLine(QByteArrayLiteral("MOVE 3 g1f3\n"), 3000);
    const bool paused = sinceReplay.elapsed() >= 900;
    const int sent = static_cast<int>(game.received.count("MOVE 3"));
    if (answered) {
        (void)game.send(QByteArrayLiteral("ACK 3 Nf3\n"));
    }
    game.finish();
    const bool ok = takebackAcked && dropped && replayed && answered && paused && sent == 1;
    if (!ok) {
        std::fprintf(stderr,
                     "FAIL: takeback during pause: acked=%d dropped=%d replayed=%d "
                     "answered=%d paused=%d sent=%d\n%s\n",
                     takebackAcked ? 1 : 0, dropped ? 1 : 0, replayed ? 1 : 0,
                     answered ? 1 : 0, paused ? 1 : 0, sent, game.received.constData());
    }
    return ok;
}

// 11. The operator disconnects while the engine's answer waits out the
//     pause: the move is never sent, neither then nor when the pause ends.
bool runDisconnectDuringPauseCase(const QString &enginePath)
{
    setFakeEngineMode("0", QString(), false, QString());
    storeEnginePreferences(enginePath, 1);
    EngineGame game;
    if (!startEngineGame(game, "disconnect during pause")) {
        return false;
    }
    pump(200);
    QTimer::singleShot(100, []() {
        if (QMessageBox *box = activeQuestion()) {
            box->button(QMessageBox::Yes)->click();
        }
    });
    game.window->testClickConnectButton();
    const bool bye = game.waitForLine(QByteArrayLiteral("BYE"), 2000);
    pump(1500);
    game.received.append(game.peer->readAll());
    const bool quiet = !game.received.contains("MOVE 1");
    game.finish();
    const bool ok = bye && quiet;
    if (!ok) {
        std::fprintf(stderr, "FAIL: disconnect during pause: bye=%d quiet=%d\n%s\n",
                     bye ? 1 : 0, quiet ? 1 : 0, game.received.constData());
    }
    return ok;
}

} // namespace

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ShatranjEngineFlowTest"));
    QApplication::setOrganizationName(QStringLiteral("NetChessZXTests"));

    if (argc < 2 || !QFile::exists(QString::fromLocal8Bit(argv[1]))) {
        std::fputs("usage: shatranj-engine-flow-test <fake-uci-engine>\n", stderr);
        return 2;
    }
    const QString enginePath = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::fputs("FAIL: temporary directory\n", stderr);
        return 1;
    }

    bool ok = runPromptDropCase(enginePath);
    ok = runEngineLossRestartCase(enginePath, dir) && ok;
    ok = runEngineLossGiveUpCase(enginePath, dir) && ok;
    ok = runPeerTakebackWhileThinkingCase(enginePath, false) && ok;
    ok = runPeerTakebackWhileThinkingCase(enginePath, true) && ok;
    ok = runPeerRejectsEngineMoveCase(enginePath) && ok;
    ok = runAnnouncementRetryCase(enginePath) && ok;
    ok = runResetRetryAfterEngineMoveCase(enginePath) && ok;
    ok = runRetryCrossesAnswerCase() && ok;
    ok = runPausedMoveCase(enginePath) && ok;
    ok = runTakebackDuringPauseCase(enginePath) && ok;
    ok = runDisconnectDuringPauseCase(enginePath) && ok;
    clearEnginePreferences();
    if (!ok) {
        return 1;
    }
    std::printf("engine flow tests ok\n");
    return 0;
}
