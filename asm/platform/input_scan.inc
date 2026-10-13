_spectrum_key_edit_pressed:
    ld hl, 0
    ld bc, 0xfefe
    in a, (c)
    rra
    ret c
    ld b, 0xf7
    in a, (c)
    rra
    ret c
    inc l
    ret

spectrum_key_scan_raw:
    ld bc, 0xfefe
    in a, (c)
    bit 0, a
    jr nz, skp_no_break
    ld b, 0x7f
    in a, (c)
    bit 0, a
    jr nz, skp_no_break
    ld hl, 0x8a
    ret

skp_no_break:
    ld b, 0xbf
    in a, (c)
    bit 0, a
    jr nz, skp_space
    ld hl, 13
    ret

skp_space:
    ld b, 0x7f
    in a, (c)
    bit 0, a
    jr nz, skp_shift
    ld hl, 32
    ret

skp_shift:
    ld b, 0xfe
    in a, (c)
    bit 0, a
    jr nz, skp_symbol_check
    ld hl, skp_shift_table
    call skp_scan_table
    ret c

skp_symbol_check:
    ld b, 0x7f
    in a, (c)
    bit 1, a
    jr nz, skp_letters

skp_symbol:
    ld hl, skp_symbol_table
    jp skp_scan_table

skp_letters:
    ld hl, skp_letters_table
    jp skp_scan_table

; Port high, mask-order offset, then five key results in priority order.
; 255 skips an absent key; zero is a real result that stops scanning.
; A zero port ends the table. C must be restored for modifier fallback.
skp_scan_table:
    ld b, (hl)
    inc hl
    ld a, b
    or a
    jr z, skp_none
    ld c, 0xfe
    in a, (c)
    cpl
    ld c, a
    ld a, (hl)
    inc hl
    ld e, a
    ld d, 0
    push hl
    ld hl, skp_mask_orders
    add hl, de
    ex de, hl
    pop hl
    ld b, 5
skp_scan_row:
    ld a, (de)
    and c
    inc de
    ld a, (hl)
    inc hl
    jr z, skp_scan_next
    cp 255
    jr z, skp_scan_next
    ld l, a
    ld h, 0
    ld c, 0xfe
    scf
    ret
skp_scan_next:
    djnz skp_scan_row
    jr skp_scan_table

skp_none:
    ld hl, 0
    ld c, 0xfe
    ret

skp_mask_orders:
    DEFB 1,2,4,8,16
    DEFB 16,8,4,2,1
    DEFB 8,2,4,16,1
    DEFB 16,4,8,1,2

skp_shift_table:
    DEFB 0xef,0,8,0,0x84,0x81,0x82
    DEFB 0xf7,0,0x88,0x89,255,255,0x83
    DEFB 0

skp_symbol_table:
    DEFB 0xfe,10,'?',':','`','/',255
    DEFB 0xfd,0,'~','|',92,'{','}'
    DEFB 0xfb,0,0,0,0,'<','>'
    DEFB 0xf7,0,'!','@','#','$','%'
    DEFB 0xef,5,'&',39,'(',')','_'
    DEFB 0xdf,0,34,';',0,']','['
    DEFB 0xbf,5,'^','-','+','=',255
    DEFB 0x7f,0,255,255,'.',44,'*'
    DEFB 0

skp_letters_table:
    DEFB 0xfe,10,'c','z','x','v',255
    DEFB 0xfb,0,'q','w','e','r','t'
    DEFB 0xfd,0,'a','s','d','f','g'
    DEFB 0x7f,15,'b','m','n',255,255
    DEFB 0xbf,5,'h','j','k','l',255
    DEFB 0xdf,0,'p','o','i','u','y'
    DEFB 0xf7,0,'1','2','3','4','5'
    DEFB 0xef,5,'6','7','8','9','0'
    DEFB 0
spectrum_input_scan_raw:
    call _spectrum_key_edit_pressed
    ld a, l
    or a
    jr z, sisr_scan_key
    ld hl, 0x90
    ret
sisr_scan_key:
    jp spectrum_key_scan_raw

_spectrum_input_flush_until_release:
    call spectrum_input_queue_clear
    call spectrum_input_scan_raw
    ld a, l
    ld (key_suppress), a
    or a
    ret nz
    ld (key_last), a
    ld (key_repeat_timer), a
    ret

_spectrum_input_suppress_until_release:
    push hl
    call spectrum_input_queue_clear
    pop hl
    ld a, l
    ld (key_suppress), a
    ret
