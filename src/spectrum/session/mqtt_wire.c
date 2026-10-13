#include "spectrum/session/mqtt_wire.h"

#include "common/protocol/mqtt_session_protocol.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/text.h"
#include "spectrum/transport/link.h"

#ifdef NETCHESSZX_HOST_SESSION_TEST
static char mqtt_presence_payload[16];
#else
#define mqtt_presence_payload ((char *)NETCHESSZX_LOWRAM_MQTT_PRESENCE_ADDR)
#endif

#define MQTT_SUFFIX_OUT 0u
#define MQTT_SUFFIX_IN 1u
#define MQTT_SUFFIX_OUT_ACK 2u
#define MQTT_SUFFIX_IN_ACK 3u
#define MQTT_SUFFIX_PRESENCE 4u
#define MQTT_SUFFIX_PEER_PRESENCE 5u
static const char mqtt_suffix_w2b[] = "w2b";
static const char mqtt_suffix_b2w[] = "b2w";
static const char mqtt_suffix_ack_b[] = "ack_b";
static const char mqtt_suffix_ack_w[] = "ack_w";
static const char mqtt_suffix_pres_w[] = "pres_w";
static const char mqtt_suffix_pres_b[] = "pres_b";

static const char *const mqtt_side_suffixes[] = {
    mqtt_suffix_w2b, mqtt_suffix_b2w,
    mqtt_suffix_ack_b, mqtt_suffix_ack_w,
    mqtt_suffix_pres_w, mqtt_suffix_pres_b
};

static const char *mqtt_side_suffix(uint8_t pair_index) __z88dk_fastcall
{
    return mqtt_side_suffixes[(uint8_t)(pair_index ^
        (netchesszx_local_is_white() ? 0u : 1u))];
}

const char *spectrum_net_mqtt_out_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_OUT);
}

const char *spectrum_net_mqtt_in_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_IN);
}

const char *spectrum_net_mqtt_out_ack_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_OUT_ACK);
}

const char *spectrum_net_mqtt_in_ack_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_IN_ACK);
}

const char *spectrum_net_mqtt_presence_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_PRESENCE);
}

const char *spectrum_net_mqtt_peer_presence_suffix(void)
{
    return mqtt_side_suffix(MQTT_SUFFIX_PEER_PRESENCE);
}

const char *spectrum_net_mqtt_presence_payload(uint8_t route)
    NETCHESSZX_FASTCALL
{
    char side = netchesszx_local_side_char();

    if (route == NETCHESSZX_SESSION_ROUTE_PRESENCE_PEER) {
        side ^= ('W' ^ 'B');
    }
    mqtt_presence_payload[0] = route ? 'F' : 'O';
    mqtt_presence_payload[1] = ' ';
    mqtt_presence_payload[2] = side;
    mqtt_presence_payload[3] = ' ';
    (void)spectrum_append_u16(mqtt_presence_payload + 4u,
                              netchesszx_mqtt_session_id);
    return mqtt_presence_payload;
}

void spectrum_net_mqtt_setup_payload(char *out) NETCHESSZX_FASTCALL
{
    uint8_t host = netchesszx_session_is_host();
    char *p = out + 2u;

    out[0] = host ? 'H' : 'J';
    out[1] = ' ';
    if (host) {
        *p++ = netchesszx_local_side_char();
        *p++ = ' ';
    }
    (void)spectrum_append_u16(p, netchesszx_mqtt_session_id);
}

/* Retained offline marker (F <side> <sid>) over a retained presence O.
   A host releasing its own seat also clears retained H metadata; peer-seat
   releases keep H because the host remains available for a replacement. */
uint8_t netchesszx_session_mqtt_publish_presence(void)
{
    return spectrum_net_mqtt_publish_presence(
        NETCHESSZX_SESSION_ROUTE_PRESENCE | NETCHESSZX_SESSION_ROUTE_RETAIN);
}

uint8_t netchesszx_session_mqtt_publish_offline(uint8_t route)
    NETCHESSZX_FASTCALL
{
    uint8_t flags = (uint8_t)(route | NETCHESSZX_SESSION_ROUTE_RETAIN |
                              NETCHESSZX_SESSION_ROUTE_OFFLINE);

    if (route == NETCHESSZX_SESSION_ROUTE_PRESENCE &&
        netchesszx_session_is_host()) {
        flags |= NETCHESSZX_SESSION_ROUTE_CLEAR_META;
    }
    return spectrum_net_mqtt_publish_presence(flags);
}

uint8_t netchesszx_session_mqtt_activate_side(void)
{
    return spectrum_net_mqtt_subscribe_side();
}

uint8_t netchesszx_session_mqtt_start(void)
{
    uint8_t host = netchesszx_session_is_host();

    return spectrum_net_mqtt_connect(
        host ? SPECTRUM_NET_MQTT_CONNECT_WILL : 0u);
}

uint8_t netchesszx_session_mqtt_probe_seat(void)
{
    return spectrum_net_mqtt_subscribe_seat();
}
