; Classic owns its frame interrupt in resident RAM. DivMMC can replace the
; ROM at 0x0038 during esxDOS calls, so ROM IM1 cannot keep FRAMES moving.
; The Spectrum ULA supplies the standard 0xff IM2 vector; its two-byte pointer
; occupies the guarded BSS reservation at 0xfcff..0xfd00, outside the ULA snow
; range. tools/check_lowmem_layout.py verifies the linked reservation address.

SECTION code_user

frame_clock_vector_page EQU 0xfc
frame_clock_vector_offset EQU 0xff
frame_clock_vector_pointer EQU 0xfcff
frame_clock_frames EQU 0x5c78

IFNDEF NETCHESSZX_NEXT_BANKING
IFNDEF NETCHESSZX_SPECTRANEXT

IFNDEF NETCHESSZX_CLASSIC_BOOT
    PUBLIC _spectrum_frame_clock_init

_spectrum_frame_clock_init:
    di
    ld hl, frame_clock_isr
    ld (frame_clock_vector_pointer), hl
    ld a, frame_clock_vector_page
    ld i, a
    im 2
    ei
    ret
ELSE
    PUBLIC frame_clock_isr
ENDIF

frame_clock_isr:
    push hl
    ld hl, (frame_clock_frames)
    inc hl
    ld (frame_clock_frames), hl
    pop hl
    ei
    reti

ENDIF
ENDIF
