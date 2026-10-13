; Classic persist append (SAVELOAD entry 3). State contract and bit layout:
; asm/spectrum/journal.asm. A storage failure only degrades the journal to
; partial/disabled; it never reports back into play.

SECTION code_user

PUBLIC _saveload_journal_append_ovl
IFDEF NETCHESSZX_SPECTRANEXT
PUBLIC _stj_spxn_adopt_new
ENDIF

EXTERN _esx_fopen
EXTERN _esx_fcreate
EXTERN _esx_fread
EXTERN _esx_fwrite
EXTERN _esx_fclose
EXTERN _esx_fsync
EXTERN _esx_funlink
EXTERN _esx_mkdir
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result
EXTERN _spectrum_journal_fd
EXTERN _spectrum_journal_pend
EXTERN _spectrum_net_background_drain
EXTERN _spectrum_journal_warning
EXTERN _game_stats_append
IFDEF NETCHESSZX_RECOVERY
EXTERN _spectrum_journal_count
EXTERN _spectrum_journal_crc
ENDIF

NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0
; Must equal the linked _overlay_scratch_base; check_lowmem_layout.py proves it.
; Next: esxDOS runs every RST 8 with MMU1 = ROM and DivMMC over 0x0000..0x3FFF,
; so the 0x3C78 scratch cannot be a file buffer. SAVELOAD keeps one 160-byte
; buffer above 0x3FFF in overlay BSS, shared with the STJ codec.
IFDEF NETCHESSZX_NEXT_BANKING
PUBLIC ja_io_buf
journal_append_buf EQU ja_io_buf
ELSE
journal_append_buf EQU 0x672B
ENDIF
IFDEF NETCHESSZX_SPECTRANEXT
JA_CHUNK EQU 32
ELSE
JA_CHUNK EQU 160
ENDIF
ASSERT JA_CHUNK > 0
ASSERT JA_CHUNK <= 160

IFDEF NETCHESSZX_SPECTRANEXT
JA_PATH EQU journal_append_buf + JA_CHUNK
JA_PATH_SIZE EQU 18
ASSERT JA_PATH + JA_PATH_SIZE <= journal_append_buf + 160
EXTERN _esx_fopen_update
EXTERN _esx_fseek_end
EXTERN _spxn_xfs_fseek
EXTERN _spxn_rom_error
EXTERN _esx_commit
EXTERN _esx_fopen_write_trunc
ENDIF

; ctx[0..1] = committed move text, or 0 for TAKEBACK. Returns HL = 1 once the
; record is on the card, 0 otherwise. HL = 1/3 is the apply-time call: Classic
; and Next first paint SHATJRNL.NEW into the move list; then an armed switch
; is consumed without appending a record (see asm/spectrum/journal.asm).
_saveload_journal_append_ovl:
    call _game_stats_append
    ld hl, (NETCHESSZX_OVERLAY_CONTEXT)
    ld a, h
    or a
    jr nz, ja_not_replay
    ld a, l
    cp 1
    jr z, ja_replay_enter
    cp 3
    jr nz, ja_not_replay
ja_replay_enter:
IFDEF NETCHESSZX_SPECTRANEXT
    jp ja_apply_consume         ; RECOVERY has already painted the history
ELSE
    jp ja_replay_visible
ENDIF
ja_not_replay:
    ld a, h
    or l
    jr nz, ja_move
    ld h, 0x80                  ; UNDO_ONE
    jr ja_have_rec
ja_move:                        ; text was validated before the board moved
    call ja_square
    ld e, a                     ; from
    call ja_square
    ld d, a                     ; to
    push de
    ld a, (hl)                  ; promotion letter or NUL
    or 0x20
    ld c, a
    ld hl, ja_promo
    ld b, 4
ja_promo_lp:
    ld a, (hl)
    cp c
    jr z, ja_promo_hit
    inc hl
    djnz ja_promo_lp
ja_promo_hit:                   ; B = 1 queen, 2 rook, 3 bishop, 4 knight, 0
    pop de                      ; E = from, D = to
    ld a, d
    rrca
    rrca
    ld h, a
    and 0xc0
    or e
    ld l, a                     ; from | (to & 3) << 6
    ld a, b
    rlca
    rlca
    rlca
    rlca
    xor h
    and 0xf0
    xor h
    ld h, a                     ; (to >> 2) | promo << 4
