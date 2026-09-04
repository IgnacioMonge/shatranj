#include "spectrum/overlay/overlay_api.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/ui_messages.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/next_baud_recovery.h"
#include "spectrum/lowram_map.h"
#include "spectrum/transport/net.h"

uint8_t mqtt_activate_side_ovl(void);

#ifdef NETCHESSZX_SPECTRANEXT

#ifndef NETCHESSZX_HOST_TEST
#include "spxn.h"
#endif

#define WAIT_LONG 2000u

extern uint16_t mqtt_next_id;
extern char last_ip[];
extern uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len);
extern uint8_t mqtt_wait_packet_into(uint8_t *packet, uint8_t wanted,
                                     uint16_t frames);
extern uint8_t mqtt_enter_stream_mode(void);
extern void mqtt_abort_stream_mode(void);
extern uint16_t mqtt_connect_packet_ovl(void);
extern const char mqtt_will_topic_prefix[];

#ifdef NETCHESSZX_HOST_TEST
static uint8_t mqtt_packet_ovl_storage[SPECTRUM_MQTT_PACKET_MAX];
#define mqtt_packet_ovl mqtt_packet_ovl_storage
#else
#define mqtt_packet_ovl ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif

static uint16_t mqtt_alloc_id_ovl(void)
{
    uint16_t id = mqtt_next_id++;

    if (mqtt_next_id == 0u) {
        mqtt_next_id = 1u;
    }
    return id;
}

static void mqtt_topic_ovl(char *out, const char *suffix)
{
    char *p = spectrum_append_text(out, mqtt_will_topic_prefix);

    p = spectrum_append_text(p, netchesszx_mqtt_code);
    p = spectrum_append_text(p, "/");
    (void)spectrum_append_text(p, suffix);
}

static uint8_t mqtt_subscribe_suffix_ovl(const char *suffix)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint16_t id;
    uint8_t len;

    mqtt_topic_ovl(topic, suffix);
    id = mqtt_alloc_id_ovl();
    len = spectrum_mqtt_subscribe(mqtt_packet_ovl,
                                  SPECTRUM_MQTT_PACKET_MAX, id, topic);
    if (len == 0u || !mqtt_send_raw_packet(mqtt_packet_ovl, len) ||
        !mqtt_wait_packet_into(mqtt_packet_ovl, SPECTRUM_MQTT_SUBACK,
                               WAIT_LONG)) {
        return 0u;
    }
    return (uint8_t)(mqtt_packet_ovl[1u] == 3u &&
                     mqtt_packet_ovl[2u] == (uint8_t)(id >> 8) &&
                     mqtt_packet_ovl[3u] == (uint8_t)id &&
                     mqtt_packet_ovl[4u] <= 2u);
}

static uint8_t mqtt_publish_suffix_ovl(const char *suffix,
                                       const char *payload,
                                       uint8_t retain)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint8_t len;

    mqtt_topic_ovl(topic, suffix);
    len = spectrum_mqtt_publish(mqtt_packet_ovl,
                                SPECTRUM_MQTT_PACKET_MAX,
                                mqtt_alloc_id_ovl(), topic, payload, retain);
    return (uint8_t)(len != 0u && mqtt_send_raw_packet(mqtt_packet_ovl, len));
}

static uint8_t mqtt_subscribe_all_ovl(void)
{
    return mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_ack_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_peer_presence_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_presence_suffix());
}

static uint8_t mqtt_publish_setup_ovl(void)
{
    char setup[32];

    spectrum_net_mqtt_setup_payload(setup);
    return mqtt_publish_suffix_ovl("meta", setup,
                                  netchesszx_session_is_host() ? 1u : 0u);
}

static uint8_t mqtt_activate_side_internal(void)
{
    return mqtt_subscribe_all_ovl() &&
           mqtt_publish_suffix_ovl(spectrum_net_mqtt_presence_suffix(),
                                   spectrum_net_mqtt_presence_payload(), 1u);
}

