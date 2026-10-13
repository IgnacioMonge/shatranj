PUBLIC _spectrum_board_clear_legal_hints

_spectrum_board_clear_legal_hints:
    ld a, SPECTRUM_OVL_HINTS
    ld e, SPECTRUM_OVL_HINTS_CLEAR
    jp call_overlay_cached_ae
