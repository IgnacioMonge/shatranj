#include <QAbstractSocket>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QSettings>
#include <QTcpServer>
#include <QTimer>
#include <QVector>

#include "common/session/session.h"
#include "pc/client/desktop_transport_codec.h"
#include "pc/client/main_window.h"

#include <cstdio>
#include <memory>

namespace {

bool portableClientId(const QByteArray &clientId)
{
    if (clientId.isEmpty() || clientId.size() > 23) {
        return false;
    }
    for (char ch : clientId) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
            return false;
        }
    }
    return true;
}

QByteArray expectedLocalMachPayload()
{
#if defined(Q_OS_MACOS)
    return QByteArrayLiteral("MACH MAC");
#elif defined(Q_OS_LINUX)
    return QByteArrayLiteral("MACH LNX");
#elif defined(Q_OS_WIN)
    return QByteArrayLiteral("MACH PC");
#else
    return QByteArray();
#endif
}

QByteArray takeDirectBytes(QTcpSocket *peer)
{
    QElapsedTimer wait;
    QElapsedTimer idle;
    wait.start();
    idle.start();
    QByteArray bytes;
    while (wait.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (peer->bytesAvailable() != 0) {
            bytes.append(peer->readAll());
            idle.restart();
        } else if (!bytes.isEmpty() && idle.elapsed() >= 50) {
            break;
        }
    }
    return bytes;
}

QByteArray mqttAckPacket(bool unsubscribe, uint16_t packetId,
                         uint8_t returnCode = 0u)
{
    QByteArray packet;
    packet.append(static_cast<char>(unsubscribe ? 0xb0u : 0x90u));
    packet.append(static_cast<char>(unsubscribe ? 2u : 3u));
    packet.append(static_cast<char>(packetId >> 8));
    packet.append(static_cast<char>(packetId));
    if (!unsubscribe) {
        packet.append(static_cast<char>(returnCode));
    }
    return packet;
}

QVector<QByteArray> takeMqttPackets(QTcpSocket *peer)
{
    QCoreApplication::processEvents();
    if (peer->bytesAvailable() == 0) {
        peer->waitForReadyRead(500);
    }
    QByteArray bytes = peer->readAll();
    while (peer->waitForReadyRead(10)) {
        bytes.append(peer->readAll());
    }

    DesktopTransportCodec codec;
    bool malformed = false;
    const QVector<QByteArray> packets = codec.feedMqtt(bytes, &malformed);
    return malformed ? QVector<QByteArray>() : packets;
}

int mqttPacketCount(const QVector<QByteArray> &packets, uint8_t type)
{
    int count = 0;
    for (const QByteArray &packet : packets) {
        if (!packet.isEmpty() &&
            (static_cast<uint8_t>(packet.at(0)) >> 4) == type) {
            ++count;
        }
    }
    return count;
}

int mqttPublishPayloadCount(const QVector<QByteArray> &packets,
                            const QByteArray &payload)
{
    int count = 0;
    for (const QByteArray &raw : packets) {
        DesktopTransportCodec::MqttPacket packet;
        if (DesktopTransportCodec::decodeMqttPacket(raw, &packet) &&
            packet.type == DesktopTransportCodec::MqttPacketType::Publish &&
            packet.payload == payload) {
            ++count;
        }
    }
    return count;
}

bool connectMqttTestPeer(MainWindow &window, QTcpServer &server,
                         std::unique_ptr<QTcpSocket> &peer,
                         bool bootstrap = false, bool host = false)
{
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return false;
    }
    QTcpSocket *socket = window.testSocket();
    socket->abort();
    socket->connectToHost(QHostAddress::LocalHost, server.serverPort());
    if (!socket->waitForConnected(2000) ||
        !server.waitForNewConnection(2000)) {
        return false;
    }
    peer.reset(server.nextPendingConnection());
    return peer != nullptr &&
           (host ? window.testPrepareMqttHostSession()
                 : bootstrap ? window.testPrepareMqttGuestBootstrap()
                             : window.testPrepareMqttGuestSession());
}

void acknowledgeSubscribes(MainWindow &window,
                           const QHash<uint16_t, QString> &pending)
{
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        window.testHandleMqttPacket(mqttAckPacket(false, it.key()));
    }
    QCoreApplication::processEvents();
}

void acknowledgeUnsubscribes(MainWindow &window,
                             const QHash<uint16_t, QString> &pending)
{
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        window.testHandleMqttPacket(mqttAckPacket(true, it.key()));
    }
    QCoreApplication::processEvents();
}

bool runMqttClientIdCase(MainWindow &first)
{
    const QByteArray host = mqttClientIdFor(true, 0x1234u);
    const QByteArray join = mqttClientIdFor(false, 0x1234u);
    const QByteArray otherJoin = mqttClientIdFor(false, 0x1235u);
    if (host != "PCH0000000000001234" || host == join ||
        join == otherJoin || !portableClientId(host) ||
        !portableClientId(join)) {
        std::fputs("FAIL: MQTT ClientId format or role separation\n", stderr);
        return false;
    }

    const QByteArray stable = first.testMqttClientId(false);
    MainWindow second;
    if (stable != first.testMqttClientId(false) ||
        stable == second.testMqttClientId(false) ||
        !portableClientId(stable)) {
        std::fputs("FAIL: MQTT ClientId instance identity\n", stderr);
        return false;
    }
    return true;
}

bool runMqttSubscriptionTransitionCase()
{
    MainWindow window;
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer)) {
        std::fputs("FAIL: prepare MQTT subscription transition\n", stderr);
        return false;
    }
    (void)takeMqttPackets(peer.get());

    window.testFeedMqtt("meta", "H W 77", false);
    QCoreApplication::processEvents();
    QHash<uint16_t, QString> pending = window.testMqttPendingSubacks();
    QVector<QByteArray> packets = takeMqttPackets(peer.get());
    if (pending.size() != 4 || window.testMqttOperational() ||
        !window.testMqttPendingUnsubacks().isEmpty() ||
        mqttPacketCount(packets, 8u) != 4 ||
        mqttPacketCount(packets, 3u) != 0) {
        std::fputs("FAIL: live host did not gate initial subscriptions\n", stderr);
        return false;
    }

    const uint16_t firstSuback = pending.constBegin().key();
    window.testHandleMqttPacket(mqttAckPacket(false, 0xfffeu));
    if (window.testMqttPendingSubacks().size() != 4) {
        std::fputs("FAIL: foreign SUBACK advanced transition\n", stderr);
        return false;
    }
    window.testHandleMqttPacket(mqttAckPacket(false, firstSuback));
    window.testHandleMqttPacket(mqttAckPacket(false, firstSuback));
    if (window.testMqttPendingSubacks().size() != 3 ||
        window.testMqttOperational()) {
        std::fputs("FAIL: duplicate SUBACK advanced transition\n", stderr);
        return false;
    }
    pending.remove(firstSuback);
    acknowledgeSubscribes(window, pending);
    packets = takeMqttPackets(peer.get());
    const QSet<QString> blackTopics = {
        QStringLiteral("meta"), QStringLiteral("w2b"),
        QStringLiteral("ack_b"), QStringLiteral("pres_w"),
        QStringLiteral("pres_b")};
    if (!window.testMqttOperational() ||
        window.testMqttActiveSubscriptions() != blackTopics ||
        !window.testMqttPendingSubacks().isEmpty() ||
        mqttPublishPayloadCount(packets, QByteArray("O B 77")) != 1) {
        std::fputs("FAIL: initial transition did not activate exactly once\n", stderr);
        return false;
    }

    window.testFeedMqtt("meta", "H B 78", false);
    QCoreApplication::processEvents();
    pending = window.testMqttPendingSubacks();
    packets = takeMqttPackets(peer.get());
    if (pending.size() != 2 || window.testMqttOperational() ||
        !window.testMqttPendingUnsubacks().isEmpty() ||
        mqttPacketCount(packets, 8u) != 2 ||
        mqttPacketCount(packets, 10u) != 0 ||
        mqttPacketCount(packets, 3u) != 0) {
        std::fputs("FAIL: side flip did not begin with new subscriptions\n", stderr);
        return false;
    }

    window.testHandleMqttPacket(DesktopTransportCodec::encodeMqttPublish(
        400u, QByteArray("netchesszx/v1/room/w2b"), QByteArray("PING"),
        false));
    packets = takeMqttPackets(peer.get());
    if (mqttPacketCount(packets, 4u) != 1 ||
        mqttPacketCount(packets, 3u) != 0) {
        std::fputs("FAIL: lateral RX was delivered during side transition\n",
                   stderr);
        return false;
    }

    acknowledgeSubscribes(window, pending);
    QHash<uint16_t, QString> pendingUnsub =
        window.testMqttPendingUnsubacks();
    packets = takeMqttPackets(peer.get());
    const QSet<QString> allDirectionalTopics = {
        QStringLiteral("meta"), QStringLiteral("w2b"),
        QStringLiteral("ack_b"), QStringLiteral("b2w"),
        QStringLiteral("ack_w"), QStringLiteral("pres_w"),
        QStringLiteral("pres_b")};
    if (pendingUnsub.size() != 2 || window.testMqttOperational() ||
        window.testMqttActiveSubscriptions() != allDirectionalTopics ||
        mqttPacketCount(packets, 10u) != 2 ||
        mqttPacketCount(packets, 3u) != 0) {
        std::fputs("FAIL: obsolete topics removed before UNSUBACK\n", stderr);
        return false;
    }

    const auto firstUnsub = pendingUnsub.constBegin();
    const uint16_t firstUnsuback = firstUnsub.key();
    const QString firstObsolete = firstUnsub.value();
    window.testHandleMqttPacket(mqttAckPacket(false, firstUnsuback));
    window.testHandleMqttPacket(mqttAckPacket(true, 0xfffdu));
    if (window.testMqttPendingUnsubacks().size() != 2) {
        std::fputs("FAIL: foreign or wrong-type ACK advanced unsubscribe\n",
                   stderr);
        return false;
    }
    window.testHandleMqttPacket(mqttAckPacket(true, firstUnsuback));
    window.testHandleMqttPacket(mqttAckPacket(true, firstUnsuback));
    if (window.testMqttPendingUnsubacks().size() != 1 ||
        window.testMqttActiveSubscriptions().contains(firstObsolete) ||
        window.testMqttOperational()) {
        std::fputs("FAIL: duplicate UNSUBACK advanced transition\n", stderr);
        return false;
    }
    pendingUnsub.remove(firstUnsuback);
    acknowledgeUnsubscribes(window, pendingUnsub);
    packets = takeMqttPackets(peer.get());
    const QSet<QString> whiteTopics = {
        QStringLiteral("meta"), QStringLiteral("b2w"),
        QStringLiteral("ack_w"), QStringLiteral("pres_w"),
        QStringLiteral("pres_b")};
    if (!window.testMqttOperational() ||
        window.testMqttActiveSubscriptions() != whiteTopics ||
        mqttPublishPayloadCount(packets, QByteArray("O W 78")) != 1 ||
        mqttPublishPayloadCount(packets, QByteArray("ACK PING")) != 0) {
        std::fputs("FAIL: side flip did not activate atomically\n", stderr);
        return false;
    }

    window.testFeedMqtt("meta", "H B 79", false);
    QCoreApplication::processEvents();
    packets = takeMqttPackets(peer.get());
    if (!window.testMqttOperational() ||
        !window.testMqttPendingSubacks().isEmpty() ||
        !window.testMqttPendingUnsubacks().isEmpty() ||
        window.testMqttActiveSubscriptions() != whiteTopics ||
        mqttPacketCount(packets, 8u) != 0 ||
        mqttPacketCount(packets, 10u) != 0 ||
        mqttPublishPayloadCount(packets, QByteArray("O W 79")) != 1) {
        std::fputs("FAIL: same-side transition was not idempotent\n", stderr);
        return false;
    }

    window.testFeedMqtt("meta", "H W 80", false);
    QCoreApplication::processEvents();
    pending = window.testMqttPendingSubacks();
    (void)takeMqttPackets(peer.get());
    if (pending.size() != 2 || window.testMqttOperational()) {
        std::fputs("FAIL: rejected dynamic transition did not start\n", stderr);
        return false;
    }
    window.testHandleMqttPacket(
        mqttAckPacket(false, pending.constBegin().key(), 0x80u));
    QCoreApplication::processEvents();
    if (window.testSocket()->state() != QAbstractSocket::UnconnectedState) {
        window.testSocket()->waitForDisconnected(2000);
    }
    if (window.testSocket()->state() != QAbstractSocket::UnconnectedState ||
        window.testMqttOperational() ||
        !window.testMqttPendingSubacks().isEmpty() ||
        !window.testMqttPendingUnsubacks().isEmpty() ||
        !window.testMqttActiveSubscriptions().isEmpty()) {
        std::fputs("FAIL: rejected dynamic SUBACK left side operational\n",
                   stderr);
        return false;
    }
    return true;
}

