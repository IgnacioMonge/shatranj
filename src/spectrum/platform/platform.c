#include "spectrum/platform/platform.h"
#include "spectrum/platform/uart.h"
#include "spectrum/lowram_map.h"

#ifdef NETCHESSZX_SPECTRANEXT
extern void spectrum_net_background_drain(void);
#else
extern void net_uart_init(void);
extern uint8_t net_uart_send(uint8_t c) NETCHESSZX_FASTCALL;
extern uint8_t net_uart_ready(void);
extern uint8_t net_uart_read(void);
#ifdef NETCHESSZX_NEXT
extern uint8_t net_uart_rx_error;
extern void net_uart_hard_reset(void);
extern void net_uart_set_baud_230400(void);
#endif
#endif

#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
extern void spectrum_spxn_frame_wait(void);
#endif

void spectrum_frame_wait(void)
{
#ifdef NETCHESSZX_SDCC_IY
/* FILEUI calls this compiler helper through the resident symbol map. Keep
   the existing runtime dependency even if resident C no longer pulls it in.
   The exported alias emits no instructions or data. The bcix variant is only
   pulled by the SpectraNext TIME overlay (driver spxtime.c); scoping it keeps
   Classic and Next residents byte-identical. */
#asm
    EXTERN ____sdcc_2_copy_src_mhl_dst_deix
    PUBLIC netchesszx_overlay_copy2_runtime
    defc netchesszx_overlay_copy2_runtime = ____sdcc_2_copy_src_mhl_dst_deix
#endasm
#ifdef NETCHESSZX_SPECTRANEXT
#asm
    EXTERN ____sdcc_2_copy_src_mhl_dst_bcix
    PUBLIC netchesszx_overlay_copy2_bcix_runtime
    defc netchesszx_overlay_copy2_bcix_runtime = ____sdcc_2_copy_src_mhl_dst_bcix
#endasm
#endif
#endif
#if defined(NETCHESSZX_HOST_TEST)
#elif defined(NETCHESSZX_SPECTRANEXT)
    spectrum_spxn_frame_wait();
#else
    /* Next's ROM IM1 expects IY=$5C3A while interrupts are enabled. Classic
       uses a resident IM2 frame ISR, but keeps the same sdcc_iy contract for
       shared live-interrupt code. Handwritten ASM that borrows IY must run
       under DI and restore it before any EI path. */
#asm
    EXTERN _spectrum_input_frame_tick
    ei
    halt
    push af
    push bc
    push de
    push hl
    call _spectrum_input_frame_tick
    pop hl
    pop de
    pop bc
    pop af
#endasm
#endif
}

#ifdef NETCHESSZX_SPECTRANEXT
void spectrum_uart_background_pump(void)
{
    spectrum_net_background_drain();
}
#else
/* RX ring under the UART wrappers. Calls to spectrum_uart_background_pump()
   stockpile pending HW bytes during multi-frame UI waits and between selected
   esxDOS calls. The pump does not run inside a synchronous esxDOS RST 8; the
   Classic UART-ESP setup relies on its required CTS flow control there.
   Readers drain the ring first via the wrappers below, so byte order is
   preserved and every consumer (DIRECT overlay, esp_at line parser, MQTT
   stream) is covered without changes. Lives in the reclaimed printer buffer
   (see lowram_map.h) so it costs no BSS; indices are uint8_t, so 256 is the
   ceiling. */
#define SPECTRUM_UART_RING_SIZE NETCHESSZX_LOWRAM_UART_RING_SIZE
#ifdef NETCHESSZX_FIXED_LOW_RAM
static __at(NETCHESSZX_LOWRAM_UART_RING_ADDR) uint8_t uart_ring[SPECTRUM_UART_RING_SIZE];
#else
static uint8_t uart_ring[SPECTRUM_UART_RING_SIZE];
#endif
static uint8_t uart_ring_head;
static uint8_t uart_ring_tail;

