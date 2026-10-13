#include "spectrum/platform/uart.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t tx_failures;
static uint8_t tx_always_fail;
static uint8_t tx_calls;
static uint8_t rx[600];
static unsigned rx_pos, rx_len;
#ifdef NETCHESSZX_NEXT
uint8_t net_uart_rx_error;
static uint8_t inject_tx_error, inject_rx_error, inject_read_error;
void net_uart_hard_reset(void) {}
void net_uart_set_baud_230400(void) {}
#endif

void net_uart_init(void)
{
    rx_pos = rx_len = 0u;
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error = 0u;
#endif
}

uint8_t net_uart_send(uint8_t c)
{
    (void)c;
    ++tx_calls;
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_tx_error;
#endif
    if (tx_always_fail) {
        return 1u;
    }
    if (tx_failures != 0u) {
        --tx_failures;
        return 1u;
    }
    return 0u;
}

uint8_t net_uart_ready(void)
{
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_rx_error;
#endif
    return (uint8_t)(rx_pos < rx_len);
}

uint8_t net_uart_read(void)
{
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_read_error;
#endif
    return rx_pos < rx_len ? rx[rx_pos++] : 0u;
}

static void check(uint8_t condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

int main(void)
{
    static const uint8_t data[] = {'A', 'B'};

    check(spectrum_uart_send_string("AB"), "string TX succeeds");
    check(tx_calls == 2u, "string TX byte count");

    tx_calls = 0u;
    tx_failures = 4u;
    check(!spectrum_uart_send_string("A"), "string TX failure propagates");
    check(tx_calls == 4u, "string TX failure retry count");

    tx_calls = 0u;
    tx_failures = 0u;
    check(spectrum_uart_send_crlf(), "CRLF TX succeeds");
    check(tx_calls == 2u, "CRLF TX byte count");

    tx_calls = 0u;
    tx_failures = 3u;
    check(spectrum_uart_send_bytes(data, sizeof(data)),
          "transient TX busy recovers");
    check(tx_calls == 5u, "transient retry count");

    tx_calls = 0u;
    tx_always_fail = 1u;
    check(!spectrum_uart_send_bytes(data, sizeof(data)),
          "persistent TX busy fails");
    check(tx_calls == 4u, "persistent failure aborts first byte");

    tx_always_fail = 0u;
    spectrum_uart_init();
    for (unsigned i = 0; i < sizeof(rx); ++i) rx[i] = (uint8_t)i;
    rx_len = sizeof(rx);
    spectrum_uart_background_pump();
    check(rx_pos == 255u, "ring has 255 usable positions");
    for (unsigned i = 0; i < 200u; ++i)
        check(spectrum_uart_ready() && spectrum_uart_read() == (uint8_t)i,
              "first ring segment including zero");
    spectrum_uart_background_pump();
    for (unsigned i = 200u; i < sizeof(rx); ++i)
        check(spectrum_uart_ready() && spectrum_uart_read() == (uint8_t)i,
              "ring wrap preserves order before hardware tail");
    check(!spectrum_uart_ready() && spectrum_uart_read() == 0u, "empty read");
#ifdef NETCHESSZX_NEXT
    spectrum_uart_init();
    rx_len = 4u;
    spectrum_uart_background_pump();
    net_uart_rx_error = 4u;
    check(!spectrum_uart_ready(), "fault suppresses previously buffered bytes");
    check(!spectrum_uart_send_string("A"), "fault prevents TX");
    check(spectrum_uart_error() == 4u, "fault remains sticky");
    spectrum_uart_flush(0u);
    check(spectrum_uart_error() == 4u, "zero-frame flush cannot clear hardware fault");
    spectrum_uart_set_baud_230400();
    check(!spectrum_uart_error() && !spectrum_uart_ready(),
          "baud probe clears ring and fault through initialization");
    rx_len = 4u;
    inject_rx_error = 0x40u;
    check(!spectrum_uart_ready(), "error discovered during status suppresses ready");
    inject_rx_error = 0u;
    spectrum_uart_flush(1u);
    check(!spectrum_uart_error() && !spectrum_uart_ready(), "explicit discard clears fault");
    inject_tx_error = 4u;
    tx_calls = 0u;
    check(!spectrum_uart_send_string("AB") && tx_calls == 1u,
          "fault discovered during TX aborts remaining bytes");
    inject_tx_error = 0u;
    check(spectrum_uart_error() == 4u, "TX poll retains RX fault");
    spectrum_uart_init();
    check(!spectrum_uart_error(), "init clears fault");
    rx[0] = 'Q';
    rx_len = 1u;
    inject_read_error = 0x40u;
    check(spectrum_uart_read() == 0u && spectrum_uart_error() == 0x40u,
          "uncached read suppresses byte when its status read detects error");
    inject_read_error = 0u;
    spectrum_uart_init();
#endif
    puts("uart platform tests ok");
    return 0;
}
