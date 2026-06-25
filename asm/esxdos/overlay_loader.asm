SECTION code_user

; SDCC/IY: two uint8 args are packed into one stack word:
;   SP+2 = ovl_id, SP+3 = entry_id.

PUBLIC _spectrum_overlay_exec
PUBLIC _spectrum_overlay_exec_cached
PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
PUBLIC _spectrum_render_about
PUBLIC _netchesszx_piece_set_load
PUBLIC _spectrum_overlay_loaded_id
PUBLIC _overlay_code_slot
PUBLIC _spectrum_overlay_context

_spectrum_overlay_context EQU 0x5FE0
_overlay_code_slot EQU 0x6800
ovl_invalid_id EQU 0xff
asset_load_addr EQU 0x6000
asset_load_size EQU 1023
asset_piece_offset EQU 639
piece_sprite_set_size EQU 384
piece_sprite_set_size_hi EQU piece_sprite_set_size / 256
piece_sprite_set_size_lo EQU piece_sprite_set_size - (piece_sprite_set_size_hi * 256)
asset_load_size_hi EQU asset_load_size / 256
asset_load_size_lo EQU asset_load_size - (asset_load_size_hi * 256)
about_board_offset EQU asset_load_size
about_board_width EQU 18
about_board_top_y EQU 32
about_board_height EQU 144
about_board_block_rows EQU 8
about_board_char_rows EQU about_board_height / about_board_block_rows
about_board_block_bytes EQU about_board_width * about_board_block_rows
about_scratch EQU _overlay_code_slot
about_board_attr_top EQU 4
about_board_attr_rows EQU 18
about_board_attr_tail_start EQU about_board_attr_rows - 2
about_board_attr_tail_rows EQU about_board_attr_rows - about_board_attr_tail_start
about_board_attr_tail_bytes EQU about_board_width * about_board_attr_tail_rows
about_board_size EQU (about_board_width * about_board_height) + (about_board_width * about_board_attr_rows)
piece_set_extra_offset EQU about_board_offset + about_board_size

SECTION bss_user

ovl_handle:   DEFS 1
ovl_entry_id: DEFS 1
ovl_id:       DEFS 1
ovl_cache_ready: DEFS 1
asset_set_index: DEFS 1
about_row:    DEFS 1
about_scan:   DEFS 1
about_block_rows: DEFS 1
_spectrum_overlay_loaded_id: DEFS 1

SECTION code_user

_spectrum_overlay_exec:
    xor a
    ld (ovl_cache_ready), a

_spectrum_overlay_exec_cached:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    ld (ovl_id), a
    inc hl
    ld a, (hl)
    ld (ovl_entry_id), a

    push ix
    push iy

    call ovl_ensure_loaded
    jp c, ovl_fail
    jp ovl_call_loaded

_spectrum_assets_load:
    push ix
    push iy
    ld hl, asset_filename
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    rst 8
    defb 0x9a
    jr c, assets_fail
    ld (ovl_handle), a

    ld a, (ovl_handle)
    ld ix, asset_load_addr
    ld bc, asset_load_size
    rst 8
    defb 0x9d
    jr c, assets_fail_close
    ld a, b
    cp asset_load_size_hi
    jr nz, assets_fail_close
    ld a, c
    cp asset_load_size_lo
    jr nz, assets_fail_close

    call ovl_close
    pop iy
    pop ix
    ld hl, 1
    ret

assets_fail_close:
    call ovl_close
assets_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

_netchesszx_piece_set_load:
    ld a, l
    cp 3
    jr c, npsl_index_ok
    xor a
npsl_index_ok:
    ld (asset_set_index), a
    push ix
    push iy

    ld hl, asset_filename
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    rst 8
    defb 0x9a
    jr c, npsl_fail
    ld (ovl_handle), a

    ld a, (asset_set_index)
    add a, a
    ld e, a
    ld d, 0
    ld hl, piece_set_offsets
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld bc, 0
    ld a, (ovl_handle)
    ld ix, 0
    ld l, 0
    rst 8
    defb 0x9f
    jr c, npsl_fail_close

    ld a, (ovl_handle)
    ld ix, asset_load_addr + asset_piece_offset
    ld bc, piece_sprite_set_size
    rst 8
    defb 0x9d
    jr c, npsl_fail_close
    ld a, b
    cp piece_sprite_set_size_hi
    jr nz, npsl_fail_close
    ld a, c
    cp piece_sprite_set_size_lo
    jr nz, npsl_fail_close

    call ovl_close
    pop iy
    pop ix
    ld hl, 1
    ret
