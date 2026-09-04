#ifndef NETCHESSZX_SPECTRUM_CONFIG_APP_CONFIG_FORMAT_H
#define NETCHESSZX_SPECTRUM_CONFIG_APP_CONFIG_FORMAT_H

#include <stdint.h>

#include "spectrum/config/session.h"

#define NETCHESSZX_APP_CONFIG_VERSION 1u
#define NETCHESSZX_APP_CONFIG_SIZE 46u
#define NETCHESSZX_APP_CONFIG_READ_SIZE (NETCHESSZX_APP_CONFIG_SIZE + 1u)

#define NETCHESSZX_APP_CONFIG_MAGIC_0 0u
#define NETCHESSZX_APP_CONFIG_MAGIC_1 1u
#define NETCHESSZX_APP_CONFIG_MAGIC_2 2u
#define NETCHESSZX_APP_CONFIG_MAGIC_3 3u
#define NETCHESSZX_APP_CONFIG_VERSION_OFF 4u
#define NETCHESSZX_APP_CONFIG_LENGTH_OFF 5u
#define NETCHESSZX_APP_CONFIG_FLAGS_OFF 6u
/* Everything chosen in GAME setup, packed into one byte. */
#define NETCHESSZX_APP_CONFIG_GAME_OFF 7u
#define NETCHESSZX_APP_CONFIG_TZ_OFF 8u
#define NETCHESSZX_APP_CONFIG_TZ_LAST_OFF 9u
#define NETCHESSZX_APP_CONFIG_PORT_LO_OFF 10u
#define NETCHESSZX_APP_CONFIG_PORT_HI_OFF 11u
#define NETCHESSZX_APP_CONFIG_ROOM_OFF 12u
#define NETCHESSZX_APP_CONFIG_HOST_OFF \
    (NETCHESSZX_APP_CONFIG_ROOM_OFF + NETCHESSZX_MQTT_CODE_MAX + 1u)
#define NETCHESSZX_APP_CONFIG_CRC_OFF (NETCHESSZX_APP_CONFIG_SIZE - 1u)

#define NETCHESSZX_APP_CONFIG_FLAG_ROLE 0x01u
#define NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT 0x02u
#define NETCHESSZX_APP_CONFIG_FLAG_RESERVED_MASK 0xfcu

#define NETCHESSZX_APP_CONFIG_GAME_COLOR 0x01u
#define NETCHESSZX_APP_CONFIG_GAME_NOTATION 0x02u
#define NETCHESSZX_APP_CONFIG_GAME_HINTS 0x04u
#define NETCHESSZX_APP_CONFIG_GAME_THEME_MASK 0x38u
#define NETCHESSZX_APP_CONFIG_GAME_THEME_SHIFT 3u
#define NETCHESSZX_APP_CONFIG_GAME_SET_MASK 0xc0u
#define NETCHESSZX_APP_CONFIG_GAME_SET_SHIFT 6u
/* Mirrors the cycle limits in asm/overlay/setup/entry_setup.asm. */
#define NETCHESSZX_APP_CONFIG_THEME_COUNT 5u
#define NETCHESSZX_APP_CONFIG_SET_COUNT 3u

static uint8_t netchesszx_app_config_crc8(const uint8_t *record)
{
    uint8_t crc = 0u;
    uint8_t i;

    for (i = 0u; i < NETCHESSZX_APP_CONFIG_CRC_OFF; ++i) {
        uint8_t bit;

        crc ^= record[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (uint8_t)((crc & 0x80u) != 0u
                                ? (uint8_t)((crc << 1) ^ 0x07u)
                                : (uint8_t)(crc << 1));
        }
    }
    return crc;
}

