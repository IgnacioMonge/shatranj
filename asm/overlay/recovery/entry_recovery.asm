; RECOVERY overlay: crash-safe checkpoints (format and slot rules in
; asm/overlay/recovery/rcv_store.asm). The capture entry encodes the snapshot
; straight into RCV_REC; WRITE completes the record with the journal link
; (_spectrum_journal_count/_crc) and the link flags from ctx[6]. The resume
; probe/stage entry lives in CONTROL (asm/overlay/recovery/rcv_resume.asm).
; SpectraNext's move-list replay rides here as entry 2 (rcv_replay.asm); other
; targets replay inside SAVELOAD and keep a stub that leaves ctx[4] = 0xFF.
; Entry 3 retires the offer after a chosen exit (recovery_ovl.c).

SECTION code_user

PUBLIC _recovery_write_ovl_entry
PUBLIC _recovery_capture_ovl_entry
PUBLIC _recovery_replay_ovl_entry
PUBLIC _recovery_retire_ovl_entry
EXTERN _recovery_capture_ovl
EXTERN _recovery_retire_ovl

    DEFB 4
    DW _recovery_write_ovl_entry
    DW _recovery_capture_ovl_entry
    DW _recovery_replay_ovl_entry
    DW _recovery_retire_ovl_entry

DEFC _recovery_capture_ovl_entry = _recovery_capture_ovl

DEFC _recovery_retire_ovl_entry = _recovery_retire_ovl

_recovery_replay_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
IFDEF NETCHESSZX_JOURNAL_HOOK
EXTERN _recovery_replay_ovl
    jp _recovery_replay_ovl
ELSE
    ret
ENDIF
ELSE
    ret
ENDIF

INCLUDE "asm/overlay/recovery/rcv_layout.inc"

IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _esx_mkdir
EXTERN _esx_funlink
EXTERN _esx_fopen_write_trunc
EXTERN _esx_frename
EXTERN _spxn_rom_error
EXTERN _spxf_replace_atomic
EXTERN rcv_stage
ELSE
EXTERN _esx_fcreate
EXTERN _esx_fsync
ENDIF
EXTERN _esx_fwrite
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN rcv_scan_both
EXTERN rcv_scan
EXTERN rcv_crc
EXTERN rcv_close
EXTERN rcv_magic
EXTERN rcv_path0
EXTERN rcv_path1
EXTERN rcv_valid
EXTERN rcv_newest
EXTERN rcv_keep
EXTERN rcv_seq0
EXTERN rcv_seq1
EXTERN _netchesszx_session_role
EXTERN _netchesszx_local_color
IFNDEF NETCHESSZX_SPECTRANEXT
EXTERN _spectrum_rcv_cache
ENDIF
IFDEF NETCHESSZX_JOURNAL_HOOK
EXTERN _spectrum_journal_fd
EXTERN _spectrum_journal_pend
EXTERN _spectrum_journal_count
EXTERN _spectrum_journal_crc
ENDIF

; ctx[2..3] = 60-byte snapshot (RCV_REC + 6 avoids the copy), ctx[6] = link
; flags. A scan with uncertain I/O aborts before any create/truncate.
_recovery_write_ovl_entry:
    xor a
    ld (rcv_keep), a
    ld hl, (RECOVERY_CTX_BUF)
    ld a, h
    or l
    jr z, rcv_bad_argument
    ld de, RCV_REC + RCV_SNAPSHOT
    or a
    sbc hl, de
    add hl, de
    jr z, rcv_snapshot_ready
    ld bc, RCV_SNAP_SIZE
    ldir
rcv_snapshot_ready:
IFNDEF NETCHESSZX_SPECTRANEXT
    ; shortcut: trusts that only this entry writes the slots between
    ; checkpoints; an SD swapped or edited without a reboot is not seen.
    ld hl, _spectrum_rcv_cache
    ld a, (hl)
    ld (rcv_newest), a
    inc a                       ; 0xFF (unknown) -> 0
    jr z, rcv_scan_slots
    ld (rcv_valid), a           ; newest 0/1 -> valid bit 0/1
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld (rcv_seq0), de
    ld (rcv_seq1), de
    jr rcv_slots_known
