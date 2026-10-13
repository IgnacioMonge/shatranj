; ROLLER -- configurable pixel transition kernel for SjASMPlus / ZX Spectrum 48K.
; Select one profile before INCLUDE; FULL is the default and preserves the original API.
ROLLER_PROFILE_FULL    EQU 0
ROLLER_PROFILE_CAPTURE EQU 1
ROLLER_PROFILE_GAME    EQU 2
ROLLER_PROFILE_LINEAR  EQU 3

        IFNDEF ROLLER_BUILD
        DEFINE ROLLER_BUILD ROLLER_PROFILE_FULL
        ENDIF
        ASSERT ROLLER_BUILD >= ROLLER_PROFILE_FULL
        ASSERT ROLLER_BUILD <= ROLLER_PROFILE_LINEAR

; FULL:    phase in A, roller_begin + roller_capture, maps and masks, 12-byte context.
; CAPTURE: phase in A, roller_capture, maps and masks, 10-byte context.
; GAME:    oriented q in A, caller-owned packed capture, maps and masks, 9-byte context.
; LINEAR:  oriented q in A, caller-owned packed capture, full bytes, 6-byte context.
roller_start:
        IF ROLLER_BUILD = ROLLER_PROFILE_FULL
R_H       EQU 0
R_STRIDE  EQU 1
R_MASK    EQU 2
R_DEST    EQU 3
R_NEXT    EQU 5
R_BUF     EQU 7
R_MAP     EQU 9
R_DIR     EQU 11
R_SIZE    EQU 12
        ENDIF
        IF ROLLER_BUILD = ROLLER_PROFILE_CAPTURE
R_H       EQU 0
R_STRIDE  EQU 1
R_MASK    EQU 2
R_DEST    EQU 3
R_BUF     EQU 5
R_MAP     EQU 7
R_DIR     EQU 9
R_SIZE    EQU 10
        ENDIF
        IF ROLLER_BUILD = ROLLER_PROFILE_GAME
R_H       EQU 0
R_STRIDE  EQU 1
R_MASK    EQU 2
R_DEST    EQU 3
R_BUF     EQU 5
R_MAP     EQU 7
R_SIZE    EQU 9
        ENDIF
        IF ROLLER_BUILD = ROLLER_PROFILE_LINEAR
R_H       EQU 0
R_STRIDE  EQU 1
R_DEST    EQU 2
R_BUF     EQU 4
R_SIZE    EQU 6
        ENDIF

        IF ROLLER_BUILD = ROLLER_PROFILE_FULL
; Capture current DEST and incoming NEXT pixels once, before animating.
roller_begin:
        ld l,(ix+R_DEST)
        ld h,(ix+R_DEST+1)
        ld c,(ix+R_NEXT)
        ld b,(ix+R_NEXT+1)
        ld e,(ix+R_BUF)
        ld d,(ix+R_BUF+1)
        ld a,(ix+R_DIR)
        or a
        push bc
        jr z,.order
        ex (sp),hl                 ; down: capture NEXT first, then DEST
.order:
        call roller_capture
        pop hl
        ; fall through into the second capture
        ENDIF

        IF ROLLER_BUILD <= ROLLER_PROFILE_CAPTURE
; Copy H screen rows from HL to packed DE.
roller_capture:
        ld b,(ix+R_H)
.row:
        push bc
        push hl
        ld c,(ix+R_STRIDE)
        ld b,0
        ldir
        pop hl
        call roller_down
        pop bc
        djnz .row
        ret
        ENDIF

        IF ROLLER_BUILD < ROLLER_PROFILE_LINEAR
; FULL/CAPTURE: A=phase. GAME: A=oriented q. Values above H do nothing.
roller_paint:
        ld c,a
        ld a,(ix+R_H)
        ld b,a
        sub c
        ret c
        IF ROLLER_BUILD <= ROLLER_PROFILE_CAPTURE
        ld l,a                     ; H-phase, needed only for downward motion
        ld a,(ix+R_DIR)
        or a
        jr z,.phase
        ld c,l
.phase:
        ENDIF
        ld l,c
        ld h,0
        ld a,c
        ld c,255                   ; identity sentinel, outside valid map rows
        or a
        jr z,.offset               ; exact endpoints: map cursor is unused
        cp b
        jr z,.offset
        ld e,(ix+R_MAP)
        ld d,(ix+R_MAP+1)
        ld a,d
        or e
        jr z,.offset
        ld a,(de)
        ld c,a                     ; previous map entry, always below 192
        add a,l
        ld l,a
        rl h                       ; retain ninth bit of f(0)+q
        inc de
.offset:
        push de                    ; map cursor stays on stack across rows
        push hl
        ld l,(ix+R_DEST)
        ld h,(ix+R_DEST+1)
        ex (sp),hl                 ; HL=initial row, stacked destination
        ld e,(ix+R_BUF)
        ld d,(ix+R_BUF+1)
.address:
        ld a,(ix+R_STRIDE)
.multiply:
        srl a
        jr nc,.skip
        ex de,hl
        add hl,de
        ex de,hl
.skip:
        add hl,hl                  ; ADD preserves Z from the final SRL
        jr nz,.multiply
        pop hl                     ; destination
.row:
        push bc
        push hl
        ex de,hl                   ; HL=source, DE=destination
        ld c,(ix+R_STRIDE)
        ld b,0
        dec c
        jr z,.last                 ; stride=1: only the masked byte remains
        ldir
.last:
        ld a,(hl)                  ; mask $FF reduces this merge to a copy
        ex de,hl
        xor (hl)
        and (ix+R_MASK)
        xor (hl)
        ld (hl),a
        inc de
        pop hl
        call roller_down
        pop bc
        djnz .cont
        pop hl                     ; discard map cursor
        ret
.cont:
        ld a,c
        inc a
        jr z,.row                  ; identity: source already points to next row
        ex (sp),hl                 ; exchange destination with map cursor
        ld a,(hl)
        scf
        sbc a,c                    ; adjustment = next - previous - 1
        ld c,(hl)
        inc hl
        ex (sp),hl
        jr z,.row
        push hl
        ld l,a
        sbc a,a                    ; sign-extend differences above 127 too
        ld h,a
        jr .address
        ELSE
; LINEAR: A=oriented q, identity rows and full bytes.
roller_paint:
        ld c,a
        ld a,(ix+R_H)
        ld b,a
        sub c
        ret c
        ld l,c
        ld h,0
        ld e,(ix+R_BUF)
        ld d,(ix+R_BUF+1)
        ld a,(ix+R_STRIDE)
.multiply:
        srl a
        jr nc,.skip
        ex de,hl
        add hl,de
        ex de,hl
.skip:
        add hl,hl
        jr nz,.multiply
        ld l,(ix+R_DEST)
        ld h,(ix+R_DEST+1)
.row:
        push bc
        push hl
        ex de,hl
        ld c,(ix+R_STRIDE)
        ld b,0
        ldir
        ex de,hl
        pop hl
        call roller_down
        pop bc
        djnz .row
        ret
        ENDIF

; Next Spectrum bitmap scanline. Preserves BC/DE.
roller_down:
        inc h
        ld a,h
        and 7
        ret nz
        ld a,l
        add a,32
        ld l,a
        ret c
        ld a,h
        sub 8
        ld h,a
        ret

roller_end:
R_CODE_SIZE EQU roller_end-roller_start
