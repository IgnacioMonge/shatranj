SECTION code_user

; overlay_loader_next.asm
;
; No-graphical Next overlay/assets loader. Same PUBLIC ABI as the esxDOS
; loader (asm/esxdos/overlay_loader.asm) but reads overlays, the DAT asset
; block and piece sets from a ZX0-compressed .nex bundle.
; At boot 30 raw 8K pages are expanded into banks 16-30. The immutable
; 0x7000..0x7fff resident window is then copied into the upper half of each of
; the seventeen 4K overlay pages; slot 3 selects them directly thereafter.
;
; Screen is asm/spectrum/screen.asm with NETCHESSZX_NEXT_GFX: board squares,
; pieces and move markers are Next hardware sprites. Cold sprite/palette setup
; and About rendering execute from the permanent slot-1 extension page.
; _spectrum_render_about displays the separate full-screen Layer 2 image;
; the ZX ULA About payload and renderer are omitted from this target.

PUBLIC _spectrum_overlay_exec
PUBLIC _spectrum_overlay_exec_cached
PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
PUBLIC _spectrum_render_about
PUBLIC _spectrum_render_about_off
PUBLIC _netchesszx_piece_set_load
PUBLIC _spectrum_overlay_loaded_id
PUBLIC _overlay_code_slot
PUBLIC _overlay_scratch_base
PUBLIC _spectrum_overlay_context
PUBLIC ovl_close_overlay_file
PUBLIC nextreg_read
PUBLIC nextreg_write
PUBLIC next_extension_restore
PUBLIC asset_load_size
PUBLIC asset_set_index
PUBLIC next_copy_bundle

EXTERN _spectrum_uart_background_pump
EXTERN next_graphics_bank_init
EXTERN next_graphics_bank_set
EXTERN next_graphics_bank_about

_spectrum_overlay_context EQU 0x5FE0
_overlay_code_slot EQU 0x6000
_overlay_scratch_base EQU 0x3C2B
ovl_invalid_id EQU 0xff
INCLUDE "overlay_atlas_table.asm"
INCLUDE "asm/next/extension_bank_layout.asm"

asset_load_size       EQU 1196
about_board_size      EQU 0

; Next MMU banking. Compressed NEX banks 8-11 map as pages 16-23. The raw
; fifteen-bank bundle is rebuilt into banks 16-30 (pages 32-61).
next_mmu_slot0        EQU 0x50
next_mmu_slot1        EQU 0x51
next_mmu_slot2        EQU 0x52
next_mmu_slot3        EQU 0x53
next_compressed_page_base EQU 16
next_bundle_page_base EQU 32
next_overlay_page_base EQU 44
next_overlay_page_count EQU 17
next_bundle_raw_bank_base EQU 16
next_bundle_page_count EQU 30
next_bundle_header_size EQU 8 + (next_bundle_page_count * 2)
next_bundle_window    EQU 0x0000
next_boot_scratch_page EQU 11

SECTION bss_user

ovl_entry_id:           DEFS 1
ovl_id:                 DEFS 1
ovl_cache_ready:        DEFS 1
asset_set_index:        DEFS 1
_spectrum_overlay_loaded_id: DEFS 1
ovl_load_size:          DEFS 2
next_saved_mmu1:        DEFS 1
next_saved_mmu0:        DEFS 1
next_saved_mmu2:        DEFS 1
next_saved_mmu3:        DEFS 1
next_copy_page:         DEFS 1
next_expand_left:       DEFS 1

SECTION code_user

; SDCC/IY: two uint8 args packed into one stack word:
;   SP+2 = ovl_id, SP+3 = entry_id.
_spectrum_overlay_exec:
    xor a
    ld (ovl_cache_ready), a

_spectrum_overlay_exec_cached:
    ld hl, 2
    add hl, sp
    ld a, (hl)
    inc hl
    ld b, (hl)
ovl_args_canonical:
    ld (ovl_id), a
    ld a, b
    ld (ovl_entry_id), a

    push ix
    push iy

    call ovl_ensure_loaded
    jp c, ovl_fail
    jp ovl_call_loaded

