SECTION code_user

; Next-only graphics services run from resident memory. The wrapper enters
; under DI; bundle reads temporarily use slot 0 and restore its exact previous
; mapping before returning here.

IFNDEF NETCHESSZX_NEXT_EXTENSION
PUBLIC next_graphics_bank_init
PUBLIC next_graphics_bank_set
PUBLIC next_graphics_bank_about
ENDIF

EXTERN _spectrum_next_sprites_hide_all
EXTERN asset_load_size
EXTERN asset_set_index
EXTERN next_copy_bundle
EXTERN nextreg_read
EXTERN nextreg_write

INCLUDE "asm/next/extension_bank_layout.asm"

IFNDEF NETCHESSZX_NEXT_EXTENSION
next_graphics_bank_init:
    jp ngb_init
next_graphics_bank_set:
    jp ngb_set
next_graphics_bank_about:
    jp ngb_about
ENDIF

ngb_init:
    call ngb_sprite_system_init
    call ngb_sprite_upload_set_current
    jp ngb_sprite_upload_common

; asset_set_index was normalized and stored by the resident ABI wrapper.
ngb_set:
    ld a, (asset_set_index)
    add a, a
    ld e, a
    ld d, 0
    ld hl, ngb_piece_set_offsets
    add hl, de
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    ld bc, next_bundle_dat_offset
    add hl, bc
    ld de, asset_load_addr + asset_piece_offset
    ld bc, piece_sprite_set_size
    push ix
    push iy
    call next_copy_bundle
    call ngb_sprite_upload_set_current
ngb_success:
    pop iy
    pop ix
    ld hl, 1
    ret

ngb_piece_set_offsets:
    DW asset_piece_offset
    DW asset_load_size
    DW asset_load_size + piece_sprite_set_size

ngb_about:
    push ix
    push iy
    call _spectrum_next_sprites_hide_all
    ld hl, next_about_pal_offset
    ld de, next_palette_stage
    ld bc, 512
    call next_copy_bundle
    ld a, nextreg_palette_control
    ld e, 0x10
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 0
    call nextreg_write
    ld hl, next_palette_stage
    ld d, 0
    ld bc, nextreg_select
    ld a, nextreg_palette_value_9
    out (c), a
    ld bc, nextreg_data
ngb_about_palette_loop:
    ld a, (hl)
    out (c), a
    inc hl
    ld a, (hl)
    out (c), a
    inc hl
    dec d
    jr nz, ngb_about_palette_loop
    ld a, nextreg_layer2_bank
    ld e, next_about_bank
    call nextreg_write
    jp ngb_success

ngb_sprite_system_init:
    ld a, nextreg_sprite_transparency_index
    ld e, next_sprite_transparency
    call nextreg_write
    ld a, nextreg_palette_control
    ld e, 0x20
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 0
    call nextreg_write
    ld hl, next_sprite_pal_offset
    ld de, next_palette_stage
    ld bc, next_sprite_palette_size * 2 + next_ula_standard_palette_size
    call next_copy_bundle
    ld hl, next_palette_stage
    ld d, next_sprite_palette_size
    call ngb_palette_upload_pairs
    ld a, nextreg_palette_control
    ld e, 0
    call nextreg_write
    ld a, nextreg_palette_index
    ld e, 192
    call nextreg_write
    ld d, next_ula_standard_palette_size / 2
    call ngb_palette_upload_pairs
    ; Groups 0/1 mirror classic ULA colours; groups 2/3 are private board/menu
    ; colours. Keep ULA+ enabled for the whole application so theme changes do
    ; not leave the attribute file interpreted under a stale palette mode.
    ld a, nextreg_ula_control
    call nextreg_read
    or 0x08
    ld e, a
    ld a, nextreg_ula_control
    call nextreg_write
    ld a, nextreg_sprite_layer_system
    call nextreg_read
    or 0x01
    ld e, a
    ld a, nextreg_sprite_layer_system
    call nextreg_write
    jp _spectrum_next_sprites_hide_all

ngb_palette_upload_pairs:
    ld bc, nextreg_select
    ld a, nextreg_palette_value_9
    out (c), a
    ld bc, nextreg_data
ngb_palette_upload_loop:
    ld a, (hl)
    out (c), a
    inc hl
    ld a, (hl)
    out (c), a
    inc hl
    dec d
    jr nz, ngb_palette_upload_loop
    ret

ngb_sprite_upload_set_current:
    ld a, (asset_set_index)
    ld h, a
    ld l, 0
    add hl, hl
    add hl, hl
    ld d, h
    ld e, l
    add hl, hl
    add hl, de
    ld de, next_sprite_offset
    add hl, de
    xor a
    ld d, next_sprite_pattern_count
    jr ngb_sprite_upload

ngb_sprite_upload_common:
    ld hl, next_common_sprite_offset
    ld a, next_common_sprite_pattern_base
    ld d, next_board_sprite_pattern_count
    call ngb_sprite_upload
    ld hl, next_marker_sprite_offset
    ld a, next_marker_sprite_pattern_base
    ld d, next_marker_sprite_pattern_count

ngb_sprite_upload:
    ld bc, sprite_slot_port
    out (c), a
ngb_sprite_pattern_loop:
    push de
    push hl
    ld de, next_sprite_stage
    ld bc, 256
    call next_copy_bundle
    pop hl
    inc h
    push hl
    ld hl, next_sprite_stage
    ld bc, sprite_pattern_port
    otir
    pop hl
    pop de
    dec d
    jr nz, ngb_sprite_pattern_loop
    ret
