; SRC2 checkpoint layout shared by RECOVERY and CONTROL (asm/overlay/recovery/
; rcv_store.asm). RCV_REC holds the record being written or the selected
; one; RCV_TMP is the scan/readback buffer. Both live in the overlay scratch.

IFDEF NETCHESSZX_NEXT_BANKING
; Next: every esxDOS RST 8 runs with MMU1 = ROM and DivMMC paged over
; 0x0000..0x3FFF, so the 0x3C78 overlay scratch cannot be a file buffer.
; The store keeps its 160-byte buffer in overlay BSS (rcv_store.asm).
IFNDEF RCV_STORE_MODULE
EXTERN rcv_io
ENDIF
RCV_SCRATCH EQU rcv_io
ELSE
RCV_SCRATCH EQU 0x672b
ENDIF

RCV_SIZE       EQU 73
RCV_SEQ        EQU 4
RCV_SNAPSHOT   EQU 6
RCV_SNAP_SIZE  EQU 60
RCV_COUNT      EQU 66
RCV_BODY_CRC   EQU 68
RCV_FLAGS      EQU 70
RCV_CRC_SIZE   EQU 71

RCV_REC        EQU RCV_SCRATCH
RCV_TMP        EQU RCV_SCRATCH + RCV_SIZE
RCV_EOF        EQU RCV_TMP + RCV_SIZE
ASSERT 2 * RCV_SIZE < 160

; SpectraNext: XFS pages its ROM module over Page B, where overlays run, so
; every path argument is first copied to low RAM. RCV_TMP is free whenever a
; path is needed: OPEN consumes it before READ refills the buffer, and the
; atomic replace takes both paths before the readback scan.
RCV_PATH       EQU RCV_TMP
RCV_PATH2      EQU RCV_TMP + 24
XFS_ENOENT     EQU 2
XFS_EIO        EQU 3
XFS_ERR_EOF    EQU 0x21

; Flags byte (offset 70). Written by the RECOVERY capture entry.
RCV_F_PARTIAL  EQU 0x01        ; journal history was already partial
RCV_F_UNLINKED EQU 0x02        ; count/CRC do not describe the TMP
RCV_F_HOST     EQU 0x04        ; writer was the session host
RCV_F_BLACK    EQU 0x08        ; writer played black
RCV_F_PLAYED   EQU 0x10        ; snapshot ply is not zero

RECOVERY_CTX        EQU 0x5fe0
RECOVERY_CTX_BUF    EQU RECOVERY_CTX + 2
RECOVERY_CTX_RESULT EQU RECOVERY_CTX + 4
RECOVERY_CTX_LINK   EQU RECOVERY_CTX + 6