_spectrum_assets_load:
    push ix
    push iy
    call next_expand_bundle
    jr c, next_assets_load_fail
    ld hl, next_bundle_dat_offset
    ld de, asset_load_addr
    ld bc, asset_load_size
    call next_copy_bundle_ei
    ld hl, next_graphics_bank_init
    call next_graphics_bank_call
    pop iy
    pop ix
    ld hl, 1
    ret
next_assets_load_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

; fastcall: piece-set index in L. Copies the 384-byte set from the bundle
; (DAT piece-set region) to the asset area right after the startup block.
_netchesszx_piece_set_load:
    ld a, l
    cp 3
    jr c, npsl_index_ok
    xor a
npsl_index_ok:
    ld (asset_set_index), a
    ld hl, next_graphics_bank_set
    jp next_graphics_bank_call

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

; Show the About screen: full-screen Layer 2 image from expanded VRAM. The
; palette is staged in the permanent slot-1 page and written as 9-bit pairs; the
; pixels are displayed in place from the bundle banks (no copy).
_spectrum_render_about:
    ld hl, next_graphics_bank_about
    call next_graphics_bank_call
    ld bc, layer2_port
    ld a, 2
    out (c), a
    ret

; Hide the About Layer 2 screen (called when the board UI is restored).
_spectrum_render_about_off:
    ld bc, layer2_port
    xor a
    out (c), a
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
    ret

ovl_load:
    xor a
    ld (ovl_cache_ready), a

    call ovl_select_atlas_entry
    jp c, ovl_load_fail

    ld a, (ovl_id)
    add a, next_overlay_page_base
    call next_map_slot3

    ld a, (ovl_id)
    ld (_spectrum_overlay_loaded_id), a
    ld a, 1
    ld (ovl_cache_ready), a
    ret

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
    ; C fastcall entries need the context in HL; native ASM entries use DE.
    ; Stack the target so both registers can carry the same context pointer.
    ld bc, ovl_return
    push bc
    push de
    ld de, _spectrum_overlay_context
    ld h, d
    ld l, e
    ; Overlay entry starts under DI; ovl_return re-enables IM1.
    di
    ret

ovl_return:
    ; RST 8/DivMMC may have owned slots 0+1 while the overlay ran. Restore the
    ; extension atomically before returning to a possible slot-1 caller.
    DEFB 0xed, 0x91, next_mmu_slot1, next_extension_page
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
    ld hl, 0
    ret

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
    ld (ovl_load_size), de
    ld a, d
    or e
    jr z, ovl_select_bad
    ld a, d
    cp 16
    jr c, ovl_select_size_ok
    jr nz, ovl_select_bad
    ld a, e
    or a
    jr nz, ovl_select_bad
ovl_select_size_ok:
    xor a
    ret
ovl_select_bad:
    scf
    ret

; In banking mode there is no .OVL file handle to close. Resident/overlays
; still call this symbol; keep it as a safe no-op so link/semantics match.
ovl_close_overlay_file:
    ret

; ---- Next cold graphics services ----

; Graphics setup uses slot 0 as a temporary bundle window and must not be
; interrupted. Its code is resident, so no executable slot is displaced.
next_graphics_bank_call:
    di
    ld de, next_graphics_bank_return
    push de
    jp (hl)

next_graphics_bank_return:
    ei
    ret

; Z80N NEXTREG n,n is atomic and preserves registers, flags and IFF. Resident
; screen stubs call this after any firmware/Layer-2 activity before jumping to
; the slot-1 extension.
next_extension_restore:
    DEFB 0xed, 0x91, next_mmu_slot1, next_extension_page
    ret

nextreg_write:
    ld bc, nextreg_select
    out (c), a
    ld bc, nextreg_data
    ld a, e
    out (c), a
    ret