#define uart_ring_next(i) ((uint8_t)(((uint8_t)(i) + 1u) & (SPECTRUM_UART_RING_SIZE - 1u)))

void spectrum_uart_background_pump(void)
{
    uint8_t next;

    while (net_uart_ready()) {
#ifdef NETCHESSZX_NEXT
        if (net_uart_rx_error) {
            break;
        }
#endif
        next = uart_ring_next(uart_ring_tail);
        if (next == uart_ring_head) {
            break; /* full: the backend retains any prefetched byte */
        }
        uart_ring[uart_ring_tail] = net_uart_read();
        uart_ring_tail = next;
    }
}

void spectrum_uart_init(void)
{
    uart_ring_head = 0u;
    uart_ring_tail = 0u;
    net_uart_init();
}

#ifdef NETCHESSZX_NEXT
void spectrum_uart_set_baud_230400(void)
{
    spectrum_uart_init();
    net_uart_set_baud_230400();
}

uint8_t spectrum_uart_error(void)
{
    return net_uart_rx_error;
}

void spectrum_uart_hard_reset(void)
{
    net_uart_hard_reset();
}
#endif

void spectrum_uart_flush(uint16_t frames) NETCHESSZX_FASTCALL
{
    uint16_t cap = 2048u;

    uart_ring_head = uart_ring_tail;
    if (frames == 0u) {
        return;
    }
    while (frames-- != 0u && cap != 0u) {
        while (cap != 0u && net_uart_ready()) {
            (void)net_uart_read();
            --cap;
        }
        spectrum_frame_wait();
    }
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error = 0u; /* explicit discard establishes a new RX boundary */
#endif
}

/* Retry a bounded local UART TX wait. Next tests FIFO-full, not ESP
   responsiveness; successful enqueue is not remote receipt/acknowledgement. */
static uint8_t uart_send_byte(uint8_t c) NETCHESSZX_FASTCALL
{
    uint8_t tries = 4u;

    do {
#ifdef NETCHESSZX_NEXT
        if (net_uart_rx_error) {
            return 0u;
        }
#endif
        if (net_uart_send(c) == 0u) {
#ifdef NETCHESSZX_NEXT
            return (uint8_t)(net_uart_rx_error == 0u);
#else
            return 1u;
#endif
        }
    } while (--tries != 0u);
    return 0u;
}

uint8_t spectrum_uart_send_string(const char *s) NETCHESSZX_FASTCALL
{
    while (*s != '\0') {
        if (!uart_send_byte((uint8_t)*s++)) {
            return 0u;
        }
    }
    return 1u;
}

uint8_t spectrum_uart_send_bytes(const uint8_t *data, uint8_t len)
{
    while (len-- != 0u) {
        if (!uart_send_byte(*data++)) {
            return 0u;
        }
    }
    return 1u;
}

uint8_t spectrum_uart_send_crlf(void)
{
    if (!uart_send_byte('\r')) {
        return 0u;
    }
    return uart_send_byte('\n');
}

uint8_t spectrum_uart_ready(void)
{
#ifdef NETCHESSZX_NEXT
    uint8_t ready;
    if (net_uart_rx_error) {
        return 0u;
    }
#endif
    if (uart_ring_head != uart_ring_tail) {
        return 1u;
    }
#ifdef NETCHESSZX_NEXT
    ready = net_uart_ready();
    return net_uart_rx_error ? 0u : ready;
#else
    return net_uart_ready();
#endif
}

uint8_t spectrum_uart_read(void)
{
    uint8_t c;

#ifdef NETCHESSZX_NEXT
    if (net_uart_rx_error) {
        return 0u;
    }
#endif
    if (uart_ring_head != uart_ring_tail) {
        c = uart_ring[uart_ring_head];
        uart_ring_head = uart_ring_next(uart_ring_head);
        return c;
    }
#ifdef NETCHESSZX_NEXT
    c = net_uart_read();
    return net_uart_rx_error ? 0u : c;
#else
    return net_uart_read();
#endif
}
#endif
