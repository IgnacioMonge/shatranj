#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/net.h"
#include "spectrum/lowram_map.h"
#include "common/protocol/game_protocol.h"

extern uint16_t mqtt_next_id;
extern char line_buf[];
extern uint8_t read_line(uint16_t frames) __z88dk_fastcall;
extern void reset_line_buf(void);
extern void net_wait_frame(void);
extern uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len);

#ifdef NETCHESSZX_HOST_SESSION_TEST
extern const char *netchesszx_host_mqtt_context_text(void);
extern void netchesszx_host_mqtt_observe_control(void);
extern void netchesszx_host_mqtt_observe_meta(void);
extern void netchesszx_host_mqtt_observe_presence(const char *suffix);
extern void netchesszx_host_mqtt_observe_ack(void);
extern void netchesszx_host_mqtt_observe_game(void);
#define MQTT_TX_CONTEXT_TEXT(ctx, lo, hi) \
    ((void)(ctx), netchesszx_host_mqtt_context_text())
#define MQTT_TX_PUBLISH_CONTROL(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_control(), \
     mqtt_tx_publish_suffix((suffix), (payload), (retain)))
#define MQTT_TX_PUBLISH_META(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_meta(), \
     mqtt_tx_publish_suffix((suffix), (payload), (retain)))
#define MQTT_TX_PUBLISH_PRESENCE(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_presence(suffix), \
     mqtt_tx_publish_suffix((suffix), (payload), (retain)))
#define MQTT_TX_PUBLISH_ACK(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_ack(), \
     mqtt_tx_publish_suffix((suffix), (payload), (retain)))
#define MQTT_TX_PUBLISH_GAME(suffix, payload, retain) \
    (netchesszx_host_mqtt_observe_game(), \
     mqtt_tx_publish_suffix((suffix), (payload), (retain)))
#else
#define MQTT_TX_CONTEXT_TEXT(ctx, lo, hi) \
    ((const char *)((uint16_t)(ctx)[(lo)] | ((uint16_t)(ctx)[(hi)] << 8)))
#define MQTT_TX_PUBLISH_CONTROL(suffix, payload, retain) \
    mqtt_tx_publish_suffix((suffix), (payload), (retain))
#define MQTT_TX_PUBLISH_META(suffix, payload, retain) \
    mqtt_tx_publish_suffix((suffix), (payload), (retain))
#define MQTT_TX_PUBLISH_PRESENCE(suffix, payload, retain) \
    mqtt_tx_publish_suffix((suffix), (payload), (retain))
#define MQTT_TX_PUBLISH_ACK(suffix, payload, retain) \
    mqtt_tx_publish_suffix((suffix), (payload), (retain))
#define MQTT_TX_PUBLISH_GAME(suffix, payload, retain) \
    mqtt_tx_publish_suffix((suffix), (payload), (retain))
#endif

#define WAIT_SHORT 150

static const char mqtt_meta[] = "meta";

#ifdef NETCHESSZX_HOST_SESSION_TEST
#define MQTT_TX_PACKET SPECTRUM_MQTT_PACKET_SCRATCH
#else
#define MQTT_TX_PACKET ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif

static uint16_t mqtt_tx_alloc_id(void)
{
    uint16_t id = mqtt_next_id++;

    if (mqtt_next_id == 0u) {
        mqtt_next_id = 1u;
    }
    return id;
}

static void mqtt_tx_topic(char *out, const char *suffix)
{
    char *p;

    p = spectrum_append_text(out, "netchesszx/v1/");
    p = spectrum_append_text(p, netchesszx_mqtt_code);
    p = spectrum_append_text(p, "/");
    (void)spectrum_append_text(p, suffix);
}

static uint8_t mqtt_tx_publish_suffix(const char *suffix,
                                      const char *payload,
                                      uint8_t retain)
{
    char topic[SPECTRUM_MQTT_TOPIC_MAX + 1u];
    uint8_t len;

    mqtt_tx_topic(topic, suffix);
    len = spectrum_mqtt_publish(MQTT_TX_PACKET,
                                SPECTRUM_MQTT_PACKET_MAX,
                                mqtt_tx_alloc_id(),
                                topic,
                                payload,
                                retain);
    if (len == 0u) {
        return 0u;
    }
    return mqtt_send_raw_packet(MQTT_TX_PACKET, len);
}

#ifdef NETCHESSZX_SPECTRANEXT

uint8_t mqtt_tx_sync_time_ovl(void)
{
    return 0u;
}

uint8_t mqtt_tx_clock_retry_start_ovl(void) { return 0u; }
uint8_t mqtt_tx_clock_retry_poll_ovl(void) { return 0u; }

#else

static const char mqtt_sntp_cfg[] = "AT+CIPSNTPCFG=1,";
static const char mqtt_sntp_query[] = "AT+CIPSNTPTIME?";

