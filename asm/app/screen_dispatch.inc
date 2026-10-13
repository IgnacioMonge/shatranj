call_overlay_ae:
    ld h, e
    ld l, a
    push hl
    call _spectrum_overlay_exec
    ret

call_overlay_cached_ae:
    ld h, e
    ld l, a
    push hl
    call _spectrum_overlay_exec_cached
    ret

IFNDEF NETCHESSZX_NEXT_EXTENSION
_spectrum_input_parse_move:
    call netchesszx_load_stack_args2
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), c
    inc hl
    ld (hl), b
    ld a, SPECTRUM_OVL_INPUT_EDIT
    ld e, SPECTRUM_OVL_INPUT_EDIT_PARSE_MOVE_PRIVATE
    jr call_overlay_cached_ae
ENDIF

_netchesszx_setup_step_overlay:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_SETUP
    ld e, SPECTRUM_OVL_SETUP_STEP
    jp call_overlay_cached_ae

; fastcall key in L. MENU_CONFIG owns vertical movement and its visual state;
; TIME and ACTION own horizontal/select input in TIME_CONFIG.
_netchesszx_setup_dispatch_overlay:
    ld a, (_setup_room_editing)
    or a
    jr z, setup_dispatch_not_editing
    ld a, (_setup_edit_row)
    cp 4
    jr z, setup_dispatch_time
    jr _netchesszx_setup_step_overlay
setup_dispatch_not_editing:
    ld a, l
    cp 0x81
    jr z, setup_dispatch_menu_nav
    cp 0x82
    jr z, setup_dispatch_menu_nav
    ld a, (_setup_cursor)
    cp 4
    jr z, setup_dispatch_time
    cp 10
    jr nz, _netchesszx_setup_step_overlay
setup_dispatch_time:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_TIME_CONFIG
    ld e, SPECTRUM_OVL_TIME_CONFIG_STEP
    jr call_overlay_cached_ae
setup_dispatch_menu_nav:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_NAV_PRIVATE
    jr call_overlay_cached_ae

_netchesszx_setup_time_ui:
    ld a, SPECTRUM_OVL_TIME_CONFIG
    ld e, SPECTRUM_OVL_TIME_CONFIG_UI
    jr call_overlay_cached_ae

; fastcall init flags in L.
_netchesszx_setup_time_init:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_TIME_CONFIG
    ld e, SPECTRUM_OVL_TIME_CONFIG_INIT
    jr call_overlay_cached_ae

_netchesszx_setup_time_commit:
    ld a, SPECTRUM_OVL_TIME_CONFIG
    ld e, SPECTRUM_OVL_TIME_CONFIG_COMMIT
    jr call_overlay_cached_ae

_netchesszx_config_load_overlay:
    ld e, SPECTRUM_OVL_CONFIG_LOAD
    jr setup_config_overlay
_netchesszx_config_save_overlay:
    ld e, SPECTRUM_OVL_CONFIG_SAVE
    jr setup_config_overlay
_netchesszx_config_defaults_overlay:
    ld e, SPECTRUM_OVL_CONFIG_DEFAULTS
setup_config_overlay:
    ld a, SPECTRUM_OVL_CONFIG
    jp call_overlay_cached_ae

; fastcall key in L. Keep the classic numeric aliases and QAOP aliases in
; one target-side table so setup and game navigation share the same mapping.
_netchesszx_setup_nav_key_alias:
    ld a, l
    ld b, 8
    ld hl, setup_nav_aliases
    jr setup_nav_alias_loop
_netchesszx_confirm_key_alias:
    ld a, l
    ld b, 3
    ld hl, confirm_key_aliases
setup_nav_alias_loop:
    cp (hl)
    inc hl
    jr z, setup_nav_alias_found
    inc hl
    djnz setup_nav_alias_loop
    ld l, a
    ret
setup_nav_alias_found:
    ld l, (hl)
    ret
setup_nav_aliases:
    DEFB '5', 0x83, 'o', 0x83
    DEFB '6', 0x82, 'a', 0x82
    DEFB '7', 0x81, 'q', 0x81
    DEFB '8', 0x84, 'p', 0x84
confirm_key_aliases:
    DEFB 13, 'y', 32, 'y', 0x8a, 'n'

_netchesszx_setup_render_edit_line:
    ld a, l
    ld (NETCHESSZX_OVERLAY_CONTEXT), a
    ld a, SPECTRUM_OVL_INPUT_EDIT
    ld e, SPECTRUM_OVL_INPUT_EDIT_SETUP_LINE_PRIVATE
    jp call_overlay_cached_ae

_netchesszx_setup_compute_visible:
    ld (NETCHESSZX_OVERLAY_CONTEXT), hl
    ld a, SPECTRUM_OVL_SETUP
    ld e, SPECTRUM_OVL_SETUP_COMPUTE_VISIBLE_PRIVATE
    jp call_overlay_cached_ae

_netchesszx_setup_render_overlay:
    call netchesszx_load_stack_args2
    ld hl, NETCHESSZX_OVERLAY_CONTEXT
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), c
    inc hl
    ld (hl), b
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_RENDER
    jp call_overlay_cached_ae

_netchesszx_setup_paint_attrs:
    ld a, SPECTRUM_OVL_MENU_CONFIG
    ld e, SPECTRUM_OVL_MENU_CONFIG_PAINT_ATTRS
    jp call_overlay_cached_ae

INCLUDE "asm/app/board_hints_clear.inc"

read_square_spec:
    ld a, (hl)
    ld (board_row), a
    inc hl
    ld a, (hl)
    ld (board_col), a
    inc hl
    ld a, (hl)
    ret

compute_square_bc:
    ld a, (board_row)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_TOP_ROW
    ld b, a
    ld a, (board_col)
    add a, a
    add a, NETCHESSZX_GAME_BOARD_LEFT_COL
    ld c, a
    ret

square_parity:
    ld a, (board_row)
    ld d, a
    ld a, (board_col)
    add a, d
    and 1
    ret
