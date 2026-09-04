#include "pc/client/desktop_session_adapter.h"

#include "common/protocol/platform_protocol.h"

#include <cstdio>
#include <cstring>

#include <QCoreApplication>

static int failures;

static void check(bool ok, const char *label)
{
    if (!ok) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    }
}

static const DesktopOwnedAction *findAction(const DesktopActionBatch &batch,
                                         uint8_t type)
{
    for (uint8_t i = 0u; i < batch.count; ++i) {
        if (batch.actions[i].action.type == type) {
            return &batch.actions[i];
        }
    }
    return nullptr;
}

static int actionIndex(const DesktopActionBatch &batch, uint8_t type)
{
    for (uint8_t i = 0u; i < batch.count; ++i) {
        if (batch.actions[i].action.type == type) {
            return i;
        }
    }
    return -1;
}

static uint8_t sendId(const DesktopActionBatch &batch)
{
    const DesktopOwnedAction *send = findAction(batch, SESSION_ACT_SEND);

    return send == nullptr ? 0u : send->action.data.send.tx_id;
}

static bool hasSessionStatus(const DesktopActionBatch &batch, uint8_t status)
{
    for (uint8_t i = 0u; i < batch.count; ++i) {
        const SessionAction &action = batch.actions[i].action;

        if (action.type == SESSION_ACT_SESSION_CHANGED &&
            action.data.session.status == status) {
            return true;
        }
    }
    return false;
}

static const DesktopOwnedAction *findEndedAction(const DesktopActionBatch &batch)
{
    for (uint8_t i = 0u; i < batch.count; ++i) {
        const DesktopOwnedAction &action = batch.actions[i];

        if (action.action.type == SESSION_ACT_SESSION_CHANGED &&
            action.action.data.session.status == SESSION_CHANGED_ENDED) {
            return &action;
        }
    }
    return nullptr;
}

static void drainLinkUp(DesktopSessionAdapter &adapter, uint8_t linkId)
{
    DesktopActionBatch batch;

    adapter.enqueueLinkUp(linkId);
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "MQTT broker link-up drains");
}

static void testRoutingAndTxFailure()
{
    DesktopSessionAdapter adapter;
    DesktopSessionAdapter unknown;
    DesktopSessionAdapter white;
    DesktopActionBatch batch;
    const DesktopOwnedAction *side;
    const DesktopOwnedAction *send;

    check(adapter.initMqtt(SESSION_ROLE_GUEST, SESSION_COLOR_UNKNOWN),
          "failure adapter init");
    drainLinkUp(adapter, 1u);
    check(!adapter.enqueueMqttRx(1u, "unknown", false, "H W 77") &&
              !adapter.hasPendingEvents(),
          "unknown MQTT topic is not routed");
    check(adapter.enqueueMqttRx(1u, "netchesszx/v1/room/w2b", false,
                            "H W 77") &&
              adapter.takeNextBatch(&batch),
          "shared side topic demuxes control verb in core");
    side = findAction(batch, SESSION_ACT_SIDE_CHANGED);
    send = findAction(batch, SESSION_ACT_SEND);
    check(side != nullptr && send != nullptr &&
              actionIndex(batch, SESSION_ACT_SIDE_CHANGED) <
                  actionIndex(batch, SESSION_ACT_SEND) &&
              side->action.data.side.color == SESSION_COLOR_BLACK &&
              side->action.data.side.session_id == 77u,
          "SIDE_CHANGED precedes dependent subscriptions and send");
    check(send != nullptr && send->payload == QByteArray("O B 77") &&
              send->action.data.send.route == SESSION_ROUTE_PRESENCE &&
              send->action.data.send.retained == 1u &&
              send->action.data.send.link_id == 1u &&
              std::memcmp(send->action.data.send.payload, "O B 77", 6u) == 0,
          "SEND preserves MQTT route retain link and owned payload");
    check(adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_PRESENCE) == "pres_b" &&
              adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_PRESENCE_PEER) ==
                  "pres_w",
          "presence routes map to own and peer topics without payload parsing");
    check(adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_META) == "meta" &&
              adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_CONTROL) == "b2w" &&
              adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME) == "b2w" &&
              adapter.mqttTopicSuffixForRoute(SESSION_ROUTE_ACK) == "ack_w",
          "control uses own OUT while ACK keeps its directional topic");
    check(white.initMqtt(SESSION_ROLE_HOST, SESSION_COLOR_WHITE, 77u) &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_META) == "meta" &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_CONTROL) == "w2b" &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_PRESENCE) == "pres_w" &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_PRESENCE_PEER) == "pres_b" &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME) == "w2b" &&
              white.mqttTopicSuffixForRoute(SESSION_ROUTE_ACK) == "ack_b",
          "white outbound routes map to exact MQTT topics");
    check(unknown.initMqtt(SESSION_ROLE_GUEST, SESSION_COLOR_UNKNOWN) &&
              unknown.mqttTopicSuffixForRoute(SESSION_ROUTE_META) == "meta" &&
              unknown.mqttTopicSuffixForRoute(SESSION_ROUTE_GAME).isEmpty(),
          "unknown side can publish only neutral meta");
    struct TopicRoute {
        const char *topic;
        uint8_t route;
    };
    static const TopicRoute topicRoutes[] = {
        {"meta", SESSION_ROUTE_META},
        {"pres_w", SESSION_ROUTE_PRESENCE},
        {"pres_b", SESSION_ROUTE_PRESENCE},
        {"w2b", SESSION_ROUTE_GAME},
        {"b2w", SESSION_ROUTE_GAME},
        {"ack_w", SESSION_ROUTE_ACK},
        {"ack_b", SESSION_ROUTE_ACK}
    };
    for (const TopicRoute &entry : topicRoutes) {
        uint8_t route = 0xffu;

        check(DesktopSessionAdapter::mqttRouteForTopic(entry.topic, &route) &&
                  route == entry.route,
              "inbound MQTT topic maps to exact route");
    }
    uint8_t unknownRoute = 0xffu;
    check(!DesktopSessionAdapter::mqttRouteForTopic("unknown", &unknownRoute) &&
              !DesktopSessionAdapter::mqttRouteForTopic("meta", nullptr),
          "invalid inbound MQTT topics are rejected");
    const uint8_t failedTx = sendId(batch);
    adapter.enqueueTxResult(failedTx, SESSION_TX_FAILED);
    check(adapter.takeNextBatch(&batch) &&
              findAction(batch, SESSION_ACT_TIMER_CANCEL) != nullptr &&
              findAction(batch, SESSION_ACT_LINK_CLOSE) != nullptr &&
              hasSessionStatus(batch, SESSION_CHANGED_ENDED),
          "failed publish is reported and ends pre-claim session");
    adapter.enqueueTxResult(failedTx, SESSION_TX_FAILED);
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "failed publish result is consumed once");
}

