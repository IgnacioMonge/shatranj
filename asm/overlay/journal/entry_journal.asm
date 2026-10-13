; JOURNAL overlay (Classic): STJ1 LOAD/SAVE wrappers over the shared
; codec (asm/overlay/journal/stj_codec.asm). Persist append is SAVELOAD
; entry 3. State contract and bit layout: asm/spectrum/journal.asm.

SECTION code_user

PUBLIC _journal_load_ovl_entry
PUBLIC _journal_save_ovl_entry

    DEFB 2
    DW _journal_load_ovl_entry
    DW _journal_save_ovl_entry

NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0

IFNDEF ATLAS_JOURNAL

_journal_load_ovl_entry:
_journal_save_ovl_entry:
    ld a, 2
    ld (NETCHESSZX_OVERLAY_CONTEXT + 4), a
    ld hl, 0
    ret

ELSE

EXTERN _stj_load_ovl
EXTERN _stj_save_ovl

DEFC _journal_load_ovl_entry = _stj_load_ovl
DEFC _journal_save_ovl_entry = _stj_save_ovl

ENDIF
