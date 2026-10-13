; Shared STJ1 LOAD/SAVE codec. Linked by the Classic JOURNAL overlay and
; by SAVELOAD entries 0/1 on Next when STJ_SAVELOAD=1. The overlay context
; contract is the SAVELOAD one: ctx[0..1] = name, ctx[2..3] = snapshot
; buffer pointer, ctx[4] = result. State contract and bit layout:
; asm/spectrum/journal.asm.

SECTION code_user

PUBLIC _stj_load_ovl
PUBLIC _stj_save_ovl
; Private to overlays which link this codec (SAVELOAD reuses the path parser).
PUBLIC stj_build_path, stj_path
IFDEF NETCHESSZX_SPECTRANEXT
PUBLIC stj_xfs_stage_path
ENDIF

stj_ctx EQU 0x5fe0
IFDEF NETCHESSZX_NEXT_BANKING
; esxDOS runs RST 8 with MMU1 = ROM and DivMMC over 0x0000..0x3FFF, so file
; buffers must live above 0x3FFF: SAVELOAD's overlay-BSS journal buffer.
EXTERN ja_io_buf
stj_scratch EQU ja_io_buf
ELSE
stj_scratch EQU 0x672B
ENDIF

STJ_HDR EQU stj_scratch
STJ_HDR_SIZE EQU 76
STJ_CHUNK EQU STJ_HDR + STJ_HDR_SIZE
; XFS also keeps two paths in scratch; esxDOS can use its whole tail.
IFDEF NETCHESSZX_SPECTRANEXT
STJ_CHUNK_SIZE EQU 32
ELSE
STJ_CHUNK_SIZE EQU 84
ENDIF
ASSERT STJ_CHUNK_SIZE > 0
ASSERT STJ_HDR_SIZE + STJ_CHUNK_SIZE <= 160
IFDEF NETCHESSZX_SPECTRANEXT
STJ_PATH_SIZE EQU 18
stj_path EQU STJ_CHUNK + STJ_CHUNK_SIZE
stj_xfs_path_arg EQU stj_path + STJ_PATH_SIZE
ASSERT stj_xfs_path_arg + STJ_PATH_SIZE <= stj_scratch + 160
ENDIF
CTX_BUF EQU stj_ctx + 2
CTX_RESULT EQU stj_ctx + 4

EXTERN _esx_fopen
EXTERN _esx_fcreate
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _esx_fopen_update
EXTERN _esx_commit
EXTERN _esx_frename
EXTERN _esx_fseek_end
EXTERN _spxn_xfs_fseek
EXTERN _spxn_regs
EXTERN _stj_spxn_adopt_new
ELSE
EXTERN _esx_fcreate_new
EXTERN _esx_fsync
EXTERN _esx_fseek_set
ENDIF
EXTERN _esx_fread
EXTERN _esx_fwrite
EXTERN _esx_fclose
EXTERN _esx_funlink
EXTERN _esx_mkdir
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _spxn_rom_error
ENDIF
EXTERN _spectrum_journal_fd
EXTERN _spectrum_journal_pend
EXTERN _spectrum_net_background_drain
EXTERN _spectrum_net_runtime_fat_date
EXTERN _spectrum_net_runtime_fat_time
EXTERN _spectrum_net_runtime_fat_day_delta

; LOAD accepts only a complete STJ1 file: exactly 76 + 2 * record_count bytes,
; valid magic/version/flags/reserved/header CRC and matching body CRC. The
; whole file is validated before any resident or card state changes, so a
; rejected file leaves snapshot, fd, pend, TMP and SHATJRNL.NEW untouched.
; After validation it copies the snapshot to the caller, stages the body in
; SHATJRNL.NEW and leaves pend unarmed (bit 7 clear).
_stj_load_ovl:
IFDEF NETCHESSZX_SPECTRANEXT
    jp stj_xfs_load
ELSE
    xor a
    ld (stj_tmp_fd), a
    call stj_build_path
    jp nz, stj_fail_name
    ld hl, stj_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jp z, stj_fail_open
    ld (stj_fd), a
    call stj_drain
    call stj_file_check
    jp nz, stj_fail_data_close
    ld a, (STJ_HDR + 5)
    and 1
    xor 1
    rrca
    rrca
    ld c, a                     ; 0x40 unless HISTORY_COMPLETE
    ld hl, STJ_HDR + 16
    call stj_load_snap
    ld hl, (STJ_HDR + 8)
    ld a, h
    or l
    jr z, stj_load_close
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, STJ_HDR_SIZE
    call _esx_fseek_set
    ld a, l
    or a
    jr nz, stj_load_body_lost
    ld hl, stj_new
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jr z, stj_load_body_lost
    ld (stj_tmp_fd), a
    ld hl, (STJ_HDR + 8)
    add hl, hl
    call stj_copy_hl
    jr nz, stj_load_body_lost
    call stj_close_tmp
    ld a, l
    or a
    jr nz, stj_load_body_lost
    ld hl, _spectrum_journal_pend
    inc (hl)                    ; BODY
    jr stj_load_close
