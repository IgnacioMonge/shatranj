; SAVELOAD-only F_SYNC. Do not add to ESX_SAVELOAD_ASM (CONFIG 15 B free).

PUBLIC _esx_fsync
EXTERN _esx_handle
IFDEF NETCHESSZX_NEXT_BANKING
INCLUDE "asm/next/extension_bank_layout.asm"
ENDIF

SECTION code_user

_esx_fsync:
    push iy
    push ix
    ld a, (_esx_handle)
    dec a
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9C               ; F_SYNC
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    pop ix
    pop iy
    ret
