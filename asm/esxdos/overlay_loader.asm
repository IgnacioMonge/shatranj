SECTION code_user

; Next ROM IM1 requires IY=$5C3A while interrupts are live. Classic now uses a
; resident IM2 frame ISR, but shared live-interrupt code keeps the same IY
; contract. The esxDOS-compatible API preserves IY; every RST 8 site also saves
; it defensively before returning to resident C or pumping the UART.
;
; SDCC/IY: two uint8 args are packed into one stack word:
;   SP+2 = ovl_id, SP+3 = entry_id.

PUBLIC _spectrum_overlay_exec
PUBLIC _spectrum_overlay_exec_cached
IFNDEF NETCHESSZX_CLASSIC_BOOT
PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
ENDIF
PUBLIC _spectrum_render_about
PUBLIC _spectrum_render_game_stats
PUBLIC _spectrum_render_chlog
PUBLIC _netchesszx_piece_set_load
PUBLIC _spectrum_overlay_loaded_id
PUBLIC _overlay_code_slot
PUBLIC _overlay_scratch_base
IFDEF NETCHESSZX_SPECTRANEXT
PUBLIC _spxn_overlay_page_table
PUBLIC _spxn_overlay_loader_start
PUBLIC _spxn_overlay_loader_end
PUBLIC _spectrum_spxn_frame_wait
PUBLIC _spxn_rom_held
PUBLIC _spxn_dat_id
ENDIF
PUBLIC _spectrum_overlay_context
PUBLIC ovl_close_overlay_file
PUBLIC ovl_read_chunked

EXTERN _spectrum_uart_background_pump

IFDEF NETCHESSZX_SPECTRANEXT
; The external Spectranext driver and compatibility adapter own the XFS
; handles. Keep these declarations local to the consumer seam so Classic
; remains a pure esxDOS/RST 8 path.
EXTERN _spxn_detect
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _spxn_xfs_open_here
EXTERN _spxn_xfs_select_slot0
EXTERN _spxn_dat_bind
EXTERN _spxn_dat_release
EXTERN _spxn_xfs_firmware_probe
EXTERN _spxn_xfs_init
EXTERN _spectrum_input_frame_tick
EXTERN asm_z80_push_di
EXTERN asm_z80_pop_ei
ENDIF

_spectrum_overlay_context EQU 0x5FE0
_overlay_scratch_base EQU 0x672B
IFDEF NETCHESSZX_SPECTRANEXT
_overlay_code_slot EQU 0x2000
_spxn_overlay_page_table EQU 0x6800
spxn_pagein EQU 0x3FF9
spxn_pageout EQU 0x007C
spxn_set_page_b EQU 0x3E36
spxn_push_page_b EQU 0x3E87
spxn_pop_page_b EQU 0x3E8A
spxn_reserve_page EQU 0x3E9F
spxn_free_page EQU 0x3EA2
spxn_overlay_owner EQU 0x4E
spxn_overlay_stage EQU 0x6820
spxn_overlay_chunk EQU 1502
spxn_overlay_chunk_size EQU spxn_overlay_stage + spxn_overlay_chunk
ELSE
_overlay_code_slot EQU 0x6800
ENDIF
ovl_invalid_id EQU 0xff
INCLUDE "overlay_atlas_table.asm"
asset_load_addr EQU 0x6001
asset_load_size EQU 1273
asset_piece_offset EQU 889
piece_sprite_set_size EQU 384
about_board_size EQU 5472
IFDEF NETCHESSZX_SPECTRANEXT
about_board_offset EQU asset_load_size
piece_set_extra_offset EQU about_board_offset + about_board_size
ELSE
piece_set_extra_offset EQU asset_load_size
about_board_offset EQU piece_set_extra_offset + (piece_sprite_set_size * 2)
ENDIF
about_ovl_id EQU 12
about_render_entry EQU 0
; The changelog text page is entry 2 of the status (MENU_LOGIC) overlay.
chlog_ovl_id EQU 7
ovl_atlas_fingerprint_offset EQU 96
ovl_atlas_fingerprint_size EQU 4

