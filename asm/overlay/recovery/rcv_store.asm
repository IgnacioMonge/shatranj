; Recovery checkpoint slots, format SRC2. Two fixed files alternate so the
; newest valid record is never the one the next checkpoint overwrites.
; Linked by RECOVERY (write) and CONTROL (resume probe/stage); every buffer
; is the overlay scratch, never retained across an overlay call.
;
; Record, 73 bytes, little endian:
;   0..3   magic "SRC2"
;   4..5   generation (modular uint16; the newer of two valid slots wins)
;   6..65  RESTORE snapshot, Base64URL (board, turn, ply, flags, clocks)
;   66..67 SHATJRNL.TMP record count the snapshot extends
;   68..69 CRC-16/CCITT-FALSE of those 2 * count bytes
;   70     flags, RCV_F_* below
;   71..72 CRC-16/CCITT-FALSE of bytes 0..70

SECTION code_user

PUBLIC rcv_scan_both
PUBLIC rcv_scan
PUBLIC rcv_crc
PUBLIC rcv_close
PUBLIC rcv_magic
PUBLIC rcv_path0
PUBLIC rcv_path1
PUBLIC rcv_valid
PUBLIC rcv_newest
PUBLIC rcv_keep
PUBLIC rcv_seq0
PUBLIC rcv_seq1
PUBLIC rcv_read

EXTERN _esx_fopen
EXTERN _esx_fread
EXTERN _esx_fclose
EXTERN _esx_handle
EXTERN _esx_buf
EXTERN _esx_count
EXTERN _esx_result

RCV_STORE_MODULE EQU 1
INCLUDE "asm/overlay/recovery/rcv_layout.inc"

IFDEF NETCHESSZX_SPECTRANEXT
PUBLIC rcv_stage
EXTERN _spxn_rom_error
ESX_ENOENT EQU XFS_ENOENT
ELSE
ESX_ENOENT EQU 5
ENDIF

; Scan both slots through RCV_TMP. Sets rcv_valid (bit per slot), rcv_seq0/1
; and rcv_newest. With rcv_keep != 0 the newest valid record is also copied
; to RCV_REC. Z = conclusive scan, NZ = uncertain I/O (never guess).
rcv_scan_both:
    xor a
    ld (rcv_valid), a
    ld hl, rcv_path0
    call rcv_scan
    cp 2
    jr z, rcv_scan_uncertain
    or a
    jr nz, rcv_scan_slot1
    ld hl, rcv_valid
    set 0, (hl)
    ld hl, (RCV_TMP + RCV_SEQ)
    ld (rcv_seq0), hl
    xor a
    call rcv_take
rcv_scan_slot1:
    ld hl, rcv_path1
    call rcv_scan
    cp 2
    jr z, rcv_scan_uncertain
    or a
    jr nz, rcv_scan_done
    ld hl, (RCV_TMP + RCV_SEQ)
    ld (rcv_seq1), hl
    ld hl, rcv_valid
    set 1, (hl)
    bit 0, (hl)
    jr z, rcv_take1
    ; Both valid: slot one is newer when seq0 - seq1 is negative (modular);
    ; equal values keep slot zero.
    ld hl, (rcv_seq0)
    ld de, (rcv_seq1)
    or a
    sbc hl, de
    bit 7, h
    jr z, rcv_scan_done
rcv_take1:
    ld a, 1
    call rcv_take
rcv_scan_done:
    xor a
    ret
rcv_scan_uncertain:
    xor a
    inc a                       ; A = 1, NZ
    ret

rcv_take:
    ld (rcv_newest), a
    ld a, (rcv_keep)
    or a
    ret z
    ld hl, RCV_TMP
    ld de, RCV_REC
    ld bc, RCV_SIZE
    ldir
    ret

; HL = path. Reads one slot into RCV_TMP and returns A = 0 valid,
; 1 absent/invalid, 2 uncertain I/O. Only the wrapper's ENOENT is absence.
; Flags are not part of the result: every caller tests A itself.
rcv_scan:
IFDEF NETCHESSZX_SPECTRANEXT
    ld de, RCV_PATH
    call rcv_stage
ENDIF
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    jr nz, rcv_scan_open
IFDEF NETCHESSZX_SPECTRANEXT
    call _spxn_rom_error        ; the adapter reports OPEN errors here
ENDIF
    ld a, l
    cp ESX_ENOENT
    jr z, rcv_scan_invalid
    jr rcv_scan_io
rcv_scan_open:
    ld hl, RCV_TMP