uint8_t net_preflight_ovl(void)
{
    struct spxn_status status;
    char *p = last_ip;
    uint8_t i;

    last_ip[0] = '\0';
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_OK] = 0u;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_RETRY] =
        SPECTRUM_OVL_PREFLIGHT_RETRY_DEFAULT;
    if (spxn_status(&status) != SPXN_OK) {
        return 1u;
    }
    for (i = 0u; i < 4u; ++i) {
        p = spectrum_append_u16(p, status.ip4host[i]);
        if (i != 3u) {
            *p++ = '.';
        }
    }
    *p = '\0';
    if (status.ip4host[0] == 0u && status.ip4host[1] == 0u &&
        status.ip4host[2] == 0u && status.ip4host[3] == 0u) {
        spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_RETRY] =
            SPECTRUM_OVL_PREFLIGHT_RETRY_AGAIN;
        return 1u;
    }
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_OK] = 1u;
    return 1u;
}

uint8_t mqtt_connect_start_ovl(void)
{
    uint8_t ip4be[4];
    uint8_t len;

    mqtt_next_id = 1u;
    spxn_close();
    if (spxn_resolve(netchesszx_mqtt_host, ip4be) != SPXN_OK ||
        spxn_connect(ip4be, netchesszx_mqtt_port) != SPXN_OK ||
        !mqtt_enter_stream_mode()) {
        mqtt_abort_stream_mode();
        return 0u;
    }
    len = (uint8_t)mqtt_connect_packet_ovl();
    if (len == 0u || !mqtt_send_raw_packet(mqtt_packet_ovl, len) ||
        !mqtt_wait_packet_into(mqtt_packet_ovl, SPECTRUM_MQTT_CONNACK,
                               WAIT_LONG) ||
        mqtt_packet_ovl[1u] != 2u || mqtt_packet_ovl[3u] != 0u ||
        !mqtt_subscribe_suffix_ovl("meta")) {
        mqtt_abort_stream_mode();
        return 0u;
    }
    if ((netchesszx_session_is_host() || netchesszx_host_color_ready) &&
        !mqtt_activate_side_ovl()) {
        return 0u;
    }
    return 1u;
}

uint8_t mqtt_activate_side_ovl(void)
{
    char offline[10];

    if (mqtt_activate_side_internal() && mqtt_publish_setup_ovl()) {
        return 1u;
    }
    (void)spectrum_append_text(offline, spectrum_net_mqtt_presence_payload());
    offline[0] = NETCHESS_MQTT_SESSION_VERB_OFFLINE;
    (void)mqtt_publish_suffix_ovl(spectrum_net_mqtt_presence_suffix(),
                                  offline, 1u);
    mqtt_abort_stream_mode();
    return 0u;
}

uint8_t mqtt_probe_seat_ovl(void)
{
    if (mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_presence_suffix())) {
        return 1u;
    }
    mqtt_abort_stream_mode();
    return 0u;
}

#else

#define WAIT_FAST 50
#define WAIT_SHORT 150
#define WAIT_MED 500
#define WAIT_LONG 2000
#define MQTT_RECOVERY_AVAILABLE 1u

#define PREFLIGHT_ROW_UART "\007"
#define PREFLIGHT_ROW_ESP "\010"
#define PREFLIGHT_ROW_WIFI "\011"
#define PREFLIGHT_ROW_IP "\012"

extern char last_ip[];
extern uint16_t mqtt_next_id;
extern uint8_t mqtt_stream_len;
extern uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len);
extern uint8_t mqtt_wait_packet_into(uint8_t *packet, uint8_t wanted, uint16_t frames);
extern uint8_t mqtt_enter_stream_mode(void);
extern void mqtt_abort_stream_mode(void);
extern uint16_t mqtt_connect_packet_ovl(void);
extern const char mqtt_will_topic_prefix[];
static void mqtt_recovery_begin_ovl(void)
{
    last_ip[NETCHESSZX_LOWRAM_LAST_IP_SIZE - 1u] = MQTT_RECOVERY_AVAILABLE;
}

static uint8_t mqtt_abort_ovl(void)
{
    mqtt_abort_stream_mode();
    return 0u;
}

#ifdef NETCHESSZX_HOST_TEST
#define spectrum_net_at_cipserver_0 "AT+CIPSERVER=0"
#define spectrum_net_at_cipclose "AT+CIPCLOSE"
#define spectrum_net_at_cipmux_0 "AT+CIPMUX=0"
#define spectrum_net_at_cipmode_0 "AT+CIPMODE=0"
#else
extern const char spectrum_net_at_cipserver_0[];
extern const char spectrum_net_at_cipclose[];
extern const char spectrum_net_at_cipmux_0[];
extern const char spectrum_net_at_cipmode_0[];
#endif

#ifdef NETCHESSZX_HOST_SESSION_TEST
extern void netchesszx_host_mqtt_observe_meta(void);
extern void netchesszx_host_mqtt_observe_presence(const char *suffix);
#define MQTT_CONNECT_PUBLISH_META(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_meta(), \
     mqtt_publish_suffix_ovl((suffix), (payload), (retain)))
#define MQTT_CONNECT_PUBLISH_PRESENCE(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_presence(suffix), \
     mqtt_publish_suffix_ovl((suffix), (payload), (retain)))
#else
#define MQTT_CONNECT_PUBLISH_META(suffix, payload, retain) \
    mqtt_publish_suffix_ovl((suffix), (payload), (retain))
#define MQTT_CONNECT_PUBLISH_PRESENCE(suffix, payload, retain) \
    mqtt_publish_suffix_ovl((suffix), (payload), (retain))
#endif

#ifdef NETCHESSZX_HOST_SESSION_TEST
uint8_t mqtt_packet_ovl[SPECTRUM_MQTT_PACKET_MAX];
#else
#define mqtt_packet_ovl \
    ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif

static uint8_t mqtt_send_packet_ovl(uint8_t len) __z88dk_fastcall
{
    return mqtt_send_raw_packet(mqtt_packet_ovl, len);
}

static uint8_t mqtt_wait_packet_ovl(uint8_t wanted) __z88dk_fastcall
{
    return mqtt_wait_packet_into(mqtt_packet_ovl, wanted, WAIT_LONG);
}

static const char at_cwmode[] = "AT+CWMODE=1";
static const char at_cwmode_def[] = "AT+CWMODE_DEF=1";
static const char at_cwautoconn[] = "AT+CWAUTOCONN=1";
static const char mqtt_meta_suffix[] = "meta";

static uint8_t at_cmd_short(const char *cmd) __z88dk_fastcall
{
    return spectrum_net_at_cmd(cmd, WAIT_SHORT);
}

static uint8_t at_cmd_med(const char *cmd) __z88dk_fastcall
{
    return spectrum_net_at_cmd(cmd, WAIT_MED);
}

static uint8_t esp_at_query_ip_with_retry(uint8_t attempts) __z88dk_fastcall
{
    while (attempts-- != 0u) {
        last_ip[0] = '\0';
        (void)at_cmd_med("AT+CIFSR");
        if (last_ip[0] != '\0') {
            return 1u;
        }
        spectrum_net_guard_wait(50u);
    }
    return 0u;
}

