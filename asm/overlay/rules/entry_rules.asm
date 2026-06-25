SECTION code_user

EXTERN _rules_play_ovl
EXTERN _rules_check_ovl

_spectrum_overlay_context EQU 0x5FE0

    DW 2
    DW _rules_play_with_context
    DW _rules_check_with_context

_rules_play_with_context:
    ld de, _spectrum_overlay_context
    jp _rules_play_ovl

_rules_check_with_context:
    ld de, _spectrum_overlay_context
    jp _rules_check_ovl
