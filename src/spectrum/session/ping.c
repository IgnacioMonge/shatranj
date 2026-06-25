#include "spectrum/session/ping.h"

#define MQTT_IDLE_PING_TICKS 120u
#define MQTT_PING_MISSES_MAX 4u
#define DIRECT_IDLE_PING_TICKS 20u
#define DIRECT_PING_MISSES_MAX 2u

void netchesszx_session_ping_reset(netchesszx_session_ping_t *ping)
{
    ping->idle_ticks = 0u;
    ping->misses = 0u;
    ping->direct_pending = 0u;
}

void netchesszx_session_ping_rx_data(netchesszx_session_ping_t *ping)
{
    ping->idle_ticks = 0u;
    ping->misses = 0u;
}

uint8_t netchesszx_session_ping_timeout(netchesszx_session_ping_t *ping,
                                        uint8_t is_mqtt)
{
    ++ping->idle_ticks;
    if (ping->idle_ticks < (is_mqtt ? MQTT_IDLE_PING_TICKS :
                                      DIRECT_IDLE_PING_TICKS)) {
        return NETCHESSZX_SESSION_PING_NONE;
    }

    ping->idle_ticks = 0u;
    if (ping->misses >= (is_mqtt ? MQTT_PING_MISSES_MAX :
                                   DIRECT_PING_MISSES_MAX) &&
        (is_mqtt || ping->direct_pending)) {
        return NETCHESSZX_SESSION_PING_LOST;
    }
    return NETCHESSZX_SESSION_PING_SEND;
}

void netchesszx_session_ping_sent(netchesszx_session_ping_t *ping,
                                  uint8_t is_mqtt)
{
    if (is_mqtt) {
        ++ping->misses;
        return;
    }
    ping->direct_pending = 1u;
    ++ping->misses;
}

uint8_t netchesszx_session_ping_ack(netchesszx_session_ping_t *ping,
                                    uint8_t is_mqtt)
{
    if (is_mqtt) {
        return 1u;
    }
    if (ping->direct_pending) {
        ping->misses = 0u;
        ping->direct_pending = 0u;
        return 1u;
    }
    return 0u;
}