rcv_scan_slots:
ENDIF
    call rcv_scan_both
    jr nz, rcv_write_scan_fail
IFNDEF NETCHESSZX_SPECTRANEXT
rcv_slots_known:
ENDIF
    call rcv_build_record
    call rcv_write_record
    jr nz, rcv_write_fail
IFNDEF NETCHESSZX_SPECTRANEXT
    ld a, (rcv_target)
    ld (_spectrum_rcv_cache), a
    ld hl, (RCV_REC + RCV_SEQ)
    ld (_spectrum_rcv_cache + 1), hl
ENDIF
    xor a
    ld hl, 1
    jr rcv_result
rcv_write_scan_fail:
    ld a, 2
    jr rcv_error
rcv_write_fail:
IFNDEF NETCHESSZX_SPECTRANEXT
    ld a, 0xff                  ; the target may hold anything: scan again
    ld (_spectrum_rcv_cache), a
ENDIF
    ld a, 3
    jr rcv_error
rcv_bad_argument:
    ld a, 1
rcv_error:
    ld hl, 0
rcv_result:
    ld (RECOVERY_CTX_RESULT), a
    ret

; Target = the slot other than the newest valid one; generation = newest + 1.
rcv_build_record:
    ld hl, 1
    xor a
    ld (rcv_target), a
    ld a, (rcv_valid)
    or a
    jr z, rcv_build_seq
    ld a, (rcv_newest)
    ld hl, (rcv_seq0)
    or a
    jr z, rcv_build_other
    ld hl, (rcv_seq1)
rcv_build_other:
    inc hl
    xor 1
    ld (rcv_target), a
rcv_build_seq:
    ld (RCV_REC + RCV_SEQ), hl
    ld hl, rcv_magic
    ld de, RCV_REC
    ld bc, 4
    ldir
    ; Flags: ctx[6] carries PLAYED from the capture; the rest is resident
    ; state. An armed journal switch (GAME START before its first ply) has
    ; not yet replaced the TMP that count/CRC describe: withhold the link.
    ld a, (RECOVERY_CTX_LINK)
    ld c, a
IFDEF NETCHESSZX_JOURNAL_HOOK
    ld hl, (_spectrum_journal_count)
    ld (RCV_REC + RCV_COUNT), hl
    ld hl, (_spectrum_journal_crc)
    ld (RCV_REC + RCV_BODY_CRC), hl
    ld a, (_spectrum_journal_fd)
    and 0x40
    jr z, rcv_flag_linked
    set 0, c                    ; RCV_F_PARTIAL
rcv_flag_linked:
    ld a, (_spectrum_journal_pend)
    rla
    jr nc, rcv_flag_role
ELSE
    ld hl, 0
    ld (RCV_REC + RCV_COUNT), hl
    dec hl
    ld (RCV_REC + RCV_BODY_CRC), hl
ENDIF
    set 1, c                    ; RCV_F_UNLINKED
rcv_flag_role:
    ld a, (_netchesszx_session_role)
    or a
    jr nz, rcv_flag_colour
    set 2, c                    ; RCV_F_HOST (role 0)
rcv_flag_colour:
    ld a, (_netchesszx_local_color)
    or a
    jr z, rcv_flag_done
    set 3, c                    ; RCV_F_BLACK
rcv_flag_done:
    ld a, c
    ld (RCV_REC + RCV_FLAGS), a
    ld de, RCV_REC
    ld hl, 0xffff
    ld c, RCV_CRC_SIZE
    call rcv_crc
    ld (RCV_REC + RCV_CRC_SIZE), hl
    ret

