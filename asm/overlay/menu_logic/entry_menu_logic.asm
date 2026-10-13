SECTION code_user

PUBLIC _status_phase_ovl_entry
PUBLIC _status_moves_header_ovl_entry
PUBLIC _chlog_render_ovl_entry

EXTERN _status_phase_ovl
EXTERN _status_moves_header_ovl

    DEFB 3
    DW _status_phase_ovl_entry
    DW _status_moves_header_ovl_entry
    DW _chlog_render_ovl_entry

DEFC _status_phase_ovl_entry = _status_phase_ovl
DEFC _status_moves_header_ovl_entry = _status_moves_header_ovl

; uint8_t moves_header_white_new(const char *text) __z88dk_fastcall
; uint8_t moves_header_black_new(const char *text) __z88dk_fastcall
; Return non-zero, and remember the text's key, when that MOVES header label
; does not show this text yet. Key: add each character, rotate left, bit 7.
; ponytail: a 7-bit key, so one name replaced directly by another keeps the
; old label once in 128; store the text itself if that ever shows up.
PUBLIC _moves_header_white_new
PUBLIC _moves_header_black_new
EXTERN _spectrum_moves_header_shown
_moves_header_black_new:
    ld de, _spectrum_moves_header_shown + 1
    jr mhn_key
_moves_header_white_new:
    ld de, _spectrum_moves_header_shown
mhn_key:
    ld b, 0
mhn_loop:
    ld a, (hl)
    or a
    jr z, mhn_done
    add a, b
    rlca
    ld b, a
    inc hl
    jr mhn_loop
mhn_done:
    ld a, b
    or 0x80
    ex de, hl
    cp (hl)
    ld (hl), a
    ld l, 0
    ret z
    inc l
    ret

IFDEF NETCHESSZX_NEXT_BANKING
; The banking Next build keeps its Layer 2 About and never opens this page.
_chlog_render_ovl_entry:
    ld hl, 0
    ret
ELSE
INCLUDE "asm/overlay/menu_logic/changelog.asm"
ENDIF
