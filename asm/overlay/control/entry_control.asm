SECTION code_user
EXTERN _control_classify_ovl
PUBLIC _control_format_busy_ovl_entry
IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _control_format_busy_ovl
ENDIF
    DEFB 2
    DW _control_classify_ovl_entry
    DW _control_format_busy_ovl_entry
DEFC _control_classify_ovl_entry = _control_classify_ovl
_control_format_busy_ovl_entry:
IFDEF NETCHESSZX_SPECTRANEXT
    jp _control_format_busy_ovl
ELSE
    ld hl, 1
    ret
ENDIF
