#ifndef NETCHESSZX_SPECTRUM_SESSION_PING_H
#define NETCHESSZX_SPECTRUM_SESSION_PING_H

#include <stdint.h>

#define NETCHESSZX_SESSION_PING_NONE 0u
#define NETCHESSZX_SESSION_PING_SEND 1u
#define NETCHESSZX_SESSION_PING_LOST 2u

typedef struct netchesszx_session_ping {
    uint8_t idle_ticks;
    uint8_t misses;
    uint8_t direct_pending;
} netchesszx_session_ping_t;

void netchesszx_session_ping_reset(netchesszx_session_ping_t *ping);
void netchesszx_session_ping_rx_data(netchesszx_session_ping_t *ping);
#define netchesszx_session_ping_rx_mqtt_pingresp netchesszx_session_ping_rx_data
uint8_t netchesszx_session_ping_timeout(netchesszx_session_ping_t *ping,
                                        uint8_t is_mqtt);
void netchesszx_session_ping_sent(netchesszx_session_ping_t *ping,
                                  uint8_t is_mqtt);
uint8_t netchesszx_session_ping_ack(netchesszx_session_ping_t *ping,
                                    uint8_t is_mqtt);

#endif
