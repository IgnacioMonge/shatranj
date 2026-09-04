SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result

EXTERN _spectrum_streq

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a

    ld hl, s_abc2
    push hl
    ld hl, s_abc
    push hl
    call _spectrum_streq
    ld a, l
    cp 1
    ld a, 1
    jr nz, test_store_result

    ld hl, s_abd
    push hl
    ld hl, s_abc
    push hl
    call _spectrum_streq
    ld a, l
    or a
    ld a, 2
    jr nz, test_store_result

    ld hl, s_ab
    push hl
    ld hl, s_abc
    push hl
    call _spectrum_streq
    ld a, l
    or a
    ld a, 3
    jr nz, test_store_result

    ld hl, s_abc
    push hl
    ld hl, s_ab
    push hl
    call _spectrum_streq
    ld a, l
    or a
    ld a, 4
    jr nz, test_store_result

    ld hl, s_empty
    push hl
    ld hl, s_empty
    push hl
    call _spectrum_streq
    ld a, l
    cp 1
    ld a, 5
    jr nz, test_store_result

    ld hl, s_abc
    push hl
    ld hl, s_empty
    push hl
    call _spectrum_streq
    ld a, l
    or a
    ld a, 6
    jr nz, test_store_result

    xor a
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_result:
    defb 0xff
s_abc:
    defm "abc"
    defb 0
s_abc2:
    defm "abc"
    defb 0
s_abd:
    defm "abd"
    defb 0
s_ab:
    defm "ab"
    defb 0
s_empty:
    defb 0