; ---- Bundle copy primitive (MMU slot 0 paging) ----
; Input: HL = bundle source offset, DE = Z80 destination, BC = byte count.
; Output: CF clear always (copy cannot fail). The local EI entry is for normal
; loader flow; next_copy_bundle is the DI-preserving cold graphics-bank entry.
next_copy_bundle_ei:
    ld a, b
    or c
    ret z
    scf
    jr next_copy_begin

next_copy_bundle:
    ld a, b
    or c
    ret z
    or a
next_copy_begin:
    push af
    di

    push bc
    push de
    push hl
    ld a, next_mmu_slot0
    call nextreg_read
    ld (next_saved_mmu0), a
    pop hl
    pop de
    pop bc

    ld a, h
    and 0xe0
    rlca
    rlca
    rlca
    add a, next_bundle_page_base
    ld (next_copy_page), a
    call next_map_copy_page

    ld a, h
    and 0x1f
    add a, next_bundle_window / 256
    ld h, a

next_copy_loop:
    ldi
    jp po, next_copy_done
    ld a, c
    and 0x1f
    call z, next_copy_pump
    ld a, h
    cp (next_bundle_window / 256) + 0x20
    jr nz, next_copy_loop
    ld hl, next_bundle_window
    ld a, (next_copy_page)
    inc a
    ld (next_copy_page), a
    call next_map_copy_page
    jr next_copy_loop

next_copy_done:
    push bc
    push de
    push hl
    ld a, (next_saved_mmu0)
    call next_map_slot0
    pop hl
    pop de
    pop bc
    pop af
    jr nc, next_copy_keep_di
    ei
next_copy_keep_di:
    xor a
    ret

; Keep polling UART starvation below 32 copied bytes. The resident pump and
; its full call closure do not use ROM while slot 0 is the bundle window.
next_copy_pump:
    push bc
    push de
    push hl
    push ix
    push iy
    call _spectrum_uart_background_pump
    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    ret

next_map_copy_page:
    push bc
    push de
    push hl
    call next_map_slot0
    pop hl
    pop de
    pop bc
    ret

next_map_slot1:
    ld e, a
    ld a, next_mmu_slot1
    jp nextreg_write

nextreg_read:
    ld bc, nextreg_select
    out (c), a
    ld bc, nextreg_data
    in a, (c)
    ret

; ---- Boot expansion of the compressed NEX bundle ----
; Directory in compressed bank 8:
;   0..3 "NXZ0", 4 version=1, 5 page shift=13, 6 page count=30,
;   7 raw 16K bank base=16, then 30 little-endian stream offsets.
; Each classic ZX0 stream expands to exactly 8K and never crosses a compressed
; 16K bank. Source occupies MMU slots 0+1; destination uses slot 2. The
; directory is staged in the lower half of the original slot-3 page. Its upper
; half still contains the 0x7000..0x7fff resident mirror, so boot code remains
; executable while that page is selected.
; This boot-only routine owns its DI/EI window.
next_expand_bundle:
    di

    ld a, next_mmu_slot0
    call nextreg_read
    ld (next_saved_mmu0), a
    ld a, next_mmu_slot1
    call nextreg_read
    ld (next_saved_mmu1), a
    ld a, next_mmu_slot2
    call nextreg_read
    ld (next_saved_mmu2), a
    ld a, next_mmu_slot3
    call nextreg_read
    ld (next_saved_mmu3), a
    ld a, next_boot_scratch_page
    call next_map_slot3

    ld a, next_compressed_page_base
    call next_map_compressed_bank
    ld hl, 0
    ld de, 0x6000
    ld bc, next_bundle_header_size
    ldir

    ld hl, 0x6000
    ld a, (hl)
    cp 0x4e
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp 0x58
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp 0x5a
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp 0x30
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp 1
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp 13
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp next_bundle_page_count
    jr nz, next_expand_bad
    inc hl
    ld a, (hl)
    cp next_bundle_raw_bank_base
    jr nz, next_expand_bad

    ld ix, 0x6000 + 8
    ld a, next_bundle_page_base
    ld (next_copy_page), a
    ld a, next_bundle_page_count
    ld (next_expand_left), a
