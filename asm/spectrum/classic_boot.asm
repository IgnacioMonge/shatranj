; Classic-only cold boot code.  The TAP loads this capsule into the overlay
; slot before the resident image; CONFIG may overwrite it after main starts.

SECTION code_boot
ORG 0x6800

INCLUDE "classic_boot_defs.asm"

PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
PUBLIC _spectrum_frame_clock_init
PUBLIC _spectrum_app_tick_init

_spectrum_assets_load:
    push ix
    push iy
    call ovl_close_overlay_file
    ld hl, asset_filename
    call ovl_esx_open_readonly
    jr c, boot_assets_fail
    ld (ovl_handle), a

    ld a, (ovl_handle)
    ld ix, asset_load_addr
    ld bc, asset_load_size
    call ovl_read_chunked
    jr c, boot_assets_fail_close

    call ovl_close
    call _spectrum_frame_clock_init
    call _spectrum_app_tick_init
    jp assets_success
boot_assets_fail_close:
    jp assets_fail_close
boot_assets_fail:
    jp assets_fail

_spectrum_frame_clock_init:
    di
    ld hl, classic_boot_frame_clock_isr
    ld (frame_clock_vector_pointer), hl
    ld a, frame_clock_vector_page
    ld i, a
    im 2
    ei
    ret

_spectrum_app_tick_init:
    ld hl, (frame_clock_frames)
    ld (_last_frame_count), hl
    ret

ASSERT _spectrum_frame_clock_init - _spectrum_assets_load = 51
ASSERT _spectrum_app_tick_init - _spectrum_assets_load = 66

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

assets_fatal_bitmap:
    DEFB 0xf8,0x38,0xfe,0x7c
    DEFB 0x84,0x44,0x10,0x82
    DEFB 0x82,0x82,0x10,0x02
    DEFB 0x82,0xfe,0x10,0x0c
    DEFB 0x82,0x82,0x10,0x10
    DEFB 0x84,0x82,0x10,0x00
    DEFB 0xf8,0x82,0x10,0x10
    DEFB 0x00,0x00,0x00,0x00

ASSERT _spectrum_assets_fatal - _spectrum_assets_load = 73
ASSERT $ - _spectrum_assets_load <= 2048
