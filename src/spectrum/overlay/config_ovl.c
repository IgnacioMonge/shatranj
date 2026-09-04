#include "spectrum/overlay/overlay_api.h"
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/config/app_config_format.h"
#include "spectrum/config/session.h"
#include "spectrum/saveload/saveload.h"
#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spectrum/lowram_map.h"
#endif

#include <string.h>

#ifdef NETCHESSZX_HOST_TEST
#define CONFIG_COPY strcpy
#else
#define CONFIG_COPY spectrum_append_text
#endif

#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spxf.h"
#elif defined(NETCHESSZX_SPECTRANEXT)
int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len);
#endif

extern uint8_t esx_handle;
extern uint16_t esx_buf;
extern uint16_t esx_count;
extern uint16_t esx_result;
void esx_fopen(const char *path) __z88dk_fastcall;
void esx_fcreate(const char *path) __z88dk_fastcall;
void esx_fcreate_new(const char *path) __z88dk_fastcall;
void esx_funlink(const char *path) __z88dk_fastcall;
void esx_fread(void);
void esx_fwrite(void);
uint8_t esx_fclose(void);

#ifdef NETCHESSZX_SPECTRANEXT
void esx_opendir(const char *path) __z88dk_fastcall;
void esx_mkdir(const char *path) __z88dk_fastcall;
void esx_commit(const char *path) __z88dk_fastcall;
#define CONFIG_DIR "/CFG"
#define CONFIG_PATH "/CFG/SHATRANJ.CFG"
/* Distinct from the SAVELOAD staging name: both live on the one slot. */
#define CONFIG_TEMP_PATH "/CFG/SHATRANJ.TMP"
#if !defined(NETCHESSZX_HOST_TEST)
#define CONFIG_PATH_ARG \
    ((char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#define CONFIG_TEMP_PATH_ARG (CONFIG_PATH_ARG + sizeof(CONFIG_PATH))
#define CONFIG_DIR_ARG (CONFIG_TEMP_PATH_ARG + sizeof(CONFIG_TEMP_PATH))

static void config_stage_paths(void)
{
    spectrum_append_text(CONFIG_PATH_ARG, CONFIG_PATH);
    spectrum_append_text(CONFIG_TEMP_PATH_ARG, CONFIG_TEMP_PATH);
    spectrum_append_text(CONFIG_DIR_ARG, CONFIG_DIR);
}
#else
#define CONFIG_PATH_ARG CONFIG_PATH
#define CONFIG_TEMP_PATH_ARG CONFIG_TEMP_PATH
#define CONFIG_DIR_ARG CONFIG_DIR
#define config_stage_paths() ((void)0)
#endif
#else
static const char config_path[] = "/SYS/SHATRANJ.CFG";
static const char config_path_alt[] = "/SYS/SHATRANJ.CF2";
static const char config_path_legacy[] = "/SYS/CONFIG/SHATRANJ.CFG";
#define CONFIG_PATH config_path
#define CONFIG_PATH_ALT config_path_alt
#define CONFIG_PATH_LEGACY config_path_legacy
#endif
extern uint8_t setup_config_record[];

uint8_t config_defaults_ovl(uint8_t *ctx) __z88dk_fastcall
{
    (void)ctx;
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 netchesszx_host_color);
    netchesszx_timezone = NETCHESSZX_TIME_RTC;
    netchesszx_timezone_last = NETCHESSZX_TZ;
    netchesszx_direct_port = NETCHESSZX_PORT;
    strncpy(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE,
            NETCHESSZX_MQTT_CODE_MAX);
    netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX] = '\0';
    netchesszx_direct_host[0] = '\0';
    return 1u;
}

