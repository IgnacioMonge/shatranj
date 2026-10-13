    call spectrum_input_scan_raw
    ld a, l
    or a
    jr nz, skp_tick_got_key
    ld (key_last), a
    ld (key_repeat_timer), a
    ld (key_suppress), a
    ret

skp_tick_got_key:
    ld b, a
    ld a, (key_suppress)
    or a
    jr z, skp_tick_check_last
    cp b
    ret z
    xor a
    ld (key_suppress), a

skp_tick_check_last:
    ld a, (key_last)
    cp b
    jr z, skp_tick_same_key

    ld a, b
    ld (key_last), a
    call skp_repeat_start_delay
    ld (key_repeat_timer), a
    ld a, b
    jr skp_latch_event

skp_tick_same_key:
    ld a, b
    call skp_repeatable
    or a
    ret z

    ld hl, key_repeat_timer
    ld a, (hl)
    or a
    jr z, skp_tick_repeat_fire
    dec (hl)
    ret

skp_tick_repeat_fire:
    ld a, b
    call skp_repeat_next_delay
    ld (key_repeat_timer), a
    ld a, b

skp_latch_event:
    jp spectrum_input_queue_latch


skp_repeatable:
    cp 8
    jr z, skp_repeatable_yes
    cp 0x81
    jr c, skp_repeatable_text
    cp 0x8a
    jr c, skp_repeatable_yes

skp_repeatable_text:
    cp 33
    jr c, skp_repeatable_no
    cp 127
    jr c, skp_repeatable_yes

skp_repeatable_no:
    xor a
    ret

skp_repeatable_yes:
    ld a, 1
    ret

skp_repeat_start_delay:
    cp 8
    jr z, skp_repeat_start_backspace
    cp 0x81
    jr c, skp_repeat_start_text
    cp 0x8a
    jr c, skp_repeat_start_nav

skp_repeat_start_text:
    ld a, 20
    ret

skp_repeat_start_backspace:
    ld a, 12
    ret

skp_repeat_start_nav:
    ld a, 15
    ret

skp_repeat_next_delay:
    cp 8
    jr z, skp_repeat_next_backspace
    cp 0x81
    jr z, skp_repeat_next_vertical
    cp 0x82
    jr z, skp_repeat_next_vertical
    cp 0x83
    jr c, skp_repeat_next_text
    cp 0x8a
    jr c, skp_repeat_next_nav

skp_repeat_next_text:
    ld a, 3
    ret

skp_repeat_next_backspace:
    ld a, 1
    ret

skp_repeat_next_nav:
    ld a, 2
    ret

skp_repeat_next_vertical:
    ld a, 5
    ret