SECTION bss_user
IFDEF NETCHESSZX_SPECTRANEXT
ORG 0x5B00
ENDIF

ovl_handle:   DEFS 1
ovl_entry_id: DEFS 1
ovl_id:       DEFS 1
ovl_cache_ready: DEFS 1
ovl_file_open: DEFS 1
IFDEF NETCHESSZX_SPECTRANEXT
ovl_xfs_ready: DEFS 1
ovl_atlas_file_fingerprint: DEFS ovl_atlas_fingerprint_size
_spxn_rom_held EQU ovl_atlas_file_fingerprint + 3
ELSE
; Reject recursive dispatch before cache/id mutation or overwriting the slot.
ovl_call_active: DEFS 1
ENDIF
asset_set_index: DEFS 1
IFDEF NETCHESSZX_SPECTRANEXT
_spectrum_overlay_loaded_id: DEFS 1
ovl_load_size: DEFS 2
; Retained SHATRANJ.DAT descriptor (fd+1, 0 = none). It must stay inside the
; 0x50 bytes _spectrum_assets_load clears and outside the XFS state/scratch;
; the byte comes from the write-only ovl_atlas_ready this change deleted.
_spxn_dat_id: DEFS 1
ELSE
ovl_load_size: DEFS 2
_spectrum_overlay_loaded_id: DEFS 1

; Only the IM2 pointer has a fixed address. frame_clock_init writes both
; bytes before enabling IM2; ordinary loader state stays in the zeroed TAP.
SECTION bss_frame_clock_vector
ORG 0xfcff
PUBLIC _spectrum_frame_clock_vector_reservation
_spectrum_frame_clock_vector_reservation: DEFS 2

; The CRT stack starts at the top of RAM (REGISTER_SP=0), reclaiming the 168
; unused UDG bytes. The two DIRECT receive slots live right above the vector,
; so the linked CODE/DATA/BSS below it gains their 96 bytes. Both are written
; before they are read; nothing relies on TAP zeroing here.
IFDEF NETCHESSZX_HIGH_SCRATCH
SECTION bss_high_scratch
ORG 0xfd01
PUBLIC _direct_rx_payload
PUBLIC _direct_rx_payload2
PUBLIC _direct_rx_payload_end
_direct_rx_payload: DEFS 48
_direct_rx_payload2: DEFS 48
_direct_rx_payload_end:
; Keep the exact MOVES cache below the IM2 vector. Its 22 initialized bytes
; count in the resident payload, paid for by constant-address ULA reductions.
SECTION data_user
INCLUDE "asm/app/player_name_layout.inc"
PUBLIC _spectrum_moves_header_shown
_spectrum_moves_header_shown: DEFS 2 * (PLAYER_NAME_MAX + 1), 0
spectrum_moves_header_shown_end:
ENDIF
SECTION bss_user
ENDIF
IFDEF NETCHESSZX_SPECTRANEXT
; These states never overlap: the fingerprint is dead after boot verification,
; and id/entry are dead while their selected overlay is executing.
ovl_spxn_pages_ready EQU ovl_atlas_file_fingerprint
ovl_spxn_page_count EQU _spectrum_overlay_loaded_id
ovl_spxn_current_page EQU ovl_id
ovl_spxn_return_value EQU ovl_entry_id
ovl_spxn_copy_dst EQU ovl_atlas_file_fingerprint + 1
; The copy destination is dead after preload. Keep activity independent of
; ROM visibility, which frame_wait temporarily drops during an overlay call.
ovl_call_active EQU ovl_atlas_file_fingerprint + 2
ENDIF

SECTION code_user

; Callee ABI: pops its packed uint8 args (C = ovl_id, B = entry_id). A nested
; plain exec leaves the cache flag alone and fails in the cached path below.
_spectrum_overlay_exec:
    ld a, (ovl_call_active)
    or a
    jr nz, _spectrum_overlay_exec_cached
    xor a
    ld (ovl_cache_ready), a

