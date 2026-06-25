#include "spectrum/session/poll.h"

#include "spectrum/config/session.h"
#include "spectrum/session/outgoing.h"
#include "spectrum/transport/link.h"
#include "spectrum/transport/keepalive_protocol.h"

uint8_t netchesszx_session_poll(netchesszx_session_ping_t *ping,
                                char *payload,
                                uint8_t payload_cap,
                                netchesszx_session_poll_result_t *out)
{
    int16_t link;
    uint8_t retained = 0u;
    uint8_t is_mqtt = netchesszx_transport_is_mqtt();
    uint8_t ping_ev;

    out->event = NETCHESSZX_SESSION_EVENT_UNKNOWN;
    out->retained = 0u;

    link = spectrum_link_read_payload(payload, payload_cap);
    if (spectrum_link_activity()) {
        netchesszx_session_ping_rx_mqtt_pingresp(ping);
        if (link == SPECTRUM_LINK_READ_TIMEOUT) {
            return NETCHESSZX_SESSION_POLL_NONE;
        }
    }
    if (link == -2) {
        return NETCHESSZX_SESSION_POLL_DISCONNECTED;
    }
    if (link == SPECTRUM_LINK_READ_TIMEOUT) {
        ping_ev = netchesszx_session_ping_timeout(ping, is_mqtt);
        if (ping_ev != NETCHESSZX_SESSION_PING_NONE) {
            if (ping_ev == NETCHESSZX_SESSION_PING_LOST ||
                !netchesszx_session_send_ping()) {
                return NETCHESSZX_SESSION_POLL_DISCONNECTED;
            }
            netchesszx_session_ping_sent(ping, is_mqtt);
        }
        return NETCHESSZX_SESSION_POLL_NONE;
    }
    if (link < 0) {
        return NETCHESSZX_SESSION_POLL_NONE;
    }

    netchesszx_session_ping_rx_data(ping);
    spectrum_link_background_drain();
    if (is_mqtt) {
        retained = (uint8_t)((spectrum_link_payload_flags() &
                              SPECTRUM_LINK_PAYLOAD_RETAINED) != 0u);
    }
    out->event = netchesszx_session_classify_event(payload,
                                                   is_mqtt,
                                                   retained,
                                                   netchesszx_session_is_host());
    out->retained = retained;
    if (out->event == NETCHESSZX_SESSION_EVENT_PING) {
        if (!netchesszx_session_send_ack_ping(payload)) {
            return NETCHESSZX_SESSION_POLL_DISCONNECTED;
        }
        out->event = NETCHESSZX_SESSION_EVENT_UNKNOWN;
        out->retained = 0u;
        return NETCHESSZX_SESSION_POLL_NONE;
    }
    if (out->event == NETCHESSZX_SESSION_EVENT_ACK_PING) {
        (void)payload;
        (void)netchesszx_session_ping_ack(ping, is_mqtt);
        out->event = NETCHESSZX_SESSION_EVENT_UNKNOWN;
        out->retained = 0u;
        return NETCHESSZX_SESSION_POLL_NONE;
    }
    if (netchesszx_session_event_ignores_retained(out->event, retained)) {
        out->event = NETCHESSZX_SESSION_EVENT_UNKNOWN;
        out->retained = 0u;
        return NETCHESSZX_SESSION_POLL_NONE;
    }
    return NETCHESSZX_SESSION_POLL_EVENT;
}