stj_load_body_lost:             ; second-pass copy failed: snapshot still loads,
    call stj_close_tmp          ; the continuation is explicitly partial
    ld hl, stj_new
    call _esx_funlink
    ld a, 0x40
    ld (_spectrum_journal_pend), a
stj_load_close:
    call stj_close_file
    xor a
    jp stj_ok

; HL = 60-byte snapshot, C = staged pend. Settle an armed switch before the
; new LOAD can overwrite SHATJRNL.NEW.
stj_load_snap:
    push bc
    ld de, (CTX_BUF)
    ld bc, 60
    ldir
    ld a, (_spectrum_journal_pend)
    or a
    jp p, stj_snap_stage
    bit 0, a
    jr nz, stj_snap_adopt
    and 0x40
stj_snap_fd:
    push af
    call stj_close_live
    pop af
    ld (_spectrum_journal_fd), a
    jr stj_snap_stage
stj_snap_adopt:
    call stj_adopt_new
stj_snap_stage:
    pop bc
    ld a, c
    ld (_spectrum_journal_pend), a
    ret

; Adopt the previously applied SHATJRNL.NEW into a live TMP. stj_fd remains
; the current LOAD handle; the destination is left open at EOF.
stj_adopt_new:
    ld a, (stj_fd)
    ld (stj_src), a
    call stj_close_live
    ld hl, stj_new
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, stj_adopt_lost
    ld (stj_fd), a
    ld hl, stj_tmp
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jr z, stj_adopt_close_src
    ld (stj_tmp_fd), a
stj_adopt_loop:
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    ld bc, STJ_CHUNK_SIZE
    call stj_io_read_any
    jr c, stj_adopt_failed
    ld a, (_esx_result)
    or a
    jr z, stj_adopt_done
    ld c, a
    ld b, 0
    ld a, (stj_tmp_fd)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    call stj_io_write
    jr z, stj_adopt_loop
stj_adopt_failed:
    call stj_close_tmp
    call stj_close_file
    ld a, 0x7f
    jr stj_adopt_state
stj_adopt_done:
    call stj_close_file
    ld hl, stj_new
    call _esx_funlink
    ld a, (stj_tmp_fd)
    ld b, a
    xor a
    ld (stj_tmp_fd), a
    ld a, (_spectrum_journal_pend)
    and 0x40
    or b
    jr stj_adopt_state
stj_adopt_close_src:
    call stj_close_file
stj_adopt_lost:
    ld a, 0x40
stj_adopt_state:
    ld (_spectrum_journal_fd), a
    ld a, (stj_src)
    ld (stj_fd), a
    ret
stj_close_file:
    ld a, (stj_fd)
    ld (_esx_handle), a
    jp _esx_fclose
ENDIF

; SAVE reads the history named by pend (armed) or fd and leaves the live handle
; at EOF. If an error also prevents that cleanup, disable append rather than
; leave the handle positioned to overwrite the unread tail.
_stj_save_ovl:
IFDEF NETCHESSZX_SPECTRANEXT
    jp stj_xfs_save
ELSE
    xor a
    ld (stj_tmp_fd), a
    ld (stj_live), a
    ld (stj_src), a
    inc a
    call stj_build_path
    jp nz, stj_fail_name
    ld hl, stj_mkdir
    call _esx_mkdir
    ld hl, stj_path
    call _esx_fcreate_new
    ld a, (_esx_handle)
    or a
    jp z, stj_fail_open
    ld (stj_fd), a
    call stj_drain
    ld hl, 0
    ld (stj_left), hl
    dec hl
    ld (stj_jcrc), hl
    ld a, (_spectrum_journal_pend)
    or a
    jp p, stj_save_fd
    ld c, a
    and 0x40
    ld (stj_flags), a
    ld hl, stj_new
    bit 0, c
    jr nz, stj_save_named
    jr stj_save_hdr
stj_save_fd:
    ld a, (_spectrum_journal_fd)
    ld c, a
    and 0x40
    ld (stj_flags), a
    ld a, c
    and 0x3f
    jr z, stj_save_hdr
    ld hl, stj_tmp
    cp 0x3f
    jr z, stj_save_named        ; disabled: reread the surviving prefix
    ld (stj_live), a
    ld (stj_src), a
    ld (_esx_handle), a
    call _esx_fsync
    jp c, stj_fail_io
    ld hl, 0
    call _esx_fseek_set
    ld a, l
    or a
    jp nz, stj_fail_io
    jr stj_save_hdr
stj_save_named:
    call _esx_fopen
    ld a, (_esx_handle)
    ld (stj_tmp_fd), a
    ld (stj_src), a
    or a
    jr nz, stj_save_hdr
    ld a, 0x40                  ; body unreadable: empty, partial
    ld (stj_flags), a
stj_save_hdr:
    ld hl, 0
    ld (stj_n), hl
    call stj_put_header         ; placeholder, rewritten below
    jp nz, stj_fail_io
    ld a, (stj_src)
    or a
    jr z, stj_save_patch
stj_save_loop:
    ld a, (stj_src)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    ld bc, STJ_CHUNK_SIZE
    call stj_io_read_any
    jp c, stj_fail_io
    ld a, (_esx_result)
    ld c, a
    cp STJ_CHUNK_SIZE
    jr z, stj_save_full
    bit 0, c
    jr z, stj_save_last
    dec c                       ; torn record at the tail: drop it
    ld hl, stj_flags
    set 6, (hl)
stj_save_last:
    call stj_save_put
    jp nz, stj_fail_io
    jr stj_save_patch
stj_save_full:
    call stj_save_put
    jp nz, stj_fail_io
    jr stj_save_loop
stj_save_patch:
    ld hl, (stj_left)
    srl h
    rr l
    ld (stj_n), hl
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, 0
    call _esx_fseek_set
    ld a, l
    or a
    jp nz, stj_fail_io
    call stj_put_header
    jp nz, stj_fail_io
    call stj_close_tmp
    call stj_live_eof
    call stj_close_file
    ld a, l
    or a
    jr nz, stj_fail_io_closed
    ld hl, stj_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, stj_fail_io_closed
    ld (stj_fd), a
    call stj_file_check
    jp nz, stj_fail_io
    call stj_close_file
    ld a, l
    or a
    jr nz, stj_fail_io_closed
stj_ok:
    ld (CTX_RESULT), a
    ld hl, 1
    ret

; C = even byte count in STJ_CHUNK. Z on success.
stj_save_put:
    ld a, c
    or a
    ret z
    ld b, 0
    ld hl, (stj_left)
    add hl, bc
    ld (stj_left), hl
    push bc
    ld de, STJ_CHUNK
    ld hl, (stj_jcrc)
    call stj_crc
    ld (stj_jcrc), hl
    pop bc
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    jp stj_io_write

stj_put_header:
    call stj_fill_header
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, STJ_HDR
    ld bc, STJ_HDR_SIZE
    jp stj_io_write

stj_fail_io:
    call stj_close_file
stj_fail_io_closed:
    call stj_close_tmp
    call stj_live_eof
    ld hl, stj_path
    call _esx_funlink
    ld a, 3
    jr stj_err
stj_fail_data_close:
    call stj_close_file
    ld a, 4
    jr stj_err
stj_fail_open:
    ld a, 2
    jr stj_err
stj_fail_name:
    ld a, 1
stj_err:
    ld (CTX_RESULT), a
    ld hl, 0
    ret
ENDIF

stj_drain:
    ld iy, 0x5c3a
    jp _spectrum_net_background_drain

IFDEF NETCHESSZX_SPECTRANEXT

; SpectraNext XFS exposes one compatibility handle. Keep format validation
; shared, but reopen each source/destination block instead of switching fd.
; ROM calls page their module over Page B, so no path argument may point into
; this overlay. Copy constants to the fixed low-RAM scratch tail first; the
; generated target path already lives there.
stj_xfs_stage_path:
    ld de, stj_xfs_path_arg
    push de
stj_xfs_stage_path_loop:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, stj_xfs_stage_path_loop
    pop hl
    ret

; Validate the currently open XFS file without closing it. XFS exposes the
; 32-bit size separately, so reject files beyond the v1 16-bit exact length.
stj_xfs_file_check:
    call _esx_fseek_end
    ld a, l
    or a
    jr nz, stj_xfs_check_bad
    ld hl, (_spxn_regs + 3)
    ld a, h
    or l
    jr nz, stj_xfs_check_bad
    ld hl, (_spxn_regs + 5)
    ld (stj_file_size), hl
    ld de, STJ_HDR_SIZE
    or a
    sbc hl, de
    jr c, stj_xfs_check_bad
    ld hl, 0
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, stj_xfs_check_bad
    call stj_read_header
    ret nz
    ld hl, (STJ_HDR + 8)
    add hl, hl
    jr c, stj_xfs_check_bad
    ld de, STJ_HDR_SIZE
    add hl, de
    jr c, stj_xfs_check_bad
    ld de, (stj_file_size)
    or a
    sbc hl, de
    ret nz
    ld hl, (STJ_HDR + 8)
    add hl, hl
    jp stj_body_check
stj_xfs_check_bad:
    or 1
    ret

stj_xfs_load:
    xor a
    call stj_build_path
    jp nz, stj_xfs_fail_name
    ld hl, stj_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jp z, stj_xfs_fail_open
    call stj_drain
    call stj_xfs_file_check
    jp nz, stj_xfs_fail_data_close
    call stj_xfs_close
    jp nz, stj_xfs_fail_data

    ld a, (STJ_HDR + 5)
    and 1
    xor 1
    rrca
    rrca
    ld (stj_flags), a           ; 0x40 unless HISTORY_COMPLETE
    ld hl, STJ_HDR + 16
    ld de, (CTX_BUF)
    ld bc, 60
    ldir
    ld hl, (STJ_HDR + 8)
    add hl, hl
    ld (stj_left), hl
    call stj_xfs_settle_pending
    ld hl, (stj_left)
    ld a, h
    or l
    jr z, stj_xfs_load_empty
    ld hl, stj_path
    ld (stj_src_path), hl
    ld hl, STJ_HDR_SIZE
    ld (stj_read_base), hl
    ld hl, stj_new
    ld (stj_dst_path), hl
    ld hl, 0
    ld (stj_write_base), hl
    ld (stj_pos), hl
    call stj_xfs_create_dst
    jr nz, stj_xfs_load_body_lost
