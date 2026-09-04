#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "spectrum/config/app_config_format.h"

static void make_valid(uint8_t *record)
{
    memset(record, 0, NETCHESSZX_APP_CONFIG_READ_SIZE);
    record[NETCHESSZX_APP_CONFIG_MAGIC_0] = 'S';
    record[NETCHESSZX_APP_CONFIG_MAGIC_1] = 'H';
    record[NETCHESSZX_APP_CONFIG_MAGIC_2] = 'C';
    record[NETCHESSZX_APP_CONFIG_MAGIC_3] = 'F';
    record[NETCHESSZX_APP_CONFIG_VERSION_OFF] = NETCHESSZX_APP_CONFIG_VERSION;
    record[NETCHESSZX_APP_CONFIG_LENGTH_OFF] = NETCHESSZX_APP_CONFIG_SIZE;
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] =
        NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT;
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = 2u;
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] = 2u;
    record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] = 0x88u;
    record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] = 0x13u;
    memcpy(record + NETCHESSZX_APP_CONFIG_ROOM_OFF, "NC12AF", 7u);
    memcpy(record + NETCHESSZX_APP_CONFIG_HOST_OFF, "192.168.1.2", 12u);
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] =
        netchesszx_app_config_crc8(record);
}

static void refresh_crc(uint8_t *record)
{
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] =
        netchesszx_app_config_crc8(record);
}

int main(void)
{
    uint8_t record[NETCHESSZX_APP_CONFIG_READ_SIZE];

    make_valid(record);
    assert(netchesszx_app_config_validate(record));

    record[0] ^= 1u;
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_VERSION_OFF]++;
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_LENGTH_OFF]--;
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    assert(!netchesszx_app_config_validate(record));

    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] |= 0x80u;
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));

    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = (uint8_t)NETCHESSZX_TIME_RTC;
    refresh_crc(record);
    assert(netchesszx_app_config_validate(record));
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] =
        (uint8_t)NETCHESSZX_TIME_RTC;
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] =
        (uint8_t)(NETCHESSZX_TIMEZONE_MIN - 1);
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] =
        (uint8_t)(NETCHESSZX_TIMEZONE_MAX + 1);
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));

    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] = 0u;
    record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] = 0u;
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    memset(record + NETCHESSZX_APP_CONFIG_ROOM_OFF, 'X',
           NETCHESSZX_MQTT_CODE_MAX + 1u);
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_ROOM_OFF] = '-';
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_ROOM_OFF + 2u] = 'G';
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_ROOM_OFF + 2u] = 'a';
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_ROOM_OFF + 6u] = '5';
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));

    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] =
        NETCHESSZX_APP_CONFIG_FLAG_ROLE;
    memcpy(record + NETCHESSZX_APP_CONFIG_HOST_OFF,
           "255.255.255.255", 16u);
    refresh_crc(record);
    assert(netchesszx_app_config_validate(record));
    memcpy(record + NETCHESSZX_APP_CONFIG_HOST_OFF, "256.1.1.1", 10u);
    memset(record + NETCHESSZX_APP_CONFIG_HOST_OFF + 10u, 0, 6u);
    refresh_crc(record);
    assert(!netchesszx_app_config_validate(record));
    make_valid(record);
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] = 0u;
    memset(record + NETCHESSZX_APP_CONFIG_HOST_OFF, 0,
           NETCHESSZX_DIRECT_HOST_MAX + 1u);
    refresh_crc(record);
    assert(netchesszx_app_config_validate(record));

    puts("app connection config format tests ok");
    return 0;
}
