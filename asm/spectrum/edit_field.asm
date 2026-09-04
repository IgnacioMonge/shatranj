; Resident blink for the SETUP field editor. Buffer mutation lives in its
; overlay. Software blink is used
; because ULA FLASH is 8x8 and would pulse two 4-pixel glyphs.

SECTION code_user

PUBLIC _spectrum_gui_edit_show
PUBLIC _spectrum_gui_edit_hide
PUBLIC _spectrum_gui_edit_tick
PUBLIC _edit_buf
PUBLIC _edit_max
PUBLIC _edit_len
PUBLIC _edit_pos

EXTERN draw_char64_pixels_at_tmp
EXTERN tmp_row
EXTERN tmp_col
EXTERN tmp_char
EXTERN tmp_scan

EDIT_FLASH EQU 16

; HL: L=row, H=first column (in 4-pixel cells).
_spectrum_gui_edit_show:
    ld a, l
    ld (edit_row), a
    ld c, h
    jp edit_snap

_spectrum_gui_edit_hide:
    ld a, (edit_on)
    or a
    ret z
    xor a
    ld (edit_on), a
    ld a, (edit_vis)
    or a
    ret nz
    ld a, (edit_cell_ch)
    jr edit_put_cell

_spectrum_gui_edit_tick:
    ld a, (edit_on)
    or a
    ret z
    ld hl, edit_phase
    inc (hl)
    ld a, (hl)
    cp EDIT_FLASH
    ret c
    xor a
    ld (hl), a
    ld a, (edit_vis)
    xor 1
    ld (edit_vis), a
    or a
    ld a, (edit_cell_ch)
    jr nz, edit_put_cell
    ld a, ' '

edit_put_cell:
    ld e, a
    ld a, (edit_row)
    ld b, a
    ld a, (edit_cell_col)
    ld c, a
    ld a, e

; A=character, B=screen row, C=4-pixel cell column.
edit_put:
    ld e, a
    ld a, b
    ld (tmp_row), a
    ld a, c
    ld (tmp_col), a
    ld a, e
    jp draw_char64_pixels_at_tmp

edit_snap:
    ld a, 1
    ld (edit_on), a
    xor a
    ld (edit_phase), a
    inc a
    ld (edit_vis), a
    ld a, (_edit_len)
    or a
    ld a, '_'
    jr z, snap_store
    ld a, (_edit_pos)
    add a, c
    ld c, a
    ld hl, (_edit_buf)
    ld a, (_edit_pos)
    ld e, a
    ld d, 0
    add hl, de
    ld a, (hl)
snap_store:
    ld (edit_cell_ch), a
    ld a, c
    ld (edit_cell_col), a
    ld a, (edit_cell_ch)
    jr edit_put_cell

SECTION bss_user

_edit_buf:     DEFS 2
_edit_max:     DEFS 1
_edit_len:     DEFS 1
_edit_pos:     DEFS 1
edit_row:      DEFS 1
edit_cell_ch:  DEFS 1
edit_cell_col: DEFS 1
edit_on:       DEFS 1
edit_vis:      DEFS 1
edit_phase:    DEFS 1
