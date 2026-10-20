; Spectranext MATCH link state, mirrored from src/spectrum/lowram_map.h and
; checked by tools/check_layering.py.
; LINK_MATCH: nonzero when the DIRECT link goes through the firmware
; matchmaking menu (Setup writes it on LINK selection, CONFIG persists it).
; LINK_FOCUS: Setup's LINK focus, 0 MQTT, 1 DIRECT, 2 MATCH (screen order).
; LINK_MATCH_OK: 1 when the firmware offers matchmaking (build 1.2 or later),
; else 0 and LINK holds only MQTT and DIRECT. TIME_CONFIG init writes it at
; every Setup open.
LINK_MATCH       EQU 0x5bd7
LINK_FOCUS       EQU 0x5bd8
LINK_MATCH_OK    EQU 0x5bd9
LINK_FOCUS_MQTT  EQU 0
LINK_FOCUS_DIRECT EQU 1
LINK_FOCUS_MATCH EQU 2
