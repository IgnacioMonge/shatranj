#ifndef NETCHESSZX_SPECTRUM_NEXT_BAUD_RECOVERY_H
#define NETCHESSZX_SPECTRUM_NEXT_BAUD_RECOVERY_H

#include "spectrum/platform/uart.h"
#include "spectrum/transport/esp_at.h"

#ifdef NETCHESSZX_NEXT
/* BRZXN runs UART_CUR at 230400. A reset/NMI can leave the ESP there while
   Shatranj starts its local UART at 115200. This cold preflight helper probes
   the normal rate first, then that one inherited rate. It restores the local
   UART to 115200 before the existing soft/hard fallback. */
static uint8_t spectrum_next_preflight_command_mode(void)
{
    uint8_t inherited_bridgezx_baud;

    if (spectrum_net_at_cmd("AT", 8u)) {
        return 1u;
    }

    spectrum_uart_set_baud_230400();
    inherited_bridgezx_baud = spectrum_net_at_cmd("AT", 8u);
    if (inherited_bridgezx_baud) {
        /* UART_CUR response baud is not stable across ESP-AT variants. */
        (void)spectrum_net_at_cmd("AT+UART_CUR=115200,8,1,0,0", 2u);
    }

    /* Reinitialize rather than only changing the divisor: this also clears
       the software ring and the UART's cached byte before verification. */
    spectrum_uart_init();
    if (inherited_bridgezx_baud && spectrum_net_at_cmd("AT", 30u)) {
        return 1u;
    }
    return spectrum_net_ensure_command_mode();
}
#endif

#endif
