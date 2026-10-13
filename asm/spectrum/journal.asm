; Classic game journal: resident state and ply hooks. All I/O is in overlays:
; append = SAVELOAD entry 3 (asm/overlay/saveload/journal_append.asm),
; LOAD/SAVE = JOURNAL (asm/overlay/journal/entry_journal.asm).
;
; _spectrum_journal_fd: bits 0..5 = live SHATJRNL.TMP handle+1. 0 = none yet
;   (the next append creates/truncates TMP); 0x3F = disabled after a failed
;   write, TMP keeps the valid prefix for SAVE. Bit 6 = history partial.
; _spectrum_journal_pend: history switch for the next append. Bit 7 = armed,
;   bit 6 = partial, bit 0 = SHATJRNL.NEW holds the body to adopt.
;   GAME START writes 0x80, a remote RESTORE 0xC0. JOURNAL LOAD stages bits
;   6/0 with bit 7 clear; applying that snapshot locally sets bit 7. A load
;   that is never applied (decode, colour, peer refusal) stays inert.
;   Every applied snapshot then calls append with HL = 1/3: Classic repaints
;   the list from SHATJRNL.NEW, and Classic, Next and SpectraNext consume the
;   armed switch at once (adopt NEW or create an empty TMP) without a record,
;   so the checkpoint written next already describes the live TMP.
; _spectrum_journal_warning: 0 = clear, 1 = pending, 2 = shown. Reset only
;   when a new game or an accepted snapshot becomes the live history.
; Recovery link (NETCHESSZX_RECOVERY): _spectrum_journal_count/_crc describe
;   the SHATJRNL.TMP prefix that append has made durable: record count and
;   CRC-16/CCITT-FALSE of those 2 * count bytes. Append resets them before
;   every TMP create, accumulates an adopted SHATJRNL.NEW and adds each
;   synced record; failures leave them on the last durable prefix. RECOVERY
;   copies them into each checkpoint so a later resume can prove the link.

SECTION data_user

PUBLIC _spectrum_journal_fd
PUBLIC _spectrum_journal_pend
PUBLIC _spectrum_journal_warning
_spectrum_journal_fd:
    defb 0
_spectrum_journal_pend:
    defb 0
_spectrum_journal_warning:
    defb 0
IFDEF NETCHESSZX_RECOVERY
PUBLIC _spectrum_journal_count
PUBLIC _spectrum_journal_crc
_spectrum_journal_count:
    defw 0
_spectrum_journal_crc:
    defw 0xffff
IFNDEF NETCHESSZX_SPECTRANEXT
; The checkpoint slot this boot last wrote and read back (0xFF = unknown)
; and its generation: RECOVERY's next write skips the two-slot scan.
PUBLIC _spectrum_rcv_cache
_spectrum_rcv_cache:
    defb 0xff
    defw 0
ENDIF
ENDIF

SECTION code_user

PUBLIC _spectrum_journal_after_move
PUBLIC _spectrum_journal_after_takeback
PUBLIC _spectrum_journal_notify

EXTERN _spectrum_overlay_exec
EXTERN _notify_error
EXTERN _spectrum_game_stats

NETCHESSZX_OVERLAY_CONTEXT EQU 0x5fe0
SPECTRUM_OVL_SAVELOAD EQU 10
SPECTRUM_OVL_SAVELOAD_JOURNAL_APPEND EQU 3

; fastcall HL = committed move text ("e7e8q"); TAKEBACK passes 0 (UNDO_ONE).
_spectrum_journal_after_takeback:
    ld hl, 0
_spectrum_journal_after_move:
    push hl                     ; preserve the request across a failed page-in
    ld (NETCHESSZX_OVERLAY_CONTEXT), hl
    ld a, 1
    ld (NETCHESSZX_OVERLAY_CONTEXT+7), a ; cleared by the capture prologue
    ld hl, SPECTRUM_OVL_SAVELOAD + (SPECTRUM_OVL_SAVELOAD_JOURNAL_APPEND * 256)
    push hl
    call _spectrum_overlay_exec
    pop de
    ld a, h
    or l
    ret nz
    ld a, (NETCHESSZX_OVERLAY_CONTEXT+7)
    or a
    jr z, journal_capture_done
    ld a, 0x41                 ; PARTIAL + missing sample, undo invalid
    ld (_spectrum_game_stats+18), a    ; GAME_STATS_FLAGS
journal_capture_done:
    ld a, d
    or a
    jr nz, journal_record_lost
    ld a, e
    and 0xfd                    ; 1/3 repaint failures did not lose a ply
    dec a
    ret z
journal_record_lost:
    ld hl, _spectrum_journal_fd
    set 6, (hl)
    ld hl, _spectrum_journal_pend
    bit 7, (hl)
    jr z, journal_warn
    set 6, (hl)                 ; a failed first append must degrade SAVE too
journal_warn:
    ld hl, _spectrum_journal_warning
    ld a, (hl)
    or a
    ret nz
    inc (hl)
    ret

; Run after the event's turn/result notices, on the next message-loop tick.
_spectrum_journal_notify:
    ld hl, _spectrum_journal_warning
    ld a, (hl)
    dec a
    ret nz
    inc (hl)
    ld hl, journal_warning_text
    push hl
    call _notify_error          ; resident app helper: must work without SD
    pop af
    ret
journal_warning_text:
    defm "History incomplete"
    defb 0