IFDEF NETCHESSZX_SPECTRANEXT
    ld bc, RCV_SIZE + 1         ; one READ binds the size: XFS carries at EOF
ELSE
    ld bc, RCV_SIZE
ENDIF
    call rcv_read
    jr c, rcv_scan_io_close
    ld hl, (_esx_result)
    ld de, RCV_SIZE
    or a
    sbc hl, de
    jr nz, rcv_scan_invalid_close
IFNDEF NETCHESSZX_SPECTRANEXT
    ld hl, RCV_EOF
    ld bc, 1
    call rcv_read
    jr c, rcv_scan_io_close
    ld hl, (_esx_result)
    ld a, h
    or l
    jr nz, rcv_scan_invalid_close
ENDIF
    call rcv_close
    jr nz, rcv_scan_io
    ld de, RCV_TMP
    ld hl, rcv_magic
    ld b, 4
rcv_magic_loop:
    ld a, (de)
    cp (hl)
    jr nz, rcv_scan_invalid
    inc de
    inc hl
    djnz rcv_magic_loop
    ld de, RCV_TMP
    ld hl, 0xffff
    ld c, RCV_CRC_SIZE
    call rcv_crc
    ld de, (RCV_TMP + RCV_CRC_SIZE)
    or a
    sbc hl, de
    jr nz, rcv_scan_invalid
    xor a
    ret
rcv_scan_invalid_close:
    call rcv_close
    jr nz, rcv_scan_io
rcv_scan_invalid:
    ld a, 1
    ret
rcv_scan_io_close:
    call rcv_close
rcv_scan_io:
    ld a, 2
    ret

; HL = buffer, BC = count. C on an I/O error; _esx_result = bytes read.
rcv_read:
    ld (_esx_buf), hl
    ld (_esx_count), bc
IFDEF NETCHESSZX_SPECTRANEXT
    ; XFS READ at end of file is an error (EOF, 0 bytes) on hardware; report
    ; it as the esxDOS-style 0-byte read. Check before CLOSE clears it.
    call _esx_fread
    call _spxn_rom_error
    ld a, l
    or a
    ret z                       ; NC
    cp XFS_ERR_EOF
    ret z                       ; NC: 0 bytes at end of file
    scf
    ret
ELSE
    jp _esx_fread
ENDIF

IFDEF NETCHESSZX_SPECTRANEXT
; HL = NUL-terminated path, DE = low-RAM copy. Returns HL = the copy.
rcv_stage:
    push de
rcv_stage_loop:
    ld a, (hl)
    ld (de), a
    inc hl
    inc de
    or a
    jr nz, rcv_stage_loop
    pop hl
    ret
ENDIF

; Z when the close succeeded.
rcv_close:
    call _esx_fclose
    ld a, l
    or a
    ret

; CRC-16/CCITT-FALSE. HL = running CRC, DE = bytes, C = count (1..255).
rcv_crc:
    ld b, c
rcv_crc_byte:
    ld a, (de)
    inc de
    xor h
    ld h, a
    ld c, 8
rcv_crc_bit:
    add hl, hl
    jr nc, rcv_crc_next
    ld a, h
    xor 0x10
    ld h, a
    ld a, l
    xor 0x21
    ld l, a
rcv_crc_next:
    dec c
    jr nz, rcv_crc_bit
    djnz rcv_crc_byte
    ret

rcv_magic:
    DEFM "SRC2"
IFDEF NETCHESSZX_SPECTRANEXT
rcv_path0:
    DEFM "/CFG/SHATRCV0.RCV"
    DEFB 0
rcv_path1:
    DEFM "/CFG/SHATRCV1.RCV"
    DEFB 0
ELSE
rcv_path0:
    DEFM "/SYS/CONFIG/SHATRCV0.RCV"
    DEFB 0
rcv_path1:
    DEFM "/SYS/CONFIG/SHATRCV1.RCV"
    DEFB 0
ENDIF

; Overlay-local; every entry sets rcv_keep and rcv_scan_both (or, for a
; write, the resident _spectrum_rcv_cache) sets the rest before use (Next
; overlay pages persist between calls).
IFDEF NETCHESSZX_NEXT_BANKING
; File buffer above 0x3FFF (rcv_layout.inc). Kept in code_user: z80asm
; resolves an EQU to a label of another section with the wrong base.
PUBLIC rcv_io
rcv_io:     DEFS 160
ENDIF

SECTION bss_user
rcv_valid:  DEFS 1
rcv_newest: DEFS 1
rcv_keep:   DEFS 1
rcv_seq0:   DEFS 2
rcv_seq1:   DEFS 2