_spectrum_overlay_exec_cached:
    pop hl
    pop bc
    push hl
    ld a, (ovl_call_active)
    or a
    jr nz, ovl_exec_nested_fail
    ld a, c
ovl_args_canonical:
    ld (ovl_id), a
    ld a, b
    ld (ovl_entry_id), a

    jr ovl_exec_after_xfs_helper
ovl_exec_nested_fail:
    ld hl, 0
    ret
IFDEF NETCHESSZX_SPECTRANEXT
; HL=ASCIIZ path. Establish cartridge identity and firmware 1.0 before the
; first XFS open; the external adapter intentionally does not repeat
; detection per operation. ovl_xfs_ready: 0 no cartridge, 1 ready, 2 firmware
; older than 1.0 (the fatal screen names it). Opens on the current mount (no
; SETMOUNTPOINT). Return A=compatibility handle with carry clear, or carry set
; on failure.
ovl_xfs_open_readonly:
    ld a, (ovl_xfs_ready)
    dec a
    jr z, ovl_xfs_open
    push hl
    call _spxn_detect
    xor a
    dec l
    jr nz, ovl_xfs_checked
    ld hl, asset_load_addr      ; reply buffer: the DAT has not loaded yet
    call _spxn_xfs_firmware_probe
    ld a, 1
    bit 0, l
    jr z, ovl_xfs_checked
    inc a
ovl_xfs_checked:
    pop hl
    ld (ovl_xfs_ready), a
    dec a
    jr nz, ovl_xfs_open_fail
ovl_xfs_open:
    call _spxn_xfs_open_here
    ld a, (_esx_handle)
    or a
    ret nz
ovl_xfs_open_fail:
    xor a
    ld (_esx_handle), a
    scf
    ret
ELSE
; HL=ASCIIZ path. Return A=esxDOS handle with carry clear, or carry set.
ovl_esx_open_readonly:
    push hl
    pop ix
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    ret
ENDIF
ovl_exec_after_xfs_helper:

    push ix
    push iy

    call ovl_ensure_loaded
    jp c, ovl_fail
    jp ovl_call_loaded

IFNDEF NETCHESSZX_CLASSIC_BOOT
_spectrum_assets_load:
IFDEF NETCHESSZX_SPECTRANEXT
    ; bss_user is anchored below the TAP CODE block to recover the inactive
    ; printer buffer.  This is the first product call from main, so clear its
    ; complete reserved range before consulting any loader or driver state.
    ld hl, 0x5B00
    ld de, 0x5B01
    ld bc, 0x004F
    xor a
    ld (hl), a
    ldir
    ld (_spxn_rom_held), a
    call _spxn_xfs_init
ENDIF
    push ix
    push iy
    call ovl_close_overlay_file
    ld hl, asset_filename
IFDEF NETCHESSZX_SPECTRANEXT
    ; Launch mount first (direct HTTP(S) start); an installed package lives
    ; on slot 0. The descriptor stays open as the retained resource.
    call ovl_xfs_open_readonly
    jr nc, assets_dat_open
    ld a, (ovl_xfs_ready)
    dec a
    jr nz, assets_fail
    call _spxn_xfs_select_slot0
    ld hl, asset_filename
    call ovl_xfs_open_readonly
    jr c, assets_fail
assets_dat_open:
    ld a, (_esx_handle)
    ld (_spxn_dat_id), a
ELSE
    call ovl_esx_open_readonly
    jr c, assets_fail
ENDIF
    ld (ovl_handle), a

    ld a, (ovl_handle)
    ld ix, asset_load_addr
    ld bc, asset_load_size
    call ovl_read_chunked
    jr c, assets_fail_close

    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
    call ovl_spxn_preload_all
    jr c, assets_fail_release
ENDIF
ENDIF
assets_success:
    pop iy
    pop ix
    ld hl, 1
    ret

assets_fail_close:
    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
assets_fail_release:
    call _spxn_dat_release