static uint8_t esp_at_prepare_radio(void)
{
    spectrum_uart_flush(30u);
    (void)at_cmd_short("ATE0");
    if (!at_cmd_med(at_cwmode)) {
        (void)at_cmd_med(at_cwmode_def);
    }
    (void)at_cmd_short(at_cwautoconn);
    (void)at_cmd_med(spectrum_net_at_cipserver_0);
    (void)at_cmd_med(spectrum_net_at_cipclose);
    (void)at_cmd_med("AT+CIPCLOSE=5");
#ifdef NETCHESSZX_NEXT
    (void)at_cmd_med("AT+CIPRECVMODE=0");
#endif
    (void)at_cmd_med(spectrum_net_at_cipmux_0);
    (void)at_cmd_med(spectrum_net_at_cipmode_0);
    /* CIFSR below is the definitive Wi-Fi gate.  Do not turn a transient
       missed setup response into a false connection failure here. */
    return 1u;
}

#define spectrum_net_query_ip_with_retry esp_at_query_ip_with_retry
#define spectrum_net_prepare_radio esp_at_prepare_radio

static uint16_t mqtt_alloc_id_ovl(void)
{
    uint16_t id = mqtt_next_id++;

    if (mqtt_next_id == 0u) {
        mqtt_next_id = 1u;
    }
    return id;
}

static void mqtt_topic_ovl(char *out, const char *suffix) NETCHESSZX_CALLEE
{
    char *p;

    p = spectrum_append_text(out, mqtt_will_topic_prefix);
    p = spectrum_append_text(p, netchesszx_mqtt_code);
    p = spectrum_append_text(p, "/");
    (void)spectrum_append_text(p, suffix);
}

static uint8_t mqtt_subscribe_suffix_ovl(const char *suffix) __z88dk_fastcall
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint8_t len;
    uint16_t id;

    mqtt_topic_ovl(topic, suffix);
    id = mqtt_alloc_id_ovl();
    len = spectrum_mqtt_subscribe(mqtt_packet_ovl,
                                  SPECTRUM_MQTT_PACKET_MAX,
                                  id,
                                  topic);
    if (len == 0u || !mqtt_send_packet_ovl(len) ||
        !mqtt_wait_packet_ovl(SPECTRUM_MQTT_SUBACK)) {
        return 0u;
    }
    return (uint8_t)(mqtt_packet_ovl[1u] == 3u &&
                     mqtt_packet_ovl[2u] == (uint8_t)(id >> 8) &&
                     mqtt_packet_ovl[3u] == (uint8_t)id &&
                     mqtt_packet_ovl[4u] <= 2u);
}

static uint8_t mqtt_publish_suffix_ovl(const char *suffix,
                                       const char *payload,
                                       uint8_t retain) NETCHESSZX_CALLEE
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint8_t len;
    uint16_t id;

    mqtt_topic_ovl(topic, suffix);
    id = mqtt_alloc_id_ovl();
    len = spectrum_mqtt_publish(mqtt_packet_ovl,
                                SPECTRUM_MQTT_PACKET_MAX,
                                id,
                                topic,
                                payload,
                                retain);
    if (len == 0u || !mqtt_send_packet_ovl(len)) {
        return 0u;
    }
    return 1u;
}

static uint8_t mqtt_subscribe_all_ovl(void)
{
    /* Own presence too: its retained snapshot reveals a seat already held
       by another client (MQTT_SEAT_TAKEN). */
    return mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_in_ack_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_peer_presence_suffix()) &&
           mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_presence_suffix());
}

static uint8_t mqtt_publish_setup_ovl(void)
{
    char setup[32];

    spectrum_net_mqtt_setup_payload(setup);
    return MQTT_CONNECT_PUBLISH_META(
        mqtt_meta_suffix, setup, netchesszx_session_is_host() ? 1u : 0u);
}

static uint8_t mqtt_activate_side_internal(void)
{
    return mqtt_subscribe_all_ovl() &&
           MQTT_CONNECT_PUBLISH_PRESENCE(
               spectrum_net_mqtt_presence_suffix(),
               spectrum_net_mqtt_presence_payload(), 1u);
}

static uint8_t at_cmd_fast(const char *cmd) __z88dk_fastcall
{
    return spectrum_net_at_cmd(cmd, WAIT_FAST);
}

