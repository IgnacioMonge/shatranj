; SAVELOAD-only F_SEEK SET. Do not add to ESX_SAVELOAD_ASM (CONFIG).

PUBLIC _esx_fseek_set
EXTERN _esx_handle
IFDEF NETCHESSZX_NEXT_BANKING
INCLUDE "asm/next/extension_bank_layout.asm"
ENDIF

SECTION code_user

; HL = offset. BC high = 0. L = IXL = SEEK_SET. L = 0 success, 0xff error.
_esx_fseek_set:
    push iy
    push ix
    ld a, (_esx_handle)
    dec a
    ld e, l
    ld d, h
    ld bc, 0
    ld ix, 0
    ld l, 0
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9F               ; F_SEEK
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    ld l, 0
    jr nc, esx_fseek_ok
    dec l
esx_fseek_ok:
    pop ix
    pop iy
    ret
