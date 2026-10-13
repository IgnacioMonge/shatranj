; Resume from the newest checkpoint (CONTROL entry 2). Linked with
; asm/overlay/recovery/rcv_store.asm; format in that file.
;
; ctx[2..3] = 0: probe. Result 0 / HL = 1 only for an offerable candidate:
;   written by a host playing the current local colour after at least one ply.
; ctx[2..3] = buffer: stage. Copies the 60-byte snapshot there, then stages
;   the linked SHATJRNL.TMP prefix in SHATJRNL.NEW like a JOURNAL LOAD and
;   leaves pend unarmed: 0x01 complete body, 0x41 partial body, 0x40 no
;   verifiable body (the apply shows RESTORED). The prefix is accepted only
;   when exactly 2 * count bytes were read and their CRC matches the record.
;   A body failure never fails the snapshot. esxDOS closes the live TMP
;   handle first (SpectraNext keeps none); TMP itself is not modified until
;   the snapshot is applied.
; Results: 0 ok, 1 no candidate, 2 uncertain I/O (nothing touched).

SECTION code_user

PUBLIC _recovery_resume_ovl

INCLUDE "asm/overlay/recovery/rcv_layout.inc"

IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _esx_fopen_update
EXTERN _spxn_xfs_fseek
EXTERN rcv_stage
EXTERN rcv_read
ENDIF
EXTERN _esx_fopen
EXTERN _esx_fcreate
EXTERN _esx_fread
EXTERN _esx_fwrite
EXTERN _esx_fclose
EXTERN _esx_funlink
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _netchesszx_local_color
EXTERN _spectrum_journal_fd
EXTERN _spectrum_journal_pend
EXTERN _spectrum_net_background_drain
EXTERN rcv_scan_both
EXTERN rcv_crc
EXTERN rcv_valid
EXTERN rcv_keep

IFDEF NETCHESSZX_SPECTRANEXT
RS_CHUNK EQU 128
RS_PATH  EQU RCV_SCRATCH + RS_CHUNK
ASSERT RS_PATH + 18 <= RCV_SCRATCH + 160
EXTERN _esx_mkdir
ELSE
RS_CHUNK EQU 160
ENDIF
RS_BUF   EQU RCV_SCRATCH

_recovery_resume_ovl:
    ld a, 1
    ld (rcv_keep), a
    call rcv_scan_both
    ld a, 2
    jr nz, rs_result
    ld a, (rcv_valid)
    or a
    jr z, rs_none
    ld a, (RCV_REC + RCV_FLAGS)
    ld c, a
    and RCV_F_HOST | RCV_F_PLAYED
    cp RCV_F_HOST | RCV_F_PLAYED
    jr nz, rs_none
    ld a, (_netchesszx_local_color)
    and 1
    add a, a
    add a, a
    add a, a
    xor c
    and RCV_F_BLACK
    jr nz, rs_none
    ld hl, (RECOVERY_CTX_BUF)
    ld a, h
    or l
    jr z, rs_ok
    ex de, hl
    ld hl, RCV_REC + RCV_SNAPSHOT
    ld bc, RCV_SNAP_SIZE
    ldir
    ld a, (RCV_REC + RCV_FLAGS)
    ld (rs_flags), a
    ld hl, (RCV_REC + RCV_COUNT)
    ld (rs_left), hl
    ld hl, (RCV_REC + RCV_BODY_CRC)
    ld (rs_want), hl
IFNDEF NETCHESSZX_SPECTRANEXT
    call rs_close_live          ; XFS journal keeps no live handle
ENDIF
    call rs_stage_body
    ld (_spectrum_journal_pend), a
rs_ok:
    xor a
    ld (RECOVERY_CTX_RESULT), a
    ld hl, 1
    ret
rs_none:
    ld a, 1
rs_result:
    ld (RECOVERY_CTX_RESULT), a
    ld hl, 0
    ret

rs_no_body_early:
    ld a, 0x40
    ret

IFNDEF NETCHESSZX_SPECTRANEXT

; The staged body replaces the live history only when applied; until then the
; next armed switch reopens TMP, so the handle need not stay open.
rs_close_live:
    ld a, (_spectrum_journal_fd)
    and 0x3f
    ret z
    cp 0x3f
    ret z
    ld (_esx_handle), a
    call _esx_fclose
    xor a
    ld (_spectrum_journal_fd), a
    ret

