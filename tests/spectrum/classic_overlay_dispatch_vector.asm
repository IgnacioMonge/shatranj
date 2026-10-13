SECTION code_user
start:
    ld sp, 0xFF00
    ld a, 254
    ld (0xF800), a
    ld ix, 0x1234
    ld iy, 0x5C3A
    ld hl, entry_table
    ld de, 0x6800
    ld bc, 8
    ldir
    ld hl, 0
    push hl
    call _spectrum_overlay_exec
    call check_success
    ; Invalid entry index and out-of-slot entry address must recover too.
    ld hl, 0x0200
    push hl
    call _spectrum_overlay_exec_cached
    call check_failure
    ld hl, 0x0100
    push hl
    call _spectrum_overlay_exec_cached
    call check_failure
    ; Failed load must not leave the dispatcher active.
    ld hl, 2
    push hl
    call _spectrum_overlay_exec
    call check_failure
    ld hl, 0
    push hl
    call _spectrum_overlay_exec_cached
    call check_success
    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, fail
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, fail
    ld hl, 0
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, fail
    xor a
    ld (0xF800), a
    jp 0
check_success:
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, fail
    ld a, i
    jp po, fail
    ret
check_failure:
    ld a, h
    or l
    jp nz, fail
    ret
entry_table:
    defb 2
    defw 0x6805, 0x67FF
    jp outer_entry
outer_entry:
    ld bc, _spectrum_overlay_context
    or a
    sbc hl, bc
    jp nz, fail
    ex de, hl
    or a
    sbc hl, bc
    jp nz, fail
    ld a, i
    jp pe, fail
    ld a, (loads)
    ld (outer_loads), a
    ; Both entry points must reject before changing ids, cache or doing I/O.
    ld hl, 0x0101
    push hl
    call _spectrum_overlay_exec_cached
    call check_failure
    ld a, i
    jp pe, fail
    ei
    ld hl, 0x0101
    push hl
    call _spectrum_overlay_exec
    call check_failure
    ld a, i
    jp po, fail
    ld a, (outer_loads)
    ld b, a
    ld a, (loads)
    cp b
    jp nz, fail
    ld a, (ovl_id)
    or a
    jp nz, fail
    ld a, (ovl_entry_id)
    or a
    jp nz, fail
    ld a, (ovl_cache_ready)
    cp 1
    jp nz, fail
    ld a, (_spectrum_overlay_loaded_id)
    or a
    jp nz, fail
    ld hl, 0x1234
    ret
ovl_ensure_loaded:
    ld a, (loads)
    inc a
    ld (loads), a
    ld a, (ovl_id)
    cp 2
    ccf
    ret c
    ld (_spectrum_overlay_loaded_id), a
    ld hl, 8
    ld (ovl_load_size), hl
    ld a, 1
    ld (ovl_cache_ready), a
    or a
    ret
fail:
    ld a, 255
    ld (0xF800), a
    jp 0
loads: defb 0
outer_loads: defb 0
