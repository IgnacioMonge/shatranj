SECTION code_user

; Vendored pixel-only Roller GAME for the isolated kernel test. Production
; timer/clock UI does not link this file.
DEFC ROLLER_BUILD = 2
INCLUDE "roller_z80.inc"
ASSERT R_CODE_SIZE = 130