bool runMqttPreSubackReplayCase()
{
    MainWindow window;
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer, true)) {
        std::fputs("FAIL: prepare MQTT pre-SUBACK replay\n", stderr);
        return false;
    }
    (void)takeMqttPackets(peer.get());

    window.testHandleMqttPacket(QByteArray::fromHex("20020000"));
    QHash<uint16_t, QString> pending = window.testMqttPendingSubacks();
    QVector<QByteArray> packets = takeMqttPackets(peer.get());
    if (pending.size() != 1 || pending.constBegin().value() != "meta" ||
        mqttPacketCount(packets, 8u) != 1 ||
        !window.testMqttActiveSubscriptions().isEmpty()) {
        std::fputs("FAIL: bootstrap meta subscription\n", stderr);
        return false;
    }

    window.testHandleMqttPacket(DesktopTransportCodec::encodeMqttPublish(
        300u, QByteArray("netchesszx/v1/room/meta"),
        QByteArray("H W 77"), true));
    packets = takeMqttPackets(peer.get());
    if (mqttPacketCount(packets, 4u) != 1 ||
        mqttPacketCount(packets, 8u) != 0 ||
        window.testMqttPendingSubacks().size() != 1 ||
        !window.testMqttActiveSubscriptions().isEmpty()) {
        std::fputs("FAIL: retained host processed before meta SUBACK\n",
                   stderr);
        return false;
    }

    window.testHandleMqttPacket(
        mqttAckPacket(false, pending.constBegin().key()));
    QCoreApplication::processEvents();
    pending = window.testMqttPendingSubacks();
    packets = takeMqttPackets(peer.get());
    if (pending.size() != 4 || mqttPacketCount(packets, 8u) != 4 ||
        window.testMqttActiveSubscriptions() !=
            QSet<QString>{QStringLiteral("meta")} ||
        window.testMqttOperational()) {
        std::fputs("FAIL: retained host not replayed after meta SUBACK\n",
                   stderr);
        return false;
    }

    window.testHandleMqttPacket(DesktopTransportCodec::encodeMqttPublish(
        301u, QByteArray("netchesszx/v1/room/pres_b"),
        QByteArray("O B 77"), true));
    packets = takeMqttPackets(peer.get());
    if (mqttPacketCount(packets, 4u) != 1 ||
        mqttPacketCount(packets, 3u) != 0 ||
        window.testSocket()->state() != QAbstractSocket::ConnectedState) {
        std::fputs("FAIL: retained occupancy processed before side SUBACK\n",
                   stderr);
        return false;
    }

    acknowledgeSubscribes(window, pending);
    packets = takeMqttPackets(peer.get());
    QCoreApplication::processEvents();
    if (window.testSocket()->state() != QAbstractSocket::UnconnectedState) {
        window.testSocket()->waitForDisconnected(2000);
    }
    if (window.testSocket()->state() != QAbstractSocket::UnconnectedState ||
        window.testSessionReady() ||
        mqttPublishPayloadCount(packets, QByteArray("O B 77")) != 0 ||
        mqttPublishPayloadCount(packets, QByteArray("J 77")) != 0) {
        std::fputs("FAIL: retained occupancy not replayed as BUSY\n", stderr);
        return false;
    }
    return true;
}