ENDIF
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
    call ovl_close_overlay_file

IFNDEF NETCHESSZX_SPECTRANEXT
    ld hl, asset_filename
    call ovl_esx_open_readonly
    jr c, npsl_fail
    ld (ovl_handle), a
ENDIF

    ld a, (asset_set_index)
    add a, a
    ld e, a
    ld d, 0
    ld hl, piece_set_offsets
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
IFDEF NETCHESSZX_SPECTRANEXT
    ex de, hl
    call _spxn_dat_bind         ; borrow the retained DAT and seek
    ld a, (_esx_handle)
    ld (ovl_handle), a
    ld a, l
    or a
    jr z, npsl_fail_close
ELSE
    ld bc, 0
    ld a, (ovl_handle)
    ld ix, 0
    ld l, 0
    push iy
    rst 8
    defb 0x9f
    pop iy
    jr c, npsl_fail_close
ENDIF

    ld a, (ovl_handle)
    ld ix, asset_load_addr + asset_piece_offset
    ld bc, piece_sprite_set_size
    call ovl_read_chunked
    jr c, npsl_fail_close

    call ovl_close
    jp assets_success
npsl_fail_close:
    call ovl_close
npsl_fail:
    jp assets_fail

piece_set_offsets:
    DW asset_piece_offset
    DW piece_set_extra_offset
    DW piece_set_extra_offset + piece_sprite_set_size

IFNDEF NETCHESSZX_CLASSIC_BOOT
_spectrum_assets_fatal:
    di
IFDEF NETCHESSZX_SPECTRANEXT
    call _spxn_dat_release
ENDIF
    ld a, 2
    out (254), a
    ld hl, assets_fatal_bitmap
    ld de, 0x4000
    ld bc, 8 * 256 + 4
IFDEF NETCHESSZX_SPECTRANEXT
    ld a, (ovl_xfs_ready)
    cp 2
    jr nz, assets_fatal_row
    ; Firmware older than 1.0: clear the screen and name the cause in the UI
    ; font's cell rows (scanlines 1..6) instead of DAT?.
    ld hl, 0x4000
    ld de, 0x4001
    ld bc, 6911
    ld (hl), l
    ldir
    ld hl, assets_firmware_bitmap
    ld de, 0x4100
    ld bc, 6 * 256 + 15
ENDIF
assets_fatal_row:
    push bc
    ld b, c
assets_fatal_col:
    ld a, (hl)
    ld (de), a
    inc hl
    inc e
    djnz assets_fatal_col
    ld a, e
    sub c
    ld e, a
    inc d
    pop bc
    djnz assets_fatal_row
    ld hl, 0x5800
    ld a, 0x42
    ld b, c
assets_fatal_attr:
    ld (hl), a
    inc hl
    djnz assets_fatal_attr
assets_fatal_halt:
    jr assets_fatal_halt
ENDIF

_spectrum_render_about:
    ld hl, about_ovl_id + (about_render_entry * 256)
    jr render_about_entry
_spectrum_render_chlog:
    ld hl, chlog_ovl_id + 512
    jr render_about_entry
_spectrum_render_game_stats:
    ld hl, about_ovl_id + 256
render_about_entry:
    push hl
    call _spectrum_overlay_exec
    ret

IFNDEF NETCHESSZX_CLASSIC_BOOT
assets_fatal_bitmap:
    DEFB 0xf8,0x38,0xfe,0x7c
    DEFB 0x84,0x44,0x10,0x82
    DEFB 0x82,0x82,0x10,0x02
    DEFB 0x82,0xfe,0x10,0x0c
    DEFB 0x82,0x82,0x10,0x10
    DEFB 0x84,0x82,0x10,0x00
    DEFB 0xf8,0x82,0x10,0x10
    DEFB 0x00,0x00,0x00,0x00
