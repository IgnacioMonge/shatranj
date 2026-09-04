; esxDOS FILE I/O - copied from SpectalkZX 60_protocol_storage.asm method.
; Parameter passing via globals; callers set esx_buf/esx_count before I/O.

IFDEF NETCHESSZX_NEXT_BANKING
INCLUDE "asm/next/extension_bank_layout.asm"
ENDIF

IFDEF ESX_FILEUI
PUBLIC _esx_fclose
PUBLIC _esx_opendir
PUBLIC _esx_readdir
ELSE
PUBLIC _esx_fopen
PUBLIC _esx_fread
PUBLIC _esx_fclose
PUBLIC _esx_fcreate
PUBLIC _esx_fcreate_new
PUBLIC _esx_fwrite
PUBLIC _esx_funlink
PUBLIC _esx_mkdir
ENDIF

PUBLIC _esx_handle
PUBLIC _esx_buf
PUBLIC _esx_count
PUBLIC _esx_result

SECTION bss_user
; Not CRT-zeroed: esx_* globals are set by callers before each operation.
_esx_handle:  defs 1
_esx_buf:     defs 2
_esx_count:   defs 2
_esx_result:  defs 2

SECTION code_user

IFDEF ESX_FILEUI

; uint8_t esx_fclose(void)
; Input: _esx_handle
; Output: L = 0 on success, 0xff on error.
; Preserves IY and IX.
_esx_fclose:
    push iy
    push ix
    ld a, (_esx_handle)
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9B           ; F_CLOSE
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    sbc a, a
    ld l, a
    jr esx_pop_ix_iy_ret

; void esx_opendir(const char *path) __z88dk_fastcall
; Output: _esx_handle = dir handle (0 on error). Preserves IY and IX.
_esx_opendir:
    push iy
    push ix
    push hl
    pop ix
    ld b, 0x00          ; short-name entries only
    ld a, '*'
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0xA3           ; F_OPENDIR
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr nc, esx_open_ok
    xor a
esx_open_ok:
    ld (_esx_handle), a
    jr esx_pop_ix_iy_ret

; void esx_readdir(void)
; Input: _esx_handle (dir), _esx_buf = entry buffer (>= 24 bytes)
; Output: _esx_result = 1 entry read, 0 on end/error. Preserves IY and IX.
_esx_readdir:
    push iy
    push ix
    ld a, (_esx_handle)
    ld ix, (_esx_buf)
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0xA4           ; F_READDIR
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr c, esx_rd_none
    ld c, a
    ld b, 0
    jr esx_io_ok
esx_rd_none:
    ld bc, 0
esx_io_ok:
    ld (_esx_result), bc
esx_pop_ix_iy_ret:
    pop ix
    pop iy
    ret

ELSE

; void esx_fopen(const char *path) __z88dk_fastcall
; Input: HL = path string
; Output: _esx_handle = file handle (0 on error)
; Preserves IY. Sets IX = HL (esxDOS needs both).
_esx_fopen:
    ld b, 0x01          ; FA_READ
    jr esx_open_common

_esx_fcreate:
    ld b, 0x0E          ; FA_WRITE | FA_CREATE_AL (0x02 write + 0x0C create/trunc)
    jr esx_open_common

_esx_fcreate_new:
    ld b, 0x06          ; FA_WRITE | FA_CREATE_NEW: never truncate an old file

esx_open_common:
    push iy
    push ix
    push hl
    pop ix              ; IX = HL = path (esxDOS wants both)
    ld a, '*'           ; default drive
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9A           ; F_OPEN
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr nc, esx_open_ok
    xor a               ; error -> handle = 0
esx_open_ok:
    ld (_esx_handle), a
    jr esx_pop_ix_iy_ret

; void esx_fread(void)
; Input: _esx_handle, _esx_buf, _esx_count
; Output: _esx_result = bytes read (0 on error)
; Preserves IY and IX.
_esx_fread:
    push iy
    push ix
    ld a, (_esx_handle)
    ld ix, (_esx_buf)
    ld bc, (_esx_count)
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9D           ; F_READ
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr esx_io_epilogue

; uint8_t esx_fclose(void)
; Input: _esx_handle
; Output: L = 0 on success, 0xff on error.
; Preserves IY and IX.
_esx_fclose:
    push iy
    push ix
    ld a, (_esx_handle)
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9B           ; F_CLOSE
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    sbc a, a
    ld l, a
    jr esx_pop_ix_iy_ret

; void esx_fwrite(void)
; Input: _esx_handle, _esx_buf, _esx_count
; Output: _esx_result = bytes written (0 on error)
; Preserves IY and IX.
_esx_fwrite:
    push iy
    push ix
    ld a, (_esx_handle)
    ld ix, (_esx_buf)
    ld bc, (_esx_count)
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0x9E           ; F_WRITE
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF

esx_io_epilogue:
    jr nc, esx_io_ok
    ld bc, 0
esx_io_ok:
    ld (_esx_result), bc
esx_pop_ix_iy_ret:
    pop ix
    pop iy
    ret

; void esx_funlink(const char *path) __z88dk_fastcall
; Output: _esx_result = 1 on success, 0 on error. Preserves IY and IX.
_esx_funlink:
    push iy
    push ix
    push hl
    pop ix
    ld a, '*'
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0xAD           ; F_UNLINK
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    ld bc, 1
    jr nc, esx_io_ok
    ld bc, 0
    jr esx_io_ok

; void esx_mkdir(const char *path) __z88dk_fastcall
; Existing-directory errors are intentionally left to the following F_OPEN.
; Preserves IY and IX.
_esx_mkdir:
    push iy
    push ix
    push hl
    pop ix
    ld a, '*'
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, 0xff
ENDIF
    rst 8
    defb 0xAA           ; F_MKDIR
IFDEF NETCHESSZX_NEXT_BANKING
    DEFB 0xed, 0x91, 0x51, next_extension_page
ENDIF
    jr esx_pop_ix_iy_ret

ENDIF