bool runMqttHostPeerReplacementCase()
{
    MainWindow window;
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer, false, true)) {
        std::fputs("FAIL: prepare MQTT host peer replacement\n", stderr);
        return false;
    }
    (void)takeMqttPackets(peer.get());

    window.testFeedMqtt("meta", "J 77", false);
    QVector<QByteArray> packets = takeMqttPackets(peer.get());
    if (!window.testSessionReady() ||
        !window.testDisconnectButtonAvailable() ||
        mqttPublishPayloadCount(packets, QByteArray("H W 77")) != 1 ||
        mqttPublishPayloadCount(packets, expectedLocalMachPayload()) != 1 ||
        !window.testStatusContextText().endsWith(QStringLiteral("VS ?"))) {
        std::fputs("FAIL: initial MQTT host peer did not become ready\n", stderr);
        return false;
    }

    window.testFeedMqtt("b2w", "MACH ZX", false);
    if (!window.testStatusContextText().endsWith(QStringLiteral("VS ZX"))) {
        std::fputs("FAIL: MQTT host did not show peer machine\n", stderr);
        return false;
    }
    window.testFeedMqtt("w2b", "MACH NXT", false);
    window.testFeedMqtt("b2w", "MACH ZZ", false);
    window.testFeedMqtt("b2w", "MACH NXT", true);
    if (!window.testStatusContextText().endsWith(QStringLiteral("VS ZX"))) {
        std::fputs("FAIL: invalid, wrong-route, or retained MACH replaced peer\n",
                   stderr);
        return false;
    }

    window.testFeedMqtt("pres_b", "F B 77", false);
    packets = takeMqttPackets(peer.get());
    if (window.testSocket()->state() != QAbstractSocket::ConnectedState ||
        window.testSessionReady() ||
        !window.testDisconnectButtonAvailable() ||
        window.testStatusContextText().contains(QStringLiteral("VS ")) ||
        mqttPublishPayloadCount(packets, QByteArray("O W 77")) != 1 ||
        mqttPublishPayloadCount(packets, QByteArray("H W 77")) != 1) {
        std::fputs("FAIL: MQTT host did not restart on the live broker link\n",
                   stderr);
        return false;
    }

    window.testFeedMqtt("meta", "J 77", false);
    packets = takeMqttPackets(peer.get());
    const bool ready = window.testSessionReady() &&
                       window.testDisconnectButtonAvailable() &&
                       mqttPublishPayloadCount(packets,
                                               QByteArray("H W 77")) == 1 &&
                       mqttPublishPayloadCount(
                           packets, expectedLocalMachPayload()) == 1 &&
                       window.testStatusContextText().endsWith(
                           QStringLiteral("VS ?"));
    if (!ready) {
        std::fputs("FAIL: replacement MQTT guest did not become ready\n", stderr);
    }
    window.testSocket()->abort();
    return ready;
}