static const char preflight_esp_fail[] = PREFLIGHT_ROW_ESP "ESP FAIL";
static const char preflight_wifi_fail[] = PREFLIGHT_ROW_WIFI "WIFI FAIL";
static const char preflight_ip_fail[] = PREFLIGHT_ROW_IP "NO IP  ";

static uint8_t preflight_quiet_ovl;

static void preflight_line_ovl(const char *line) __z88dk_fastcall
{
    /* Quiet (warm return to Setup): the info panel still shows the session
       layout, so writing cold-boot rows into it would corrupt the display. */
    if (preflight_quiet_ovl) {
        return;
    }
    spectrum_info_line(line);
    spectrum_frame_wait();
    spectrum_gui_tick();
}

static uint8_t preflight_fail_ovl(const char *line, uint8_t retry) NETCHESSZX_CALLEE
{
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_RETRY] = retry;
    preflight_line_ovl(line);
    return 1u;
}

#ifdef NETCHESSZX_NEXT
#define preflight_command_mode_ovl spectrum_next_preflight_command_mode
#else
#define preflight_command_mode_ovl spectrum_net_ensure_command_mode
#endif

uint8_t net_preflight_ovl(void)
{
    preflight_quiet_ovl = spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_QUIET];
    last_ip[0] = '\0';
    /* The sole reason a warm return runs this at all: replenish the one-shot
       ESP hard-reset budget (Next only) that a wedged link may already have
       spent, and force a fresh CIFSR into resident last_ip. */
    mqtt_recovery_begin_ovl();
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_OK] = 0u;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_RETRY] = SPECTRUM_OVL_PREFLIGHT_RETRY_DEFAULT;

    if (!preflight_quiet_ovl) {
        spectrum_info_show_preflight();
        preflight_line_ovl(PREFLIGHT_ROW_UART "UART WAIT");
        /* The UART is only down on a cold start; a reconnect never lost it. */
        spectrum_net_start_uart();
        preflight_line_ovl(PREFLIGHT_ROW_UART "UART OK  ");
    }

    preflight_line_ovl(PREFLIGHT_ROW_ESP "ESP WAIT");
    if (!preflight_command_mode_ovl()) {
        return preflight_fail_ovl(
            preflight_esp_fail, SPECTRUM_OVL_PREFLIGHT_RETRY_DEFAULT);
    }
    preflight_line_ovl(PREFLIGHT_ROW_ESP "ESP OK  ");

    if (!preflight_quiet_ovl) {
        preflight_line_ovl(PREFLIGHT_ROW_WIFI "WIFI WAIT");
        /* Despite the label this joins no network: it resets CWMODE and tears
           down socket state. The DIRECT listen/connect and MQTT connect paths
           each repeat that teardown themselves, so a reconnect can skip it. */
        if (!spectrum_net_prepare_radio()) {
            return preflight_fail_ovl(
                preflight_wifi_fail, SPECTRUM_OVL_PREFLIGHT_RETRY_DEFAULT);
        }
        preflight_line_ovl(PREFLIGHT_ROW_WIFI "WIFI OK  ");
    }

    preflight_line_ovl(PREFLIGHT_ROW_IP "IP WAIT");
    if (!spectrum_net_query_ip_with_retry(8u)) {
        return preflight_fail_ovl(
            preflight_ip_fail, SPECTRUM_OVL_PREFLIGHT_RETRY_AGAIN);
    }
    spectrum_gui_set_connected(1u);
    preflight_line_ovl(PREFLIGHT_ROW_IP "IP OK  ");

    spectrum_overlay_context[SPECTRUM_OVL_CTX_PREFLIGHT_OK] = 1u;
    return 1u;
}

static void mqtt_prepare_single_link_ovl(void)
{
    (void)at_cmd_fast(spectrum_net_at_cipserver_0);
    (void)at_cmd_fast(spectrum_net_at_cipclose);
    (void)at_cmd_fast(spectrum_net_at_cipmux_0);
    (void)at_cmd_fast(spectrum_net_at_cipmode_0);
}