; Z only after write, sync, close and an exact readback of the new slot.
rcv_write_record:
    ld hl, rcv_path0
    ld a, (rcv_target)
    or a
    jr z, rcv_write_path
    ld hl, rcv_path1
rcv_write_path:
    ld (rcv_target_path), hl
IFDEF NETCHESSZX_SPECTRANEXT
    ; XFS: write a committed temp and RENAME it over the slot, the product's
    ; atomic replace. A new file in cartridge RAM would not survive power-off.
    ; MKDIR materializes a committed /CFG for the new temp (EEXIST ignored).
    ld hl, rcv_dir
    ld de, RCV_PATH
    call rcv_stage
    call _esx_mkdir
    ld hl, (rcv_target_path)
    ld de, RCV_PATH
    call rcv_stage
    ld hl, rcv_temp
    ld de, RCV_PATH2
    call rcv_stage
    ld hl, RCV_SIZE
    push hl
    ld hl, RCV_REC
    push hl
    ld hl, RCV_PATH2
    push hl
    ld hl, RCV_PATH
    push hl
    call _spxf_replace_atomic   ; callee pops its four arguments
    ld a, h
    or l
    jr z, rcv_replaced
    ; A cut between the helper's CHMOD and RENAME leaves a committed temp
    ; that can be neither unlinked (EIO) nor committed again, so every later
    ; replace would fail. SpectraNext's recipe for that leftover: rewrite it
    ; with O_WRONLY|O_TRUNC, skip CHMOD and RENAME it over the target.
    ld hl, RCV_PATH2
    call _esx_funlink
    ld hl, (_esx_result)
    ld a, h
    or l
    jr nz, rcv_write_bad        ; removable: a genuine failure
    call _spxn_rom_error
    ld a, l
    cp XFS_EIO
    jr nz, rcv_write_bad
    ld hl, RCV_PATH2
    call _esx_fopen_write_trunc
    ld a, (_esx_handle)
    or a
    jr z, rcv_write_bad
    ld hl, RCV_REC
    ld bc, RCV_SIZE
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fwrite
    ld hl, (_esx_result)
    ld de, RCV_SIZE
    or a
    sbc hl, de
    jr nz, rcv_write_bad_close
    call rcv_close
    jr nz, rcv_write_bad
    ld hl, RCV_PATH
    ld (_esx_buf), hl
    ld hl, RCV_PATH2
    call _esx_frename           ; RENAME temp over the slot
    ld hl, (_esx_result)
    ld a, h
    or l
    jr z, rcv_write_bad
rcv_replaced:
ELSE
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jr z, rcv_write_bad
    ld hl, RCV_REC
    ld bc, RCV_SIZE
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fwrite
    jr c, rcv_write_bad_close
    ld hl, (_esx_result)
    ld de, RCV_SIZE
    or a
    sbc hl, de
    jr nz, rcv_write_bad_close
    call _esx_fsync
    jr c, rcv_write_bad_close
    call rcv_close
    jr nz, rcv_write_bad
ENDIF
    ld hl, (rcv_target_path)
    call rcv_scan
    or a
    jr nz, rcv_write_bad
    ld hl, RCV_REC
    ld de, RCV_TMP
    ld b, RCV_SIZE
rcv_compare_loop:
    ld a, (de)
    cp (hl)
    jr nz, rcv_write_bad
    inc de
    inc hl
    djnz rcv_compare_loop
    xor a
    ret
rcv_write_bad_close:
    call rcv_close
rcv_write_bad:
    xor a
    inc a                       ; NZ
    ret

IFDEF NETCHESSZX_SPECTRANEXT
rcv_dir:
    DEFM "/CFG"
    DEFB 0
rcv_temp:
    DEFM "/CFG/SHATRCV.NEW"
    DEFB 0
ENDIF

SECTION bss_user
rcv_target:      DEFS 1
rcv_target_path: DEFS 2
