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
; Return non-zero and remember the exact text when that label changes.
; Inputs obey PLAYER_NAME_MAX, including the terminating NUL in the cache.
INCLUDE "asm/app/player_name_layout.inc"
PUBLIC _moves_header_white_new
PUBLIC _moves_header_black_new
EXTERN _spectrum_moves_header_shown
_moves_header_black_new:
    ld de, _spectrum_moves_header_shown + PLAYER_NAME_MAX + 1
    jr mhn_compare
_moves_header_white_new:
    ld de, _spectrum_moves_header_shown
mhn_compare:
    ld b, 0
mhn_loop:
    ld a, (de)
    cp (hl)
    jr z, mhn_same
    ld b, 1
mhn_same:
    ld a, (hl)
    ld (de), a
    inc de
    inc hl
    or a
    jr nz, mhn_loop
    ld l, b
    ret

IFDEF NETCHESSZX_NEXT_BANKING
; The banking Next build keeps its Layer 2 About and never opens this page.
_chlog_render_ovl_entry:
    ld hl, 0
    ret
ELSE
INCLUDE "asm/overlay/menu_logic/changelog.asm"
ENDIF