stj_xfs_load_stage:
    call stj_xfs_take_left
    call stj_xfs_read_chunk
    jr nz, stj_xfs_load_body_lost
    ld a, (stj_chunk_n)
    ld b, a
    ld a, (stj_take)
    cp b
    jr nz, stj_xfs_load_body_lost
    call stj_xfs_write_chunk
    jr nz, stj_xfs_load_body_lost
    call stj_xfs_advance
    ld hl, (stj_left)
    ld a, h
    or l
    jr nz, stj_xfs_load_stage
    ld a, (stj_flags)
    or 1
    jr stj_xfs_load_set_pend
stj_xfs_load_empty:
    ld hl, stj_new
    call stj_xfs_stage_path
    call _esx_funlink
    ld a, (stj_flags)
stj_xfs_load_set_pend:
    ld (_spectrum_journal_pend), a
    jp stj_xfs_ok

stj_xfs_load_body_lost:
    ld hl, stj_new
    call stj_xfs_stage_path
    call _esx_funlink
    ld a, 0x40
    ld (_spectrum_journal_pend), a
    jp stj_xfs_ok

; Settle an older accepted LOAD before replacing SHATJRNL.NEW.
stj_xfs_settle_pending:
    ld a, (_spectrum_journal_pend)
    or a
    ret p
    ld c, a
    and 0x40
    ld (_spectrum_journal_fd), a
    bit 0, c
    ret z
    call _stj_spxn_adopt_new
    ret z
    ld a, 0x40
    ld (_spectrum_journal_fd), a
    ret

stj_xfs_save:
    xor a
    inc a
    call stj_build_path
    jp nz, stj_xfs_fail_name
    call stj_drain
    ld hl, 0
    ld (stj_src_path), hl
    ld (stj_left), hl
    ld (stj_pos), hl
    dec hl
    ld (stj_jcrc), hl
    ld a, (_spectrum_journal_pend)
    or a
    jp p, stj_xfs_save_fd
    ld c, a
    and 0x40
    ld (stj_flags), a
    bit 0, c
    jr z, stj_xfs_save_target
    ld hl, stj_new
    ld (stj_src_path), hl
    jr stj_xfs_save_target
stj_xfs_save_fd:
    ld a, (_spectrum_journal_fd)
    ld c, a
    and 0x40
    ld (stj_flags), a
    ld a, c
    and 0x3f
    jr z, stj_xfs_save_target
    ld hl, stj_tmp
    ld (stj_src_path), hl
stj_xfs_save_target:
    ld hl, stj_mkdir
    call stj_xfs_stage_path
    call _esx_mkdir
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_funlink
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jp z, stj_xfs_fail_open
    ld hl, 0
    ld (stj_n), hl
    call stj_fill_header
    ld hl, STJ_HDR
    ld bc, STJ_HDR_SIZE
    call stj_io_write
    jp nz, stj_xfs_save_fail_close
    call stj_xfs_close
    jp nz, stj_xfs_save_fail
    ld hl, (stj_src_path)
    ld a, h
    or l
    jr z, stj_xfs_save_patch
    ld hl, 0
    ld (stj_read_base), hl
    ld (stj_pos), hl
    ld hl, stj_save_tmp
    ld (stj_dst_path), hl
    ld hl, STJ_HDR_SIZE
    ld (stj_write_base), hl
stj_xfs_save_loop:
    ld a, STJ_CHUNK_SIZE
    ld (stj_take), a
    call stj_xfs_read_chunk
    jp nz, stj_xfs_save_fail
    ld a, (stj_chunk_n)
    or a
    jr z, stj_xfs_save_patch
    bit 0, a
    jr z, stj_xfs_save_even
    dec a                       ; torn record at EOF: drop it
    ld (stj_chunk_n), a
    ld hl, stj_flags
    set 6, (hl)
stj_xfs_save_even:
    ld a, (stj_chunk_n)
    or a
    jr z, stj_xfs_save_patch
    ld c, a
    ld b, 0
    ld hl, (stj_left)
    add hl, bc
    ld (stj_left), hl
    push bc
    ld de, STJ_CHUNK
    ld hl, (stj_jcrc)
    call stj_crc
    ld (stj_jcrc), hl
    pop bc
    call stj_xfs_write_chunk
    jp nz, stj_xfs_save_fail
    call stj_xfs_advance_pos
    ld a, (stj_chunk_n)
    cp STJ_CHUNK_SIZE
    jr z, stj_xfs_save_loop
stj_xfs_save_patch:
    ld hl, (stj_left)
    srl h
    rr l
    ld (stj_n), hl
    call stj_fill_header
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_fopen_update
    ld a, (_esx_handle)
    or a
    jr z, stj_xfs_save_fail
    ld hl, 0
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, stj_xfs_save_fail_close
    ld hl, STJ_HDR
    ld bc, STJ_HDR_SIZE
    call stj_io_write
    jr nz, stj_xfs_save_fail_close
    call stj_xfs_close
    jr nz, stj_xfs_save_fail
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, stj_xfs_save_fail
    call stj_xfs_file_check
    jr nz, stj_xfs_save_fail_close
    call stj_xfs_close
    jr nz, stj_xfs_save_fail
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_commit
    ld hl, (_esx_result)
    ld a, h
    or l
    jr z, stj_xfs_save_fail
    ld hl, stj_path
    ld (_esx_buf), hl
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_frename
    ld hl, (_esx_result)
    ld a, h
    or l
    jr nz, stj_xfs_ok