; Returns A = staged pend value.
rs_stage_body:
    xor a
    ld (rs_src), a
    ld (rs_dst), a
    ld a, (rs_flags)
    and RCV_F_UNLINKED
    jr nz, rs_no_body_early
    ld hl, (rs_left)
    ld a, h
    or l
    jr z, rs_no_body_early      ; ply > 0 with no linked record: RESTORED
    bit 7, h
    jr nz, rs_no_body_early     ; beyond the STJ1 record limit
    add hl, hl
    ld (rs_left), hl            ; bytes to copy
    ld hl, 0xffff
    ld (rs_crc), hl
    ld hl, rs_tmp
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, rs_no_body_early
    ld (rs_src), a
    ld hl, rs_new
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jp z, rs_fail
    ld (rs_dst), a
rs_copy:
    ld iy, 0x5c3a
    call _spectrum_net_background_drain
    ld hl, (rs_left)
    ld a, h
    or l
    jr z, rs_copied
    ld bc, RS_CHUNK
    or a
    sbc hl, bc
    jr nc, rs_chunk_full
    ld bc, (rs_left)
rs_chunk_full:
    ld (rs_chunk), bc
    ld a, (rs_src)
    ld (_esx_handle), a
    ld hl, RS_BUF
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fread
    jr c, rs_fail
    ld hl, (_esx_result)
    ld bc, (rs_chunk)
    or a
    sbc hl, bc
    jr nz, rs_fail              ; TMP shorter than the linked prefix
    ld a, (rs_dst)
    ld (_esx_handle), a
    ld hl, RS_BUF
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fwrite
    jr c, rs_fail
    ld hl, (_esx_result)
    ld bc, (rs_chunk)
    or a
    sbc hl, bc
    jr nz, rs_fail
    ld hl, (rs_crc)
    ld de, RS_BUF
    call rcv_crc                ; C = chunk bytes (<= 160)
    ld (rs_crc), hl
    ld hl, (rs_left)
    ld bc, (rs_chunk)
    or a
    sbc hl, bc
    ld (rs_left), hl
    jr rs_copy
rs_copied:
    call rs_close_src
    ld a, (rs_dst)
    ld (_esx_handle), a
    xor a
    ld (rs_dst), a
    call _esx_fclose
    ld a, l
    or a
    jr nz, rs_discard
    ld hl, (rs_crc)
    ld de, (rs_want)
    or a
    sbc hl, de
    jr nz, rs_discard
    ld a, (rs_flags)
    and RCV_F_PARTIAL           ; bit 0 -> pend bit 6
    rrca
    rrca
    or 0x01                     ; BODY staged in SHATJRNL.NEW
    ret
rs_fail:
    call rs_close_src
    ld a, (rs_dst)
    or a
    jr z, rs_discard
    ld (_esx_handle), a
    call _esx_fclose
rs_discard:
    ld hl, rs_new
    call _esx_funlink
rs_no_body:
    ld a, 0x40
    ret

rs_close_src:
    ld a, (rs_src)
    or a
    ret z
    ld (_esx_handle), a
    xor a
    ld (rs_src), a
    jp _esx_fclose

rs_tmp:
    DEFM "/SYS/CONFIG/SHATJRNL.TMP"
    DEFB 0
rs_new:
    DEFM "/SYS/CONFIG/SHATJRNL.NEW"
    DEFB 0

ELSE

; SpectraNext: the XFS adapter keeps one handle, so each chunk reopens TMP,
; seeks to the copied offset and reads, then reopens NEW for update, seeks
; and writes. Every path is staged in low RAM (XFS pages over Page B).
; Returns A = staged pend value.
rs_stage_body:
    ld a, (rs_flags)
    and RCV_F_UNLINKED
    jr nz, rs_no_body_early
    ld hl, (rs_left)
    ld a, h
    or l
    jr z, rs_no_body_early      ; ply > 0 with no linked record: RESTORED
    bit 7, h
    jr nz, rs_no_body_early     ; beyond the STJ1 record limit
    add hl, hl
    ld (rs_left), hl            ; bytes to copy
    ld hl, 0xffff
    ld (rs_crc), hl
    ld hl, 0
    ld (rs_off), hl
    ld hl, rs_dir
    call rs_stage_path
    call _esx_mkdir             ; materialize a committed /CFG (EEXIST ok)
    ld hl, rs_new
    call rs_stage_path
    call _esx_fcreate           ; truncate any stale NEW once
    ld a, (_esx_handle)
    or a
    jp z, rs_discard
    call _esx_fclose
    ld a, l
    or a
    jp nz, rs_discard