bool runMqttGuestHostReplacementCase()
{
    MainWindow window;
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer)) {
        std::fputs("FAIL: prepare MQTT guest host replacement\n", stderr);
        return false;
    }
    (void)takeMqttPackets(peer.get());

    window.testFeedMqtt("meta", "H W 77", false);
    acknowledgeSubscribes(window, window.testMqttPendingSubacks());
    QVector<QByteArray> packets = takeMqttPackets(peer.get());
    if (!window.testSessionReady() ||
        mqttPublishPayloadCount(packets, QByteArray("O B 77")) != 1 ||
        mqttPublishPayloadCount(packets, QByteArray("J 77")) != 1 ||
        mqttPublishPayloadCount(packets, expectedLocalMachPayload()) != 1 ||
        !window.testStatusContextText().endsWith(QStringLiteral("VS ?"))) {
        std::fputs("FAIL: initial MQTT guest did not become ready\n", stderr);
        return false;
    }
    window.testFeedMqtt("w2b", "MACH NXT", false);
    if (!window.testStatusContextText().endsWith(QStringLiteral("VS NXT"))) {
        std::fputs("FAIL: MQTT guest did not show peer machine\n", stderr);
        return false;
    }

    window.testFeedMqtt("pres_w", "F W 77", false);
    (void)takeMqttPackets(peer.get());
    if (window.testSocket()->state() != QAbstractSocket::ConnectedState ||
        window.testSessionReady() ||
        !window.testDisconnectButtonAvailable() ||
        window.testStatusContextText().contains(QStringLiteral("VS "))) {
        std::fputs("FAIL: MQTT guest did not rearm on the live broker link\n",
                   stderr);
        return false;
    }

    window.testFeedMqtt("meta", "H W 88", false);
    packets = takeMqttPackets(peer.get());
    const bool ready = window.testSessionReady() &&
                       window.testDisconnectButtonAvailable() &&
                       mqttPublishPayloadCount(packets,
                                               QByteArray("O B 88")) == 1 &&
                       mqttPublishPayloadCount(packets,
                                               QByteArray("J 88")) == 1 &&
                       mqttPublishPayloadCount(
                           packets, expectedLocalMachPayload()) == 1 &&
                       window.testStatusContextText().endsWith(
                           QStringLiteral("VS ?"));
    if (!ready) {
        std::fputs("FAIL: replacement MQTT host did not restore guest ready\n",
                   stderr);
    }
    window.testSocket()->abort();
    return ready;
}

bool runMqttRestoreReplacementUiCase()
{
    MainWindow window;
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer)) {
        std::fputs("FAIL: prepare MQTT restore replacement UI\n", stderr);
        return false;
    }
    (void)takeMqttPackets(peer.get());
    window.testFeedMqtt("meta", "H W 77", false);
    acknowledgeSubscribes(window, window.testMqttPendingSubacks());
    (void)takeMqttPackets(peer.get());

    if (!window.testSessionReady() || !window.testBeginMqttRestore() ||
        mqttPublishPayloadCount(takeMqttPackets(peer.get()),
                                QByteArray("RQ")) != 1) {
        std::fputs("FAIL: local MQTT restore did not become busy\n", stderr);
        return false;
    }
    window.testFeedMqtt("meta", "H W 88", false);
    QVector<QByteArray> packets = takeMqttPackets(peer.get());
    if (!window.testRestoreUiIdle() ||
        mqttPublishPayloadCount(packets, QByteArray("O B 88")) != 1 ||
        mqttPublishPayloadCount(packets, QByteArray("J 88")) != 1) {
        std::fputs("FAIL: peer replacement did not release restore busy\n",
                   stderr);
        return false;
    }

    window.testFeedMqtt("w2b", "RQ", false);
    if (window.testRestoreUiIdle()) {
        std::fputs("FAIL: remote MQTT restore did not open prompt\n", stderr);
        return false;
    }
    window.testFeedMqtt("meta", "H W 99", false);
    packets = takeMqttPackets(peer.get());
    const bool ok = window.testRestoreUiIdle() &&
                    mqttPublishPayloadCount(packets,
                                            QByteArray("O B 99")) == 1 &&
                    mqttPublishPayloadCount(packets,
                                            QByteArray("J 99")) == 1;
    if (!ok) {
        std::fputs("FAIL: peer replacement did not close restore prompt\n",
                   stderr);
    }
    window.testSocket()->abort();
    return ok;
}