stj_xfs_save_fail:
    ld hl, stj_save_tmp
    call stj_xfs_stage_path
    call _esx_funlink
    ld a, 3
    jr stj_xfs_err
stj_xfs_save_fail_close:
    call _esx_fclose
    jr stj_xfs_save_fail

stj_xfs_fail_data_close:
    call _esx_fclose
stj_xfs_fail_data:
    ld a, 4
    jr stj_xfs_err
stj_xfs_fail_open:
    ld a, 2
    jr stj_xfs_err
stj_xfs_fail_name:
    ld a, 1
stj_xfs_err:
    ld (CTX_RESULT), a
    ld hl, 0
    ret
stj_xfs_ok:
    xor a
    ld (CTX_RESULT), a
    ld hl, 1
    ret

; Create an empty destination, closing it before another path is opened.
stj_xfs_create_dst:
    ld hl, (stj_dst_path)
    call stj_xfs_stage_path
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
    jp z, stj_xfs_io_bad
    jp stj_xfs_close

; Select min(left, 32) for the exact LOAD staging loop.
stj_xfs_take_left:
    ld hl, (stj_left)
    ld a, h
    or a
    ld a, STJ_CHUNK_SIZE
    jr nz, stj_xfs_take_store
    ld a, l
    cp STJ_CHUNK_SIZE + 1
    jr c, stj_xfs_take_store
    ld a, STJ_CHUNK_SIZE
stj_xfs_take_store:
    ld (stj_take), a
    ret

; Read up to stj_take bytes from src_path at read_base + pos.
stj_xfs_read_chunk:
    ld hl, (stj_src_path)
    call stj_xfs_stage_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, stj_xfs_io_bad
    ld hl, (stj_pos)
    ld de, (stj_read_base)
    add hl, de
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, stj_xfs_io_bad_close
    ld hl, STJ_CHUNK
    ld a, (stj_take)
    ld c, a
    ld b, 0
    call stj_io_read_any
    jr c, stj_xfs_io_bad_close
    ld a, (_esx_result)
    ld (stj_chunk_n), a
    jp stj_xfs_close

; Write stj_chunk_n bytes to dst_path at write_base + pos.
stj_xfs_write_chunk:
    ld hl, (stj_dst_path)
    call stj_xfs_stage_path
    call _esx_fopen_update
    ld a, (_esx_handle)
    or a
    jr z, stj_xfs_io_bad
    ld hl, (stj_pos)
    ld de, (stj_write_base)
    add hl, de
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, stj_xfs_io_bad_close
    ld hl, STJ_CHUNK
    ld a, (stj_chunk_n)
    ld c, a
    ld b, 0
    call stj_io_write
    jr nz, stj_xfs_io_bad_close
    jp stj_xfs_close

stj_xfs_advance_pos:
    ld a, (stj_chunk_n)
    ld c, a
    ld b, 0
    ld hl, (stj_pos)
    add hl, bc
    ld (stj_pos), hl
    ret
stj_xfs_advance:
    call stj_xfs_advance_pos
    ld hl, (stj_left)
    or a
    sbc hl, bc
    ld (stj_left), hl
    ret

stj_xfs_io_bad_close:
    call _esx_fclose
stj_xfs_io_bad:
    or 1
    ret
stj_xfs_close:
    call _esx_fclose
    ld a, l
    or a
    ret

ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
stj_close_live:
    ld a, (_spectrum_journal_fd)
    and 0x3f
    ret z
    cp 0x3f
    ret z
    ld (_esx_handle), a
    jp _esx_fclose

; The live handle is shared with append: park it at EOF again after a read.
stj_live_eof:
    ld a, (stj_live)
    or a
    ret z
    ld (_esx_handle), a
stj_eof_lp:
    ld hl, STJ_CHUNK
    ld bc, STJ_CHUNK_SIZE
    call stj_io_read_any
    jr c, stj_eof_lost
    ld a, (_esx_result)
    or a
    jr nz, stj_eof_lp
    ret
stj_eof_lost:
    call _esx_fclose
    ld a, 0x7f
    ld (_spectrum_journal_fd), a
    ret

stj_close_tmp:
    ld a, (stj_tmp_fd)
    or a
    ret z
    ld (_esx_handle), a
    xor a
    ld (stj_tmp_fd), a
    jp _esx_fclose

; Copy HL bytes stj_fd -> stj_tmp_fd after full validation. Z on success.
stj_copy_hl:
    ld (stj_left), hl
stj_copy_loop:
    ld hl, (stj_left)
    ld a, h
    or l
    ret z
    ld b, STJ_CHUNK_SIZE
    ld a, h
    or a
    jr nz, stj_copy_take
    ld a, l
    cp STJ_CHUNK_SIZE + 1
    jr nc, stj_copy_take
    ld b, a
