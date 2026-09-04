SECTION code_user

PUBLIC test_start
PUBLIC test_done
PUBLIC test_result

EXTERN spectrum_input_queue_clear
EXTERN spectrum_input_queue_latch
EXTERN _spectrum_key_poll

test_start:
    ld sp, 0xff00
    call spectrum_input_queue_clear

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    call _spectrum_key_poll
    ld a, l
    cp 'a'
    jr nz, test_bad_order
    call _spectrum_key_poll
    ld a, l
    cp 'b'
    jr nz, test_bad_order
    call _spectrum_key_poll
    ld a, l
    or a
    jr nz, test_bad_order

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 0x8a
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    call _spectrum_key_poll
    ld a, l
    cp 0x8a
    jr nz, test_bad_priority
    call _spectrum_key_poll
    ld a, l
    or a
    jr nz, test_bad_priority

    ld a, 'a'
    call spectrum_input_queue_latch
    ld a, 'b'
    call spectrum_input_queue_latch
    call spectrum_input_queue_clear
    call _spectrum_key_poll
    ld a, l
    or a
    jr nz, test_bad_clear

    xor a
    jr test_store_result
test_bad_order:
    ld a, 1
    jr test_store_result
test_bad_priority:
    ld a, 2
    jr test_store_result
test_bad_clear:
    ld a, 3
test_store_result:
    ld (test_result), a
test_done:
    jp test_done

test_result:
    DEFB 0xff