IFDEF NETCHESSZX_SPECTRANEXT
; "FIRMWARE OUTDATED: SEE README" in the 4x6 UI font (font_packed/font_lut
; of assets/spectrum/ui_runtime_assets.asm), which is not loaded yet.
assets_firmware_bitmap:
    DEFB 0xee,0xca,0xa4,0xce,0x04,0xae,0xc4,0xee,0xc0,0x06,0xee,0x0c,0xe4,0xca,0xe0
    DEFB 0x84,0xae,0xaa,0xa8,0x0a,0xa4,0xaa,0x48,0xa0,0x08,0x88,0x0a,0x8a,0xae,0x80
    DEFB 0xc4,0xae,0xea,0xac,0x0a,0xa4,0xaa,0x4c,0xa8,0x04,0xcc,0x0a,0xca,0xae,0xc0
    DEFB 0x84,0xce,0xee,0xc8,0x0a,0xa4,0xae,0x48,0xa0,0x02,0x88,0x0c,0x8e,0xae,0x80
    DEFB 0x84,0xaa,0xea,0xa8,0x0a,0xa4,0xaa,0x48,0xa0,0x0a,0x88,0x0a,0x8a,0xaa,0x80
    DEFB 0x8e,0xaa,0x4a,0xae,0x04,0xe4,0xca,0x4e,0xc8,0x04,0xee,0x0a,0xea,0xca,0xe0
ENDIF
ENDIF

ovl_ensure_loaded:
IFDEF NETCHESSZX_SPECTRANEXT
    ld a, (ovl_spxn_pages_ready)
    or a
    jr z, ovl_spxn_not_ready
    call ovl_select_atlas_entry
    ret c
    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
    ret
ovl_spxn_not_ready:
    scf
    ret
ELSE
    ld a, (ovl_cache_ready)
    or a
    jr z, ovl_load
    ld a, (_spectrum_overlay_loaded_id)
    ld hl, ovl_id
    cp (hl)
    jr nz, ovl_load
    ret
ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
; One retry for a failed load: the failed attempt closed the file, so the
; second one reopens, seeks and reads again from scratch.
ovl_load:
    call ovl_load_try
    ret nc
ovl_load_try:
    xor a
    ld (ovl_cache_ready), a

    ld a, (ovl_file_open)
    or a
    jr nz, ovl_file_ready
    ld hl, ovl_filename
    call ovl_esx_open_readonly
    jp c, ovl_load_fail
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_file_open), a

ovl_file_ready:
    call ovl_select_atlas_entry
    jp c, ovl_load_fail_close

ovl_read:
    ld a, (ovl_handle)
    ld ix, _overlay_code_slot
    ld bc, (ovl_load_size)
    call ovl_read_chunked
    jr c, ovl_load_fail_close

    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
    ret

ENDIF

IFDEF NETCHESSZX_SPECTRANEXT
; Reject a stale SHATRANJ.OVL before executing any of its offsets.  The build
; fingerprint covers the atlas layout, consumer sources, flags and linked
; cartridge sources, so a partial installation fails closed instead of calling
; an entry from the wrong overlay bytes.
ovl_verify_atlas:
    ld a, (ovl_handle)
    ld ix, _overlay_scratch_base
    ld bc, ovl_atlas_fingerprint_offset + ovl_atlas_fingerprint_size
    call ovl_read_chunked
    jr c, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 0)
    cp ovl_atlas_fingerprint_0
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 1)
    cp ovl_atlas_fingerprint_1
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 2)
    cp ovl_atlas_fingerprint_2
    jr nz, ovl_verify_atlas_bad
    ld a, (_overlay_scratch_base + ovl_atlas_fingerprint_offset + 3)
    cp ovl_atlas_fingerprint_3
    jr nz, ovl_verify_atlas_bad
    or a
    ret
ovl_verify_atlas_bad:
    scf
    ret

SECTION spxn_overlay_backend
ORG 0x6E00
_spxn_overlay_loader_start:

; Verify the packed atlas through the ordinary VFS seam, reserve one cartridge
; SRAM page per payload, then stage XFS reads in Spectrum RAM and copy them
; into Page B. ROM READ rejects destinations in the cart window.
ovl_spxn_preload_all:
    xor a
    ld (ovl_spxn_pages_ready), a
    ld (ovl_spxn_page_count), a
    call ovl_close_overlay_file
    ld hl, ovl_filename
    call ovl_xfs_open_readonly
    jp c, ovl_spxn_preload_fail
    ld a, (_esx_handle)
    ld (ovl_handle), a
    ld a, 1
    ld (ovl_file_open), a
    call ovl_verify_atlas
    jp c, ovl_spxn_preload_fail_close
ovl_spxn_preload_loop:
    ld a, (ovl_spxn_page_count)
    cp ovl_atlas_count
    jp z, ovl_spxn_preload_done
    ld (ovl_id), a
    call ovl_select_atlas_entry
    jp c, ovl_spxn_preload_fail_allocated

    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, spxn_overlay_owner
    call spxn_reserve_page
    jp c, ovl_spxn_reserve_fail_paged
    ld (ovl_spxn_current_page), a
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
    ld a, (ovl_spxn_page_count)
    or a
    jr z, ovl_spxn_page_unique
    ld b, a
    ld hl, _spxn_overlay_page_table
ovl_spxn_page_unique_loop:
    ld a, (ovl_spxn_current_page)
    cp (hl)
    jp z, ovl_spxn_preload_fail_allocated
    inc hl
    djnz ovl_spxn_page_unique_loop
ovl_spxn_page_unique:
    ld a, (ovl_spxn_page_count)
    ld e, a
    ld d, 0
    ld hl, _spxn_overlay_page_table
    add hl, de
    ld a, (ovl_spxn_current_page)
    ld (hl), a
    ld a, e
    inc a
    ld (ovl_spxn_page_count), a

    ld hl, _overlay_code_slot
    ld (ovl_spxn_copy_dst), hl
ovl_spxn_copy_loop:
    ld hl, (ovl_load_size)
    ld a, h
    or l
    jr z, ovl_spxn_preload_loop
    ld bc, spxn_overlay_chunk
    or a
    sbc hl, bc
    jr nc, ovl_spxn_chunk_ready
    add hl, bc
    ld b, h
    ld c, l
ovl_spxn_chunk_ready:
    ld (spxn_overlay_chunk_size), bc
    ld a, (ovl_handle)
    ld ix, spxn_overlay_stage
    call ovl_read_chunked
    jr c, ovl_spxn_preload_fail_allocated

    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, (ovl_spxn_current_page)
    call spxn_push_page_b
    ld bc, (spxn_overlay_chunk_size)
    ld hl, spxn_overlay_stage
    ld de, (ovl_spxn_copy_dst)
    ldir
    ld (ovl_spxn_copy_dst), de
    call spxn_pop_page_b
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei

    ld bc, (spxn_overlay_chunk_size)
    ld hl, (ovl_load_size)
    or a
    sbc hl, bc
    ld (ovl_load_size), hl
    jr ovl_spxn_copy_loop

ovl_spxn_reserve_fail_paged:
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
    jr ovl_spxn_preload_fail_allocated

ovl_spxn_preload_done:
    call ovl_close_overlay_file
    jr c, ovl_spxn_preload_fail_allocated
    xor a
    ld (ovl_call_active), a
    ld a, 1
    ld (ovl_spxn_pages_ready), a
    or a
    ret

ovl_spxn_preload_fail_close:
    call ovl_close_overlay_file
ovl_spxn_preload_fail:
    scf
    ret

ovl_spxn_preload_fail_allocated:
    call ovl_close_overlay_file
    ld a, (ovl_spxn_page_count)
    or a
    jr z, ovl_spxn_preload_freed
    call asm_z80_push_di
    push iy
    call spxn_pagein
    ld a, (ovl_spxn_page_count)
    ld b, a
    ld hl, _spxn_overlay_page_table
