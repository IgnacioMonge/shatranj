; Table-driven top option bar. taboption_roots has one entry per visible tab:
; direct entries carry their returned key (bit 7 set), groups carry a label id.
; The fixed-width label, transition and action tables live in the UI assets.
; Roll one focused five-character label through the timer renderer.
; Fastcall HL packs old/new ids in L; H carries q=1..4 and bit 6 selects the
; reverse direction. Every glyph and its inverted background participate.
_netchesszx_asm_menu_roll:
    push ix
    ld a,h
    or 0x20
    ld (timer_roll_buffer),a
    push hl
    ld a,l
    rrca
    rrca
    rrca
    rrca
    and 15
    call taboption_label_ptr
    ld de,taboption_cache
    ld bc,5
    ldir
    pop bc
    ld a,c
    and 15
    call taboption_label_ptr
    ld ix,timer_roll_buffer
    ld de,taboption_cache
    ld b,5
    ld c,6
    cp 3
    jr c,taboption_roll_start
    ld c,12
    cp 7
    jr c,taboption_roll_start
    ld c,23
taboption_roll_start:
    ld a,0x42
    jp tr_start

taboption_label_ptr:
    ld e,a
    add a,a
    add a,a
    add a,e
    add a,taboption_labels & 0xff
    ld l,a
    ld h,taboption_labels >> 8
    ld a,e
    ret
    ASSERT ((taboption_labels & 0xff) + 54) <= 255

taboption_roots:
    DEFB 0x91,0,3,0x8b,7,0x8c
    ASSERT $ - taboption_roots = NETCHESSZX_TABOPTION_COUNT

; Resident controller. Selection paints four phases and pumps UART
; between frame waits, keeping transport responsive without another overlay.
_netchesszx_asm_menu_key:
    ld a,l
    cp 0x90
    jr nz,taboption_key_visible
    ld a,(_spectrum_gui_about_visible_state)
    or a
    ret nz
    jr taboption_key_toggle
taboption_key_visible:
    ld a,(taboption_visible_addr)
    or a
    ret z
    call _netchesszx_setup_nav_key_alias
    ld a,l
    cp 0x81
    jr c,taboption_key_enter_test
    sub 0x81
    cp 4
    jr nc,taboption_key_zero
    cp 2
    jr c,taboption_key_roll
    sub 2
    jr taboption_key_move
taboption_key_enter_test:
    cp 13
    jp z,taboption_key_enter
    cp 32
    jp z,taboption_key_enter
taboption_key_zero:
    ld l,0
    ret

taboption_key_toggle:
    ld a,(taboption_visible_addr)
    xor 1
    ld (taboption_visible_addr),a
    jr z,taboption_key_render_zero
    call taboption_select_root
    ld a,(taboption_focus_addr)
    inc a
; Focus landed on a tab: a group rolls straight into its first action, the
; mirror of the roll back to its label when focus leaves.
taboption_key_render_enter:
    ld l,a
    call _spectrum_render_menu
    ld a,1
    jr taboption_key_roll
taboption_key_render_zero:
    ld l,a
    call _spectrum_render_menu
    jr taboption_key_zero

taboption_key_move:
    or a
    ld a,(taboption_focus_addr)
    ld b,a
    jr nz,taboption_key_move_right
    dec a
    jp p,taboption_key_moved
    ld a,NETCHESSZX_TABOPTION_COUNT-1
    jr taboption_key_moved
taboption_key_move_right:
    inc a
    cp NETCHESSZX_TABOPTION_COUNT
    jr c,taboption_key_moved
    xor a
taboption_key_moved:
    ld c,a
    push bc
    ld a,2
    call taboption_key_roll
    pop bc
    ld a,c
    ld (taboption_focus_addr),a
    call taboption_select_root
    ld a,b
    add a,a
    add a,a
    add a,a
    or c
    or 0x80
    jr taboption_key_render_enter

taboption_key_roll:
    ld c,a
    ld a,(taboption_item_addr)
    bit 7,a
    jr nz,taboption_key_zero
    ld e,a
    ld d,0
    bit 1,c
    jr nz,taboption_key_roll_root
    ld hl,taboption_transitions
    add hl,de
    ld a,(hl)
    ld h,1
    dec c
    jr z,taboption_key_roll_next
    set 6,h
    rrca
    rrca
    rrca
    rrca
    jr taboption_key_roll_next
taboption_key_roll_root:
    ld b,e
    call taboption_focus_root
    cp b
    jr z,taboption_key_zero
    ld h,0x41
    ld e,b
taboption_key_roll_next:
    and 15
    ld b,a
    ld a,e
    rlca
    rlca
    rlca
    rlca
    or b
    ld l,a
taboption_key_roll_phase:
    push hl
    call _netchesszx_asm_menu_roll
    pop hl
    inc h
    ld a,h
    and 7
    cp 5
    jr z,taboption_key_roll_done
    push hl
    call _spectrum_frame_wait
    call _spectrum_uart_background_pump
    pop hl
    jr taboption_key_roll_phase
taboption_key_roll_done:
    ld a,l
    and 15
    ld (taboption_item_addr),a
    jp taboption_key_zero

taboption_key_enter:
    ld a,(taboption_item_addr)
    bit 7,a
    jr nz,taboption_key_action
    ld e,a
    ld d,0
    ld hl,taboption_actions
    add hl,de
    ld a,(hl)
taboption_key_action:
    push af
    xor a
    ld (taboption_visible_addr),a
    ld l,a
    call _spectrum_render_menu
    pop af
    ld l,a
    ret

taboption_focus_root:
    ld a,(taboption_focus_addr)
    cp NETCHESSZX_TABOPTION_COUNT
    jr c,taboption_focus_valid
    xor a
    ld (taboption_focus_addr),a
taboption_focus_valid:
    ld e,a
    ld d,0
    ld hl,taboption_roots
    add hl,de
    ld a,(hl)
    ret

taboption_select_root:
    call taboption_focus_root
    ld (taboption_item_addr),a
    ret
