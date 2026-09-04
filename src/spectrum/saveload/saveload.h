#ifndef NETCHESSZX_SPECTRUM_SAVELOAD_H
#define NETCHESSZX_SPECTRUM_SAVELOAD_H

#include <stdint.h>
#include "spectrum/overlay/overlay.h"

#define SPECTRUM_CONFIG_RECORD_SIZE 46u
#define SPECTRUM_CONFIG_STATE_SAVED 3u
#define SPECTRUM_CONFIG_STATE_INVALID 4u

uint8_t spectrum_saveload_run(uint8_t entry, const char *name,
                              const char *buf);

#define spectrum_saveload_write(name, b64) \
    spectrum_saveload_run(SPECTRUM_OVL_SAVELOAD_SAVE_NCZS, (name), (b64))
#define spectrum_saveload_read(name, b64) \
    spectrum_saveload_run(SPECTRUM_OVL_SAVELOAD_LOAD_NCZS, (name), (b64))
#define spectrum_saveload_erase(name) \
    spectrum_saveload_run(SPECTRUM_OVL_SAVELOAD_ERASE_NCZS, (name), \
                          (const char *)0)
/* Returns 0 for no file, SAVED for a clean complete config, or INVALID. */
uint8_t netchesszx_config_load_overlay(void);
uint8_t netchesszx_config_save_overlay(void);
uint8_t netchesszx_config_defaults_overlay(void);

#endif
