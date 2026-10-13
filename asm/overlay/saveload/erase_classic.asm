; Classic LOAD/SAVE use JOURNAL. Keep the old entry table and a compact ERASE
; operation instead of linking the generic C save-path builder for one caller.
SECTION code_user
PUBLIC _saveload_load_nczs_ovl, _saveload_save_nczs_ovl
PUBLIC _saveload_erase_nczs_ovl
EXTERN _esx_funlink, _esx_result

_saveload_load_nczs_ovl:
_saveload_save_nczs_ovl:
    ld a, 2
    jr ec_error

_saveload_erase_nczs_ovl:
    ld hl, (0x5fe0)
    ld a, h
    or l
    jr z, ec_bad_name
    ld a, (hl)
    or a
    jr z, ec_bad_name
    push hl
    ld hl, ec_dir
    ld de, ec_path
    ld bc, 12
    ldir
    pop hl
    ld b, 8
ec_name:
    ld a, (hl)
    or a
    jr z, ec_suffix
    cp '0'
    jr c, ec_bad_name
    cp '9'+1
    jr c, ec_put
    cp 'A'
    jr c, ec_bad_name
    cp 'Z'+1
    jr c, ec_put
    cp '_'
    jr z, ec_put
    cp 'a'
    jr c, ec_bad_name
    cp 'z'+1
    jr nc, ec_bad_name
    sub 32
ec_put:
    ld (de), a
    inc de
    inc hl
    djnz ec_name
    ld a, (hl)
    or a
    jr nz, ec_bad_name
ec_suffix:
    ld hl, ec_ext
    ld bc, 5
    ldir
    ld a, 2
    ld (0x5fe4), a
    ld hl, ec_path
    call _esx_funlink
    ld hl, (_esx_result)
    ld a, h
    or l
    ret z
    xor a
    ld (0x5fe4), a
    ld hl, 1
    ret
ec_bad_name:
    ld a, 1
ec_error:
    ld (0x5fe4), a
    ld hl, 0
    ret
ec_dir: defm "/SYS/CONFIG/"
ec_ext: defm ".STJ"
    defb 0
ec_path: defs 25
