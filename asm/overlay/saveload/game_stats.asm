; Statistics capture before any journal I/O; common/chess/game_stats.c is the
; byte-for-byte host oracle. No resident code or BSS is added by this routine.
SECTION code_user

PUBLIC _game_stats_append
EXTERN _spectrum_game_stats
EXTERN _spectrum_stats_undo
EXTERN _spectrum_journal_pend
EXTERN _netchesszx_session_state
EXTERN _spectrum_time_snapshot

; Layout: common/chess/game_stats.h. Side entries are 8 bytes: total,
; longest + AT, fastest + AT; an AT of 0 means the side has no move yet.
gs_base EQU _spectrum_game_stats + 16
gs_flags EQU _spectrum_game_stats + 18
gs_undo EQU _spectrum_game_stats + 19
gs_captured EQU _spectrum_game_stats + 28

_game_stats_append:
    push ix
    xor a
    ld (0x5fe7), a              ; capture ran even if subsequent I/O fails
    ld hl, (0x5fe0)
    ld a, h
    or a
    jr nz, gs_move
    ld a, l
    or a
    jr z, gs_takeback
    ; Applied snapshot (1/3): no timed history in saves, even with a journal.
    ld hl, (_netchesszx_session_state)
    ld c, 1
    call gs_reset
    jr gs_done
gs_move:
    ld a, (_spectrum_journal_pend)
    bit 7, a
    jr nz, gs_pending_sample
    ld a, (gs_flags)
    bit 6, a                   ; a previous SAVELOAD page-in lost a sample
    jr z, gs_record
gs_pending_sample:
    and 0x40
    rlca
    rlca                       ; 0x40 -> PARTIAL (0x01)
    ld c, a
    ld hl, (_netchesszx_session_state)
    dec hl
    call gs_reset
gs_record:
    ld hl, (_netchesszx_session_state)
    dec hl
    call gs_side_entry
    push ix
    pop hl
    ld de, gs_undo
    ld bc, 8
    ldir
    ld a, (gs_flags)
    ld (de), a
    or 0x20
    ld (gs_flags), a
    call gs_seconds
    ld (gs_sample), hl
    sbc a, a                   ; 0xff: the sample itself saturated
    ld e, (ix+0)
    ld d, (ix+1)
    add hl, de
    jr c, gs_sum_capped
    inc a
    jr nz, gs_sum_store
gs_sum_capped:
    ld a, (gs_side)
    inc a
    add a, a                   ; TOTAL_LIMIT_WHITE << side
    ld hl, gs_flags
    or (hl)
    ld (hl), a
    ld hl, 65535
gs_sum_store:
    ld (ix+0), l
    ld (ix+1), h
    push ix
    pop hl
    inc hl
    inc hl
    ld c, 0                    ; longest: replace when the sample is larger
    call gs_extreme
    ld c, 1                    ; fastest: replace when the sample is smaller
    call gs_extreme
    ld a, 1
    call gs_capture
gs_done:
    pop ix
    ret

gs_takeback:
    ld hl, (_netchesszx_session_state)
    ld a, (gs_flags)
    and 0x20
    jr nz, gs_undo_valid
    ld c, 1
    call gs_reset
    jr gs_done
gs_undo_valid:
    call gs_side_entry
    push ix
    pop de
    ld hl, gs_undo
    ld bc, 8
    ldir
    ld a, (hl)
    and 0xdf
    ld (gs_flags), a
    ld a, 255
    call gs_capture
    jr gs_done

; HL -> value(2), AT(1); C=0 longest, 1 fastest. AT 0 always replaces and
; ties keep the first occurrence. Returns HL past the AT byte.
gs_extreme:
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld a, (hl)
    or a
    jr z, gs_extreme_take
    push hl
    ld hl, (gs_sample)
    dec c
    jr z, gs_extreme_cmp       ; fastest: sample - current
    ex de, hl                  ; longest: current - sample
gs_extreme_cmp:
    or a
    sbc hl, de
    pop hl
    jr nc, gs_extreme_next