ja_have_rec:
    ld (ja_rec), hl
    xor a                       ; a record follows the switch
    jr ja_run
ja_apply_consume:
    ld a, 1                     ; apply: consume the switch only
ja_run:
    ld (ja_norec), a
    ld iy, 0x5c3a
    call _spectrum_net_background_drain
IFDEF NETCHESSZX_SPECTRANEXT
    jp ja_spxn_append
ELSE
    ld hl, _spectrum_journal_pend
    ld a, (hl)
    or a
    jp p, ja_live
    ld (hl), 0                  ; armed switch is consumed exactly once
    push af
    call ja_close_live
    pop af
    ld c, a
    and 0x40
    ld (_spectrum_journal_fd), a
    bit 0, c
    jp z, ja_create
    ld hl, ja_new
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, ja_body_lost
    ld (ja_src), a
    call ja_create_tmp
    jr z, ja_adopt_nocreate
    ld (ja_dst), a
ja_adopt_lp:
    call ja_read_chunk
    jr c, ja_adopt_failed
    ld a, c                    ; ja_read_chunk returns BC = bytes read
    or a
    jr z, ja_adopt_done
    ld c, a
    ld b, 0
    ld a, (ja_dst)
    ld (_esx_handle), a
    ld hl, journal_append_buf
    call ja_io_write
IFDEF NETCHESSZX_RECOVERY
    jr nz, ja_adopt_failed
    call ja_link_chunk          ; C = bytes now in TMP
    jr ja_adopt_lp
ELSE
    jr z, ja_adopt_lp
ENDIF
ja_adopt_failed:
    ld a, (ja_dst)
    ld (_esx_handle), a
    call _esx_fclose            ; what reached TMP is the valid prefix
    call ja_close_src
    jr ja_disable
ja_adopt_nocreate:
    call ja_close_src
    jr ja_create_failed
ja_adopt_done:
    ld a, (ja_dst)
    ld (_esx_handle), a
    call _esx_fsync             ; TMP durable before NEW goes: an apply-time
    jr c, ja_adopt_failed       ; adoption may wait minutes for a record
    call ja_close_src
    ld hl, ja_new
    call _esx_funlink
    ld a, (ja_dst)
    jr ja_have_handle
ja_body_lost:
IFDEF NETCHESSZX_RECOVERY
    call ja_link_reset          ; the old TMP no longer names this history
ENDIF
    jr ja_create_failed          ; old TMP is ineligible; NEW stays available
ja_live:
    ld a, (ja_norec)
    or a
    jr nz, ja_ok                ; apply with nothing armed: no-op
    ld a, (_spectrum_journal_fd)
    and 0x3f
    cp 0x3f
    jr z, ja_fail               ; disabled for the rest of this history
    or a
    jr nz, ja_write
ja_create:
    call ja_create_tmp
    jr z, ja_create_failed
ja_have_handle:
    ld hl, _spectrum_journal_fd
    or (hl)
    ld (hl), a
    ld a, (ja_norec)
    or a
    jr nz, ja_ok                ; apply: the new history has no record yet
ja_write:
    ld a, (_spectrum_journal_fd)
    and 0x3f
    ld (_esx_handle), a
    ld hl, ja_rec
    ld bc, 2
    call ja_io_write
    jr nz, ja_write_failed
    call _esx_fsync
    jr c, ja_write_failed
IFDEF NETCHESSZX_RECOVERY
    ld c, 2
    ld de, ja_rec
    call ja_link_add
ENDIF
ja_ok:
    ld hl, 1
    ret
ja_create_failed:
    ld a, 0x40                  ; no TMP: retry next ply, history partial
    jr ja_lost
ja_write_failed:
    call _esx_fclose
ja_disable:
    ld a, 0x7f                  ; partial + disabled
ja_lost:
    ld (_spectrum_journal_fd), a
    ld hl, _spectrum_journal_warning
    ld a, (hl)
    or a
    jr nz, ja_fail
    inc (hl)                    ; an apply-time loss has no resident caller
ja_fail:
    ld hl, 0
    ret

ENDIF