static uint8_t mqtt_tcp_connect_single_ovl(const char *host, uint16_t port)
{
    (void)spectrum_append_u16(
        spectrum_append_text(
            spectrum_append_text(spectrum_append_text((char *)mqtt_packet_ovl, "AT+CIPSTART=\"TCP\",\""), host),
            "\","),
        port);
    return spectrum_net_at_cmd((char *)mqtt_packet_ovl, WAIT_LONG);
}

static uint8_t radio_warm_up_ovl(void)
{
    if (!spectrum_net_ensure_command_mode()) {
        return 0u;
    }
    if (!at_cmd_short(at_cwmode)) {
        (void)at_cmd_short(at_cwmode_def);
    }
    (void)at_cmd_fast(at_cwautoconn);
    if (last_ip[0] == '\0' && !spectrum_net_query_ip_with_retry(3u)) {
        return 0u;
    }
    return 1u;
}

static uint8_t mqtt_open_session_ovl(void)
{
    uint8_t len;

    if (!mqtt_tcp_connect_single_ovl(netchesszx_mqtt_host, netchesszx_mqtt_port) ||
        !mqtt_enter_stream_mode()) {
        return 0u;
    }
    len = mqtt_connect_packet_ovl();
    if (len == 0u || !mqtt_send_packet_ovl(len) ||
        !mqtt_wait_packet_ovl(SPECTRUM_MQTT_CONNACK) ||
        mqtt_packet_ovl[1u] != 2u ||
        mqtt_packet_ovl[3u] != 0u) {
        return mqtt_abort_ovl();
    }
    return 1u;
}

uint8_t mqtt_connect_start_ovl(void)
{
    mqtt_recovery_begin_ovl();
    mqtt_stream_len = 0u;
    mqtt_next_id = 1u;

    if (!radio_warm_up_ovl()) {
        return 0u;
    }

    if (!mqtt_open_session_ovl()) {
        spectrum_net_guard_wait(30u);
        if (!spectrum_net_ensure_command_mode()) {
            return 0u;
        }
        mqtt_prepare_single_link_ovl();
        if (!mqtt_open_session_ovl()) {
            return 0u;
        }
    }

    if (!mqtt_subscribe_suffix_ovl(mqtt_meta_suffix)) {
        return mqtt_abort_ovl();
    }
    if (netchesszx_session_is_host() || netchesszx_host_color_ready) {
        if (!mqtt_activate_side_ovl()) {
            return 0u;
        }
    }
    mqtt_recovery_begin_ovl();
    return 1u;
}

uint8_t mqtt_activate_side_ovl(void)
{
    char offline[10];

    if (!mqtt_activate_side_internal()) {
        return mqtt_abort_ovl();
    }
    if (mqtt_publish_setup_ovl()) {
        return 1u;
    }
    /* Offline presence differs from the canonical local payload only by verb. */
    (void)spectrum_append_text(offline, spectrum_net_mqtt_presence_payload());
    offline[0] = NETCHESS_MQTT_SESSION_VERB_OFFLINE;
    (void)MQTT_CONNECT_PUBLISH_PRESENCE(
        spectrum_net_mqtt_presence_suffix(),
        offline, 1u);
    return mqtt_abort_ovl();
}

/* Subscribe to our own presence topic WITHOUT claiming the seat (no O
   publish). Its retained snapshot reveals an occupant already on our side
   (MQTT_SEAT_TAKEN) so a stray guest reports BUSY instead of hanging. */
uint8_t mqtt_probe_seat_ovl(void)
{
    if (mqtt_subscribe_suffix_ovl(spectrum_net_mqtt_presence_suffix())) {
        return 1u;
    }
    return mqtt_abort_ovl();
}

#endif
