#include "spectrum/overlay/overlay_api.h"
#include "spectrum/transport/net.h"
#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spectrum/lowram_map.h"
#endif
#ifndef NETCHESSZX_HOST_TEST
#include "spxtime.h"
#include "spxudp.h"
#endif

#ifndef NETCHESSZX_TIME_HOST_TOKEN
#define NETCHESSZX_TIME_HOST_TOKEN pool.ntp.org
#endif
#define NETCHESSZX_TIME_STRINGIFY_IMPL(value) #value
#define NETCHESSZX_TIME_STRINGIFY(value) NETCHESSZX_TIME_STRINGIFY_IMPL(value)

#define SPECTRANEXT_TIME_WAIT_SHORT 150u

#ifdef NETCHESSZX_SPECTRANEXT
#ifdef NETCHESSZX_HOST_TEST
static uint8_t time_retry_packet[SPXTIME_SNTP_PACKET_SIZE];
#define TIME_RETRY_PACKET time_retry_packet
#else
#define TIME_RETRY_PACKET \
    ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif

typedef struct spectranext_time_retry_state {
    struct spxudp_socket socket;
    struct spxudp_endpoint destination;
    struct spxudp_endpoint source;
    uint16_t ticks;
    int8_t timezone;
    uint8_t active;
} spectranext_time_retry_state_t;
#endif

static uint8_t time_days_in_month_ovl(uint8_t fat_year, uint8_t month)
{
    static const uint8_t days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };
    uint8_t result = days[month - 1u];

    /* FAT dates cover 1980..2107; 2100 is FAT year 120. */
    if (month == 2u && (fat_year & 3u) == 0u && fat_year != 120u) {
        ++result;
    }
    return result;
}

/* spxtime returns UTC. Convert the displayed clock and FAT stamp to the
   configured civil offset so Spectranext matches the existing ESP-AT path. */
static uint8_t time_apply_timezone_ovl(struct spxtime_result *result,
                                       int8_t timezone)
{
    uint16_t date = result->fat_date;
    uint8_t fat_year = (uint8_t)(date >> 9);
    uint8_t month = (uint8_t)((date >> 5) & 0x0fu);
    uint8_t day = (uint8_t)(date & 0x1fu);
    int8_t hour = (int8_t)(result->hour + timezone);

    /* A successful spxtime parse has already validated the FAT date. */
    if (hour < 0) {
        hour = (int8_t)(hour + 24);
        if (day > 1u) {
            --day;
        } else if (month > 1u) {
            --month;
            day = time_days_in_month_ovl(fat_year, month);
        } else if (fat_year != 0u) {
            --fat_year;
            month = 12u;
            day = 31u;
        } else {
            return 0u;
        }
    } else if (hour >= 24) {
        hour = (int8_t)(hour - 24);
        if (day < time_days_in_month_ovl(fat_year, month)) {
            ++day;
        } else if (month < 12u) {
            ++month;
            day = 1u;
        } else if (fat_year < 127u) {
            ++fat_year;
            month = 1u;
            day = 1u;
        } else {
            return 0u;
        }
    }
    result->hour = (uint8_t)hour;
    result->fat_date = (uint16_t)(((uint16_t)fat_year << 9) |
                                  ((uint16_t)month << 5) | day);
    result->fat_time = (uint16_t)((result->fat_time & 0x07ffu) |
                                  ((uint16_t)result->hour << 11));
    return 1u;
}

static int8_t time_timezone_ovl(void)
{
    int8_t timezone = netchesszx_timezone;

    if (timezone == NETCHESSZX_TIME_RTC) {
        /* Spectranext is Classic 48K and has no product RTC syscall. Preserve
           the configured RTC fallback by using the last numeric zone. */
        netchesszx_rtc_available = 0u;
        timezone = netchesszx_timezone_last;
        netchesszx_timezone = timezone;
    }
    if ((uint8_t)(timezone - NETCHESSZX_TIMEZONE_MIN) >
        (uint8_t)(NETCHESSZX_TIMEZONE_MAX - NETCHESSZX_TIMEZONE_MIN)) {
        timezone = 0;
    }
    return timezone;
}

