; SAVELOAD entries 0/1 with STJ_SAVELOAD=1: alias the shared STJ1 codec.
; entry_saveload.asm keeps its DEFC to _saveload_load_nczs_ovl and
; _saveload_save_nczs_ovl; this file provides those symbols instead of the
; wire-snapshot C bodies.

SECTION code_user

PUBLIC _saveload_load_nczs_ovl
PUBLIC _saveload_save_nczs_ovl
PUBLIC _saveload_erase_nczs_ovl

EXTERN _stj_load_ovl
EXTERN _stj_save_ovl

DEFC _saveload_load_nczs_ovl = _stj_load_ovl
DEFC _saveload_save_nczs_ovl = _stj_save_ovl

EXTERN stj_build_path, stj_path, _esx_result
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN stj_xfs_stage_path, _esx_buf, _esx_frename
ELSE
EXTERN _esx_funlink
ENDIF

; Entry 2 shares the codec's validated 8.3 path. No second C path builder,
; overlay path buffer or SpectraNext staging copy is needed.
_saveload_erase_nczs_ovl:
    ld hl, (0x5fe0)
    ld a, h
    or l
    jr z, se_bad_name
    xor a
    call stj_build_path
    jr nz, se_bad_name
    ld a, 2
    ld (0x5fe4), a
    ld hl, stj_path
IFDEF NETCHESSZX_SPECTRANEXT
    ; Rename to the same-name .DEL tombstone, preserving flash/XFS semantics.
    call stj_xfs_stage_path
    ld (_esx_buf), hl
    dec de
    dec de
    ld a, 'L'
    ld (de), a
    dec de
    ld a, 'E'
    ld (de), a
    dec de
    ld a, 'D'
    ld (de), a
    ld hl, stj_path
    call _esx_frename
ELSE
    call _esx_funlink
ENDIF
    ld hl, (_esx_result)
    ld a, h
    or l
    ret z
    xor a
    ld (0x5fe4), a
    ld hl, 1
    ret
se_bad_name:
    ld a, 1
    ld (0x5fe4), a
    ld hl, 0
    ret