static void testBootstrapMetaSubscription()
{
    DesktopSessionAdapter host;
    DesktopSessionAdapter guest;
    DesktopActionBatch hostBatch;
    DesktopActionBatch guestBatch;
    const DesktopOwnedAction *send;

    check(host.initMqtt(SESSION_ROLE_HOST, SESSION_COLOR_WHITE, 77u),
          "host bootstrap adapter init");
    host.enqueueLinkUp(1u);
    check(host.takeNextBatch(&hostBatch) && sendId(hostBatch) != 0u,
          "host online publish starts bootstrap");
    host.enqueueTxResult(sendId(hostBatch), SESSION_TX_OK);
    check(host.takeNextBatch(&hostBatch), "host online publish completes");
    send = findAction(hostBatch, SESSION_ACT_SEND);
    const QByteArray topic = send == nullptr
        ? QByteArray() : host.mqttTopicSuffixForRoute(send->action.data.send.route);
    const QByteArray payload = send == nullptr ? QByteArray() : send->payload;
    check(send != nullptr && payload == QByteArray("H W 77") &&
              send->action.data.send.route == SESSION_ROUTE_META &&
              send->action.data.send.retained == 1u && topic == "meta",
          "host retained H uses neutral meta route");

    check(guest.initMqtt(SESSION_ROLE_GUEST, SESSION_COLOR_UNKNOWN),
          "pre-side guest adapter init");
    drainLinkUp(guest, 2u);
    check(topic == "meta" &&
              guest.enqueueMqttRx(2u, topic, true, payload) &&
              guest.takeNextBatch(&guestBatch) &&
              findAction(guestBatch, SESSION_ACT_SIDE_CHANGED) != nullptr,
          "pre-side guest meta-only subscription receives retained H");
}