bool runDirectRetryReconnectCase(MainWindow &window)
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: listen for direct retry\n", stderr);
        return false;
    }

    window.testStartDirectGuestConnection(QStringLiteral("127.0.0.1"),
                                          server.serverPort());
    QElapsedTimer wait;
    wait.start();
    while (!server.hasPendingConnections() && wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!server.hasPendingConnections()) {
        std::fputs("FAIL: accept initial direct guest\n", stderr);
        return false;
    }

    std::unique_ptr<QTcpSocket> stale(server.nextPendingConnection());
    wait.restart();
    constexpr int handshakeTimeoutMs =
        SESSION_PROTOCOL_TICK_MS * SESSION_DIRECT_HELLO_TICKS *
        (SESSION_DIRECT_HELLO_RETRIES + 1);
    while (!window.testDirectRetryPending() &&
           wait.elapsed() < handshakeTimeoutMs + 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!window.testDirectRetryPending()) {
        std::fputs("FAIL: unanswered DIRECT handshake did not schedule retry\n",
                   stderr);
        return false;
    }

    wait.restart();
    while (!server.hasPendingConnections() &&
           wait.elapsed() < kDirectConnectRetryIntervalMs + 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!server.hasPendingConnections()) {
        std::fputs("FAIL: direct guest did not reconnect after handshake timeout\n",
                   stderr);
        return false;
    }

    std::unique_ptr<QTcpSocket> host(server.nextPendingConnection());
    QByteArray guestHello = host->readAll();
    QEventLoop helloLoop;
    QTimer helloTimeout;
    helloTimeout.setSingleShot(true);
    const QMetaObject::Connection helloReadyConnection =
        QObject::connect(host.get(), &QTcpSocket::readyRead, &helloLoop, [&]() {
            guestHello.append(host->readAll());
            if (guestHello.contains('\n')) {
                helloLoop.quit();
            }
        });
    QObject::connect(&helloTimeout, &QTimer::timeout, &helloLoop,
                     &QEventLoop::quit);
    if (!guestHello.contains('\n')) {
        helloTimeout.start(2000);
        helloLoop.exec();
    }
    QObject::disconnect(helloReadyConnection);
    if (guestHello != "HELLO DIRECT GUEST\n") {
        std::fputs("FAIL: reconnected direct guest did not send HELLO\n", stderr);
        return false;
    }
    host->write("HELLO DIRECT HOST WHITE=HOST\n");
    host->flush();
    wait.restart();
    while (!window.testSessionReady() && wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const bool sessionConnected = window.testSessionReady() &&
        window.testSocket()->state() == QAbstractSocket::ConnectedState;
    const QByteArray directOutput = takeDirectBytes(host.get());
    const bool announcementSent = directOutput.count(
        expectedLocalMachPayload() + '\n') == 1;
    const bool legacyShown = window.testStatusContextText().endsWith(
        QStringLiteral("VS ?"));
    const bool connected = sessionConnected && announcementSent && legacyShown;
    if (!sessionConnected) {
        std::fputs("FAIL: direct MACH session not connected\n", stderr);
    } else if (!announcementSent) {
        std::fprintf(stderr, "FAIL: direct MACH announcement missing: %s\n",
                     directOutput.toHex().constData());
    } else if (!legacyShown) {
        std::fprintf(stderr, "FAIL: direct legacy machine missing: %s\n",
                     window.testStatusContextText().toUtf8().constData());
    }
    if (connected &&
        (host->write("MACH NXT\n") != 9 ||
         !host->waitForBytesWritten(2000))) {
        std::fputs("FAIL: send direct peer machine\n", stderr);
        return false;
    }
    wait.restart();
    while (connected &&
           !window.testStatusContextText().endsWith(QStringLiteral("VS NXT")) &&
           wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    const bool recognized = connected &&
        window.testStatusContextText().endsWith(QStringLiteral("VS NXT"));
    window.testSocket()->abort();
    QCoreApplication::processEvents();
    if (!recognized) {
        std::fputs("FAIL: direct retry did not exchange peer machine\n", stderr);
    }
    return recognized;
}

bool runDirectRemoteMoveFlashCancellationCase()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: listen for remote move flash cancellation\n", stderr);
        return false;
    }

    MainWindow window;
    window.testStartDirectGuestConnection(QStringLiteral("127.0.0.1"),
                                          server.serverPort());
    QElapsedTimer wait;
    wait.start();
    while (!server.hasPendingConnections() && wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!server.hasPendingConnections()) {
        std::fputs("FAIL: accept remote move flash peer\n", stderr);
        return false;
    }

    std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
    if (!peer || takeDirectBytes(peer.get()) !=
                     QByteArrayLiteral("HELLO DIRECT GUEST\n")) {
        std::fputs("FAIL: remote move flash guest HELLO\n", stderr);
        return false;
    }

    const QByteArray hostHello =
        QByteArrayLiteral("HELLO DIRECT HOST WHITE=HOST\n");
    if (peer->write(hostHello) != hostHello.size() ||
        !peer->waitForBytesWritten(2000)) {
        std::fputs("FAIL: send remote move flash host HELLO\n", stderr);
        return false;
    }
    wait.restart();
    while (!window.testSessionReady() && wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!window.testSessionReady()) {
        std::fputs("FAIL: remote move flash direct session not ready\n", stderr);
        return false;
    }
    (void)takeDirectBytes(peer.get());

    const QByteArray start = QByteArrayLiteral("GAME START WHITE=HOST\n");
    if (peer->write(start) != start.size() ||
        !peer->waitForBytesWritten(2000)) {
        std::fputs("FAIL: send remote move flash GAME START\n", stderr);
        return false;
    }
    QByteArray startReply;
    wait.restart();
    while (!startReply.contains("ACK GAME START\n") && wait.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        startReply.append(peer->readAll());
    }
    if (!startReply.contains("ACK GAME START\n")) {
        std::fputs("FAIL: remote move flash GAME START ACK\n", stderr);
        return false;
    }

    const QByteArray move = QByteArrayLiteral("MOVE 1 e2e4\n");
    if (peer->write(move) != move.size() ||
        !peer->waitForBytesWritten(2000)) {
        std::fputs("FAIL: send remote move flash MOVE\n", stderr);
        return false;
    }
    bool cancelled = false;
    wait.restart();
    while (!cancelled && wait.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        cancelled = window.testCancelPendingPieceFlash();
    }

    QByteArray reply;
    wait.restart();
    while (!reply.contains("NACK 1") && wait.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        reply.append(peer->readAll());
    }
    const bool ok = cancelled && reply.contains("NACK 1");
    if (!ok) {
        std::fputs("FAIL: cancelled remote move stranded reducer/busy state\n",
                   stderr);
    }
    window.testSocket()->abort();
    peer->abort();
    QCoreApplication::processEvents();
    return ok;
}

