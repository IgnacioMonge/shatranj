; UART backend: status-only ready and defensive read; no prefetch cache.
;
; UART backend for ESP connected through a divMMC/divTIESUS
; ZX-Uno compatible UART.
;
; Keep this close to SpectalkZX's proven SDCC/IY backend. Shatranj exposes
; ready polls status without consuming data; read polls status again.

SECTION code_user

PUBLIC _net_uart_init
PUBLIC _net_uart_send
PUBLIC _net_uart_read
PUBLIC _net_uart_ready
PUBLIC _net_uart_ready_fast
PUBLIC uartRead

UART_DATA_REG     EQU 0xC6
UART_STAT_REG     EQU 0xC7
UART_BYTE_RECEIVED EQU 0x80
UART_BYTE_SENDING EQU 0x40
ZXUNO_ADDR        EQU 0xFC3B

SECTION code_user

; Internal helper: uartRead
;   Returns: CF=1 and A=byte if data available
;            CF=0 if nothing to read

uartRead:
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    inc b
    in a, (c)
    add a, a
    ret nc

    dec b
    ld a, UART_DATA_REG
    out (c), a

    inc b
    in a, (c)       ; IN preserves CF from the RX-ready status test.
    ret

_net_uart_init:
    ; Prime status/data register reads, as in SpectalkZX.
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    inc b
    in a, (c)

    dec b
    ld a, UART_DATA_REG
    out (c), a
    inc b
    in a, (c)

    ; The transport owns startup draining via spectrum_uart_flush(25).
    ret

; _net_uart_send
;   fastcall: byte in L

_net_uart_send:
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    inc b
    ld d, 0
uartSend_wait_tx:
    in a, (c)
    and UART_BYTE_SENDING
    jr z, uartSend_ready_tx
    dec d
    jr nz, uartSend_wait_tx
    ld l, 1              ; timed out: report so caller retries, not silent drop
    ret

uartSend_ready_tx:
    dec b
    ld a, UART_DATA_REG
    out (c), a

    inc b
    out (c), l
    ld l, 0             ; sent
    ret

; _net_uart_ready
;   Returns L=1 if one byte is available, else L=0

_net_uart_ready:
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a
    inc b
    in a, (c)
    rlca
    and 1
    ld l, a
    ret

DEFC _net_uart_ready_fast = _net_uart_ready

; _net_uart_read
;   Returns L=byte if available, else L=0

_net_uart_read:
    call uartRead
    ld l, a
    ret c
    ld l, 0
    ret
