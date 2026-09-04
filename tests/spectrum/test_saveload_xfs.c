#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_SPECTRANEXT 1
#define __z88dk_fastcall

#include "common/savegame/savegame_format.h"

uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static uint8_t mock_dir_present;
static uint8_t mock_short_write;
static uint8_t mock_close_result;
static uint8_t mock_mkdir_ok = 1u;
static uint8_t mock_commit_ok = 1u;
static uint8_t mock_unlink_count;
static uint8_t mock_mkdir_count;
static uint8_t mock_commit_count;
static uint8_t mock_replace_count;
static int16_t mock_replace_result;
static uint16_t mock_replace_len;
static const void *mock_replace_buf;
static char mock_last_replace[64];
static char mock_last_temp[64];
static char mock_last_commit[64];
static char mock_last_unlink[64];

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
    esx_result = mock_short_write
                     ? (uint16_t)(esx_count - 1u)
                     : esx_count;
}

uint8_t esx_fclose(void)
{
    uint8_t was_file = (uint8_t)(esx_handle == 1u);

    esx_handle = 0u;
    return was_file ? mock_close_result : 0u;
}

void esx_funlink(const char *path)
{
    ++mock_unlink_count;
    mock_path_copy(mock_last_unlink, path);
    esx_result = 1u;
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

uint16_t spectrum_net_runtime_fat_date(void)
{
    return (uint16_t)((46u << 9) | (8u << 5) | 19u);
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return (uint16_t)((15u << 11) | (26u << 5));
}

#include "../../src/spectrum/overlay/saveload_ovl.c"

static void reset_mock(void)
{
    esx_handle = 0u;
    esx_buf = 0u;
    esx_count = 0u;
    esx_result = 0u;
    mock_dir_present = 0u;
    mock_short_write = 0u;
    mock_close_result = 0u;
    mock_mkdir_ok = 1u;
    mock_commit_ok = 1u;
    mock_unlink_count = 0u;
    mock_mkdir_count = 0u;
    mock_commit_count = 0u;
    mock_replace_count = 0u;
    mock_replace_result = 0;
    mock_replace_len = 0u;
    mock_replace_buf = NULL;
    mock_last_replace[0] = mock_last_temp[0] = 0;
    mock_last_commit[0] = '\0';
    mock_last_unlink[0] = '\0';
}

int main(void)
{
    char payload[NETCHESSZX_SAVE_WIRE_B64_SIZE] = {0};
    uint8_t result;

    reset_mock();
    /* The slot is replaced in one transaction. /CFG is the only commit;
       the existing save is never opened for truncation. */
    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_mkdir_count == 1u);
    CHECK(mock_commit_count == 1u);
    CHECK(strcmp(mock_last_commit, "/CFG") == 0);
    CHECK(mock_replace_count == 1u);
    CHECK(strcmp(mock_last_replace, "/CFG/0168JF26.STJ") == 0);
    CHECK(strcmp(mock_last_temp, "/CFG/SHATSAVE.TMP") == 0);
    CHECK(mock_replace_buf == (const void *)payload);
    CHECK(mock_replace_len == NETCHESSZX_SAVE_WIRE_B64_SIZE);
    CHECK(mock_unlink_count == 0u);

    /* Overwriting an existing slot takes the same single path. */
    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_replace_count == 2u);
    CHECK(mock_commit_count == 1u);

    /* A failed transaction reports IO and unlinks nothing: the previous
       save is still the one on the slot. */
    mock_replace_result = -1;
    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_replace_count == 3u);
    CHECK(mock_unlink_count == 0u);

    mock_replace_result = 0;
    CHECK(netchesszx_saveload_test_save("bad-name", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_NAME);
    CHECK(mock_replace_count == 3u);

    if (failures != 0) {
        return 1;
    }
    puts("Spectranext XFS saveload tests ok");
    return 0;
}
