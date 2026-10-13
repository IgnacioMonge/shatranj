SECTION code_user
PUBLIC _app_workspace
EXTERN _spectrum_setup_set_visible, _spectrum_setup_accept_set
test_start:
    ld sp, 0xff00
    ld iy, 0x5c3a
    ld a, 2
    ld (_app_workspace + 12), a
    ld hl, 0x1234
    call _spectrum_setup_set_visible
    call _spectrum_setup_accept_set
    ld (saved_sp), sp
    ld (saved_iy), iy
    xor a
    ld (test_result), a
test_done: jp test_done
test_result: DEFB 255
saved_sp: DEFW 0
saved_iy: DEFW 0
_app_workspace: DEFS 55, 0x5a
