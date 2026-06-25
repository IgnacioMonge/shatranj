#include "spectrum/platform/platform.h"
#include "spectrum/platform/uart.h"

extern void net_uart_init(void);
extern void net_uart_send(uint8_t c) __z88dk_fastcall;
extern uint8_t net_uart_ready(void);
extern uint8_t net_uart_read(void);

void spectrum_frame_wait(void)
{
    /* ROM IM1 expects IY=$5C3A while interrupts are enabled. The sdcc_iy
       build keeps generated C off IY; handwritten ASM that borrows IY must
       run under DI and restore it before any EI path. */
#asm
    ei
    halt
#endasm
}

void spectrum_uart_init(void)
{
    net_uart_init();
}

void spectrum_uart_flush(uint16_t frames) NETCHESSZX_FASTCALL
{
    uint16_t cap = 2048u;

    while (frames-- != 0u && cap != 0u) {
        while (net_uart_ready() && cap != 0u) {
            (void)net_uart_read();
            --cap;
        }
        spectrum_frame_wait();
    }
}

void spectrum_uart_send_string(const char *s) NETCHESSZX_FASTCALL
{
    while (*s != '\0') {
        net_uart_send((uint8_t)*s++);
    }
}

void spectrum_uart_send_bytes(const uint8_t *data, uint16_t len)
{
    while (len-- != 0u) {
        net_uart_send(*data++);
    }
}

void spectrum_uart_send_crlf(void)
{
    net_uart_send('\r');
    net_uart_send('\n');
}

uint8_t spectrum_uart_ready(void)
{
    return net_uart_ready();
}

uint8_t spectrum_uart_read(void)
{
    return net_uart_read();
}