static char *mqtt_format_sntp_cfg_ovl(int8_t timezone) __z88dk_fastcall
{
    char *p = (char *)MQTT_TX_PACKET;

    if (timezone == NETCHESSZX_TIME_RTC ||
        timezone < NETCHESSZX_TIMEZONE_MIN ||
        timezone > NETCHESSZX_TIMEZONE_MAX) {
        timezone = 0;
    }
    p = spectrum_append_text(p, mqtt_sntp_cfg);
    if (timezone < 0) {
        *p++ = '-';
        timezone = (int8_t)-timezone;
    }
    return spectrum_append_u16(p, (uint8_t)timezone);
}

static uint8_t mqtt_parse_2digits_ovl(const char *p) __z88dk_fastcall
{
    uint8_t tens = (uint8_t)(p[0] - '0');

    return (uint8_t)((tens << 3) + (tens << 1) + (uint8_t)(p[1] - '0'));
}

static uint8_t mqtt_month_ovl(const char *p) __z88dk_fastcall
{
    switch (p[0]) {
    case 'A':
        return (uint8_t)(p[1] == 'p' ? 4u : 8u);
    case 'D':
        return 12u;
    case 'F':
        return 2u;
    case 'J':
        if (p[1] == 'a') {
            return 1u;
        }
        return (uint8_t)(p[2] == 'n' ? 6u : 7u);
    case 'M':
        return (uint8_t)(p[2] == 'r' ? 3u : 5u);
    case 'N':
        return 11u;
    case 'O':
        return 10u;
    case 'S':
        return 9u;
    default:
        return 0u;
    }
}

static void mqtt_set_fat_stamp_ovl(const char *base, const char *p,
                                   uint8_t hour, uint8_t minute)
{
    uint8_t month;
    uint8_t day;
    uint8_t year;
    union {
        uint16_t word;
        uint8_t byte[2];
    } date, time;

    if (p < base + 11u || p[-1] != ' ' || p[-4] != ' ' ||
        p[8] != ' ' || p[9] != '2' || p[10] != '0' ||
        p[11] < '0' || p[11] > '9' ||
        p[12] < '0' || p[12] > '9') {
        return;
    }
    month = mqtt_month_ovl(p - 7u);
    if (month == 0u || p[-2] < '0' || p[-2] > '9') {
        return;
    }
    day = (uint8_t)(p[-2] - '0');
    if (p[-3] >= '0' && p[-3] <= '9') {
        day = (uint8_t)(((uint8_t)(p[-3] - '0') * 10u) + day);
    } else if (p[-3] != ' ') {
        return;
    }
    year = mqtt_parse_2digits_ovl(p + 11u);
    if (year < 20u || year > 51u || day == 0u || day > 31u) {
        return;
    }
    date.byte[0] = (uint8_t)((month << 5) | day);
    date.byte[1] = (uint8_t)(((year + 20u) << 1) | (month >> 3));
    time.byte[0] = (uint8_t)(minute << 5);
    time.byte[1] = (uint8_t)((hour << 3) | (minute >> 3));
    spectrum_net_runtime_set_fat_stamp(date.word, time.word);
}
static uint8_t mqtt_time_payload_ovl(const char *p) __z88dk_fastcall
{
    const char *base = p;

    while (p[0] != '\0') {
        if (p[0] >= '0' && p[0] <= '2' &&
            p[1] >= '0' && p[1] <= '9' &&
            p[2] == ':' &&
            p[3] >= '0' && p[3] <= '5' &&
            p[4] >= '0' && p[4] <= '9' &&
            p[5] == ':' &&
            p[6] >= '0' && p[6] <= '5' &&
            p[7] >= '0' && p[7] <= '9') {
            uint8_t hour;
            uint8_t minute;
            uint8_t second;

            if (netchess_after_prefix(p + 8u, " 1970")) {
                return 0u;
            }
            hour = mqtt_parse_2digits_ovl(p);
            minute = mqtt_parse_2digits_ovl(p + 3);
            second = mqtt_parse_2digits_ovl(p + 6);
            if (hour < 24u) {
                spectrum_net_runtime_set_clock(hour, minute, second);
                mqtt_set_fat_stamp_ovl(base, p, hour, minute);
                return 1u;
            }
            return 0u;
        }
        ++p;
    }
    return 0u;
}

static uint8_t mqtt_capture_time_ovl(void)
{
    /* ATE0 lines start with the response token; mqtt_time_payload_ovl still
       scans its payload, so only leading garbage before the token is lost. */
    const char *p = netchess_after_prefix(line_buf, "+CIPSNTPTIME:");

    if (p == 0) {
        return 0u;
    }
    return mqtt_time_payload_ovl(p);
}