bool runDirectHostPreHelloCancelCase()
{
    QTcpServer portPicker;
    if (!portPicker.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: reserve direct host cancel port\n", stderr);
        return false;
    }
    const quint16 port = portPicker.serverPort();
    portPicker.close();

    MainWindow window;
    if (!window.testStartDirectHostListener(port)) {
        std::fputs("FAIL: direct host did not start listener\n", stderr);
        return false;
    }

    QTcpSocket peer;
    peer.connectToHost(QHostAddress::LocalHost, port);
    if (!peer.waitForConnected(2000)) {
        std::fputs("FAIL: connect pre-HELLO direct peer\n", stderr);
        return false;
    }
    QElapsedTimer acceptTimer;
    acceptTimer.start();
    while (window.testSocket()->state() != QAbstractSocket::ConnectedState &&
           acceptTimer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!window.testDirectListenerActive() ||
        window.testSocket()->state() != QAbstractSocket::ConnectedState ||
        window.testSessionReady()) {
        std::fputs("FAIL: prepare connected pre-HELLO direct host\n", stderr);
        return false;
    }

    window.testClickConnectButton();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    const bool closed =
        !window.testDirectListenerActive() &&
        window.testSocket()->state() == QAbstractSocket::UnconnectedState;
    if (!closed) {
        std::fputs("FAIL: Cancel left direct listener or socket open\n", stderr);
    }
    peer.abort();
    return closed;
}

bool runDirectHostRapidReplacementCase()
{
    QTcpServer portPicker;
    if (!portPicker.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: reserve direct replacement port\n", stderr);
        return false;
    }
    const quint16 port = portPicker.serverPort();
    portPicker.close();

    MainWindow window;
    if (!window.testStartDirectHostListener(port)) {
        std::fputs("FAIL: direct replacement host did not listen\n", stderr);
        return false;
    }

    QTcpSocket first;
    first.connectToHost(QHostAddress::LocalHost, port);
    if (!first.waitForConnected(2000)) {
        std::fputs("FAIL: connect first direct peer\n", stderr);
        return false;
    }
    QElapsedTimer readyTimer;
    readyTimer.start();
    while (window.testSocket()->state() != QAbstractSocket::ConnectedState &&
           readyTimer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (window.testSocket()->state() != QAbstractSocket::ConnectedState ||
        first.write("HELLO DIRECT GUEST\n") != 19 ||
        !first.waitForBytesWritten(2000)) {
        std::fputs("FAIL: prepare first direct peer\n", stderr);
        return false;
    }
    readyTimer.restart();
    while (!window.testSessionReady() && readyTimer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!window.testSessionReady()) {
        std::fputs("FAIL: prepare active direct peer\n", stderr);
        return false;
    }

    QTcpSocket replacement;
    replacement.connectToHost(QHostAddress::LocalHost, port);
    if (!replacement.waitForConnected(2000) ||
        first.write("BYE\n") != 4 ||
        !first.waitForBytesWritten(2000)) {
        std::fputs("FAIL: prepare rapid direct replacement\n", stderr);
        return false;
    }

    const bool clean = window.testReplaceDirectClientBeforeDisconnect();
    if (!clean) {
        std::fputs("FAIL: rapid direct replacement left a stale socket link\n",
                   stderr);
    }
    replacement.abort();
    first.abort();
    return clean;
}

bool runConnectedWindowShutdownCase()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: listen for connected window shutdown\n", stderr);
        return false;
    }

    auto window = std::make_unique<MainWindow>();
    QTcpSocket *socket = window->testSocket();
    socket->connectToHost(QHostAddress::LocalHost, server.serverPort());
    if (!socket->waitForConnected(2000) ||
        !server.waitForNewConnection(2000)) {
        std::fputs("FAIL: connect for connected window shutdown\n", stderr);
        return false;
    }
    std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
    if (!peer) {
        std::fputs("FAIL: accept connected window shutdown peer\n", stderr);
        return false;
    }

    window.reset();
    return true;
}

bool runSubscriptionWriteFailureCase(MainWindow &window)
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fputs("FAIL: listen\n", stderr);
        return false;
    }

    QTcpSocket *socket = window.testSocket();
    socket->abort();
    socket->connectToHost(QHostAddress::LocalHost, server.serverPort());
    if (!socket->waitForConnected(2000) || !server.waitForNewConnection(2000)) {
        std::fputs("FAIL: connect\n", stderr);
        return false;
    }
    std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
    if (!peer || !window.testPrepareMqttGuestSession()) {
        std::fputs("FAIL: prepare guest session\n", stderr);
        return false;
    }

    window.testSetMqttWriteFailure(true);
    window.testFeedMqtt("meta", "H W 77", false);
    QCoreApplication::processEvents();
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->waitForDisconnected(2000);
    }

    bool ok = true;
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        std::fputs("FAIL: MQTT SUBSCRIBE failure did not close link\n", stderr);
        ok = false;
    }
    if (window.testSessionReady() || window.testMqttOperational() ||
        !window.testMqttPendingSubacks().isEmpty() ||
        !window.testMqttActiveSubscriptions().isEmpty()) {
        std::fputs("FAIL: SUBSCRIBE failure left deferred MQTT state\n", stderr);
        ok = false;
    }
    socket->abort();
    return ok;
}