static void reachReady(DesktopSessionAdapter &adapter, DesktopActionBatch &batch)
{
    check(adapter.initMqtt(SESSION_ROLE_GUEST, SESSION_COLOR_UNKNOWN),
          "ready adapter init");
    drainLinkUp(adapter, 1u);
    check(adapter.enqueueMqttRx(1u, "meta", true, "H W 77") &&
              adapter.takeNextBatch(&batch) && batch.count == 1u &&
              findAction(batch, SESSION_ACT_SIDE_CHANGED) != nullptr,
          "retained meta maps to side probe only");
    check(adapter.enqueueMqttRx(1u, "meta", false, "H W 77") &&
              adapter.takeNextBatch(&batch) &&
              findAction(batch, SESSION_ACT_SEND) != nullptr &&
              findAction(batch, SESSION_ACT_TIMER_SET) != nullptr,
          "live meta starts claim and timer");
    const uint8_t onlineTx = sendId(batch);
    adapter.enqueueTxResult(onlineTx, SESSION_TX_OK);
    check(adapter.takeNextBatch(&batch) &&
              actionIndex(batch, SESSION_ACT_TIMER_CANCEL) >= 0 &&
              actionIndex(batch, SESSION_ACT_TIMER_CANCEL) <
                  actionIndex(batch, SESSION_ACT_TIMER_SET),
          "successful publish invalidates old timer before replacement");
    const uint8_t joinTx = sendId(batch);
    adapter.enqueueTxResult(onlineTx, SESSION_TX_OK);
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "successful publish result is consumed once");
    adapter.enqueueTxResult(joinTx, SESSION_TX_OK);
    check(adapter.takeNextBatch(&batch) &&
              hasSessionStatus(batch, SESSION_CHANGED_READY) &&
              findAction(batch, SESSION_ACT_TIMER_CANCEL) != nullptr &&
              findAction(batch, SESSION_ACT_TIMER_SET) != nullptr,
          "join publish reaches READY with timer replacement");
}

static void testLinkDownPriority()
{
    DesktopSessionAdapter adapter;
    DesktopActionBatch batch;

    reachReady(adapter, batch);
    check(adapter.enqueueMqttRx(1u, "w2b", false, "CHAT before-down"),
          "MQTT external RX queues before link-down");
    adapter.enqueueLinkDown(1u);
    check(adapter.takeNextBatch(&batch) &&
              hasSessionStatus(batch, SESSION_CHANGED_ENDED),
          "MQTT link-down precedes already-queued external RX");
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "queued MQTT RX is inert after prioritized link-down");
}

static void testReadyStartedEndedAndRoutes()
{
    DesktopSessionAdapter adapter;
    DesktopActionBatch batch;
    const DesktopOwnedAction *action;

    reachReady(adapter, batch);
    check(adapter.enqueueMqttRx(1u, "meta", false, "GAME START") &&
              adapter.takeNextBatch(&batch),
          "live GAME START control drains");
    action = findAction(batch, SESSION_ACT_SEND);
    check(action != nullptr &&
              action->payload == QByteArray("ACK GAME START") &&
              action->action.data.send.route == SESSION_ROUTE_CONTROL &&
              action->action.data.send.retained == 0u &&
              action->action.data.send.link_id == 1u,
          "start ACK preserves live control route");
    const uint8_t startTx = sendId(batch);
    adapter.enqueueTxResult(startTx, SESSION_TX_OK);
    check(adapter.takeNextBatch(&batch) &&
              hasSessionStatus(batch, SESSION_CHANGED_STARTED),
          "start publish reaches STARTED");

    check(adapter.enqueueMqttRx(1u, "netchesszx/v1/room/w2b", false,
                            "CHAT hi") &&
              adapter.takeNextBatch(&batch),
          "game topic drains");
    action = findAction(batch, SESSION_ACT_DELIVER_GAME);
    check(action != nullptr && action->payload == QByteArray("hi") &&
              action->action.data.game.kind == SESSION_DELIVER_CHAT &&
              action->action.data.game.value == SESSION_CHAT_REMOTE,
          "game topic maps to SESSION_ROUTE_GAME");

    check(adapter.enqueueMqttRx(1u, "w2b", false, "MACH ZX") &&
              adapter.takeNextBatch(&batch) && batch.count == 1u,
          "live game MACH drains as one typed action");
    action = findAction(batch, SESSION_ACT_DELIVER_GAME);
    check(action != nullptr &&
              action->action.data.game.kind == SESSION_DELIVER_PLATFORM &&
              action->action.data.game.value == NETCHESS_PLAT_ZX &&
              action->action.data.game.delivery_id == 0u &&
              action->payload.isEmpty(),
          "MQTT MACH action carries typed platform without payload");
    check(adapter.enqueueMqttRx(1u, "w2b", true, "MACH PC") &&
              adapter.takeNextBatch(&batch) && batch.count == 0u,
          "retained MACH does not deliver or credit liveness");
    check(adapter.enqueueMqttRx(1u, "meta", false, "MACH PC") &&
              adapter.takeNextBatch(&batch) && batch.count == 0u,
          "non-game MACH route is ignored");
    check(adapter.enqueueMqttRx(1u, "w2b", false, "MACH ZZ") &&
              adapter.takeNextBatch(&batch) && batch.count == 0u,
          "unknown MACH is ignored");

    adapter.enqueueLocalRequest(SESSION_REQUEST_MOVE, 0u, "e2e4",
                                SESSION_PHASE_ACTIVE);
    check(adapter.takeNextBatch(&batch), "local MQTT move drains");
    const uint8_t moveTx = sendId(batch);
    adapter.enqueueTxResult(moveTx, SESSION_TX_OK);
    check(adapter.takeNextBatch(&batch), "move publish result drains");
    check(adapter.enqueueMqttRx(1u, "ack_b", false, "ACK 1 e4") &&
              adapter.takeNextBatch(&batch),
          "ack topic drains");
    action = findAction(batch, SESSION_ACT_DELIVER_GAME);
    check(action != nullptr &&
              action->action.data.game.kind == SESSION_DELIVER_LOCAL_MOVE &&
              action->payload == QByteArray("e2e4"),
          "ack topic maps to SESSION_ROUTE_ACK");

    adapter.enqueueLinkDown(1u);
    check(adapter.takeNextBatch(&batch) &&
              findAction(batch, SESSION_ACT_TIMER_CANCEL) != nullptr &&
              hasSessionStatus(batch, SESSION_CHANGED_ENDED) &&
              findAction(batch, SESSION_ACT_LINK_CLOSE) == nullptr,
          "broker disconnect emits LINK_DOWN outcome once without close");
    adapter.enqueueLinkDown(1u);
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "duplicate broker disconnect is inert");
    adapter.enqueueTimeout(SESSION_TIMER_LIVENESS);
    check(adapter.takeNextBatch(&batch) && batch.count == 0u,
          "stale timer is inert after cancellation");
}

