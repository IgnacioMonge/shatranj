; Resident editor storage. Order is the existing INPUT_EDIT ABI.
; Only INPUT_EDIT receives these private addresses from the resident map.
editor_input EQU 0x5eb2
editor_input_max EQU 42

SECTION bss_compiler
_local_input_len: DEFS 1
_local_input_cursor: DEFS 1
_local_input_mode: DEFS 1
_input_history_count: DEFS 1
_input_history_pos: DEFS 1

; Application sees only a read-only activity view, not editor internals.
PUBLIC _spectrum_editor_active
DEFC _spectrum_editor_active = _local_input_mode

SECTION code_user

PUBLIC _spectrum_editor_append_hot
EXTERN _spectrum_render_input_cell
EXTERN _spectrum_overlay_context

; L=key. Return L=1 when an ordinary append-at-end was handled, including a
; printable no-op at the full-buffer limit; otherwise return 0.
; The cold INPUT_EDIT overlay retains cursor, deletion and history operations.
_spectrum_editor_append_hot:
    ld a, l
    cp 32
    jr c, editor_append_cold
    cp 127
    jr nc, editor_append_cold
    ld c, a
    ld a, (_local_input_len)
    cp editor_input_max
    jr nc, editor_append_handled
    ld b, a
    ld a, (_local_input_cursor)
    cp b
    jr nz, editor_append_cold

    ld hl, editor_input
    ld d, 0
    ld e, b
    add hl, de
    ld (hl), c
    inc hl
    ld (hl), 0
    inc b
    ld a, b
    ld (_local_input_len), a
    ld (_local_input_cursor), a
    ld a, 0xff
    ld (_input_history_pos), a
    ld a, b
    dec a
    ld b, 0
    call editor_paint_cell
    ld a, (_local_input_cursor)
    ld c, ' '
    ld b, 1
    call editor_paint_cell
editor_append_handled:
    ld l, 1
    ret

editor_append_cold:
    ld l, 0
    ret

; A=index, C=glyph, B=cursor flag. HL = context for fastcall paint.
editor_paint_cell:
    ld hl, _spectrum_overlay_context
    ld (hl), a
    inc hl
    ld (hl), c
    inc hl
    ld (hl), b
    dec hl
    dec hl
    jp _spectrum_render_input_cell
editor_append_hot_end:
