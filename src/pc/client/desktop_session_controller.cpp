#include "desktop_session_controller.h"

#include <utility>

DesktopSessionController::DesktopSessionController(QObject *parent)
    : QObject(parent)
{
    for (uint8_t timerId = 0u; timerId < SESSION_TIMER_COUNT; ++timerId) {
        QTimer &timer = timers_[timerId];

        timer.setSingleShot(true);
        timer.setTimerType(Qt::PreciseTimer);
        connect(&timer, &QTimer::timeout, this, [this, timerId]() {
            if (!initialized_) {
                return;
            }
            session_.enqueueTimeout(timerId);
            pump();
        });
    }
}

void DesktopSessionController::setCallbacks(Callbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

bool DesktopSessionController::initializeDirect(uint8_t role, uint8_t hostColor)
{
    stopTimers();
    invalidateDeferredMqttActions();
    mode_ = Mode::Direct;
    initialized_ = session_.initDirect(role, hostColor);
    return initialized_;
}

bool DesktopSessionController::initializeMqtt(uint8_t role,
                                              uint8_t hostColor,
                                              uint16_t sessionId)
{
    stopTimers();
    invalidateDeferredMqttActions();
    mode_ = Mode::Mqtt;
    initialized_ = session_.initMqtt(role, hostColor, sessionId);
    return initialized_;
}

bool DesktopSessionController::linkUp(uint8_t linkId)
{
    if (!initialized_) {
        return false;
    }
    session_.enqueueLinkUp(linkId);
    return pump();
}

bool DesktopSessionController::linkDown(uint8_t linkId)
{
    if (!initialized_) {
        return false;
    }
    if (mode_ == Mode::Mqtt) {
        invalidateDeferredMqttActions();
    }
    session_.enqueueLinkDown(linkId);
    return pump();
}

void DesktopSessionController::receiveDirect(uint8_t linkId,
                                             const QByteArray &payload)
{
    if (!initialized_ || mode_ != Mode::Direct) {
        return;
    }
    session_.enqueueDirectRx(linkId, payload);
}

bool DesktopSessionController::receiveMqtt(uint8_t linkId,
                                           const QByteArray &topic,
                                           bool retained,
                                           const QByteArray &payload)
{
    if (!initialized_ || mode_ != Mode::Mqtt ||
        !session_.enqueueMqttRx(linkId, topic, retained, payload)) {
        return false;
    }
    pump();
    return true;
}

void DesktopSessionController::txResult(uint8_t txId, uint8_t result)
{
    if (!initialized_) {
        return;
    }
    session_.enqueueTxResult(txId, result);
    pump();
}

bool DesktopSessionController::submitLocalRequest(uint8_t request,
                                                  uint16_t value,
                                                  const QByteArray &payload,
                                                  uint8_t phase)
{
    if (!initialized_) {
        return false;
    }
    session_.enqueueLocalRequest(request, value, payload, phase);
    return pump();
}

void DesktopSessionController::submitUserDecision(uint8_t requestId,
                                                  uint8_t decision)
{
    if (!initialized_) {
        return;
    }
    session_.enqueueUserDecision(requestId, decision);
    pump();
}

void DesktopSessionController::submitGameResult(uint8_t deliveryId,
                                                uint16_t value,
                                                uint8_t result,
                                                const QByteArray &detail)
{
    if (!initialized_) {
        return;
    }
    session_.enqueueGameResult(deliveryId, value, result, detail);
    pump();
}

QByteArray DesktopSessionController::mqttTopicSuffixForRoute(uint8_t route) const
{
    return session_.mqttTopicSuffixForRoute(route);
}

void DesktopSessionController::cancelTimer(uint8_t timerId)
{
    if (timerId >= SESSION_TIMER_COUNT) {
        return;
    }
    timers_[timerId].stop();
}

void DesktopSessionController::setTimer(uint8_t timerId,
                                        uint16_t durationTicks)
{
    if (timerId >= SESSION_TIMER_COUNT) {
        return;
    }
    cancelTimer(timerId);
    timers_[timerId].start(static_cast<int>(durationTicks) *
                           SESSION_PROTOCOL_TICK_MS);
}

void DesktopSessionController::stopTimers()
{
    for (uint8_t timerId = 0u; timerId < SESSION_TIMER_COUNT; ++timerId) {
        cancelTimer(timerId);
    }
}

void DesktopSessionController::dispatch(
    const SessionAction &action,
    const QByteArray &payload,
    QVector<DesktopSessionFollowup> &followups)
{
    switch (action.type) {
    case SESSION_ACT_SEND: {
        const bool sent = callbacks_.send && callbacks_.send(mode_, action, payload);
        followups.append(DesktopSessionFollowup{
            SESSION_EV_TX_RESULT,
            action.data.send.tx_id,
            static_cast<uint8_t>(sent ? SESSION_TX_OK : SESSION_TX_FAILED),
            0u,
            QByteArray()});
        break;
    }
    case SESSION_ACT_TIMER_SET:
        setTimer(action.data.timer_set.timer_id,
                 action.data.timer_set.duration_ticks);
        break;
    case SESSION_ACT_TIMER_CANCEL:
        cancelTimer(action.data.timer_cancel.timer_id);
        break;
    case SESSION_ACT_LINK_CLOSE:
        if (callbacks_.closeLink) {
            callbacks_.closeLink(mode_, action.data.link_close.link_id);
        }
        break;
    case SESSION_ACT_REQUEST_DECISION:
        if (callbacks_.decision) {
            callbacks_.decision(action.data.decision.request_id,
                                action.data.decision.control,
                                action.data.decision.value);
        }
        break;
    case SESSION_ACT_DELIVER_GAME:
        if (callbacks_.game) {
            callbacks_.game(mode_, action.data.game.kind,
                            action.data.game.delivery_id,
                            action.data.game.value, payload, followups);
        }
        break;
    case SESSION_ACT_SESSION_CHANGED:
        if (callbacks_.sessionChanged) {
            callbacks_.sessionChanged(action.data.session.status,
                                      action.data.session.end_reason);
        }
        break;
    case SESSION_ACT_SIDE_CHANGED:
        if (callbacks_.sideChanged) {
            callbacks_.sideChanged(mode_, action.data.side.color,
                                   action.data.side.session_id);
        }
        break;
    default:
        if (callbacks_.error) {
            callbacks_.error(QStringLiteral("unknown session action %1")
                                 .arg(action.type));
        }
        break;
    }
}

void DesktopSessionController::applyFollowups(
    const QVector<DesktopSessionFollowup> &followups)
{
    for (qsizetype i = followups.size(); i > 0; --i) {
        const DesktopSessionFollowup &followup = followups.at(i - 1);
        if (followup.type == SESSION_EV_TX_RESULT) {
            session_.enqueueTxResult(followup.id, followup.result);
        } else {
            session_.enqueueGameResult(followup.id, followup.value,
                                       followup.result, followup.detail);
        }
    }
}

bool DesktopSessionController::dispatchMqttBatch(
    const DesktopActionBatch &batch,
    uint8_t *next,
    QVector<DesktopSessionFollowup> &followups,
    uint32_t generation)
{
    while (*next < batch.count) {
        if (generation != mqttDispatchGeneration_) {
            return false;
        }
        const DesktopOwnedAction &owned = batch.actions[*next];
        const bool blocked =
            callbacks_.mqttTransportReady &&
            !callbacks_.mqttTransportReady() &&
            (owned.action.type == SESSION_ACT_SEND ||
             owned.action.type == SESSION_ACT_DELIVER_GAME);
        if (blocked) {
            return false;
        }
        dispatch(owned.action, owned.payload, followups);
        if (generation != mqttDispatchGeneration_) {
            return false;
        }
        ++*next;
    }
    return true;
}

void DesktopSessionController::clearDeferredMqttActions()
{
    deferredMqttBatch_ = DesktopActionBatch{};
    deferredMqttFollowups_.clear();
    deferredMqttNext_ = 0u;
    mqttBatchDeferred_ = false;
}

void DesktopSessionController::invalidateDeferredMqttActions()
{
    ++mqttDispatchGeneration_;
    clearDeferredMqttActions();
}

bool DesktopSessionController::pump()
{
    if (!initialized_ || pumping_) {
        return false;
    }
    pumping_ = true;
    bool produced = false;

    if (mode_ == Mode::Mqtt) {
        if (mqttBatchDeferred_) {
            const uint32_t generation = mqttDispatchGeneration_;
            if (!dispatchMqttBatch(deferredMqttBatch_, &deferredMqttNext_,
                                   deferredMqttFollowups_, generation)) {
                if (generation == mqttDispatchGeneration_) {
                    pumping_ = false;
                    return false;
                }
                clearDeferredMqttActions();
            } else {
                applyFollowups(deferredMqttFollowups_);
                clearDeferredMqttActions();
                produced = true;
            }
        }

        DesktopActionBatch batch;
        while (initialized_ && session_.takeNextBatch(&batch)) {
            produced = produced || batch.count != 0u;
            QVector<DesktopSessionFollowup> followups;
            uint8_t next = 0u;
            const uint32_t generation = mqttDispatchGeneration_;
            if (!dispatchMqttBatch(batch, &next, followups, generation)) {
                if (generation != mqttDispatchGeneration_) {
                    continue;
                }
                deferredMqttBatch_ = batch;
                deferredMqttFollowups_ = std::move(followups);
                deferredMqttNext_ = next;
                mqttBatchDeferred_ = true;
                break;
            }
            applyFollowups(followups);
        }
    } else {
        DesktopActionBatch batch;
        while (session_.takeNextBatch(&batch)) {
            produced = produced || batch.count != 0u;
            QVector<DesktopSessionFollowup> followups;
            for (uint8_t i = 0u; i < batch.count; ++i) {
                dispatch(batch.actions[i].action,
                         batch.actions[i].payload, followups);
            }
            applyFollowups(followups);
        }
    }
    pumping_ = false;
    return produced;
}
