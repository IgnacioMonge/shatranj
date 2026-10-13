#include "spectrum/transport/link.h"

#include "spectrum/overlay/overlay.h"

#ifdef NETCHESSZX_HOST_SESSION_TEST
extern void netchesszx_host_mqtt_set_context_payload(const char *payload);
#endif

void spectrum_net_mqtt_set_publish_flags(uint8_t flags) NETCHESSZX_FASTCALL
{
    spectrum_overlay_context[NETCHESSZX_MQTT_TX_CTX_FLAGS] = flags;
}

uint8_t spectrum_net_mqtt_publish(const char *payload) NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_HOST_SESSION_TEST
    uint16_t payload_addr = 0u;

    netchesszx_host_mqtt_set_context_payload(payload);
#else
    uint16_t payload_addr = (uint16_t)payload;
#endif
    SPECTRUM_OVL_CTX_SET16(NETCHESSZX_MQTT_TX_CTX_PAYLOAD_LO,
                           NETCHESSZX_MQTT_TX_CTX_PAYLOAD_HI, payload_addr);
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_SEND_TEXT);
}

uint8_t spectrum_net_mqtt_publish_setup(uint8_t mode) NETCHESSZX_FASTCALL
{
    spectrum_overlay_context[NETCHESSZX_MQTT_TX_CTX_FLAGS] = mode;
    return spectrum_overlay_exec_cached(SPECTRUM_OVL_MQTT_TX,
                                        SPECTRUM_OVL_MQTT_TX_PUBLISH_SETUP);
}

uint8_t spectrum_net_mqtt_publish_presence(uint8_t flags) NETCHESSZX_FASTCALL
{
    spectrum_overlay_context[NETCHESSZX_MQTT_TX_CTX_FLAGS] = flags;
    return spectrum_overlay_exec_cached(
        SPECTRUM_OVL_MQTT_TX, SPECTRUM_OVL_MQTT_TX_PUBLISH_PRESENCE);
}