next_expand_loop:
    ld l, (ix+0)
    ld h, (ix+1)
    ld a, h
    and 0xc0
    rlca
    rlca
    add a, a
    add a, next_compressed_page_base
    call next_map_compressed_bank
    ld a, h
    and 0x3f
    ld h, a

    ld a, (next_copy_page)
    call next_map_slot2
    ld de, 0x4000
    call next_dzx0_standard
    ld a, d
    cp 0x60
    jr nz, next_expand_bad
    ld a, e
    or a
    jr nz, next_expand_bad

    inc ix
    inc ix
    ld a, (next_copy_page)
    inc a
    ld (next_copy_page), a
    ld a, (next_expand_left)
    dec a
    ld (next_expand_left), a
    jr nz, next_expand_loop
    call next_install_overlay_mirrors
    jr next_expand_restore

next_expand_bad:
    ld a, 1
    ld (next_expand_left), a
next_expand_restore:
    ld a, (next_saved_mmu2)
    call next_map_slot2
    ld a, (next_expand_left)
    or a
    jr nz, next_expand_restore_slot1
    ld a, next_extension_page
    jr next_expand_map_slot1
next_expand_restore_slot1:
    ld a, (next_saved_mmu1)
next_expand_map_slot1:
    call next_map_slot1
    ld a, (next_saved_mmu3)
    call next_map_slot3
    ld a, (next_saved_mmu0)
    call next_map_slot0
    ei
    ld a, (next_expand_left)
    or a
    ret z
    scf
    ret

; Reuse the first compressed page after expansion as a 4K staging page. The
; source mirror remains mapped in slot 3 until every runtime overlay page has
; received its copy. This runs once at boot under DI.
next_install_overlay_mirrors:
    ld a, next_compressed_page_base
    call next_map_slot2
    ld hl, 0x7000
    ld de, 0x4000
    ld bc, 0x1000
    ldir

    ld a, next_overlay_page_base
    ld b, next_overlay_page_count
next_install_overlay_mirrors_loop:
    push bc
    call next_map_slot3
    inc a
    ld hl, 0x4000
    ld de, 0x7000
    ld bc, 0x1000
    ldir
    pop bc
    djnz next_install_overlay_mirrors_loop
    ret

; A = first MMU page of a compressed 16K bank. Maps both source slots.
next_map_compressed_bank:
    push af
    call next_map_slot0
    pop af
    inc a
    jp next_map_slot1

next_map_slot0:
    ld e, a
    ld a, next_mmu_slot0
    jp nextreg_write

next_map_slot2:
    ld e, a
    ld a, next_mmu_slot2
    jp nextreg_write

next_map_slot3:
    ld e, a
    ld a, next_mmu_slot3
    jp nextreg_write

; Classic ZX0 standard decoder by Einar Saukas (69 bytes). HL=source,
; DE=destination. Uses main registers only and preserves IX/IY.
next_dzx0_standard:
    ld bc, 0xffff
    push bc
    inc bc
    ld a, 0x80
next_dzx0_literals:
    call next_dzx0_elias
    ldir
    add a, a
    jr c, next_dzx0_new_offset
    call next_dzx0_elias
next_dzx0_copy:
    ex (sp), hl
    push hl
    add hl, de
    ldir
    pop hl
    ex (sp), hl
    add a, a
    jr nc, next_dzx0_literals
next_dzx0_new_offset:
    call next_dzx0_elias
    ex af, af'
    pop af
    xor a
    sub c
    ret z
    ld b, a
    ex af, af'
    ld c, (hl)
    inc hl
    rr b
    rr c
    push bc
    ld bc, 1
    call nc, next_dzx0_elias_backtrack
    inc bc
    jr next_dzx0_copy
next_dzx0_elias:
    inc c
next_dzx0_elias_loop:
    add a, a
    jr nz, next_dzx0_elias_skip
    ld a, (hl)
    inc hl
    rla
next_dzx0_elias_skip:
    ret c
next_dzx0_elias_backtrack:
    add a, a
    rl c
    rl b
    jr next_dzx0_elias_loop
