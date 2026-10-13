; Move-list replay of SHATJRNL.NEW, shared by Classic/Next SAVELOAD
; (asm/overlay/saveload/journal_append.asm) and SpectraNext RECOVERY
; (asm/overlay/recovery/rcv_replay.asm). Entry: ja_replay_visible with
; ctx[0..1] = 3 (odd ply) or 1 (even ply); it paints the move log
; (NETCHESSZX_LOWRAM_MOVE_LOG_ADDR, 7 rows x 32 bytes) from ply zero, keeps
; the last 14 plies visible, and exits through ja_apply_consume. A partial,
; missing or unreadable body paints the RESTORED marker instead, in the
; column of the side that moved last (white for 3, black for 1).
;
; The includer defines NETCHESSZX_OVERLAY_CONTEXT, journal_append_buf,
; JA_CHUNK (even), ja_new, ja_rec, ja_apply_consume and the esxDOS/XFS
; externs, plus ja_src (esxDOS) or JA_PATH (SpectraNext path staging).

EXTERN _move_line_count
EXTERN _last_ply_seen
EXTERN _spectrum_gui_side_panels_visible
EXTERN _spectrum_render_moves

; HL was 1 or 3. Only a complete staged body can be replayed from ply zero;
; a partial/unreadable body uses the same RESTORED marker as GUI_LOG.
ja_replay_visible:
    ld a, (_spectrum_journal_pend)
    and 0x41                   ; BODY set, PARTIAL clear
    dec a
    jp nz, ja_replay_marker
    ld iy, 0x5c3a
    call _spectrum_net_background_drain
    xor a
    ld (_move_line_count), a
    ld h, a
    ld l, a
    ld (_last_ply_seen), hl
    ld (ja_replay_ply), hl
    ld (ja_replay_base), hl
    ld (ja_replay_pass), a
    ld hl, 0x5cb6
    ld de, 0x5cb7
    ld bc, 223
    ld (hl), 0
    ldir
IFDEF NETCHESSZX_SPECTRANEXT
    ld hl, 0
    ld (ja_off), hl             ; each chunk read reopens NEW at ja_off
ELSE
    ld hl, ja_new
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jp z, ja_replay_marker
    ld (ja_src), a
ENDIF
    ld c, 0                    ; records remaining in the scratch chunk
ja_replay_loop:
    push bc
    push hl
    call _spectrum_net_background_drain
    pop hl
    pop bc
    ld a, c
    or a
    jr nz, ja_replay_record_ready
    call ja_read_chunk
    jr c, ja_replay_error
    ld a, c
    or a
    jr z, ja_replay_close       ; only a successful zero-byte read is EOF
    rrca                       ; reject a torn record, then count record pairs
    jr c, ja_replay_error
    ld c, a
    ld hl, journal_append_buf
ja_replay_record_ready:
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld (ja_rec), de
    dec c
    push bc
    push hl
    call ja_replay_record
    pop hl
    pop bc
    jr ja_replay_loop
ja_replay_error:
    ld a, 1
ja_replay_close:
    push af
IFDEF NETCHESSZX_SPECTRANEXT
    ld l, 0                    ; no handle outlives a chunk read
ELSE
    ld a, (ja_src)
    ld (_esx_handle), a
    call _esx_fclose
    xor a
    ld (ja_src), a
ENDIF
    pop af
    or l                       ; a failed close also discards the visible prefix
    jp nz, ja_replay_marker
    ld a, (ja_replay_pass)
    or a
    jr nz, ja_replay_done
    inc a
    ld (ja_replay_pass), a
    ld hl, (ja_replay_ply)
    ld (_last_ply_seen), hl
    ld a, h
    or a
    jr nz, ja_replay_window
    ld a, l
    cp 15
    jr c, ja_replay_window_zero
ja_replay_window:
    ld de, 13
    or a
    sbc hl, de
    res 0, l
    jr ja_replay_window_set
ja_replay_window_zero:
    ld hl, 0