; Recovery link: reset before any TMP create, add every durable byte.
IFDEF NETCHESSZX_RECOVERY
ja_link_reset:
    ld hl, 0
    ld (_spectrum_journal_count), hl
    dec hl
    ld (_spectrum_journal_crc), hl
    ret

; C = even byte count at journal_append_buf (adoption chunk).
ja_link_chunk:
    ld de, journal_append_buf
; C = byte count at DE (1..255): count += C / 2, CRC over the bytes.
ja_link_add:
    ld a, c
    srl a
    push de
    ld hl, (_spectrum_journal_count)
    ld e, a
    ld d, 0
    add hl, de
    ld (_spectrum_journal_count), hl
    pop de
    ld b, c
    ld hl, (_spectrum_journal_crc)
ja_link_byte:
    ld a, (de)
    inc de
    xor h
    ld h, a
    ld c, 8
ja_link_bit:
    add hl, hl
    jr nc, ja_link_next
    ld a, h
    xor 0x10
    ld h, a
    ld a, l
    xor 0x21
    ld l, a
ja_link_next:
    dec c
    jr nz, ja_link_bit
    djnz ja_link_byte
    ld (_spectrum_journal_crc), hl
    ret
ENDIF

IFDEF NETCHESSZX_SPECTRANEXT

; --- SpectraNext: one adapter handle at a time -----------------------------
; fd carries no live handle: low bit 1 = TMP exists, 0x3F = disabled, bit 6 =
; partial. Every append reopens TMP with esx_fopen_update() and seeks to EOF
; with esx_fseek_end(); no descriptor survives another open. XFS calls page
; their ROM module over Page B, so stage every overlay-local path in low RAM.
ja_spxn_stage_path:
    ld de, JA_PATH
    push de
ja_spxn_stage_path_loop:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, ja_spxn_stage_path_loop
    pop hl
    ret

ja_spxn_append:
    ld hl, _spectrum_journal_pend
    ld a, (hl)
    or a
    jp p, ja_spxn_live
    ld (hl), 0                  ; armed switch is consumed exactly once
    ld c, a
    and 0x40
    ld (_spectrum_journal_fd), a
    bit 0, c
    jr z, ja_spxn_create        ; new game: fresh TMP
    call _stj_spxn_adopt_new
    jr nz, ja_spxn_body_lost
    jr ja_spxn_live
ja_spxn_body_lost:
    ld a, (_spectrum_journal_fd)
    bit 0, a                    ; set only after TMP was safely truncated
    jr z, ja_spxn_create_failed
    ld a, 0x7f                  ; preserve NEW/new prefix and stop this history
    ld (_spectrum_journal_fd), a
    jr ja_spxn_fail
ja_spxn_live:
    ld a, (ja_norec)
    or a
    jr nz, ja_spxn_ok           ; apply: the switch is consumed, no record
    ld a, (_spectrum_journal_fd)
    and 0x3f
    cp 0x3f
    jr z, ja_spxn_fail          ; disabled for the rest of this history
    and 1
    jr z, ja_spxn_create        ; no TMP yet
ja_spxn_open:
    ld hl, ja_tmp
    call ja_spxn_stage_path
    call _esx_fopen_update
    ld a, (_esx_handle)
    or a
    jr nz, ja_spxn_io
    ld a, 0x7f                  ; known prefix is unreachable: partial + off
    ld (_spectrum_journal_fd), a
    jr ja_spxn_fail
ja_spxn_create:
    call ja_create_tmp          ; empty, closed and committed on success
    jr z, ja_spxn_create_failed
    ld hl, _spectrum_journal_fd
    set 0, (hl)
    jr ja_spxn_live             ; apply stops here; a record reopens TMP
ja_spxn_create_failed:
    ld a, 0x40                  ; no TMP: retry next ply, history partial
    ld (_spectrum_journal_fd), a
    jr ja_spxn_fail
ja_spxn_io:
    call _esx_fseek_end
    ld a, l
    or a
    jr nz, ja_spxn_write_failed
    ld hl, (ja_rec)
    ld (journal_append_buf), hl ; XFS ROM pages over overlay-local ja_rec
    ld hl, journal_append_buf
    ld bc, 2
    call ja_io_write
    jr nz, ja_spxn_write_failed
    call _esx_fclose
    ld a, l
    or a
    jr nz, ja_spxn_write_closed_failed