npsl_fail_close:
    call ovl_close
npsl_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

piece_set_offsets:
    DW asset_piece_offset
    DW piece_set_extra_offset
    DW piece_set_extra_offset + piece_sprite_set_size

_spectrum_assets_fatal:
    di
    ld a, 2
    out (254), a
    ld hl, assets_fatal_bitmap
    ld de, 0x4000
    ld b, 8
assets_fatal_row:
    push bc
    ld b, 4
assets_fatal_col:
    ld a, (hl)
    ld (de), a
    inc hl
    inc e
    djnz assets_fatal_col
    ld a, e
    sub 4
    ld e, a
    inc d
    pop bc
    djnz assets_fatal_row
    ld hl, 0x5800
    ld a, 0x42
    ld b, 4
assets_fatal_attr:
    ld (hl), a
    inc hl
    djnz assets_fatal_attr
assets_fatal_halt:
    jr assets_fatal_halt

_spectrum_render_about:
    push ix
    push iy
    ld hl, asset_filename
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    rst 8
    defb 0x9a
    jp c, about_fail
    ld (ovl_handle), a

    ld a, (ovl_handle)
    ld de, about_board_offset
    ld bc, 0
    ld ix, 0
    ld l, 0
    rst 8
    defb 0x9f
    jp c, about_fail_close

    xor a
    ld (ovl_cache_ready), a
    ld a, ovl_invalid_id
    ld (_spectrum_overlay_loaded_id), a
    xor a
    ld (about_row), a

about_pixel_row:
    ld a, (about_row)
    cp about_board_char_rows
    jr nc, about_attr_start
    ld bc, about_board_block_bytes
    call about_read_scratch
    ld hl, about_scratch
    ld a, (about_row)
    add a, a
    add a, a
    add a, a
    ld (about_scan), a
    ld b, about_board_block_rows
about_pixel_scan:
    push bc
    push hl
    ld a, (about_scan)
    call about_screen_addr
    ex de, hl
    pop hl
    ld bc, about_board_width
    ldir
    ld a, (about_scan)
    inc a
    ld (about_scan), a
    pop bc
    djnz about_pixel_scan
    ld hl, about_row
    inc (hl)
    jr about_pixel_row

about_attr_start:
    xor a
    ld (about_row), a

about_attr_row:
    ld a, (about_row)
    cp about_board_attr_rows
    jr nc, about_ok
    ld bc, about_board_block_bytes
    ld a, (about_row)
    cp about_board_attr_tail_start
    jr c, about_attr_read
    ld bc, about_board_attr_tail_bytes
    ld a, about_board_attr_tail_rows
    jr about_attr_rows_ready
about_attr_read:
    ld a, about_board_block_rows
about_attr_rows_ready:
    ld (about_block_rows), a
    call about_read_scratch
    ld hl, about_scratch
    ld a, (about_block_rows)
    ld b, a
about_attr_copy:
    push bc
    push hl
    ld a, (about_row)
    call about_attr_addr
    ex de, hl
    pop hl
    ld bc, about_board_width
    ldir
    ld a, (about_row)
    inc a
    ld (about_row), a
    pop bc
    djnz about_attr_copy
    jr about_attr_row

about_read_scratch:
    push bc
    ld ix, about_scratch
    ld a, (ovl_handle)
    rst 8
    defb 0x9d
    jr c, about_read_fail_stacked
    pop de
    ld a, b
    cp d
    jr nz, about_read_fail
    ld a, c
    cp e
    jr nz, about_read_fail
    ret

about_read_fail_stacked:
    pop de
