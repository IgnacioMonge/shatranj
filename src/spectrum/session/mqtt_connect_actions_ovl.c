#include "common/protocol/mqtt_session_protocol.h"
#include "spectrum/config/session.h"
#include "spectrum/platform/text.h"
#include "spectrum/session/mqtt_wire.h"

static uint8_t mqtt_connect_transport_start_ovl(void);
static uint8_t mqtt_connect_transport_subscribe_side_ovl(void);
static uint8_t mqtt_connect_transport_publish_ovl(const char *suffix,
                                            const char *payload,
                                            uint8_t retain) NETCHESSZX_CALLEE;
static uint8_t mqtt_connect_transport_abort_ovl(void);

#ifdef NETCHESSZX_HOST_SESSION_TEST
extern void netchesszx_host_mqtt_observe_route(uint8_t route);
#define MQTT_CONNECT_PUBLISH_META(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_route(NETCHESSZX_SESSION_ROUTE_META), \
     mqtt_connect_transport_publish_ovl((suffix), (payload), (retain)))
#define MQTT_CONNECT_PUBLISH_PRESENCE(suffix, payload) \
    (netchesszx_host_mqtt_observe_route(NETCHESSZX_SESSION_ROUTE_PRESENCE), \
     mqtt_connect_transport_publish_ovl((suffix), (payload), 1u))
#else
#define MQTT_CONNECT_PUBLISH_META(suffix, payload, retain) \
    mqtt_connect_transport_publish_ovl((suffix), (payload), (retain))
#define MQTT_CONNECT_PUBLISH_PRESENCE(suffix, payload) \
    mqtt_connect_transport_publish_ovl((suffix), (payload), 1u)
#endif

uint8_t mqtt_activate_side_ovl(void)
{
    char payload[10];
    const char *presence_suffix = spectrum_net_mqtt_presence_suffix();

    if (!mqtt_connect_transport_subscribe_side_ovl() ||
        !MQTT_CONNECT_PUBLISH_PRESENCE(
            presence_suffix, spectrum_net_mqtt_presence_payload(
                NETCHESSZX_SESSION_ROUTE_DEFAULT))) {
        return mqtt_connect_transport_abort_ovl();
    }
    spectrum_net_mqtt_setup_payload(payload);
    if (MQTT_CONNECT_PUBLISH_META(
            mqtt_meta_suffix, payload, netchesszx_session_is_host())) {
        return 1u;
    }
    (void)spectrum_append_text(payload, spectrum_net_mqtt_presence_payload(
        NETCHESSZX_SESSION_ROUTE_DEFAULT));
    payload[0] = NETCHESS_MQTT_SESSION_VERB_OFFLINE;
    (void)MQTT_CONNECT_PUBLISH_PRESENCE(presence_suffix, payload);
    return mqtt_connect_transport_abort_ovl();
}

uint8_t mqtt_connect_start_ovl(void)
{
    if (!mqtt_connect_transport_start_ovl()) {
        return 0u;
    }
    if (netchesszx_session_is_host() || netchesszx_session_identity_host_color_ready()) {
        return mqtt_activate_side_ovl();
    }
    return 1u;
}