IFDEF NETCHESSZX_RECOVERY
    ld c, 2
    ld de, journal_append_buf
    call ja_link_add            ; the record is durable once CLOSE succeeds
ENDIF
ja_spxn_ok:
    ld hl, 1
    ret
ja_spxn_write_failed:
    call _esx_fclose
ja_spxn_write_closed_failed:
    ld a, 0x7f                  ; partial + disabled
    ld (_spectrum_journal_fd), a
ja_spxn_fail:
    ld a, (ja_norec)
    or a
    jr z, ja_spxn_fail_ret      ; a lost record is reported by the resident hook
    ld hl, _spectrum_journal_warning
    ld a, (hl)
    or a
    jr nz, ja_spxn_fail_ret
    inc (hl)                    ; an apply-time loss has no resident caller
ja_spxn_fail_ret:
    ld hl, 0
    ret

; Copy SHATJRNL.NEW into SHATJRNL.TMP in bounded blocks, one live handle at
; a time. Z when TMP now holds the staged body; NZ on failure.
_stj_spxn_adopt_new:
    xor a
    ld (ja_off), a
    ld (ja_off + 1), a
    call ja_create_tmp           ; truncate/create exactly once before copying
    jp z, ja_spxn_adopt_lost
    ld hl, _spectrum_journal_fd
    set 0, (hl)                 ; TMP was truncated and closed successfully
ja_spxn_adopt_loop:
    ld hl, ja_new
    call ja_spxn_stage_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jp z, ja_spxn_adopt_lost
    ld hl, (ja_off)
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jp nz, ja_spxn_adopt_close_lost
    ld hl, journal_append_buf
    ld (_esx_buf), hl
    ld bc, JA_CHUNK
    ld (_esx_count), bc
    call _esx_fread
    call _spxn_rom_error         ; CLOSE clears the transient read error
    ld a, l
    or a
    jr z, ja_spxn_adopt_read_ok
    cp 0x21                      ; hardware READ at EOF: 0 bytes, carry
    jr nz, ja_spxn_adopt_close_lost
ja_spxn_adopt_read_ok:
    call _esx_fclose
    ld a, l
    or a
    jp nz, ja_spxn_adopt_lost
    ld hl, (_esx_result)
    ld a, h
    or l
    jr z, ja_spxn_adopt_done
    ld (ja_chunk_n), hl
    ld hl, ja_tmp
    call ja_spxn_stage_path
    call _esx_fopen_update
    ld a, (_esx_handle)
    or a
    jr nz, ja_spxn_adopt_seek
    jr ja_spxn_adopt_lost        ; never recreate a failed copy mid-stream
ja_spxn_adopt_seek:
    ld hl, (ja_off)
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, ja_spxn_adopt_close_lost
ja_spxn_adopt_write:
    ld hl, journal_append_buf
    ld (_esx_buf), hl
    ld hl, (ja_chunk_n)
    ld (_esx_count), hl
    call _esx_fwrite
    ld hl, (_esx_result)
    ld de, (ja_chunk_n)
    or a
    sbc hl, de
    jr nz, ja_spxn_adopt_close_lost
    call _esx_fclose
    ld a, l
    or a
    jr nz, ja_spxn_adopt_lost
IFDEF NETCHESSZX_RECOVERY
    ld a, (ja_chunk_n)
    ld c, a
    call ja_link_chunk          ; this chunk is durable in TMP
ENDIF
    ld hl, (ja_off)
    ld de, (ja_chunk_n)
    add hl, de
    ld (ja_off), hl
    jp ja_spxn_adopt_loop
ja_spxn_adopt_close_lost:
    call _esx_fclose
ja_spxn_adopt_lost:
    ld a, 1
    or a
    ret
ja_spxn_adopt_done:
    ld hl, ja_new
    call ja_spxn_stage_path
    call _esx_funlink
    ld a, (_spectrum_journal_fd)
    or 1
    ld (_spectrum_journal_fd), a
    xor a
    ret

ENDIF

; HL = "e2" text -> A = square 0..63 (a8 = 0), HL advanced by two.
ja_square:
    ld a, (hl)
    sub 'a'
    ld c, a
    inc hl
    ld a, '8'
    sub (hl)
    inc hl
    add a, a
    add a, a
    add a, a
    add a, c
    and 63
    ret

