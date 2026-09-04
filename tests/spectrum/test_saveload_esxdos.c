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
static char last_path[32];

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

void esx_fopen(const char *path)
{
    remember(path);
    esx_handle = file_present ? 1u : 0u;
}

void esx_fcreate(const char *path)
{
    (void)path;
    esx_handle = 0u;
}

void esx_fcreate_new(const char *path)
{
    remember(path);
    esx_handle = (uint8_t)(dir_present && !file_present);
    if (esx_handle != 0u) {
        file_present = 1u;
    }
}

void esx_mkdir(const char *path)
{
    CHECK(strcmp(path, "/SYS/CONFIG") == 0);
    dir_present = 1u;
    ++mkdir_count;
}

void esx_fread(void) { esx_result = 0u; }
void esx_fwrite(void) { esx_result = short_write ? esx_count - 1u : esx_count; }

uint8_t esx_fclose(void)
{
    esx_handle = 0u;
    return 0u;
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
    return (uint16_t)((46u << 9) | (8u << 5) | 19u);
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return (uint16_t)((15u << 11) | (26u << 5));
}

#include "../../src/spectrum/overlay/saveload_ovl.c"

int main(void)
{
    char payload[NETCHESSZX_SAVE_WIRE_B64_SIZE] = {0};
    uint8_t result;

    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(file_present);
    CHECK(dir_present && mkdir_count == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/0168JF26.STJ") == 0);

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