static uint8_t netchesszx_app_config_has_nul(const uint8_t *text,
                                              uint8_t size)
{
    while (size-- != 0u) {
        if (*text++ == 0u) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t netchesszx_app_config_room_valid(const uint8_t *text)
{
    uint8_t i;

    if (text[0] != 'N' || text[1] != 'C') {
        return 0u;
    }
    for (i = 2u; i < 6u; ++i) {
        if (!((text[i] >= '0' && text[i] <= '9') ||
              (text[i] >= 'A' && text[i] <= 'F'))) {
            return 0u;
        }
    }
    return (uint8_t)(text[6] == 0u);
}

static uint8_t netchesszx_app_config_ipv4_valid(const uint8_t *text)
{
    uint8_t groups = 0u;

    while (*text != 0u) {
        uint16_t value = 0u;
        uint8_t digits = 0u;

        while (*text >= '0' && *text <= '9') {
            value = (uint16_t)(value * 10u + (uint8_t)(*text++ - '0'));
            if (++digits > 3u || value > 255u) {
                return 0u;
            }
        }
        if (digits == 0u || ++groups > 4u) {
            return 0u;
        }
        if (*text == 0u) {
            break;
        }
        if (*text++ != '.' || *text == 0u) {
            return 0u;
        }
    }
    return (uint8_t)(groups == 4u);
}

static uint8_t netchesszx_app_config_timezone_valid(int8_t timezone)
{
    return (uint8_t)(timezone == NETCHESSZX_TIME_RTC ||
                     (timezone >= NETCHESSZX_TIMEZONE_MIN &&
                      timezone <= NETCHESSZX_TIMEZONE_MAX));
}

static uint8_t netchesszx_app_config_validate(const uint8_t *record)
{
    uint8_t flags;
    uint8_t game;
    uint16_t port;
    int8_t timezone;
    int8_t timezone_last;

    if (record[NETCHESSZX_APP_CONFIG_MAGIC_0] != 'S' ||
        record[NETCHESSZX_APP_CONFIG_MAGIC_1] != 'H' ||
        record[NETCHESSZX_APP_CONFIG_MAGIC_2] != 'C' ||
        record[NETCHESSZX_APP_CONFIG_MAGIC_3] != 'F' ||
        record[NETCHESSZX_APP_CONFIG_VERSION_OFF] !=
            NETCHESSZX_APP_CONFIG_VERSION ||
        record[NETCHESSZX_APP_CONFIG_LENGTH_OFF] !=
            NETCHESSZX_APP_CONFIG_SIZE ||
        record[NETCHESSZX_APP_CONFIG_CRC_OFF] !=
            netchesszx_app_config_crc8(record)) {
        return 0u;
    }
    flags = record[NETCHESSZX_APP_CONFIG_FLAGS_OFF];
    if ((flags & NETCHESSZX_APP_CONFIG_FLAG_RESERVED_MASK) != 0u) {
        return 0u;
    }
    game = record[NETCHESSZX_APP_CONFIG_GAME_OFF];
    if (((game & NETCHESSZX_APP_CONFIG_GAME_THEME_MASK) >>
         NETCHESSZX_APP_CONFIG_GAME_THEME_SHIFT) >=
            NETCHESSZX_APP_CONFIG_THEME_COUNT ||
        ((game & NETCHESSZX_APP_CONFIG_GAME_SET_MASK) >>
         NETCHESSZX_APP_CONFIG_GAME_SET_SHIFT) >=
            NETCHESSZX_APP_CONFIG_SET_COUNT) {
        return 0u;
    }
    timezone = (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_OFF];
    timezone_last = (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF];
    if (!netchesszx_app_config_timezone_valid(timezone) ||
        timezone_last < NETCHESSZX_TIMEZONE_MIN ||
        timezone_last > NETCHESSZX_TIMEZONE_MAX) {
        return 0u;
    }
    port = (uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] |
           ((uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] << 8);
    if (port == 0u ||
        !netchesszx_app_config_has_nul(
            record + NETCHESSZX_APP_CONFIG_ROOM_OFF,
            NETCHESSZX_MQTT_CODE_MAX + 1u) ||
        !netchesszx_app_config_has_nul(
            record + NETCHESSZX_APP_CONFIG_HOST_OFF,
            NETCHESSZX_DIRECT_HOST_MAX + 1u)) {
        return 0u;
    }
    if ((flags & NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT) != 0u) {
        return netchesszx_app_config_room_valid(
            record + NETCHESSZX_APP_CONFIG_ROOM_OFF);
    }
    if ((flags & NETCHESSZX_APP_CONFIG_FLAG_ROLE) != 0u) {
        return netchesszx_app_config_ipv4_valid(
            record + NETCHESSZX_APP_CONFIG_HOST_OFF);
    }
    return 1u;
}

#endif