bool runTxFailureCase(MainWindow &window)
{
    QTcpServer server;
    std::unique_ptr<QTcpSocket> peer;
    if (!connectMqttTestPeer(window, server, peer)) {
        std::fputs("FAIL: prepare MQTT TX failure\n", stderr);
        return false;
    }

    window.testFeedMqtt("meta", "H W 77", true);
    QCoreApplication::processEvents();
    const QHash<uint16_t, QString> pending =
        window.testMqttPendingSubacks();
    if (pending.size() != 4) {
        std::fputs("FAIL: retained host did not request subscriptions\n",
                   stderr);
        return false;
    }
    acknowledgeSubscribes(window, pending);
    (void)takeMqttPackets(peer.get());
    if (!window.testMqttOperational()) {
        std::fputs("FAIL: retained host subscriptions did not activate\n",
                   stderr);
        return false;
    }

    if (!window.testEndAndRelinkMqttSession()) {
        std::fputs("FAIL: ENDED disabled later MQTT LINK_UP\n", stderr);
        return false;
    }

    window.testSetMqttWriteFailure(true);
    window.testFeedMqtt("meta", "H W 77", false);
    QCoreApplication::processEvents();
    if (window.testSocket()->state() != QAbstractSocket::UnconnectedState) {
        window.testSocket()->waitForDisconnected(2000);
    }
    const bool closed =
        window.testSocket()->state() == QAbstractSocket::UnconnectedState;
    if (!closed) {
        std::fputs("FAIL: MQTT SEND failure did not close link\n", stderr);
    }
    window.testSocket()->abort();
    return closed;
}

bool runDirectIpHistoryCase()
{
    const QStringList initial = {
        QStringLiteral("192.168.0.2"),
        QStringLiteral("invalid"),
        QStringLiteral("192.168.0.3"),
        QStringLiteral("192.168.0.2"),
    };
    const QStringList expected = {
        QStringLiteral("192.168.0.3"),
        QStringLiteral("192.168.0.2"),
    };
    QSettings settings;
    settings.remove(kDirectIpHistorySettingsKey);
    settings.setValue(kDirectIpHistorySettingsKey,
                      directIpHistoryWith(initial, QStringLiteral(" 192.168.0.3 ")));
    settings.sync();

    QSettings restored;
    QStringList many;
    for (int octet = 1; octet <= kDirectIpHistoryMax; ++octet) {
        many.append(QStringLiteral("10.0.0.%1").arg(octet));
    }
    const QStringList capped = directIpHistoryWith(many, QStringLiteral("10.0.0.9"));
    const bool ok = restored.value(kDirectIpHistorySettingsKey).toStringList() == expected &&
                    capped.size() == kDirectIpHistoryMax &&
                    capped.at(0) == QStringLiteral("10.0.0.9") &&
                    capped.at(kDirectIpHistoryMax - 1) == QStringLiteral("10.0.0.7");
    restored.remove(kDirectIpHistorySettingsKey);
    restored.sync();
    if (!ok) {
        std::fputs("FAIL: Direct IP history did not persist unique valid entries\n", stderr);
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
    QApplication::setApplicationName(QStringLiteral("ShatranjMqttTxFailureTest"));
    QApplication::setOrganizationName(QStringLiteral("NetChessZXTests"));

    if (!runDirectIpHistoryCase()) {
        return 1;
    }
    MainWindow window;
    if (!window.testStatusBarAligned()) {
        std::fputs("FAIL: status bar is not aligned to board controls\n", stderr);
        return 1;
    }
    if (!window.testSessionEndPresentation()) {
        std::fputs("FAIL: session end cause presentation\n", stderr);
        return 1;
    }
    if (!window.testResignRestartUiProjection() ||
        !window.testRestoredMoveProjection() ||
        !runMqttClientIdCase(window) ||
        !runMqttPreSubackReplayCase() ||
        !runMqttSubscriptionTransitionCase() ||
        !runMqttHostPeerReplacementCase() ||
        !runMqttRestoreReplacementUiCase() ||
        !runDirectRetryReconnectCase(window) ||
        !runDirectRemoteMoveFlashCancellationCase() ||
        !runDirectHostPreHelloCancelCase() ||
        !runDirectHostRapidReplacementCase() ||
        !runMqttGuestHostReplacementCase() ||
        !runConnectedWindowShutdownCase() ||
        !runSubscriptionWriteFailureCase(window) ||
        !runTxFailureCase(window)) {
        return 1;
    }
    std::puts("pc mqtt lifecycle, tx failure, and direct retry policy: ok");
    return 0;
}
