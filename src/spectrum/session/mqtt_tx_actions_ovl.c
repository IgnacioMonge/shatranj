#include "spectrum/platform/text.h"
#include "spectrum/session/mqtt_wire.h"

static uint8_t mqtt_tx_publish_flags_ovl(uint8_t flags, const char *payload)
    NETCHESSZX_CALLEE;

uint8_t mqtt_tx_publish_presence_ovl(uint8_t *ctx) NETCHESSZX_FASTCALL
{
    uint8_t flags = ctx[NETCHESSZX_SESSION_MQTT_TX_CTX_FLAGS];
    uint8_t route = (uint8_t)(flags & NETCHESSZX_SESSION_MQTT_TX_ROUTE_MASK);
    uint8_t result = mqtt_tx_publish_flags_ovl(
        flags,
        spectrum_net_mqtt_presence_payload(
            (flags & NETCHESSZX_SESSION_ROUTE_OFFLINE)
                ? route : NETCHESSZX_SESSION_ROUTE_DEFAULT));

    if (!result || !(flags & NETCHESSZX_SESSION_ROUTE_CLEAR_META)) {
        return result;
    }
    return mqtt_tx_publish_flags_ovl(
        NETCHESSZX_SESSION_ROUTE_META | NETCHESSZX_SESSION_ROUTE_RETAIN, "");
}