static uint8_t mqtt_send_at_ovl(const char *cmd) __z88dk_fastcall
{
    reset_line_buf();
    return (uint8_t)(spectrum_uart_send_string(cmd) &&
                     spectrum_uart_send_crlf());
}

#define CLOCK_RETRY_IDLE 0u
#define CLOCK_RETRY_CONFIG 1u
#define CLOCK_RETRY_DELAY 2u
#define CLOCK_RETRY_QUERY 3u
#define CLOCK_RETRY_CONFIG_WAIT 150u
#define CLOCK_RETRY_QUERY_WAIT 150u
#define CLOCK_RETRY_SETTLE_WAIT 100u
#define CLOCK_RETRY_REPEAT_WAIT 50u
#define CLOCK_RETRY_QUERY_COUNT 3u

typedef struct mqtt_clock_retry_state {
    uint16_t ticks;
    uint8_t phase;
    uint8_t queries;
} mqtt_clock_retry_state_t;

static mqtt_clock_retry_state_t *mqtt_clock_retry_state_ovl(void)
{
#ifdef NETCHESSZX_HOST_TEST
    extern char *spectrum_net_payload_scratch(void);
    return (mqtt_clock_retry_state_t *)spectrum_net_payload_scratch();
#else
    uint16_t addr = (uint16_t)spectrum_overlay_context[0] |
                    ((uint16_t)spectrum_overlay_context[1] << 8);

    return (mqtt_clock_retry_state_t *)addr;
#endif
}

static uint8_t mqtt_clock_retry_query_ovl(
    mqtt_clock_retry_state_t *state) __z88dk_fastcall
{
    if (!mqtt_send_at_ovl(mqtt_sntp_query)) {
        state->phase = CLOCK_RETRY_IDLE;
        return 0u;
    }
    ++state->queries;
    state->ticks = CLOCK_RETRY_QUERY_WAIT;
    state->phase = CLOCK_RETRY_QUERY;
    return 1u;
}

uint8_t mqtt_tx_clock_retry_start_ovl(void)
{
    mqtt_clock_retry_state_t *state = mqtt_clock_retry_state_ovl();

    state->phase = CLOCK_RETRY_IDLE;
    (void)mqtt_format_sntp_cfg_ovl(netchesszx_timezone);
    if (!mqtt_send_at_ovl((const char *)MQTT_TX_PACKET)) {
        return 0u;
    }
    state->ticks = CLOCK_RETRY_CONFIG_WAIT;
    state->queries = 0u;
    state->phase = CLOCK_RETRY_CONFIG;
    return 1u;
}

uint8_t mqtt_tx_clock_retry_poll_ovl(void)
{
    mqtt_clock_retry_state_t *state = mqtt_clock_retry_state_ovl();

    if (state->phase == CLOCK_RETRY_IDLE) {
        return 0u;
    }
    if (state->phase == CLOCK_RETRY_DELAY) {
        if (--state->ticks != 0u) {
            return 1u;
        }
        return mqtt_clock_retry_query_ovl(state);
    }
    if (read_line(0u)) {
        if (state->phase == CLOCK_RETRY_CONFIG) {
            if (netchess_after_prefix(line_buf, "OK") != 0) {
                state->ticks = CLOCK_RETRY_SETTLE_WAIT;
                state->phase = CLOCK_RETRY_DELAY;
                return 1u;
            }
        } else if (mqtt_capture_time_ovl()) {
            state->phase = CLOCK_RETRY_IDLE;
            return 0u;
        }
    }
    if (--state->ticks != 0u) {
        return 1u;
    }
    if (state->phase == CLOCK_RETRY_CONFIG ||
        state->queries == CLOCK_RETRY_QUERY_COUNT) {
        state->phase = CLOCK_RETRY_IDLE;
        return 0u;
    }
    state->ticks = CLOCK_RETRY_REPEAT_WAIT;
    state->phase = CLOCK_RETRY_DELAY;
    return 1u;
}

static uint8_t mqtt_wait_time_ovl(uint16_t frames) __z88dk_fastcall
{
    while (frames-- != 0u) {
        if (read_line(1u) && mqtt_capture_time_ovl()) {
            return 1u;
        }
        net_wait_frame();
    }
    return spectrum_net_runtime_clock_ready();
}

#ifdef NETCHESSZX_NEXT
#include "spectrum/transport/msdos_time.h"

/* NextZXOS RTC via RST 8: M_DRVAPI 0x92 first, M_GETDATE 0x8E as backup.
   reset_line_buf() zeroes line_buf[0..5]; a failed call leaves date 0, which
   msdos_time_valid rejects — no success flag needed. Volatile reads because
   the asm stores behind the compiler's back. */