static void testEndReasons()
{
    DesktopSessionAdapter remote;
    DesktopSessionAdapter local;
    DesktopSessionAdapter lost;
    DesktopActionBatch batch;
    const DesktopOwnedAction *ended;
    uint8_t byeTx;
    uint8_t offlineTx;

    reachReady(remote, batch);
    check(remote.enqueueMqttRx(1u, "w2b", false, "BYE") &&
              remote.takeNextBatch(&batch),
          "remote MQTT BYE drains");
    ended = findEndedAction(batch);
    check(ended != nullptr &&
              ended->action.data.session.end_reason ==
                  SESSION_END_REASON_REMOTE_BYE,
          "remote MQTT BYE marks remote end");

    reachReady(local, batch);
    local.enqueueLocalRequest(SESSION_REQUEST_BYE);
    check(local.takeNextBatch(&batch), "local MQTT BYE drains");
    byeTx = sendId(batch);
    check(byeTx != 0u, "local MQTT BYE sends");
    local.enqueueTxResult(byeTx, SESSION_TX_OK);
    check(local.takeNextBatch(&batch), "local MQTT BYE offline cleanup drains");
    offlineTx = sendId(batch);
    check(offlineTx != 0u, "local MQTT BYE clears retained presence");
    local.enqueueTxResult(offlineTx, SESSION_TX_OK);
    check(local.takeNextBatch(&batch), "local MQTT BYE completion drains");
    ended = findEndedAction(batch);
    check(ended != nullptr &&
              ended->action.data.session.end_reason ==
                  SESSION_END_REASON_LOCAL_BYE,
          "local MQTT BYE marks local end");

    reachReady(lost, batch);
    lost.enqueueLinkDown(1u);
    check(lost.takeNextBatch(&batch), "MQTT link loss drains");
    ended = findEndedAction(batch);
    check(ended != nullptr &&
              ended->action.data.session.end_reason ==
                  SESSION_END_REASON_TRANSPORT_LOST,
          "MQTT link loss marks transport loss");
}

static void testBusy()
{
    DesktopSessionAdapter adapter;
    DesktopActionBatch batch;

    check(adapter.initMqtt(SESSION_ROLE_GUEST, SESSION_COLOR_UNKNOWN),
          "busy adapter init");
    drainLinkUp(adapter, 1u);
    check(adapter.enqueueMqttRx(1u, "meta", true, "H W 77") &&
              adapter.takeNextBatch(&batch),
          "busy side probe drains");
    check(adapter.enqueueMqttRx(1u, "netchesszx/v1/room/pres_b", true,
                            "O B 77") &&
              adapter.takeNextBatch(&batch) &&
              hasSessionStatus(batch, SESSION_CHANGED_BUSY) &&
              findAction(batch, SESSION_ACT_LINK_CLOSE) != nullptr &&
              hasSessionStatus(batch, SESSION_CHANGED_ENDED),
          "retained presence maps to BUSY then ENDED");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testRoutingAndTxFailure();
    testBootstrapMetaSubscription();
    testLinkDownPriority();
    testReadyStartedEndedAndRoutes();
    testEndReasons();
    testBusy();

    if (failures != 0) {
        std::printf("PC MQTT adapter tests failed: %d\n", failures);
        return 1;
    }
    std::printf("PC MQTT adapter tests ok\n");
    return 0;
}