gs_extreme_take:
    ld de, (gs_sample)
    dec hl
    ld (hl), d
    dec hl
    ld (hl), e
    inc hl
    inc hl
    ld de, (_netchesszx_session_state)
    srl d
    rr e
    jr nc, gs_at_ready
    inc de
gs_at_ready:
    ld a, d
    or a
    ld a, e
    jr z, gs_at_store
    ld a, 255                  ; move 255 or later
gs_at_store:
    ld (hl), a
gs_extreme_next:
    inc hl
    ret

; HL=base ply, C=initial flags: 0 or PARTIAL.
gs_reset:
    push hl
    push bc
    ld hl, _spectrum_game_stats
    ld de, _spectrum_game_stats + 1
    ld bc, 32
    ld (hl), 0
    ldir
    pop bc
    ld a, c
    ld (gs_flags), a
    pop hl
    ld (gs_base), hl
    ret

; L parity selects the mover: ply-1 for commit, ply-after for TAKEBACK.
gs_side_entry:
    ld a, l
    and 1
    ld (gs_side), a
    ld ix, _spectrum_game_stats
    ret z
    ld ix, _spectrum_game_stats + 8
    ret

; HL=clamped TURN seconds, carry iff overflow. Maximum 99 additions; no
; arithmetic library, I/O, nested overlay or interrupt-state changes.
gs_seconds:
    call _spectrum_time_snapshot
    ld de, 8
    add hl, de
    ld b, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld a, (hl)
    push af
    ld hl, 0
    ld de, 3600
    ld a, b
    or a
    jr z, gs_minutes
gs_hours:
    add hl, de
    jr c, gs_seconds_cap
    djnz gs_hours
gs_minutes:
    ld b, c
    ld de, 60
    ld a, b
    or a
    jr z, gs_seconds_tail
gs_minutes_loop:
    add hl, de
    jr c, gs_seconds_cap
    djnz gs_minutes_loop
gs_seconds_tail:
    pop af
    ld e, a
    ld d, 0
    add hl, de
    ret nc
    jr gs_seconds_capped
gs_seconds_cap:
    pop af
gs_seconds_capped:
    ld hl, 65535
    scf
    ret

; A=+1 commit or -1 undo. Existing board undo supplies real captures,
; including en passant. Promotion without taking a piece contributes none.
gs_capture:
    ld (gs_delta), a
    ld a, (_spectrum_stats_undo+2)
    cp '.'
    jr nz, gs_capture_type
    ld a, (_spectrum_stats_undo+1)
    ld c, a
    ld a, (_spectrum_stats_undo+4)
    cp c
    ret nz
    ld a, (gs_delta)
    inc a
    ld a, c
    jr nz, gs_ep_square
    ld a, (_spectrum_stats_undo)
    and 0x3f
gs_ep_square:
    ld l, a
    ld h, 0
    ld de, 0x5f60
    add hl, de
    ld a, (hl)
    or 0x20
    cp 'p'
    ret nz
gs_capture_type:
    or 0x20
    ld hl, gs_types
    ld de, gs_captured
    ld b, 5
gs_type_loop:
    cp (hl)
    jr z, gs_capture_count
    inc hl
    inc de
    djnz gs_type_loop
    ret
gs_capture_count:
    ld a, (gs_side)
    or a
    ld a, (de)
    jr z, gs_nibble_ready
    rrca
    rrca
    rrca
    rrca
gs_nibble_ready:
    ld c, a
    and 15
    ld b, a
    ld a, (gs_delta)
    add a, b
    cp 16
    jr nc, gs_capture_partial
    ld b, a
    ld a, c
    and 0xf0
    or b
    ld c, a
    ld a, (gs_side)
    or a
    ld a, c
    jr z, gs_capture_store
    rrca
    rrca
    rrca
    rrca
gs_capture_store:
    ld (de), a
    ret
gs_capture_partial:
    ld hl, gs_flags
    set 0, (hl)
    ret

gs_types: defm "pnbrq"
gs_side: defb 0
gs_delta: defb 0
gs_sample: defw 0