static void config_pack(uint8_t *record)
{
    uint8_t flags = 0u;

    memset(record, 0, NETCHESSZX_APP_CONFIG_READ_SIZE);
    record[NETCHESSZX_APP_CONFIG_MAGIC_0] = 'S';
    record[NETCHESSZX_APP_CONFIG_MAGIC_1] = 'H';
    record[NETCHESSZX_APP_CONFIG_MAGIC_2] = 'C';
    record[NETCHESSZX_APP_CONFIG_MAGIC_3] = 'F';
    record[NETCHESSZX_APP_CONFIG_VERSION_OFF] = NETCHESSZX_APP_CONFIG_VERSION;
    record[NETCHESSZX_APP_CONFIG_LENGTH_OFF] = NETCHESSZX_APP_CONFIG_SIZE;
    if (netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_ROLE;
    }
    if (netchesszx_transport == NETCHESSZX_TRANSPORT_MQTT) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT;
    }
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] = flags;
    record[NETCHESSZX_APP_CONFIG_GAME_OFF] = (uint8_t)(
        (netchesszx_host_color == NETCHESSZX_COLOR_BLACK
             ? NETCHESSZX_APP_CONFIG_GAME_COLOR : 0u) |
        (netchesszx_notation_is_san()
             ? NETCHESSZX_APP_CONFIG_GAME_NOTATION : 0u) |
        (netchesszx_movement_hints
             ? NETCHESSZX_APP_CONFIG_GAME_HINTS : 0u) |
        (uint8_t)(netchesszx_board_theme_index <<
                  NETCHESSZX_APP_CONFIG_GAME_THEME_SHIFT) |
        (uint8_t)(netchesszx_piece_set_index <<
                  NETCHESSZX_APP_CONFIG_GAME_SET_SHIFT));
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = (uint8_t)netchesszx_timezone;
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] =
        (uint8_t)netchesszx_timezone_last;
    record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] =
        (uint8_t)netchesszx_direct_port;
    record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] =
        (uint8_t)(netchesszx_direct_port >> 8);
    (void)CONFIG_COPY(
        (char *)(record + NETCHESSZX_APP_CONFIG_ROOM_OFF),
        netchesszx_mqtt_code);
    (void)CONFIG_COPY(
        (char *)(record + NETCHESSZX_APP_CONFIG_HOST_OFF),
        netchesszx_direct_host);
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] =
        netchesszx_app_config_crc8(record);
}

static void config_apply(const uint8_t *record)
{
    uint8_t flags = record[NETCHESSZX_APP_CONFIG_FLAGS_OFF];
    uint8_t game = record[NETCHESSZX_APP_CONFIG_GAME_OFF];

    netchesszx_host_color =
        (game & NETCHESSZX_APP_CONFIG_GAME_COLOR)
            ? NETCHESSZX_COLOR_BLACK : NETCHESSZX_COLOR_WHITE;
    netchesszx_notation =
        (game & NETCHESSZX_APP_CONFIG_GAME_NOTATION)
            ? NETCHESSZX_NOTATION_SAN : NETCHESSZX_NOTATION_COORD;
    netchesszx_movement_hints =
        (uint8_t)((game & NETCHESSZX_APP_CONFIG_GAME_HINTS) != 0u);
    netchesszx_board_theme_index =
        (uint8_t)((game & NETCHESSZX_APP_CONFIG_GAME_THEME_MASK) >>
                  NETCHESSZX_APP_CONFIG_GAME_THEME_SHIFT);
    netchesszx_piece_set_index =
        (uint8_t)((game & NETCHESSZX_APP_CONFIG_GAME_SET_MASK) >>
                  NETCHESSZX_APP_CONFIG_GAME_SET_SHIFT);

    netchesszx_session_configure(
        (uint8_t)(flags & NETCHESSZX_APP_CONFIG_FLAG_ROLE),
        (uint8_t)((flags & NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT) != 0u),
        netchesszx_host_color);
    netchesszx_timezone = (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_OFF];
    netchesszx_timezone_last =
        (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF];
    netchesszx_direct_port =
        (uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] |
        ((uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] << 8);
    memcpy(netchesszx_mqtt_code,
           record + NETCHESSZX_APP_CONFIG_ROOM_OFF,
           NETCHESSZX_MQTT_CODE_MAX + 1u);
    memcpy(netchesszx_direct_host,
           record + NETCHESSZX_APP_CONFIG_HOST_OFF,
           NETCHESSZX_DIRECT_HOST_MAX + 1u);
}

#ifdef NETCHESSZX_SPECTRANEXT
/* XFS state is durable only after a verified directory exists.  The adapter
   owns the one live handle and closes directories through CLOSEDIR/VCLOSE. */
static uint8_t config_ensure_dir(void)
{
    esx_opendir(CONFIG_DIR_ARG);
    if (esx_handle != 0u) {
        return (uint8_t)(esx_fclose() == 0u);
    }
    esx_mkdir(CONFIG_DIR_ARG);
    if (!esx_result) {
        return 0u;
    }
    esx_commit(CONFIG_DIR_ARG);
    if (!esx_result) {
        return 0u;
    }
    esx_opendir(CONFIG_DIR_ARG);
    if (esx_handle == 0u) {
        return 0u;
    }
    return (uint8_t)(esx_fclose() == 0u);
}
#endif

static uint8_t config_read_record(const char *path, uint8_t *record);