stj_copy_take:
    ld a, b
    ld (stj_chunk_n), a
    ld a, (stj_fd)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    ld c, b
    ld b, 0
    call stj_io_read
    ret nz
    ld a, (stj_tmp_fd)
    ld (_esx_handle), a
    ld hl, STJ_CHUNK
    ld a, (stj_chunk_n)
    ld c, a
    ld b, 0
    call stj_io_write
    ret nz
    ld hl, (stj_left)
    ld a, (stj_chunk_n)
    ld c, a
    ld b, 0
    or a
    sbc hl, bc
    ld (stj_left), hl
    jr stj_copy_loop
ENDIF

stj_io_write:
    push bc
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fwrite
    pop bc
    ld hl, (_esx_result)
    or a
    sbc hl, bc
    ret

stj_io_read:
    push bc
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fread
    pop bc
    ld hl, (_esx_result)
    or a
    sbc hl, bc
    ret

stj_io_read_any:
    ld (_esx_buf), hl
    ld (_esx_count), bc
IFDEF NETCHESSZX_SPECTRANEXT
    ; Physical XFS READ at end of file carries with EOF ($21) and 0 bytes:
    ; the esxDOS 0-byte read every caller treats as end of file.
    call _esx_fread
    call _spxn_rom_error
    ld a, l
    or a
    ret z
    cp 0x21
    ret z
    scf
    ret
ELSE
    jp _esx_fread
ENDIF

; CRC-16/CCITT-FALSE. HL = running CRC, DE = data, C = count (1..255).
stj_crc:
    ld b, c
    ld a, b
    or a
    ret z
stj_crc_byte:
    ld a, (de)
    inc de
    xor h
    ld h, a
    push bc
    ld b, 8
stj_crc_bit:
    sla l
    rl h
    jr nc, stj_crc_nox
    ld a, l
    xor 0x21
    ld l, a
    ld a, h
    xor 0x10
    ld h, a
stj_crc_nox:
    djnz stj_crc_bit
    pop bc
    djnz stj_crc_byte
    ret

stj_fill_header:
    ld hl, stj_magic
    ld de, STJ_HDR
    ld bc, 8
    ldir
    ld a, (stj_flags)
    and 0x40
    jr nz, stj_fill_partial
    ld hl, STJ_HDR + 5
    inc (hl)                    ; HISTORY_COMPLETE
stj_fill_partial:
    ld hl, (stj_n)
    ld (STJ_HDR + 8), hl
    ld hl, (stj_jcrc)
    ld (STJ_HDR + 10), hl
    ld hl, 0
    ld (STJ_HDR + 12), hl
    ld (STJ_HDR + 14), hl
    ld hl, (CTX_BUF)
    ld de, STJ_HDR + 16
    ld bc, 60
    ldir
    call stj_header_crc
    ld (STJ_HDR + 14), hl
    ret

; Z when magic, version, flags, size, reserved and header CRC are valid.
; Clears bytes 14..15 as a side effect.
stj_header_ok:
    ld hl, stj_magic
    ld de, STJ_HDR
    ld b, 8
stj_hok_lp:
    ld a, b
    cp 3                        ; byte 5 (flags) is checked below
    jr z, stj_hok_next
    ld a, (de)
    cp (hl)
    ret nz
stj_hok_next:
    inc hl
    inc de
    djnz stj_hok_lp
    ld a, (STJ_HDR + 5)
    and 0xfe
    ret nz
    ld hl, (STJ_HDR + 12)
    ld a, h
    or l
    ret nz
    ld hl, (STJ_HDR + 14)
    push hl
    ld hl, 0
    ld (STJ_HDR + 14), hl
    call stj_header_crc
    pop de
    or a
    sbc hl, de
    ret

stj_header_crc:
    ld de, STJ_HDR
    ld hl, 0xffff
    ld c, STJ_HDR_SIZE
    jp stj_crc

; Validate the complete file at its current position. Z on success.
stj_file_check:
    call stj_read_header
    ret nz
    ld hl, (STJ_HDR + 8)
    add hl, hl
    jr nc, stj_file_body
    or 1
    ret
stj_file_body:
    jp stj_body_check

stj_read_header:
    ld hl, STJ_HDR
    ld bc, STJ_HDR_SIZE
    call stj_io_read
    ret nz
    jp stj_header_ok

; HL = expected body byte count. Reads exactly that many bytes into the CRC,
; validates every v1 record, then requires EOF. A short file, trailing byte,
; invalid record or CRC mismatch fails with Z clear. Reads only; stj_fd must be
; positioned at the body start.
stj_body_check:
    ld (stj_left), hl
    ld hl, 0xffff
    ld (stj_jcrc), hl
stj_bc_loop:
    ld hl, (stj_left)
    ld a, h
    or l
    jr z, stj_bc_eof
    ld b, STJ_CHUNK_SIZE
    ld a, h
    or a
    jr nz, stj_bc_take
    ld a, l
    cp STJ_CHUNK_SIZE + 1
    jr nc, stj_bc_take
    ld b, a
