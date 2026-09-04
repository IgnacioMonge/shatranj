#ifndef NETCHESSZX_SPECTRUM_ESP_AT_H
#define NETCHESSZX_SPECTRUM_ESP_AT_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

void net_wait_frame(void);
void reset_line_buf(void);
uint8_t read_line(uint16_t frames) NETCHESSZX_FASTCALL;
uint8_t wait_for_prompt(uint16_t frames) NETCHESSZX_FASTCALL;

const char *spectrum_net_last_ip(void);
uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames);
void spectrum_net_guard_wait(uint16_t frames) NETCHESSZX_FASTCALL;
uint8_t spectrum_net_ensure_command_mode(void);
uint8_t spectrum_net_sync_time(void);

#define spectrum_esp_at_last_ip spectrum_net_last_ip

#ifdef NETCHESSZX_HOST_TEST
void netchesszx_esp_at_test_capture_ip(const char *line);
void netchesszx_esp_at_test_recovery_begin(void);
uint8_t netchesszx_esp_at_test_capture_msdos_time(uint16_t date,
                                                  uint16_t time);
uint8_t netchesszx_esp_at_test_validate_msdos_time(uint16_t date,
                                                   uint16_t time,
                                                   uint8_t apply);
#endif

#endif