ovl_spxn_preload_free_loop:
    ld a, (hl)
    push hl
    push bc
    call spxn_free_page
    pop bc
    pop hl
    inc hl
    djnz ovl_spxn_preload_free_loop
    call spxn_pageout
    pop iy
    call asm_z80_pop_ei
ovl_spxn_preload_freed:
    xor a
    ld (ovl_spxn_page_count), a
    scf
    ret

ovl_spxn_call_loaded:
ovl_spxn_map:
    pop iy
    pop ix
    di
    push ix
    push iy
    call spxn_pagein
    ld a, (ovl_id)
    ld e, a
    ld d, 0
    ld hl, _spxn_overlay_page_table
    add hl, de
    ld a, (hl)
    ld (ovl_spxn_current_page), a
    call spxn_push_page_b
    ld a, 1
    ld (_spxn_rom_held), a
    ld (ovl_call_active), a

    ld a, (ovl_entry_id)
    ld hl, _overlay_code_slot
    cp (hl)
    jr nc, ovl_spxn_bad_entry
    add a, a
    jr c, ovl_spxn_bad_entry
    ld e, a
    ld d, 0
    ld hl, _overlay_code_slot + 1
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    push de
    ex de, hl
    ld de, _overlay_code_slot
    or a
    sbc hl, de
    jr c, ovl_spxn_bad_entry_pop
    ld de, (ovl_load_size)
    or a
    sbc hl, de
    jr nc, ovl_spxn_bad_entry_pop
    pop de
    ld bc, ovl_spxn_return
    push bc
    push de
    ld de, _spectrum_overlay_context
    ld h, d
    ld l, e
    di
    ret

ovl_spxn_bad_entry_pop:
    pop de
ovl_spxn_bad_entry:
    call spxn_pop_page_b
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    pop iy
    xor a
    ld (ovl_call_active), a
    pop ix
    ei
    ld hl, 0
    ret

ovl_spxn_return:
    ld (ovl_spxn_return_value), hl
    di
    call spxn_pop_page_b
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    pop iy
    xor a
    ld (ovl_call_active), a
    pop ix
    ei
    ld hl, (ovl_spxn_return_value)
    ret

; Called by spectrum_frame_wait.  While an overlay owns Page B, expose the
; Spectrum ROM only for HALT/IM1 and the resident input tick, then restore the
; exact overlay page before returning.
_spectrum_spxn_frame_wait:
    push ix
    push iy
    push af
    push bc
    push de
    push hl
    ld a, (_spxn_rom_held)
    or a
    jr z, ovl_spxn_frame_plain
    di
    xor a
    ld (_spxn_rom_held), a
    call spxn_pageout
    ld a, 1
    jr ovl_spxn_frame_marker
ovl_spxn_frame_plain:
    xor a
ovl_spxn_frame_marker:
    push af
    ld iy, 0x5C3A
    ei
    halt
    call _spectrum_input_frame_tick
    pop af
    or a
    jr z, ovl_spxn_frame_done
    di
    call spxn_pagein
    ld a, (ovl_spxn_current_page)
    call spxn_set_page_b
    ld a, 1
    ld (_spxn_rom_held), a
ovl_spxn_frame_done:
    pop hl
    pop de
    pop bc
    pop af
    pop iy
    pop ix
    ret

_spxn_overlay_loader_end:
SECTION code_user
ENDIF

; A=handle, IX=destination, BC=length. Classic reads the whole length with one
; F_READ and drains the polling UART once afterwards: the pump never runs inside
; a synchronous RST 8, where the required CTS flow control holds the ESP, as it
; already does for F_OPEN and F_SYNC. Returns the original length in BC, CF on
; error or short read; IX is not preserved. The handle is mirrored in
; ovl_handle because esxDOS may clobber A.
ovl_read_chunked:
IFDEF NETCHESSZX_SPECTRANEXT
    ld (ovl_handle), a
    ld (_esx_handle), a
    push bc
    push ix
    pop hl
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fread
    pop bc
    ld hl, (_esx_result)
    or a
    sbc hl, bc
    ret z
    scf
    ret