stj_bc_take:
    ld a, b
    ld (stj_chunk_n), a
IFNDEF NETCHESSZX_SPECTRANEXT
    ld a, (stj_fd)
    ld (_esx_handle), a
ENDIF
    ld hl, STJ_CHUNK
    ld c, b
    ld b, 0
    call stj_io_read
    ret nz                      ; short read: smaller than record_count
    ld hl, STJ_CHUNK + 1        ; high byte of each little-endian record
    ld a, (stj_chunk_n)
    srl a
    ld b, a
stj_bc_record:
    ld a, (hl)
    bit 7, a
    jr nz, stj_bc_control
    and 0x70                    ; v1 promotion is 0..4
    cp 0x50
    jr nc, stj_bc_bad_record
    jr stj_bc_record_next
stj_bc_control:
    cp 0x80                    ; v1 control is exactly UNDO_ONE
    jr nz, stj_bc_bad_record
    dec hl
    ld a, (hl)
    inc hl
    or a
    jr nz, stj_bc_bad_record
stj_bc_record_next:
    inc hl
    inc hl
    djnz stj_bc_record
    ld de, STJ_CHUNK
    ld hl, (stj_jcrc)
    ld a, (stj_chunk_n)
    ld c, a
    call stj_crc
    ld (stj_jcrc), hl
    ld hl, (stj_left)
    ld a, (stj_chunk_n)
    ld c, a
    ld b, 0
    or a
    sbc hl, bc
    ld (stj_left), hl
    jr stj_bc_loop
stj_bc_bad_record:
    or 1
    ret
stj_bc_eof:
IFNDEF NETCHESSZX_SPECTRANEXT
    ld a, (stj_fd)
    ld (_esx_handle), a
ENDIF
    ld hl, STJ_CHUNK
    ld bc, 1
    call stj_io_read_any
    jr c, stj_bc_bad_record
    ld a, (_esx_result)
    or a
    ret nz                      ; trailing byte: larger than record_count
    ld hl, (stj_jcrc)
    ld de, (STJ_HDR + 10)
    or a
    sbc hl, de
    ret

; A = 0 load (8.3 name as given), 1 save (a two-digit slot gets the FAT stamp).
stj_build_path:
    ld (stj_chunk_n), a
    ld hl, stj_dir
    ld de, stj_path
stj_cpdir:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, stj_cpdir
    dec de
    ld hl, (stj_ctx)
    ld a, (hl)
    or a
    jr z, stj_path_bad
    ld a, (stj_chunk_n)
    or a
    jr z, stj_cp8
    ld a, (hl)
    cp '0'
    jr c, stj_cp8
    cp '9' + 1
    jr nc, stj_cp8
    inc hl
    ld a, (hl)
    dec hl
    cp '0'
    jr c, stj_cp8
    cp '9' + 1
    jr nc, stj_cp8
    inc hl
    inc hl
    ld a, (hl)
    dec hl
    dec hl
    or a
    jr nz, stj_cp8
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    ld a, (hl)
    ld (de), a
    inc de
    call stj_write_stamp
    jr stj_suf
stj_cp8:
    ld b, 8
stj_cpname:
    ld a, (hl)
    or a
    jr z, stj_suf
    cp '0'
    jr c, stj_path_bad
    cp '9' + 1
    jr c, stj_put
    cp 'A'
    jr c, stj_path_bad
    cp 'Z' + 1
    jr c, stj_put
    cp '_'
    jr z, stj_put
    cp 'a'
    jr c, stj_path_bad
    cp 'z' + 1
    jr nc, stj_path_bad
    sub 32
stj_put:
    ld (de), a
    inc hl
    inc de
    djnz stj_cpname
    ld a, (hl)
    or a
    jr nz, stj_path_bad
stj_suf:
    ld hl, stj_ext
stj_cpsuf:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, stj_cpsuf
    xor a
    ret
stj_path_bad:
    or 1
    ret

; DE = dest for the 6-char FAT stamp YMDHmm (base 32 + decimal minute).
; Same rules as saveload_write_stamp(): runtime date moved by the day delta,
; year 2020..2051, otherwise 000000.
stj_t EQU stj_n                 ; year, month, day, hour, minute (5 bytes)
stj_write_stamp:
    push de
    ld iy, 0x5c3a
    call _spectrum_net_runtime_fat_day_delta
    push hl
    call _spectrum_net_runtime_fat_date
    ld a, h
    srl a
    ld b, a                     ; year since 1980
    ld a, l
    rlca
    rlca
    rlca
    and 7
    bit 0, h
    jr z, stj_ex_m
    or 8
stj_ex_m:
    ld c, a                     ; month
    ld a, l
    and 31
    ld e, a                     ; day
    pop hl
    call stj_adjust_date
    ld l, b
    ld h, c
    ld (stj_t), hl
    ld a, e
    ld (stj_t + 2), a
    call _spectrum_net_runtime_fat_time
    ld a, h
    rrca
    rrca
    rrca
    and 31
    ld (stj_t + 3), a
    add hl, hl
    add hl, hl
    add hl, hl
    ld a, h
    and 63
    ld (stj_t + 4), a
    ld hl, stj_t
    ld de, stj_limits
    ld b, 5