static uint8_t next_rtc_sync_time(uint8_t apply) __z88dk_fastcall
{
    reset_line_buf();
#asm
    push hl
    push ix
    push iy
    ld iy, 0x5C3A
    xor a
    ld b, a
    ld c, a
    ld d, a
    ld e, a
    ld h, a
    ld l, a
#ifdef NETCHESSZX_NEXT_BANKING
    defb 0xed, 0x91, 0x51, 0xff
#endif
    rst 8
    defb 0x92
#ifdef NETCHESSZX_NEXT_BANKING
    defb 0xed, 0x91, 0x51, 0x20
#endif
    jr nc, next_rtc_store
    ld iy, 0x5C3A
#ifdef NETCHESSZX_NEXT_BANKING
    defb 0xed, 0x91, 0x51, 0xff
#endif
    rst 8
    defb 0x8E
#ifdef NETCHESSZX_NEXT_BANKING
    defb 0xed, 0x91, 0x51, 0x20
#endif
    jr c, next_rtc_done
next_rtc_store:
    ld (_line_buf), bc
    ld (_line_buf + 2), de
next_rtc_done:
    pop iy
    pop ix
    pop hl
#endasm
    /* Z80 little-endian: BC/DE landed in line_buf as LSB-first words. */
    return msdos_time_valid(*(volatile uint16_t *)&line_buf[0],
                            *(volatile uint16_t *)&line_buf[2], apply);
}
#endif

uint8_t mqtt_tx_sync_time_ovl(void)
{
    uint8_t i;
    int8_t timezone = netchesszx_timezone;
    char *p;
    char *numeric_end;

#ifdef NETCHESSZX_NEXT
    {
        uint8_t rtc_selected = (uint8_t)(timezone == NETCHESSZX_TIME_RTC);

        if (next_rtc_sync_time(rtc_selected)) {
            netchesszx_rtc_available = 1u;
            if (rtc_selected) {
                return 1u;
            }
        } else {
            netchesszx_rtc_available = 0u;
        }
        if (rtc_selected) {
            timezone = netchesszx_timezone_last;
            netchesszx_timezone = timezone;
        }
    }
#endif
    numeric_end = mqtt_format_sntp_cfg_ovl(timezone);
    p = numeric_end;
    p = spectrum_append_text(p, ",\"pool.ntp.org\",\"time.google.com\"");
    (void)p;
    if (!spectrum_net_at_cmd((const char *)MQTT_TX_PACKET, WAIT_SHORT)) {
        *numeric_end = '\0';
        (void)spectrum_net_at_cmd((const char *)MQTT_TX_PACKET, WAIT_SHORT);
    }

    spectrum_net_guard_wait(100u);
    for (i = 0u; i < 3u; ++i) {
        if (!mqtt_send_at_ovl(mqtt_sntp_query)) {
            return 0u;
        }
        if (mqtt_wait_time_ovl(WAIT_SHORT)) {
            return 1u;
        }
        spectrum_net_guard_wait(50u);
    }
    return 0u;
}
#endif

uint8_t mqtt_tx_send_text_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const char *text = MQTT_TX_CONTEXT_TEXT(ctx, 0u, 1u);

    if (netchess_after_prefix(text + (uint8_t)(text[0] == 'N'),
                              "ACK GAME START")) {
        return MQTT_TX_PUBLISH_CONTROL(mqtt_meta, text, 0u);
    }
    if (netchess_after_prefix(text + (uint8_t)(text[0] == 'N'), "ACK ")) {
        return MQTT_TX_PUBLISH_ACK(spectrum_net_mqtt_out_ack_suffix(),
                                   text,
                                   0u);
    }
    if (netchess_after_prefix(text, netchesszx_text_game_start)) {
        return MQTT_TX_PUBLISH_CONTROL(mqtt_meta, text, 0u);
    }
    return MQTT_TX_PUBLISH_GAME(spectrum_net_mqtt_out_suffix(), text, 0u);
}

uint8_t mqtt_tx_publish_setup_ovl(uint8_t *ctx) __z88dk_fastcall
{
    char setup[32];

    if (ctx[0] == SPECTRUM_LINK_MQTT_SETUP_CLEAR) {
        return MQTT_TX_PUBLISH_META(mqtt_meta, "", 1u);
    }
    spectrum_net_mqtt_setup_payload(setup);
    return MQTT_TX_PUBLISH_META(
        mqtt_meta, setup,
        (uint8_t)(ctx[0] == SPECTRUM_LINK_MQTT_SETUP_RETAINED));
}

uint8_t mqtt_tx_publish_presence_ovl(void)
{
    return MQTT_TX_PUBLISH_PRESENCE(spectrum_net_mqtt_presence_suffix(),
                                    spectrum_net_mqtt_presence_payload(),
                                    1u);
}