about_read_fail:
    pop hl
    jr about_fail_close

about_ok:
    call ovl_close
    pop iy
    pop ix
    ld hl, 1
    ret

about_fail_close:
    call ovl_close
about_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

about_screen_addr:
    add a, about_board_top_y
    ld b, a
    and 0x07
    add a, 0x40
    ld h, a
    ld a, b
    and 0xc0
    srl a
    srl a
    srl a
    add a, h
    ld h, a
    ld a, b
    and 0x38
    add a, a
    add a, a
    ld l, a
    ret

about_attr_addr:
    add a, about_board_attr_top
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld de, 0x5800
    add hl, de
    ret

assets_fatal_bitmap:
    DEFB 0xf8,0x38,0xfe,0x7c
    DEFB 0x84,0x44,0x10,0x82
    DEFB 0x82,0x82,0x10,0x02
    DEFB 0x82,0xfe,0x10,0x0c
    DEFB 0x82,0x82,0x10,0x10
    DEFB 0x84,0x82,0x10,0x00
    DEFB 0xf8,0x82,0x10,0x10
    DEFB 0x00,0x00,0x00,0x00

ovl_ensure_loaded:
    ld a, (ovl_cache_ready)
    or a
    jr z, ovl_load
    ld a, (_spectrum_overlay_loaded_id)
    ld hl, ovl_id
    cp (hl)
    jr nz, ovl_load
    xor a
    ret

ovl_load:
    xor a
    ld (ovl_cache_ready), a

    ld hl, ovl_filename
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    rst 8
    defb 0x9a
    jr c, ovl_load_fail
    ld (ovl_handle), a

    ld a, (ovl_id)
    or a
    jr z, ovl_read
    call ovl_seek_block
    jr c, ovl_load_fail_close

ovl_read:
    ld a, (ovl_handle)
    ld ix, _overlay_code_slot
    ld bc, 2048
    rst 8
    defb 0x9d
    jr c, ovl_load_fail_close

    ld a, b
    cp 8
    jr nz, ovl_load_fail_close
    ld a, c
    or a
    jr nz, ovl_load_fail_close

    call ovl_close

    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
    xor a
    ret

ovl_load_fail_close:
    call ovl_close

ovl_load_fail:
    xor a
    ld (ovl_cache_ready), a
    ld a, ovl_invalid_id
    ld (_spectrum_overlay_loaded_id), a
    scf
    ret

ovl_call_loaded:
    ld a, (ovl_entry_id)
    ld hl, _overlay_code_slot
    cp (hl)
    jr nc, ovl_fail
    add a, a
    jr c, ovl_fail
    ld e, a
    ld d, 0
    ld hl, _overlay_code_slot + 2
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)

    push de
    ex de, hl
    ld de, _overlay_code_slot
    or a
    sbc hl, de
    jr c, ovl_bad_entry
    ld de, 2048
    or a
    sbc hl, de
    jr nc, ovl_bad_entry
    pop de
    ld hl, _spectrum_overlay_context
    ex de, hl
    pop iy
    pop ix
    ; Overlay execution is framed under DI. Overlay code must return with IX/IY
    ; restored and must not be called from a caller-owned DI critical section:
    ; ovl_return re-enables ROM IM1 for normal app flow.
    ld bc, ovl_return
    push bc
    di
    jp (hl)

ovl_return:
    ei
    ret

ovl_bad_entry:
    pop de
    jr ovl_fail

ovl_fail:
    xor a
    ld (ovl_cache_ready), a
    pop iy
    pop ix
    ld h, 0
    ld l, 0
    ret

ovl_seek_block:
    add a, a
    add a, a
    add a, a
    ld d, a
    ld e, 0
    ld bc, 0
    ld a, (ovl_handle)
    ld ix, 0
    ld l, 0
    rst 8
    defb 0x9f
    ret

ovl_close:
    ld a, (ovl_handle)
    rst 8
    defb 0x9b
    ret

ovl_filename:
    DEFM "SHATRANJ.OVL"
    DEFB 0

asset_filename:
    DEFM "SHATRANJ.DAT"
    DEFB 0
