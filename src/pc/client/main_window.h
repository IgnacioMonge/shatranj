#pragma once

#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTcpSocket>

#include <memory>

class MainWindowImpl;

QByteArray mqttClientIdFor(bool host, quint64 nonce);
QStringList directIpHistoryWith(const QStringList &history, const QString &host);

inline constexpr int kDirectConnectRetryIntervalMs = 2000;
inline constexpr int kDirectIpHistoryMax = 8;
inline constexpr char kDirectIpHistorySettingsKey[] = "connection/directIpHistory";

class MainWindow final {
public:
    MainWindow();
    ~MainWindow();

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;

    static QIcon appIcon();
    void setWindowIcon(const QIcon &icon);
    void showNormal();

    // Integration-test seam. The shared UI target provides these methods so
    // the production executable and the MQTT lifecycle test reuse one build.
    QTcpSocket *testSocket() const;
    bool testPrepareMqttGuestSession();
    bool testPrepareMqttHostSession();
    bool testEndAndRelinkMqttSession();
    bool testPrepareMqttGuestBootstrap();
    void testFeedMqtt(const QByteArray &suffix, const QByteArray &payload,
                      bool retained);
    bool testBeginMqttRestore();
    bool testRestoreUiIdle() const;
    void testSetMqttWriteFailure(bool enabled);
    bool testSessionReady() const;
    QString testStatusContextText() const;
    bool testStatusBarAligned();
    bool testDisconnectButtonAvailable() const;
    QByteArray testMqttClientId(bool host) const;
    void testHandleMqttPacket(const QByteArray &packet);
    QHash<uint16_t, QString> testMqttPendingSubacks() const;
    QHash<uint16_t, QString> testMqttPendingUnsubacks() const;
    QSet<QString> testMqttActiveSubscriptions() const;
    bool testMqttOperational() const;
    void testStartDirectGuestConnection(const QString &host, quint16 port);
    bool testDirectRetryPending() const;
    bool testStartDirectHostListener(quint16 port);
    bool testDirectListenerActive() const;
    void testClickConnectButton();
    bool testReplaceDirectClientBeforeDisconnect();
    bool testResignRestartUiProjection();
    bool testRestoredMoveProjection();
    bool testSessionEndPresentation();
    bool testCancelPendingPieceFlash();

private:
    std::unique_ptr<MainWindowImpl> impl_;
};
