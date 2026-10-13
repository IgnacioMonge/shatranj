; Read-only dump TAP I/O. Do not link into product overlays.

PUBLIC _esx_fopen
PUBLIC _esx_fread
PUBLIC _esx_fclose
PUBLIC _esx_handle
PUBLIC _esx_buf
PUBLIC _esx_count
PUBLIC _esx_result

SECTION bss_user
_esx_handle:  defs 1
_esx_buf:     defs 2
_esx_count:   defs 2
_esx_result:  defs 2

SECTION code_user

_esx_fopen:
    ld b, 0x01              ; FA_READ
    push iy
    push ix
    push hl
    pop ix
    ld a, '*'
    rst 8
    defb 0x9A               ; F_OPEN
    jr nc, esx_open_ok
    ld l, a
    xor a
    ld (_esx_handle), a
    jr esx_pop_ix_iy_ret
esx_open_ok:
    inc a
    ld (_esx_handle), a
    ld l, 0
    jr esx_pop_ix_iy_ret

_esx_fread:
    push iy
    push ix
    ld a, (_esx_handle)
    dec a
    ld ix, (_esx_buf)
    ld bc, (_esx_count)
    rst 8
    defb 0x9D               ; F_READ
    jr nc, esx_io_ok
    ld bc, 0
esx_io_ok:
    ld (_esx_result), bc
esx_pop_ix_iy_ret:
    pop ix
    pop iy
    ret

_esx_fclose:
    push iy
    push ix
    ld a, (_esx_handle)
    dec a
    rst 8
    defb 0x9B               ; F_CLOSE
    sbc a, a
    ld l, a
    jr esx_pop_ix_iy_ret
