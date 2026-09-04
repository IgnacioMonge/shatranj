#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_CONFIG_PACK_APPLY_TEST 1
#define __z88dk_fastcall

#include "../../src/spectrum/config/app_config_format.h"

uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;
uint8_t setup_config_record[NETCHESSZX_APP_CONFIG_READ_SIZE];
static uint8_t mock_create_ok;
static uint8_t mock_close_result;
static uint8_t mock_write_ok;

typedef struct {
    uint8_t present;
    uint16_t size;
    uint8_t data[NETCHESSZX_APP_CONFIG_READ_SIZE];
} mock_config_file_t;

static mock_config_file_t mock_primary;
static mock_config_file_t mock_alt;
static mock_config_file_t mock_legacy;
static mock_config_file_t *mock_read_file;
static mock_config_file_t *mock_write_file;
static mock_config_file_t *mock_unlink_fail_file;
static uint8_t mock_alt_open_count;

static mock_config_file_t *mock_file_for_path(const char *path)
{
    if (strstr(path, "/CONFIG/") != NULL) {
        return &mock_legacy;
    }
    if (strstr(path, ".CF2") != NULL) {
        return &mock_alt;
    }
    return &mock_primary;
}

void esx_fopen(const char *path)
{
    mock_read_file = mock_file_for_path(path);
    if (mock_read_file == &mock_alt) {
        ++mock_alt_open_count;
    }
    esx_handle = mock_read_file->present;
}

void esx_fcreate(const char *path)
{
    (void)path;
    esx_handle = mock_create_ok;
}

void esx_fcreate_new(const char *path)
{
    mock_write_file = mock_file_for_path(path);
    if (!mock_create_ok || mock_write_file->present) {
        esx_handle = 0u;
        return;
    }
    mock_write_file->present = 1u;
    mock_write_file->size = 0u;
    esx_handle = 1u;
}

void esx_funlink(const char *path)
{
    mock_config_file_t *file = mock_file_for_path(path);

    if (file == mock_unlink_fail_file) {
        mock_unlink_fail_file = NULL;
        esx_result = 0u;
        return;
    }
    esx_result = file->present;
    file->present = 0u;
    file->size = 0u;
}

void esx_fread(void)
{
    uint16_t count;

    if (esx_handle == 0u || mock_read_file == NULL) {
        esx_result = 0u;
        return;
    }
    count = mock_read_file->size;
    if (count > esx_count) {
        count = esx_count;
    }
    memcpy(setup_config_record, mock_read_file->data, count);
    esx_result = count;
}

void esx_fwrite(void)
{
    esx_result = mock_write_ok ? esx_count : 0u;
    if (mock_write_file != NULL && mock_write_ok) {
        mock_write_file->size = esx_count;
        memcpy(mock_write_file->data, setup_config_record, esx_count);
    }
}

uint8_t esx_fclose(void)
{
    return mock_close_result;
}

#include "../../src/spectrum/overlay/config_ovl.c"

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void mock_config_files_reset(void)
{
    memset(&mock_primary, 0, sizeof(mock_primary));
    memset(&mock_alt, 0, sizeof(mock_alt));
    memset(&mock_legacy, 0, sizeof(mock_legacy));
    mock_read_file = NULL;
    mock_write_file = NULL;
    mock_unlink_fail_file = NULL;
    mock_alt_open_count = 0u;
    mock_close_result = 0u;
}

static void mock_config_file_set(mock_config_file_t *file,
                                 const uint8_t *record)
{
    file->present = 1u;
    file->size = NETCHESSZX_APP_CONFIG_SIZE;
    memcpy(file->data, record, NETCHESSZX_APP_CONFIG_SIZE);
}

static void config_runtime_sentinel(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_notation = NETCHESSZX_NOTATION_SAN;
    netchesszx_movement_hints = 1u;
    netchesszx_piece_set_index = 2u;
    netchesszx_board_theme_index = 4u;
    netchesszx_timezone = 7;
    netchesszx_timezone_last = 7;
    netchesszx_direct_port = 1234u;
    strcpy(netchesszx_mqtt_code, "NC1234");
    strcpy(netchesszx_direct_host, "10.0.0.1");
}

static void check_game_setup_unchanged(void)
{
    CHECK(netchesszx_host_color == NETCHESSZX_COLOR_BLACK);
    CHECK(netchesszx_notation == NETCHESSZX_NOTATION_SAN);
    CHECK(netchesszx_movement_hints == 1u);
    CHECK(netchesszx_piece_set_index == 2u);
    CHECK(netchesszx_board_theme_index == 4u);
}

static void test_config_load_paths(const uint8_t *valid_record)
{
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};

    mock_config_files_reset();
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 0u);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_HOST);
    CHECK(netchesszx_transport == NETCHESSZX_TRANSPORT_MQTT);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_direct_port == NETCHESSZX_PORT);
    check_game_setup_unchanged();

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 4u);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_HOST);
    check_game_setup_unchanged();

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.size = NETCHESSZX_APP_CONFIG_SIZE - 1u;
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_INVALID);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    check_game_setup_unchanged();

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.size = NETCHESSZX_APP_CONFIG_READ_SIZE;
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_INVALID);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    check_game_setup_unchanged();

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    mock_config_file_set(&mock_alt, valid_record);
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_SAVED);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_alt_open_count == 1u);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN);
    CHECK(netchesszx_transport == NETCHESSZX_TRANSPORT_DIRECT);
    CHECK(netchesszx_timezone == -11);
    CHECK(netchesszx_timezone_last == -11);
    CHECK(netchesszx_direct_port == 65535u);
    CHECK(strcmp(netchesszx_direct_host, "255.255.255.255") == 0);
    check_game_setup_unchanged();

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_SAVED);
    CHECK(mock_alt_open_count == 0u);
    check_game_setup_unchanged();
}

int main(void)
{
    uint8_t record[NETCHESSZX_APP_CONFIG_READ_SIZE];
    uint8_t valid_record[NETCHESSZX_APP_CONFIG_READ_SIZE];

    strcpy(netchesszx_mqtt_code, "OTHER");
    CHECK(config_defaults_ovl(NULL) == 1u);
    CHECK(strcmp(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE) == 0);

    config_runtime_sentinel();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_timezone = -11;
    netchesszx_timezone_last = -11;
    netchesszx_direct_port = 65535u;
    strcpy(netchesszx_mqtt_code, "NC1234");
    strcpy(netchesszx_direct_host, "255.255.255.255");

    config_pack(record);
    CHECK(netchesszx_app_config_validate(record));
    memcpy(valid_record, record, sizeof(valid_record));
    CHECK(record[NETCHESSZX_APP_CONFIG_SIZE] == 0u);
    CHECK((record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] &
           NETCHESSZX_APP_CONFIG_FLAG_ROLE) != 0u);
    CHECK((record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] &
           NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT) == 0u);
    CHECK((int8_t)record[NETCHESSZX_APP_CONFIG_TZ_OFF] == -11);

    config_runtime_sentinel();
    config_apply(record);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN);
    CHECK(netchesszx_transport == NETCHESSZX_TRANSPORT_DIRECT);
    CHECK(netchesszx_timezone == -11);
    CHECK(netchesszx_timezone_last == -11);
    CHECK(netchesszx_direct_port == 65535u);
    CHECK(strcmp(netchesszx_mqtt_code, "NC1234") == 0);
    CHECK(strcmp(netchesszx_direct_host, "255.255.255.255") == 0);
    check_game_setup_unchanged();

    test_config_load_paths(valid_record);

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    CHECK(config_save_record(setup_config_record) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_primary.present && !mock_alt.present && !mock_legacy.present);

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_config_file_set(&mock_legacy, valid_record);
    CHECK(config_save_record(setup_config_record) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_primary.present && mock_legacy.present);

    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = NETCHESSZX_TIME_RTC;
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] = 3u;
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] =
        netchesszx_app_config_crc8(record);
    CHECK(netchesszx_app_config_validate(record));
    config_apply(record);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_timezone_last == 3);
    check_game_setup_unchanged();

    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_close_result = 0xffu;
    mock_config_file_set(&mock_primary, valid_record);
    CHECK(config_save_record(setup_config_record) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);
    mock_close_result = 0u;
    mock_write_ok = 0u;
    CHECK(config_save_record(setup_config_record) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);
    mock_write_ok = 1u;
    CHECK(config_save_record(setup_config_record) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(!mock_primary.present && mock_alt.present);
    mock_create_ok = 0u;
    CHECK(config_save_record(setup_config_record) ==
          SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(mock_alt.present);

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 0u;
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    mock_config_file_set(&mock_alt, valid_record);
    CHECK(config_save_record(setup_config_record) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(!mock_primary.present && mock_alt.present);
    CHECK(netchesszx_app_config_validate(mock_alt.data));

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_config_file_set(&mock_primary, valid_record);
    mock_unlink_fail_file = &mock_primary;
    CHECK(config_save_record(setup_config_record) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);

    if (failures != 0) {
        return 1;
    }
    puts("connection config overlay tests ok");
    return 0;
}