ELSE
    ld (ovl_handle), a
    ld a, b
    or c
    ret z
    ld a, (ovl_handle)
    push bc
    push iy
    rst 8
    defb 0x9d
    pop iy
    pop de
    jr c, ovl_read_fail
    ld h, b
    ld l, c
    sbc hl, de
    jr nz, ovl_read_fail
    push de
    call _spectrum_uart_background_pump
    pop bc
    or a
    ret
ovl_read_fail:
    ld b, d
    ld c, e
    scf
    ret
ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
ovl_load_fail_close:
    call ovl_close_overlay_file

ovl_load_fail:
    xor a
    ld (ovl_cache_ready), a
    ld a, ovl_invalid_id
    ld (_spectrum_overlay_loaded_id), a
    scf
    ret
ENDIF

ovl_call_loaded:
IFDEF NETCHESSZX_SPECTRANEXT
    jp ovl_spxn_call_loaded
ELSE
    ld a, (ovl_entry_id)
    ld hl, _overlay_code_slot
    cp (hl)
    jr nc, ovl_fail
    add a, a
    jr c, ovl_fail
    ld e, a
    ld d, 0
    ld hl, _overlay_code_slot + 1
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
    ld de, (ovl_load_size)
    or a
    sbc hl, de
    jr nc, ovl_bad_entry
    pop de
    pop iy
    pop ix
    ld a, 1
    ld (ovl_call_active), a
    ; C fastcall entries need the context in HL; native ASM entries use DE.
    ; Stack the target so both registers can carry the same context pointer.
    ld bc, ovl_return
    push bc
    push de
    ld de, _spectrum_overlay_context
    ld h, d
    ld l, e
    ; Overlay entry starts under DI and ovl_return enables interrupts.
    ; Overlay callees may re-enable interrupts (for example via frame_wait), so
    ; this is not a whole-overlay DI guarantee.
    di
    ret

ovl_return:
    xor a
    ld (ovl_call_active), a
    ei
    ret

ovl_bad_entry:
    pop de
    jr ovl_fail
ENDIF

ovl_fail:
    xor a
    ld (ovl_cache_ready), a
    jp assets_fail

ovl_select_atlas_entry:
    ld a, (ovl_id)
    cp ovl_atlas_count
    jr nc, ovl_select_bad
    add a, a
    ld e, a
    ld d, 0
    ld hl, ovl_atlas_table
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld h, b
    ld l, c
    or a
    sbc hl, de
    ld (ovl_load_size), hl
    ld a, h
    or l
    jr z, ovl_select_bad
    ld a, h
IFDEF NETCHESSZX_SPECTRANEXT
    cp 16
ELSE
    cp 8
ENDIF
    jr c, ovl_select_size_ok
    jr nz, ovl_select_bad
    ld a, l
    or a
    jr nz, ovl_select_bad
ovl_select_size_ok:
IFDEF NETCHESSZX_SPECTRANEXT
    or a
    ret
ELSE
    ld bc, 0
    ld a, (ovl_handle)
    ld ix, 0
    ld l, 0
    push iy
    rst 8
    defb 0x9f
    pop iy
    ret
ENDIF
ovl_select_bad:
    scf
    ret

ovl_close_overlay_file:
    ld a, (ovl_file_open)
    or a
    ret z
    call ovl_close
IFDEF NETCHESSZX_SPECTRANEXT
    ret c
ENDIF
    ld a, 0
    ld (ovl_file_open), a
    ret

ovl_close:
    ld a, (ovl_handle)
IFDEF NETCHESSZX_SPECTRANEXT
    ld (_esx_handle), a
    call _esx_fclose
    ld a, l
    or a
    ret z
    scf
    ret
ELSE
    push iy
    rst 8
    defb 0x9b
    pop iy
    ret
ENDIF


ovl_filename:
    DEFM "SHATRANJ.OVL"
    DEFB 0

asset_filename:
    DEFM "SHATRANJ.DAT"
    DEFB 0