ja_replay_window_set:
    ld (ja_replay_base), hl
    ex de, hl
    ld hl, (ja_replay_ply)
    or a
    sbc hl, de
    inc l
    srl l
    ld a, l
    ld (_move_line_count), a
    ld hl, 0
    ld (ja_replay_ply), hl
IFDEF NETCHESSZX_SPECTRANEXT
    ld (ja_off), hl             ; HL = 0: the render pass rereads NEW
ELSE
    ld hl, ja_new
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jp z, ja_replay_marker
    ld (ja_src), a
ENDIF
    ld c, 0
    jp ja_replay_loop
ja_replay_done:
    call _spectrum_gui_side_panels_visible
    ld a, l
    or a
    jr z, ja_replay_quiet
    ld hl, 0x5cb6
    call _spectrum_render_moves
ja_replay_quiet:
    jp ja_apply_consume         ; the painted body becomes the live TMP

ja_replay_marker:
    xor a
    ld (_move_line_count), a
    ld h, a
    ld l, a
    ld (_last_ply_seen), hl
    ld hl, 0x5cb6
    ld d, h
    ld e, l
    inc de
    ld bc, 223
    ld (hl), 0
    ldir
    ld hl, (NETCHESSZX_OVERLAY_CONTEXT)
    bit 1, l
    jr z, ja_replay_marker_black
    ld hl, 1
    ld (_last_ply_seen), hl
    ld a, 1
    ld (_move_line_count), a
    ld hl, 0x5cb6
    call ja_replay_marker_text
    jr ja_replay_done
ja_replay_marker_black:
    ld hl, 2
    ld (_last_ply_seen), hl
    ld a, 1
    ld (_move_line_count), a
    ld hl, 0x5cc8
    call ja_replay_marker_text
    jr ja_replay_done

ja_replay_marker_text:
    push hl
    ld hl, ja_replay_restored
    pop de
    ld bc, 9
    ldir
    ret

; DE is the record, also saved in ja_rec for the text renderer.
ja_replay_record:
    ld a, (ja_replay_pass)
    or a
    jr nz, ja_replay_render
    ld hl, (ja_replay_ply)
    bit 7, d
    jr nz, ja_replay_count_drop
    inc hl
    ld (ja_replay_ply), hl
    ret
ja_replay_count_drop:
    ld a, h
    or l
    ret z
    dec hl
    ld (ja_replay_ply), hl
    ret

ja_replay_render:
    bit 7, d
    jp nz, ja_replay_drop
    ld hl, (ja_replay_ply)
    inc hl
    ld (ja_replay_ply), hl
    ld de, (ja_replay_base)
    or a
    sbc hl, de
    ret z
    ld a, h
    or a
    ret nz
    ld a, l
    cp 15
    ret nc
    ld c, a
    dec a
    srl a
    call ja_replay_row
    bit 0, c
    jr z, ja_replay_black
    call ja_replay_clear
    jp ja_replay_text
ja_replay_black:
    ld de, 18
    add hl, de
    jp ja_replay_text

ja_replay_drop:
    ld hl, (ja_replay_ply)
    ld a, h
    or l
    ret z
    push hl
    ld de, (ja_replay_base)
    or a
    sbc hl, de
    jr z, ja_replay_drop_skip
    ld a, h
    or a
    jr nz, ja_replay_drop_skip
    ld a, l
    cp 15
    jr nc, ja_replay_drop_skip
    ld c, a
    dec a
    srl a
    call ja_replay_row
    bit 0, c
    jr z, ja_replay_drop_black
    call ja_replay_clear
    jr ja_replay_drop_pop
ja_replay_drop_black:
    ld de, 18
    add hl, de
    xor a
    ld b, 14
ja_replay_drop_blank:
    ld (hl), a
    inc hl
    djnz ja_replay_drop_blank
ja_replay_drop_pop:
    pop hl
    jr ja_replay_drop_dec_hl
ja_replay_drop_skip:
    pop hl
ja_replay_drop_dec_hl:
    dec hl
    ld (ja_replay_ply), hl
    ret

; A = row. HL = move-log slot. 0x5cb6 is NETCHESSZX_LOWRAM_MOVE_LOG_ADDR.
ja_replay_row:
    ld hl, 0x5cb6
    ld de, 32
    inc a
