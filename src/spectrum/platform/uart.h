#ifndef NETCHESSZX_SPECTRUM_UART_H
#define NETCHESSZX_SPECTRUM_UART_H

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

void spectrum_uart_init(void);
void spectrum_uart_flush(uint16_t frames) NETCHESSZX_FASTCALL;
void spectrum_uart_send_string(const char *s) NETCHESSZX_FASTCALL;
void spectrum_uart_send_bytes(const uint8_t *data, uint16_t len);
void spectrum_uart_send_crlf(void);
uint8_t spectrum_uart_ready(void);
uint8_t spectrum_uart_read(void);

#endif
