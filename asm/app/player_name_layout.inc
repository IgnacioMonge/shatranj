; Low-RAM player identity, mirrored from src/spectrum/lowram_map.h and
; checked for both branches by tools/check_layering.py.
; PLAYER_NAME_LINE: "MNAME " plus the configured name (17 bytes).
; PEER_NAME: the name accepted from the current peer (11 bytes).
IFDEF NETCHESSZX_NEXT_BANKING
PLAYER_NAME_LINE EQU 0x3f2a
ELSE
PLAYER_NAME_LINE EQU 0x65d9
ENDIF
PEER_NAME        EQU PLAYER_NAME_LINE + 17
PLAYER_NAME      EQU PLAYER_NAME_LINE + 6
PLAYER_NAME_MAX  EQU 10
