SECTION code_user

PUBLIC _status_phase_ovl_entry

EXTERN _status_phase_ovl

    DW 1
    DW _status_phase_ovl_entry

_status_phase_ovl_entry:
    ex de, hl
    jp _status_phase_ovl
