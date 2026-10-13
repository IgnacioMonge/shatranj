#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define __z88dk_fastcall

#include "common/savegame/savegame_format.h"

uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;

static uint8_t failures;
static uint8_t file_present;
static uint8_t short_write;
static uint8_t dir_present;
static uint8_t mkdir_count;
static uint8_t close_count;
static uint8_t disk[512];
static uint16_t disk_len;
static uint16_t disk_pos;
static void *esx_xfer;
static char last_path[32];
static int16_t fat_day_delta;
static uint16_t fat_date = (uint16_t)((46u << 9) | (8u << 5) | 19u);
static uint16_t fat_time = (uint16_t)((15u << 11) | (26u << 5));

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void remember(const char *path)
{
    (void)strncpy(last_path, path, sizeof(last_path) - 1u);
    last_path[sizeof(last_path) - 1u] = '\0';
}

void esx_host_prepare(void *p)
{
    esx_xfer = p;
}

void esx_fseek_set(uint16_t off)
{
    disk_pos = off;
}

void esx_fopen(const char *path)
{
    remember(path);
    esx_handle = file_present ? 1u : 0u;
    disk_pos = 0u;
}

void esx_fcreate(const char *path)
{
    remember(path);
    esx_handle = 1u;
    file_present = 1u;
    disk_len = 0u;
    disk_pos = 0u;
}

void esx_fcreate_new(const char *path)
{
    remember(path);
    esx_handle = (uint8_t)(dir_present && !file_present);
    if (esx_handle != 0u) {
        file_present = 1u;
        disk_len = 0u;
        disk_pos = 0u;
    }
}

void esx_mkdir(const char *path)
{
    CHECK(strcmp(path, "/SYS/CONFIG") == 0);
    dir_present = 1u;
    ++mkdir_count;
}

void esx_fread(void)
{
    uint16_t remaining = disk_len > disk_pos ?
        (uint16_t)(disk_len - disk_pos) : 0u;

    esx_result = remaining < esx_count ? remaining : esx_count;
    if (esx_xfer != 0 && esx_result != 0u) {
        memcpy(esx_xfer, disk + disk_pos, esx_result);
    }
    disk_pos = (uint16_t)(disk_pos + esx_result);
}

void esx_fwrite(void)
{
    if (short_write) {
        esx_result = (uint16_t)(esx_count - 1u);
        return;
    }
    if (esx_xfer != 0 && esx_count != 0u &&
        disk_pos + esx_count <= sizeof(disk)) {
        memcpy(disk + disk_pos, esx_xfer, esx_count);
        disk_pos = (uint16_t)(disk_pos + esx_count);
        if (disk_pos > disk_len) {
            disk_len = disk_pos;
        }
    }
    esx_result = esx_count;
}

uint8_t esx_fclose(void)
{
    ++close_count;
    esx_handle = 0u;
    return 0u;
}

void esx_fsync(void)
{
}

void esx_funlink(const char *path)
{
    remember(path);
    file_present = 0u;
    esx_result = 1u;
}

void spectrum_net_background_drain(void) {}

char *spectrum_append_text(char *dst, const char *src)
{
    while ((*dst = *src) != '\0') {
        ++dst;
        ++src;
    }
    return dst;
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return fat_date;
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return fat_time;
}

int16_t spectrum_net_runtime_fat_day_delta(void)
{
    return fat_day_delta;
}

#include "../../src/spectrum/overlay/saveload_ovl.c"

int main(void)
{
    char payload[NETCHESSZX_SAVE_WIRE_B64_SIZE] = {0};
    uint8_t result;

    file_present = 1u;
    disk_len = NETCHESSZX_SAVE_WIRE_B64_SIZE;
    disk_pos = 0u;
    close_count = 0u;
    CHECK(netchesszx_saveload_test_load("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(close_count == 1u);

    disk_len = NETCHESSZX_SAVE_WIRE_B64_SIZE - 1u;
    disk_pos = 0u;
    close_count = 0u;
    CHECK(netchesszx_saveload_test_load("01", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    CHECK(close_count == 1u);

    disk_len = NETCHESSZX_SAVE_WIRE_B64_SIZE + 1u;
    disk_pos = 0u;
    close_count = 0u;
    CHECK(netchesszx_saveload_test_load("01", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    CHECK(close_count == 1u);

    file_present = 0u;
    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(file_present);
    CHECK(dir_present && mkdir_count == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/0168JF26.STJ") == 0);

    file_present = 0u;
    fat_day_delta = 13;
    CHECK(netchesszx_saveload_test_save("02", payload, &result) == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/02691F26.STJ") == 0);

    file_present = 0u;
    fat_date = fat_time = 0u;
    fat_day_delta = 0;
    CHECK(netchesszx_saveload_test_save("03", payload, &result) == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/03000000.STJ") == 0);

    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(file_present);

    file_present = 0u;
    short_write = 1u;
    CHECK(netchesszx_saveload_test_save("02", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(!file_present);

    if (failures != 0u) {
        return 1;
    }
    puts("esxDOS saveload durability tests ok");
    return 0;
}