stj_chk:
    ld a, (de)
    inc de
    ld c, a
    ld a, (hl)
    cp c
    jr c, stj_stamp0
    ex de, hl
    cp (hl)
    ex de, hl
    jr nc, stj_stamp0
    inc de
    inc hl
    djnz stj_chk
    ld hl, stj_t
    ld a, (hl)
    sub 40
    ld (hl), a
    jr stj_stamp_out
stj_stamp0:
    ld hl, stj_t
    ld b, 5
stj_zero:
    ld (hl), 0
    inc hl
    djnz stj_zero
stj_stamp_out:
    pop de
    ld hl, stj_t
    ld b, 4
stj_b32:
    ld a, (hl)
    inc hl
    cp 10
    jr c, stj_b32d
    add a, 'A' - 10 - '0'
stj_b32d:
    add a, '0'
    ld (de), a
    inc de
    djnz stj_b32
    ld a, (hl)
    ld b, '0' - 1
stj_tens:
    inc b
    sub 10
    jr nc, stj_tens
    add a, '0' + 10
    ex de, hl
    ld (hl), b
    inc hl
    ld (hl), a
    inc hl
    ex de, hl                   ; DE = past the stamp
    ret

; B=year, C=month, E=day, HL=signed day delta.
stj_adjust_date:
    ld a, h
    or l
    ret z
    bit 7, h
    jr nz, stj_adj_neg
stj_adj_pos:
    ld a, h
    or l
    ret z
    inc e
    ld a, c
    call stj_month_days
    cp e
    jr nc, stj_adj_pos1
    ld e, 1
    inc c
    ld a, c
    cp 13
    jr c, stj_adj_pos1
    ld c, 1
    inc b
stj_adj_pos1:
    dec hl
    jr stj_adj_pos

stj_adj_neg:
    ld a, l
    cpl
    ld l, a
    ld a, h
    cpl
    ld h, a
    inc hl
stj_adj_neg_lp:
    ld a, h
    or l
    ret z
    dec e
    jr nz, stj_adj_neg1
    dec c
    jr nz, stj_adj_neg_md
    ld c, 12
    dec b
stj_adj_neg_md:
    ld a, c
    call stj_month_days
    ld e, a
stj_adj_neg1:
    dec hl
    jr stj_adj_neg_lp

; A=month, B=year. Returns A=days in month. Preserves B,C,E,HL.
stj_month_days:
    cp 2
    jr nz, stj_md_n2
    ld a, b
    and 3
    ld a, 28
    ret nz
    inc a
    ret
stj_md_n2:
    cp 8
    jr c, stj_md_lo
    inc a
stj_md_lo:
    and 1
    add a, 30
    ret

stj_magic:
    defm "STJ1"
    defb 1, 0, STJ_HDR_SIZE, 0
stj_limits:                     ; [lo, hi) for year, month, day, hour, minute
    defb 40, 72, 1, 13, 1, 32, 0, 24, 0, 60
stj_ext:
    defm ".STJ"
    defb 0
IFDEF NETCHESSZX_SPECTRANEXT
stj_dir:
    defm "/CFG/"
    defb 0
stj_dir_end:
stj_mkdir:
    defm "/CFG"
    defb 0
stj_mkdir_end:
stj_tmp:
    defm "/CFG/SHATJRNL.TMP"
    defb 0
stj_tmp_end:
stj_new:
    defm "/CFG/SHATJRNL.NEW"
    defb 0
stj_new_end:
stj_save_tmp:
    defm "/CFG/SHATSAVE.TMP"
    defb 0
stj_save_tmp_end:
ASSERT stj_dir_end - stj_dir <= STJ_PATH_SIZE
ASSERT stj_mkdir_end - stj_mkdir <= STJ_PATH_SIZE
ASSERT stj_tmp_end - stj_tmp <= STJ_PATH_SIZE
ASSERT stj_new_end - stj_new <= STJ_PATH_SIZE
ASSERT stj_save_tmp_end - stj_save_tmp <= STJ_PATH_SIZE
ELSE
stj_dir:
    defm "/SYS/CONFIG/"
    defb 0
stj_mkdir:
    defm "/SYS/CONFIG"
    defb 0
stj_tmp:
    defm "/SYS/CONFIG/SHATJRNL.TMP"
    defb 0
stj_new:
    defm "/SYS/CONFIG/SHATJRNL.NEW"
    defb 0
ENDIF
IFDEF NETCHESSZX_SPECTRANEXT
stj_src_path:
    defw 0
stj_dst_path:
    defw 0
stj_read_base:
    defw 0
stj_write_base:
    defw 0
stj_pos:
    defw 0
stj_file_size:
    defw 0
stj_take:
    defb 0
ELSE
stj_path:
    defs 26
stj_fd:
    defb 0
stj_tmp_fd:
    defb 0
stj_src:
    defb 0
stj_live:
    defb 0
ENDIF
stj_flags:
    defb 0
stj_n:
    defw 0
stj_jcrc:
    defw 0
stj_left:
    defw 0
stj_chunk_n:
    defb 0
