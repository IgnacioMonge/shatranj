; divmmc_uart.asm
;
; UART backend for ESP connected through a divMMC/divTIESUS
; ZX-Uno compatible UART.
;
; Keep this close to SpectalkZX's proven SDCC/IY backend. Shatranj exposes
; ready/read wrappers, so those wrappers cache a short burst around uartRead.

SECTION code_user

EXTERN _spectrum_frame_wait
PUBLIC _net_uart_init
PUBLIC _net_uart_send
PUBLIC _net_uart_read
PUBLIC _net_uart_ready
PUBLIC _net_uart_ready_fast
PUBLIC uartRead

UART_DATA_REG     EQU 0xC6
UART_STAT_REG     EQU 0xC7
UART_BYTE_RECIVED EQU 0x80
UART_BYTE_SENDING EQU 0x40
ZXUNO_ADDR        EQU 0xFC3B
ZXUNO_REG         EQU 0xFD3B
UART_RX_CACHE_SIZE EQU 8
UART_RX_CACHE_MASK EQU UART_RX_CACHE_SIZE - 1

SECTION bss_user

_rx_count:   DEFS 1
_rx_head:    DEFS 1
_rx_tail:    DEFS 1
_rx_cache:   DEFS UART_RX_CACHE_SIZE

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
    xor a
    ld (_rx_count), a
    ld (_rx_head), a
    ld (_rx_tail), a

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

    ld b, 10
uartInit_wait:
    push bc
    call uartRead
    pop bc
    call _spectrum_frame_wait
    djnz uartInit_wait

    ld bc, 0x0200
uartInit_flush:
    push bc
    call uartRead
    pop bc
    dec bc
    ld a, b
    or c
    jr nz, uartInit_flush

    ret

; _net_uart_send
;   fastcall: byte in L

_net_uart_send:
    ld bc, ZXUNO_ADDR
    ld a, UART_STAT_REG
    out (c), a

    inc b
uartSend_wait_tx:
    in a, (c)
    and UART_BYTE_SENDING
    jr nz, uartSend_wait_tx

    dec b
    ld a, UART_DATA_REG
    out (c), a

    inc b
    out (c), l
    ret

; _net_uart_ready
;   Returns L=1 if one byte is available, else L=0

_net_uart_ready:
    call uartCache_drain
    ld a, (_rx_count)
    or a
    jr nz, uartReady_yes

uartReady_no:
    ld l, 0
    ret

uartReady_yes:
    ld l, 1
    ret

DEFC _net_uart_ready_fast = _net_uart_ready

; _net_uart_read
;   Returns L=byte if available, else L=0

_net_uart_read:
    ld a, (_rx_count)
    or a
    jr z, uartRead_direct

    ld a, (_rx_head)
    ld e, a
    ld d, 0
    ld hl, _rx_cache
    add hl, de
    ld c, (hl)
    ld a, (_rx_head)
    inc a
    and UART_RX_CACHE_MASK
    ld (_rx_head), a
    ld hl, _rx_count
    dec (hl)
    ld l, c
    ret

uartRead_direct:
    call uartRead
    jr nc, uartRead_none
    ld l, a
    ret

uartRead_none:
    ld l, 0
    ret

uartCache_drain:
    ld a, (_rx_count)
    cp UART_RX_CACHE_SIZE
    ret nc

uartCache_drain_loop:
    call uartRead
    ret nc

    ld c, a
    ld a, (_rx_tail)
    ld e, a
    ld d, 0
    ld hl, _rx_cache
    add hl, de
    ld (hl), c
    ld a, (_rx_tail)
    inc a
    and UART_RX_CACHE_MASK
    ld (_rx_tail), a
    ld hl, _rx_count
    inc (hl)
    ld a, (hl)
    cp UART_RX_CACHE_SIZE
    jr c, uartCache_drain_loop
    ret
