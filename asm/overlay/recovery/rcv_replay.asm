; SpectraNext: RECOVERY entry 2 paints SHATJRNL.NEW into the move list after a
; LOAD or a cold resume, before the resident hook consumes the armed switch.
; SAVELOAD has no room for it on this target; RECOVERY already links the XFS
; adapter. The replay itself is shared with Classic/Next SAVELOAD:
; asm/overlay/journal/journal_replay.inc.
;
; ctx[0..1] = 3 (odd ply) or 1 (even ply). ctx[4] = 0 once the move
; list holds the history or the RESTORED marker; the caller paints the marker
; itself when the overlay could not run.

SECTION code_user

PUBLIC _recovery_replay_ovl

EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _spxn_xfs_fseek
EXTERN _spxn_rom_error
EXTERN _spectrum_journal_pend
EXTERN _spectrum_net_background_drain

NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0
RECOVERY_REPLAY_RESULT EQU NETCHESSZX_OVERLAY_CONTEXT + 4
; Overlay scratch (NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR, 160 bytes): one
; chunk plus the staged path. Nothing else in RECOVERY runs during a replay.
journal_append_buf EQU 0x672B
JA_CHUNK EQU 128
JA_PATH EQU journal_append_buf + JA_CHUNK
JA_PATH_SIZE EQU 18
ASSERT (JA_CHUNK & 1) = 0
ASSERT JA_PATH + JA_PATH_SIZE <= journal_append_buf + 160

; No jump: the include's first code is ja_replay_visible (RECOVERY has no
; bytes to spare on SpectraNext; tests/tools/test_recovery_spectranext.py
; enters here).
_recovery_replay_ovl:
INCLUDE "asm/overlay/journal/journal_replay.inc"

ja_apply_consume:
    xor a
    ld (RECOVERY_REPLAY_RESULT), a
    ld hl, 1
    ret

ja_new:
    defm "/CFG/SHATJRNL.NEW"
    defb 0
ja_new_end:
ASSERT ja_new_end - ja_new <= JA_PATH_SIZE
ja_rec:
    defw 0
