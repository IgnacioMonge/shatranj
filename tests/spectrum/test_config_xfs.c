#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_SPECTRANEXT 1
#define NETCHESSZX_TZ 0
#define NETCHESSZX_CONFIG_PACK_APPLY_TEST 1
#define __z88dk_fastcall

#include "spectrum/config/app_config_format.h"

uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;
uint8_t setup_config_record[NETCHESSZX_APP_CONFIG_READ_SIZE];

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static uint8_t mock_dir_present;
static uint8_t mock_write_ok;
static uint8_t mock_close_result;
static uint8_t mock_mkdir_ok = 1u;
static uint8_t mock_commit_ok = 1u;
static uint8_t mock_mkdir_count;
static uint8_t mock_commit_count;
static uint8_t mock_replace_count;
static int16_t mock_replace_result;
static uint16_t mock_replace_len;
static const void *mock_replace_buf;
static char mock_last_replace[64];
static char mock_last_temp[64];
static char mock_last_commit[64];

static void mock_path_copy(char *dst, const char *src)
{
    (void)strncpy(dst, src, 63u);
    dst[63] = '\0';
}

void esx_fopen(const char *path)
{
    (void)path;
    esx_handle = 0u;
}

void esx_fcreate(const char *path)
{
    (void)path;
    esx_handle = 0u;
}

void esx_fread(void)
{
    esx_result = 0u;
}

void esx_fwrite(void)
{
    esx_result = mock_write_ok ? esx_count : 0u;
}

uint8_t esx_fclose(void)
{
    uint8_t was_file = (uint8_t)(esx_handle == 1u);

    esx_handle = 0u;
    return was_file ? mock_close_result : 0u;
}

int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len)
{
    ++mock_replace_count;
    mock_path_copy(mock_last_replace, target);
    mock_path_copy(mock_last_temp, temp);
    mock_replace_buf = buf;
    mock_replace_len = len;
    return mock_replace_result;
}

void esx_opendir(const char *path)
{
    CHECK(strcmp(path, "/CFG") == 0);
    esx_handle = mock_dir_present ? 2u : 0u;
}

void esx_mkdir(const char *path)
{
    CHECK(strcmp(path, "/CFG") == 0);
    ++mock_mkdir_count;
    esx_result = mock_mkdir_ok;
    if (esx_result) {
        mock_dir_present = 1u;
    }
}

void esx_commit(const char *path)
{
    ++mock_commit_count;
    mock_path_copy(mock_last_commit, path);
    esx_result = mock_commit_ok;
}

#include "../../src/spectrum/overlay/config_ovl.c"

static void reset_mock(void)
{
    esx_handle = 0u;
    esx_buf = 0u;
    esx_count = 0u;
    esx_result = 0u;
    mock_dir_present = 0u;
    mock_write_ok = 1u;
    mock_close_result = 0u;
    mock_mkdir_ok = 1u;
    mock_commit_ok = 1u;
    mock_mkdir_count = 0u;
    mock_commit_count = 0u;
    mock_replace_count = 0u;
    mock_replace_result = 0;
    mock_replace_len = 0u;
    mock_replace_buf = NULL;
    mock_last_replace[0] = mock_last_temp[0] = 0;
    mock_last_commit[0] = '\0';
}

static void set_valid_runtime(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_timezone = NETCHESSZX_TZ;
    netchesszx_timezone_last = NETCHESSZX_TZ;
    netchesszx_direct_port = NETCHESSZX_PORT;
    strcpy(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE);
    netchesszx_direct_host[0] = '\0';
}

int main(void)
{
    uint8_t record[NETCHESSZX_APP_CONFIG_READ_SIZE];

    reset_mock();
    netchesszx_timezone = 9;
    netchesszx_timezone_last = -4;
    CHECK(config_defaults_ovl(NULL) == 1u);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_timezone_last == 0);
    set_valid_runtime();
    /* A first save creates /CFG and commits the directory, then replaces
       the record in one transaction. The directory is now the only commit:
       the stored record is never opened for truncation. */
    CHECK(config_save_record(record) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_mkdir_count == 1u);
    CHECK(mock_commit_count == 1u);
    CHECK(strcmp(mock_last_commit, "/CFG") == 0);
    CHECK(mock_replace_count == 1u);
    CHECK(strcmp(mock_last_replace, "/CFG/SHATRANJ.CFG") == 0);
    CHECK(strcmp(mock_last_temp, "/CFG/SHATRANJ.TMP") == 0);
    CHECK(mock_replace_buf == (const void *)record);
    CHECK(mock_replace_len == NETCHESSZX_APP_CONFIG_SIZE);

    /* Overwriting an existing record takes the same single path. */
    CHECK(config_save_record(record) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_replace_count == 2u);
    CHECK(mock_mkdir_count == 1u);
    CHECK(mock_commit_count == 1u);

    /* A failed transaction reports IO and leaves nothing to roll back:
       the stored record was never touched. */
    mock_replace_result = -1;
    CHECK(config_save_record(record) == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_replace_count == 3u);
    CHECK(mock_commit_count == 1u);

    /* Without a usable directory the transaction is never attempted. */
    mock_replace_result = 0;
    mock_mkdir_ok = 0u;
    mock_dir_present = 0u;
    CHECK(config_save_record(record) == SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(mock_replace_count == 3u);

    if (failures != 0) {
        return 1;
    }
    puts("Spectranext XFS config tests ok");
    return 0;
}