static uint8_t config_save_record(uint8_t *record)
{
#ifndef NETCHESSZX_SPECTRANEXT
    const char *old_path;
    const char *new_path;
    uint8_t result;
    uint8_t write_ok;
#endif

#ifdef NETCHESSZX_SPECTRANEXT
    config_pack(record);
    if (!netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    config_stage_paths();
    if (!config_ensure_dir()) {
        return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
    }
    /* One transaction: write the temp, commit it, then RENAME it over
       the target, which is never truncated. The previous
       freplace/write/commit sequence destroyed the stored record the
       moment it opened, so a power cut before the write completed lost
       the configuration. */
    if (spxf_replace_atomic(CONFIG_PATH_ARG, CONFIG_TEMP_PATH_ARG, record,
                            NETCHESSZX_APP_CONFIG_SIZE) != 0) {
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
    return SPECTRUM_OVL_SAVELOAD_OK;
#else
    /* esxDOS has no atomic replace primitive. Alternate two files instead:
       the old copy stays valid until the new copy has closed successfully. */
    result = config_read_record(CONFIG_PATH, record);
    if (result == SPECTRUM_OVL_SAVELOAD_ERR_IO) {
        return result;
    }
    if (result == SPECTRUM_OVL_SAVELOAD_OK) {
        old_path = CONFIG_PATH;
        new_path = CONFIG_PATH_ALT;
    } else {
        result = config_read_record(CONFIG_PATH_ALT, record);
        if (result == SPECTRUM_OVL_SAVELOAD_ERR_IO) {
            return result;
        }
        if (result == SPECTRUM_OVL_SAVELOAD_OK) {
            old_path = CONFIG_PATH_ALT;
        } else {
            /* The legacy path remains a read-only fallback. */
            old_path = 0;
        }
        new_path = CONFIG_PATH;
    }

    config_pack(record);
    if (!netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    esx_funlink(new_path);
    esx_fcreate_new(new_path);
    if (esx_handle == 0u) {
        return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
    }
#ifdef NETCHESSZX_CONFIG_PACK_APPLY_TEST
    esx_buf = 0u;
#else
    spectrum_net_background_drain();
    esx_buf = (uint16_t)record;
#endif
    esx_count = NETCHESSZX_APP_CONFIG_SIZE;
    esx_fwrite();
    write_ok = (uint8_t)(esx_result == NETCHESSZX_APP_CONFIG_SIZE);
    if (esx_fclose()) {
        write_ok = 0u;
    }
    if (!write_ok) {
        esx_funlink(new_path);
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
    if (old_path != 0) {
        esx_funlink(old_path);
        if (!esx_result) {
            esx_funlink(new_path);
            return SPECTRUM_OVL_SAVELOAD_ERR_IO;
        }
    }
    return SPECTRUM_OVL_SAVELOAD_OK;
#endif
}

static uint8_t config_read_record(const char *path, uint8_t *record)
{
    esx_fopen(path);
    if (esx_handle == 0u) {
        return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
    }
#ifdef NETCHESSZX_CONFIG_PACK_APPLY_TEST
    esx_buf = 0u;
#else
#ifndef NETCHESSZX_SPECTRANEXT
    spectrum_net_background_drain();
#endif
    esx_buf = (uint16_t)record;
#endif
    esx_count = NETCHESSZX_APP_CONFIG_READ_SIZE;
    esx_fread();
    if (esx_fclose()) {
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
    if (esx_result != NETCHESSZX_APP_CONFIG_SIZE ||
        !netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    return SPECTRUM_OVL_SAVELOAD_OK;
}

uint8_t config_load_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t *record = setup_config_record;
    uint8_t result;
#ifndef NETCHESSZX_SPECTRANEXT
    uint8_t alt_result;
#endif

    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_ERR_DATA;
#ifdef NETCHESSZX_SPECTRANEXT
    config_stage_paths();
    result = config_read_record(CONFIG_PATH_ARG, record);
#else
    result = config_read_record(CONFIG_PATH, record);
#endif
#ifndef NETCHESSZX_SPECTRANEXT
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
        alt_result = config_read_record(CONFIG_PATH_ALT, record);
        if (alt_result == SPECTRUM_OVL_SAVELOAD_OK ||
            result == SPECTRUM_OVL_SAVELOAD_ERR_OPEN) {
            result = alt_result;
        }
    }
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
        alt_result = config_read_record(CONFIG_PATH_LEGACY, record);
        if (alt_result == SPECTRUM_OVL_SAVELOAD_OK ||
            result == SPECTRUM_OVL_SAVELOAD_ERR_OPEN) {
            result = alt_result;
        }
    }
#endif
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = result;
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
        (void)config_defaults_ovl(ctx);
        return result == SPECTRUM_OVL_SAVELOAD_ERR_DATA
                   ? SPECTRUM_CONFIG_STATE_INVALID
                   : 0u;
    }
    config_apply(record);
    return SPECTRUM_CONFIG_STATE_SAVED;
}

uint8_t config_save_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t result;

    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    result = config_save_record(setup_config_record);
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = result;
    return (uint8_t)(result == SPECTRUM_OVL_SAVELOAD_OK);
}
