SECTION code_user
EXTERN _control_classify_ovl
PUBLIC _control_format_busy_ovl_entry
PUBLIC _control_recovery_resume_ovl_entry
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _control_format_busy_ovl
ENDIF
    DEFB 3
    DW _control_classify_ovl_entry
    DW _control_format_busy_ovl_entry
    DW _control_recovery_resume_ovl_entry
DEFC _control_classify_ovl_entry = _control_classify_ovl

; const char *netchess_proto_name_tail(const char *rx) __z88dk_fastcall
; HL = NUL-terminated payload. Returns HL = the name after "MNAME " when it
; is 1..10 characters from A-Z, 0-9, '-' and '_', else HL = 0. Mirrors the
; portable C in src/common/protocol/game_protocol_mach.c.
PUBLIC _netchess_proto_name_tail
_netchess_proto_name_tail:
    ld de, control_name_prefix
control_name_prefix_loop:
    ld a, (de)
    or a
    jr z, control_name_body
    cp (hl)
    jr nz, control_name_fail
    inc hl
    inc de
    jr control_name_prefix_loop
control_name_body:
    ld a, (hl)
    or a
    jr z, control_name_fail
    push hl
    ld b, 11
control_name_char:
    ld a, (hl)
    or a
    jr z, control_name_end
    cp '-'
    jr z, control_name_ok
    cp '_'
    jr z, control_name_ok
    sub '0'
    cp 10
    jr c, control_name_ok
    sub 'A' - '0'
    cp 26
    jr nc, control_name_reject
control_name_ok:
    inc hl
    djnz control_name_char
control_name_reject:
    pop hl
control_name_fail:
    ld hl, 0
    ret
control_name_end:
    pop hl
    ret
control_name_prefix:
    DEFM "MNAME ", 0
_control_format_busy_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _control_format_busy_ovl
ELSE
    ld l, 1
    ret
ENDIF
; Game recovery probe/stage (asm/overlay/recovery/rcv_resume.asm). Cold,
; once per connection: CONTROL has the room RECOVERY's checkpoint path lacks.
; Without the journal hook (tools/build_overlays.py) nothing is ever offered.
IFDEF NETCHESSZX_RECOVERY
IFDEF NETCHESSZX_JOURNAL_HOOK
EXTERN _recovery_resume_ovl
DEFC _control_recovery_resume_ovl_entry = _recovery_resume_ovl
ELSE
_control_recovery_resume_ovl_entry:
    ld a, 1
    ld (0x5fe4), a
    ld hl, 0
    ret
ENDIF
ELSE
_control_recovery_resume_ovl_entry:
    ld a, 1
    ld (0x5fe4), a
    ld hl, 0
    ret
ENDIF