ja_close_live:
    ld a, (_spectrum_journal_fd)
    and 0x3f
    ret z
    cp 0x3f
    ret z
    ld (_esx_handle), a
    jp _esx_fclose

ja_close_src:
    ld a, (ja_src)
    ld (_esx_handle), a
    jp _esx_fclose

; esxDOS: returns A = handle+1 and NZ, or Z on failure.
; SpectraNext: NZ once an empty TMP exists and is closed, Z on failure. A new
; XFS file lives in cartridge RAM until its first commit; committing TMP puts
; the journal in flash, so it survives power-off like the checkpoints. An
; existing TMP is truncated in place with O_WRONLY|O_TRUNC (never O_CREAT on
; a flash path); a missing one is created after MKDIR. COMMIT follows either
; way: on a file already in flash it returns EIO, which SpectraNext's
; open-update hardware proof records as expected and harmless.
ja_create_tmp:
IFDEF NETCHESSZX_RECOVERY
    call ja_link_reset
ENDIF
IFDEF NETCHESSZX_SPECTRANEXT
    ld hl, ja_tmp
    call ja_spxn_stage_path
    call _esx_fopen_write_trunc
    ld a, (_esx_handle)
    or a
    jr nz, ja_create_opened     ; existing TMP: truncated in place
ENDIF                           ; absent (or unreadable): create as before
    ld hl, ja_dir
IFDEF NETCHESSZX_SPECTRANEXT
    call ja_spxn_stage_path
ENDIF
    call _esx_mkdir             ; also materializes a committed /CFG
    ld hl, ja_tmp
IFDEF NETCHESSZX_SPECTRANEXT
    call ja_spxn_stage_path
ENDIF
    call _esx_fcreate
    ld a, (_esx_handle)
    or a
IFDEF NETCHESSZX_SPECTRANEXT
    ret z
ja_create_opened:
    call _esx_fclose
    ld a, l
    or a
    jr nz, ja_create_fail
    ld hl, ja_tmp
    call ja_spxn_stage_path
    call _esx_commit            ; result ignored: flash paths answer EIO
    or 1                        ; NZ: TMP exists, committed or not
    ret
ja_create_fail:
    xor a
ENDIF
    ret

; HL = buffer, BC = count. Z when every byte was written.
ja_io_write:
    push bc
    ld (_esx_buf), hl
    ld (_esx_count), bc
    call _esx_fwrite
    pop bc
    ld hl, (_esx_result)
    or a
    sbc hl, bc
    ret

IFNDEF NETCHESSZX_SPECTRANEXT
; SpectraNext replays from RECOVERY before this consume (rcv_replay.asm).
INCLUDE "asm/overlay/journal/journal_replay.inc"
ENDIF

ja_promo:
    defm "nbrq"
IFDEF NETCHESSZX_SPECTRANEXT
ja_dir:
    defm "/CFG"
    defb 0
ja_dir_end:
ja_tmp:
    defm "/CFG/SHATJRNL.TMP"
    defb 0
ja_tmp_end:
ja_new:
    defm "/CFG/SHATJRNL.NEW"
    defb 0
ja_new_end:
ASSERT ja_dir_end - ja_dir <= JA_PATH_SIZE
ASSERT ja_tmp_end - ja_tmp <= JA_PATH_SIZE
ASSERT ja_new_end - ja_new <= JA_PATH_SIZE
ja_off:
    defw 0
ja_chunk_n:
    defw 0
ELSE
ja_dir:
    defm "/SYS/CONFIG"
    defb 0
ja_tmp:
    defm "/SYS/CONFIG/SHATJRNL.TMP"
    defb 0
ja_new:
    defm "/SYS/CONFIG/SHATJRNL.NEW"
    defb 0
ENDIF
ja_rec:
    defw 0
ja_norec:
    defb 0
ja_src:
    defb 0
ja_dst:
    defb 0

IFDEF NETCHESSZX_NEXT_BANKING
; esxDOS file buffer above 0x3FFF. Kept in code_user: z80asm resolves an EQU
; to a label of another section with the wrong base.
ja_io_buf:
    defs 160
ENDIF
