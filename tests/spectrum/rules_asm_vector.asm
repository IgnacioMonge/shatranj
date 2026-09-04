SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result
PUBLIC _spectrum_board_view_redraw_square
PUBLIC _netchesszx_board_theme_index
PUBLIC _netchesszx_hinted_rows
PUBLIC _spectrum_board_view_flipped
PUBLIC board_theme_hint_inks
PUBLIC compute_square_bc
PUBLIC compute_attr_base
PUBLIC set_square_attr_2x2
PUBLIC compute_screen_base
PUBLIC board_row
PUBLIC board_col
PUBLIC tmp_attr

EXTERN _rules_play_ovl
EXTERN _rules_check_ovl
EXTERN rules_case_table
EXTERN rules_case_count

test_start:
    ld sp, 0xff00
    ld a, 0xff
    ld (test_result), a
    ld hl, rules_case_table
    ld (test_table_ptr), hl
    xor a
    ld (test_case_index), a

test_case_loop:
    ld hl, (test_table_ptr)
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld (test_ctx_ptr), de
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld (test_expected_ptr), de
    ld a, (hl)
    inc hl
    ld (test_expected_check), a
    ld (test_table_ptr), hl
    ld hl, (test_expected_ptr)
    ld a, (hl)
    ld (test_expected_byte), a
    ld a, 1
    ld (test_expected_mask), a
    xor a
    ld (test_from), a
    ld (test_to), a
    ld (test_phase), a

test_pair_loop:
    ld de, (test_ctx_ptr)
    ld hl, 3
    add hl, de
    ld a, (test_from)
    ld (hl), a
    inc hl
    ld a, (test_to)
    ld (hl), a
    call _rules_play_ovl
    ld a, l
    or a
    jr z, test_got_ready
    ld a, 1
test_got_ready:
    ld (test_got), a
    ld b, a
    ld a, (test_expected_byte)
    ld c, a
    ld a, (test_expected_mask)
    and c
    jr z, test_expected_ready
    ld a, 1
test_expected_ready:
    ld (test_want), a
    cp b
    jr nz, test_fail

    ld a, (test_expected_mask)
    rlca
    ld (test_expected_mask), a
    cp 1
    jr nz, test_next_pair
    ld hl, (test_expected_ptr)
    inc hl
    ld (test_expected_ptr), hl
    ld a, (hl)
    ld (test_expected_byte), a
test_next_pair:
    ld a, (test_to)
    inc a
    ld (test_to), a
    cp 64
    jr nz, test_pair_loop
    xor a
    ld (test_to), a
    ld a, (test_from)
    inc a
    ld (test_from), a
    cp 64
    jr nz, test_pair_loop

    ld a, 1
    ld (test_phase), a
    ld de, (test_ctx_ptr)
    call _rules_check_ovl
    ld a, l
    ld (test_got), a
    ld a, (test_expected_check)
    ld (test_want), a
    cp l
    jr nz, test_fail
    ld a, (test_case_index)
    inc a
    ld (test_case_index), a
    ld hl, rules_case_count
    cp (hl)
    jp nz, test_case_loop
    xor a
    jr test_store_result

test_fail:
    ld a, (test_case_index)
    inc a
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

_spectrum_board_view_redraw_square:
compute_square_bc:
compute_attr_base:
set_square_attr_2x2:
compute_screen_base:
    ret

test_result: DEFB 0xff
test_case_index: DEFB 0
test_from: DEFB 0
test_to: DEFB 0
test_phase: DEFB 0
test_got: DEFB 0
test_want: DEFB 0
test_expected_check: DEFB 0
test_expected_byte: DEFB 0
test_expected_mask: DEFB 0
test_table_ptr: DEFW 0
test_ctx_ptr: DEFW 0
test_expected_ptr: DEFW 0

_netchesszx_board_theme_index: DEFB 0
_netchesszx_hinted_rows: DEFS 8, 0
_spectrum_board_view_flipped: DEFB 0
board_theme_hint_inks: DEFB 0
board_row: DEFB 0
board_col: DEFB 0
tmp_attr: DEFB 0