ja_replay_row_lp:
    dec a
    ret z
    add hl, de
    jr ja_replay_row_lp

ja_replay_clear:
    push hl
    ld d, h
    ld e, l
    inc de
    ld bc, 31
    ld (hl), 0
    ldir
    pop hl
    ret

ja_replay_text:
    push hl
    ld hl, ja_replay_clock
    pop de
    ld bc, 6
    ldir
    ex de, hl
    ld de, (ja_rec)
    ld a, e
    and 0x3f
    call ja_replay_sq
    ld a, e
    rlca
    rlca
    and 0x03
    ld c, a
    ld a, d
    and 0x0f
    add a, a
    add a, a
    or c
    call ja_replay_sq
    ld a, d
    and 0x70
    jr z, ja_replay_pad
    rrca
    rrca
    rrca
    rrca
    dec a
    push hl
    ld hl, ja_replay_promo
    ld c, a
    ld b, 0
    add hl, bc
    ld a, (hl)
    pop hl
    ld (hl), a
    inc hl
    ld c, 5
    jr ja_replay_pad_n
ja_replay_pad:
    ld c, 4
ja_replay_pad_n:
    ld a, 7
    sub c
    ld b, a
ja_replay_pad_lp:
    ld (hl), ' '
    inc hl
    djnz ja_replay_pad_lp
    ld (hl), 0
    ret

ja_replay_sq:
    push af
    and 7
    add a, 'a'
    ld (hl), a
    inc hl
    pop af
    rrca
    rrca
    rrca
    and 7
    ld c, a
    ld a, '8'
    sub c
    ld (hl), a
    inc hl
    ret

; BC = bytes read into journal_append_buf; Carry on failure (BC undefined).
IFDEF NETCHESSZX_SPECTRANEXT
; The adapter keeps one handle and replay drains the network between
; records, so every chunk opens NEW, seeks to ja_off, reads and closes. XFS
; pages its ROM module over Page B, where overlays run: the path is staged in
; low RAM. A hardware READ at EOF answers 0 bytes with ROM error 0x21.
ja_read_chunk:
    ld hl, ja_new
    ld de, JA_PATH
    push de
ja_read_path:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, ja_read_path
    pop hl
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr z, ja_read_fail
    ld hl, (ja_off)
    call _spxn_xfs_fseek
    ld a, l
    dec a
    or h
    jr nz, ja_read_close_fail
    ld hl, journal_append_buf
    ld (_esx_buf), hl
    ld hl, JA_CHUNK
    ld (_esx_count), hl
    call _esx_fread
    call _spxn_rom_error        ; CLOSE clears the transient read error
    ld a, l
    or a
    jr z, ja_read_ok
    cp 0x21
    jr nz, ja_read_close_fail
ja_read_ok:
    call _esx_fclose
    ld a, l
    or a
    jr nz, ja_read_fail
    ld bc, (_esx_result)
    ld hl, (ja_off)
    add hl, bc
    ld (ja_off), hl
    or a                        ; A = 0 from the CLOSE check: NC
    jr ja_read_iy
ja_read_close_fail:
    call _esx_fclose
ja_read_fail:
    scf
ja_read_iy:
    ld iy, 0x5c3a               ; the drain between records needs IY
    ret
ELSE
; Also the adoption reader. The esxDOS wrapper (Classic and Next) returns
; BC = _esx_result (zero on error); its POP IX/IY epilogue preserves Carry.
ja_read_chunk:
    ld hl, journal_append_buf
    ld bc, JA_CHUNK
    ld a, (ja_src)
    ld (_esx_handle), a
    ld (_esx_buf), hl
    ld (_esx_count), bc
    jp _esx_fread
ENDIF

ja_replay_ply:
    defw 0
ja_replay_base:
    defw 0
ja_replay_pass:
    defb 0
ja_replay_clock:
    defm "--:-- "
ja_replay_promo:
    defm "qrbn"
ja_replay_restored:
    defm "RESTORED"
    defb 0
IFDEF NETCHESSZX_SPECTRANEXT
ja_off:
    defw 0
ENDIF