uint8_t spectranext_time_sync_ovl(void)
{
    static const char ntp_host[] =
        NETCHESSZX_TIME_STRINGIFY(NETCHESSZX_TIME_HOST_TOKEN);
    uint8_t scratch[SPXTIME_SNTP_PACKET_SIZE];
    struct spxtime_result result;
    struct spxtime_request request;
    int8_t timezone = time_timezone_ovl();
#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
    char *ntp_host_arg = (char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR;

    spectrum_append_text(ntp_host_arg, ntp_host);
#else
    const char *ntp_host_arg = ntp_host;
#endif

    request.host = ntp_host_arg;
    request.scratch = scratch;
    request.scratch_size = sizeof(scratch);
    request.timeout_ticks = SPECTRANEXT_TIME_WAIT_SHORT;
    request.out = &result;
    if (spxtime_sntp(&request) != SPXN_OK ||
        !time_apply_timezone_ovl(&result, timezone)) {
        return 0u;
    }
    spectrum_net_runtime_set_clock(result.hour, result.minute, result.second);
    spectrum_net_runtime_set_fat_stamp(result.fat_date, result.fat_time);
    return 1u;
}

#ifdef NETCHESSZX_SPECTRANEXT
static spectranext_time_retry_state_t *time_retry_state_ovl(void)
{
#ifdef NETCHESSZX_HOST_TEST
    extern char *spectrum_net_payload_scratch(void);
    return (spectranext_time_retry_state_t *)spectrum_net_payload_scratch();
#else
    uint16_t addr = (uint16_t)spectrum_overlay_context[0] |
                    ((uint16_t)spectrum_overlay_context[1] << 8);

    return (spectranext_time_retry_state_t *)addr;
#endif
}

static void time_retry_close_ovl(spectranext_time_retry_state_t *state)
{
    spxudp_close(&state->socket);
    state->active = 0u;
}

uint8_t spectranext_time_retry_start_ovl(void)
{
    static const char ntp_host[] =
        NETCHESSZX_TIME_STRINGIFY(NETCHESSZX_TIME_HOST_TOKEN);
    spectranext_time_retry_state_t *state = time_retry_state_ovl();
    uint8_t i;
    int16_t result;

    state->active = 0u;
    spectrum_append_text((char *)TIME_RETRY_PACKET, ntp_host);
    result = spxn_resolve((const char *)TIME_RETRY_PACKET,
                          state->destination.ip4be);
    if (result < 0) {
        return 0u;
    }
    state->destination.port = SPXTIME_SNTP_PORT;
    state->destination.local_port = 0u;
    for (i = 0u; i < SPXTIME_SNTP_PACKET_SIZE; ++i) {
        TIME_RETRY_PACKET[i] = 0u;
    }
    TIME_RETRY_PACKET[0] = 0x23u;
    result = spxudp_open(&state->socket);
    if (result < 0) {
        return 0u;
    }
    result = spxudp_sendto(&state->socket, TIME_RETRY_PACKET,
                           SPXTIME_SNTP_PACKET_SIZE, &state->destination);
    if (result != (int16_t)SPXTIME_SNTP_PACKET_SIZE) {
        time_retry_close_ovl(state);
        return 0u;
    }
    state->ticks = SPECTRANEXT_TIME_WAIT_SHORT;
    state->timezone = time_timezone_ovl();
    state->active = 1u;
    return 1u;
}

uint8_t spectranext_time_retry_poll_ovl(void)
{
    spectranext_time_retry_state_t *state = time_retry_state_ovl();
    struct spxtime_result result;
    int16_t received;
    int16_t polled;

    if (!state->active) {
        return 0u;
    }
    polled = spxudp_poll(&state->socket);
    if (polled < 0 || (polled & SPXN_POLLNVAL)) {
        time_retry_close_ovl(state);
        return 0u;
    }
    if (polled & SPXN_POLLIN) {
        received = spxudp_recvfrom(&state->socket, TIME_RETRY_PACKET,
                                   SPXTIME_SNTP_PACKET_SIZE, &state->source);
        if (received >= 0 && state->source.port == SPXTIME_SNTP_PORT &&
            state->source.ip4be[0] == state->destination.ip4be[0] &&
            state->source.ip4be[1] == state->destination.ip4be[1] &&
            state->source.ip4be[2] == state->destination.ip4be[2] &&
            state->source.ip4be[3] == state->destination.ip4be[3] &&
            spxtime_parse_sntp(TIME_RETRY_PACKET, (uint16_t)received,
                               &result) == SPXN_OK &&
            time_apply_timezone_ovl(&result, state->timezone)) {
            time_retry_close_ovl(state);
            spectrum_net_runtime_set_clock(result.hour, result.minute,
                                            result.second);
            spectrum_net_runtime_set_fat_stamp(result.fat_date,
                                                result.fat_time);
            return 0u;
        }
    }
    if (--state->ticks != 0u) {
        return 1u;
    }
    time_retry_close_ovl(state);
    return 0u;
}

uint8_t spectranext_time_retry_cancel_ovl(void)
{
    spectranext_time_retry_state_t *state = time_retry_state_ovl();

    if (state->active) {
        time_retry_close_ovl(state);
    }
    return 0u;
}
#endif