rs_copy:
    ld iy, 0x5c3a
    call _spectrum_net_background_drain
    ld hl, (rs_left)
    ld a, h
    or l
    jp z, rs_copied
    ld bc, RS_CHUNK
    or a
    sbc hl, bc
    jr nc, rs_chunk_full
    ld bc, (rs_left)
rs_chunk_full:
    ld (rs_chunk), bc
    ld hl, rs_tmp
    xor a
    call rs_open_at
    jp z, rs_discard            ; TMP missing or unseekable: RESTORED
    ld hl, RS_BUF
    ld bc, (rs_chunk)
    call rcv_read
    jr c, rs_fail_close
    ld hl, (_esx_result)
    ld bc, (rs_chunk)
    or a
    sbc hl, bc
    jr nz, rs_fail_close        ; TMP shorter than the linked prefix
    call _esx_fclose
    ld a, l
    or a
    jr nz, rs_discard
    ld hl, rs_new
    ld a, 1
    call rs_open_at
    jr z, rs_discard
    ld hl, RS_BUF
    ld (_esx_buf), hl
    ld hl, (rs_chunk)
    ld (_esx_count), hl
    call _esx_fwrite
    ld hl, (_esx_result)
    ld bc, (rs_chunk)
    or a
    sbc hl, bc
    jr nz, rs_fail_close
    call _esx_fclose
    ld a, l
    or a
    jr nz, rs_discard
    ld hl, (rs_crc)
    ld de, RS_BUF
    ld a, (rs_chunk)
    ld c, a
    call rcv_crc                ; C = chunk bytes (<= 128)
    ld (rs_crc), hl
    ld bc, (rs_chunk)
    ld hl, (rs_off)
    add hl, bc
    ld (rs_off), hl
    ld hl, (rs_left)
    or a
    sbc hl, bc
    ld (rs_left), hl
    jp rs_copy
rs_copied:
    ld hl, (rs_crc)
    ld de, (rs_want)
    or a
    sbc hl, de
    jr nz, rs_discard
    ld a, (rs_flags)
    and RCV_F_PARTIAL           ; bit 0 -> pend bit 6
    rrca
    rrca
    or 0x01                     ; BODY staged in SHATJRNL.NEW
    ret
rs_fail_close:
    call _esx_fclose
rs_discard:
    ld hl, rs_new
    call rs_stage_path
    call _esx_funlink
    ld a, 0x40
    ret

; HL = overlay path, A = 0 read / 1 update. Opens it and seeks to rs_off.
; NZ with the handle open, Z on failure with nothing left open.
rs_open_at:
    push af
    call rs_stage_path
    pop af
    or a
    jr nz, rs_open_update
    call _esx_fopen
    jr rs_open_seek
rs_open_update:
    call _esx_fopen_update
rs_open_seek:
    ld a, (_esx_handle)
    or a
    ret z
    ld hl, (rs_off)
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, rs_open_bad          ; HL = 1 only on success
    inc a
    ret
rs_open_bad:
    call _esx_fclose
    xor a
    ret

rs_stage_path:
    ld de, RS_PATH
    jp rcv_stage

rs_dir:
    DEFM "/CFG"
    DEFB 0
rs_tmp:
    DEFM "/CFG/SHATJRNL.TMP"
    DEFB 0
rs_new:
    DEFM "/CFG/SHATJRNL.NEW"
    DEFB 0

ENDIF

SECTION bss_user
rs_flags: DEFS 1
rs_left:  DEFS 2
rs_chunk: DEFS 2
rs_crc:   DEFS 2
rs_want:  DEFS 2
IFDEF NETCHESSZX_SPECTRANEXT
rs_off:   DEFS 2
ELSE
rs_src:   DEFS 1
rs_dst:   DEFS 1
ENDIF
