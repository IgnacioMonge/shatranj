; Executes the production loader with modeled ROM vectors, not cartridge hardware.
SECTION code_user
test_start:
    ld sp, 0xFF00
    ld ix, 0x1234
    im 1
    ld a, 0xFE
    ld (0x7000), a
    ld a, 0xFB
    ld (0x0038), a
    ld a, 0xC9
    ld (0x0039), a
    ld hl, mock_pagein
    ld de, 0x3FF9
    call install_jump
    ld hl, mock_pageout
    ld de, 0x007C
    call install_jump
    ld hl, mock_setpage
    ld de, 0x3E36
    call install_jump
    ld hl, mock_pushpage
    ld de, 0x3E87
    call install_jump
    ld hl, mock_poppage
    ld de, 0x3E8A
    call install_jump
    ld hl, mock_reserve
    ld de, 0x3E9F
    call install_jump
    ld hl, mock_free
    ld de, 0x3EA2
    call install_jump
    xor a
    ld (test_error), a
    ld (test_mode), a
    ld (ovl_file_open), a
    ld (ovl_xfs_ready), a
    ld (_spxn_rom_held), a
    ld a, 0x49
    ld (test_page), a
    ld iy, 0x5C3A
    ld hl, ovl_handle
    ld b, __bss_user_size
    ld a, 0xAA
dirty_bss:
    ld (hl), a
    inc hl
    djnz dirty_bss
    ld hl, 0xA55A
    ld (test_bss_tail), hl
    ei
IFDEF REAL_SEAM
    jp real_scenarios
ENDIF
    call _spectrum_assets_load
    ld a, l
    cp 1
    jp nz, test_fail
    ld a, (test_reserves)
    cp ovl_atlas_count
    jp nz, test_fail
    ld a, (test_page)
    cp 0x49
    jp nz, test_fail
    ld a, (test_reads)
    cp ovl_atlas_count + 3 ; fingerprint + three chunks + remaining small overlays
    jp nz, test_fail
    ld a, (0x2008)
    cp 2
    jp nz, test_fail
    ld a, (0x25DE)
    cp 3
    jp nz, test_fail
    ld a, (0x2BBC)
    cp 4
    jp nz, test_fail
    ld a, (0x2FFF)
    cp 4
    jp nz, test_fail
    call assert_restored

    ; Successful entry calls the bridge and attempts nested dispatch while
    ; held and again from the input tick while the ROM is temporarily out.
    ld hl, test_overlay
    ld de, 0x2000
    ld bc, test_overlay_end - test_overlay
    ldir
    ld hl, ovl_atlas_count - 1 ; Last cartridge page: TIME.
    push hl
    call _spectrum_overlay_exec
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_fail
    call assert_restored
    ld a, (test_ticks)
    cp 1
    jp nz, test_fail

    ; Net dispatcher retains its documented normal-flow unconditional EI.
    di
    ld hl, 0
    push hl
    call _spectrum_overlay_exec_cached
    ld a, i
    jp po, test_fail
    ei

    ; Invalid ID, invalid entry index, and an out-of-payload target fail closed.
    ld hl, ovl_atlas_count
    call expect_rejected
    ld hl, 0x0100
    call expect_rejected
    ld hl, 0x3000
    ld (0x2001), hl
    ld hl, 0
    call expect_rejected

    ; Fingerprint mismatch allocates nothing.
    ld a, 1
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_reserves)
    or a
    jp nz, test_fail
    ; Allocation failure frees all pages already reserved.
    ld a, 2
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 2
    jp nz, test_fail
    ; Duplicate allocation is rejected and freed only once.
    ld a, 3
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 1
    jp nz, test_fail
    ; A short read releases its page and never publishes ready.
    ld a, 4
    ld (test_mode), a
    push ix ; assets_load owns IX across this private preload call.
    call ovl_spxn_preload_all
    pop ix
    jp nc, test_fail
    ld a, (test_frees)
    cp 1
    jp nz, test_fail
    ld a, (ovl_spxn_pages_ready)
    or a
    jp nz, test_fail
    call assert_restored
    ld a, 1
    ld (ovl_file_open), a
    ld (test_close_failure), a
    call ovl_close_overlay_file
    jp nc, test_fail
    ld a, (ovl_file_open)
    cp 1
    jp nz, test_fail
    xor a
    ld (test_close_failure), a
    call ovl_close_overlay_file
    jp c, test_fail
    ld a, (ovl_file_open)
    or a
    jp nz, test_fail
    ld hl, 0
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (test_error)
    ld (0x7000), a
    jp 0
test_fail:
    ld a, 0xFF
    ld (0x7000), a
    jp 0

expect_rejected:
    push hl
    call _spectrum_overlay_exec_cached
    ld a, h
    or l
    jp nz, test_fail
assert_restored:
    push ix
    pop hl
    ld de, 0x1234
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (test_mapped)
    or a
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    or a
    jp nz, test_fail
    ld a, (test_page)
    cp 0x49
    jp nz, test_fail
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, i
    jp po, test_fail
    ret

test_overlay:
    defb 1
    defw 0x2003
    jp test_entry
test_overlay_end:
test_entry:
    ld bc, 0x5FE0
    or a
    sbc hl, bc
    jp nz, test_fail
    ex de, hl
    or a
    sbc hl, bc
    jp nz, test_fail
    ld hl, 0x0101
    call expect_nested
    ld ix, 0x5678
    call _spectrum_spxn_frame_wait
    push ix
    pop hl
    ld de, 0x5678
    or a
    sbc hl, de
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    cp 1
    jp nz, test_fail
    ld a, i
    jp pe, test_fail
    ld hl, 0x1234
    ret
expect_nested:
    push hl
    call _spectrum_overlay_exec_cached
    ld a, h
    or l
    jp nz, test_fail
    ret
_spectrum_input_frame_tick:
    ld a, (test_mapped)
    or a
    jp nz, test_fail
    ld a, (_spxn_rom_held)
    or a
    jp nz, test_fail
    ld hl, test_ticks
    inc (hl)
    ld hl, 0x0202
    jp expect_nested

install_jump:
    ld a, 0xC3
    ld (de), a
    inc de
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a
    ret
mock_pagein:
    ld iy, 0xDEAD
    ld ix, 0xDEAD ; ROM ABI promises neither index register.
    call assert_di
    ld a, 1
    ld (test_mapped), a
    ret
mock_pageout:
    ld iy, 0xDEAD
    ld ix, 0xDEAD
    call assert_di
    xor a
    ld (test_mapped), a
    ret
assert_di:
    push af
    ld a, i
    jp pe, test_fail
    pop af
    ret
mock_setpage:
    call assert_di
    ld (test_page), a
    ld iy, 0xDEAD ; ROM does not promise IY
    ld ix, 0xDEAD
    ret
mock_pushpage:
    pop hl
    ld c, a
    ld a, (test_page)
    push af
    push hl
    ld a, c
    jp mock_setpage
mock_poppage:
    pop hl
    pop af
    push hl
    jp mock_setpage
mock_reserve:
    call assert_di
    or a
    jp z, test_fail
    ld a, (test_reserves)
    ld b, a
    ld a, (test_mode)
    cp 2
    jr nz, reserve_no_fail
    ld a, b
    cp 2
    jr nz, reserve_no_fail
    scf
    ret
reserve_no_fail:
    ld a, b
    inc a
    ld (test_reserves), a
    ld a, (test_mode)
    cp 3
    ld a, 0xC3
    ret z
    add a, b
    or a
    ret
mock_free:
    call assert_di
    ld hl, test_frees
    inc (hl)
    ret

_spxn_detect:
    ld hl, 1
    ret
IFNDEF REAL_SEAM
_spxn_xfs_open_here:
_esx_fopen:
    xor a
    ld (test_reserves), a
    ld (test_reads), a
    ld (test_frees), a
    inc a
    ld (_esx_handle), a
    ret
_esx_fread:
ENDIF
model_read:
    ld hl, (_esx_buf)
    ld a, h
    cp 0x40
    jp c, test_fail
    ld a, (test_reads)
    inc a
    ld (test_reads), a
    cp 1
    jr nz, read_payload
    ld hl, _overlay_scratch_base + 96
    ld (hl), 11
    inc hl
    ld (hl), 22
    inc hl
    ld (hl), 33
    inc hl
    ld a, (test_mode)
    cp 1
    ld a, 44
    jr nz, fingerprint_good
    xor a
fingerprint_good:
    ld (hl), a
    jr read_count
read_payload:
    ld hl, (_esx_buf)
    ld bc, (_esx_count)
    ld a, (test_reads)
read_fill:
    ld (hl), a
    inc hl
    dec bc
    ld d, a
    ld a, b
    or c
    ld a, d
    jr nz, read_fill
read_count:
    ld hl, (_esx_count)
    ld a, (test_mode)
    cp 4
    jr nz, read_done
    ld a, (test_reads)
    cp 2
    jr nz, read_done
    dec hl
read_done:
    ld (_esx_result), hl
    ret
IFNDEF REAL_SEAM
_esx_fclose:
    ld a, (test_close_failure)
    ld l, a
    ret
_spxn_xfs_fseek:
    ld hl, 0
    ret
_spxn_xfs_select_slot0:
    ret
_spxn_dat_release:
    xor a
    ret
_spxn_dat_bind:
    ld hl, 0
    ret
_spxn_xfs_firmware_probe:
    ld l, 0                     ; firmware 1.0 or later
    ret
_spxn_xfs_init:
    ld hl, ovl_handle
    ld b, __bss_user_size
check_bss_clear:
    ld a, (hl)
    or a
    jp nz, test_fail
    inc hl
    djnz check_bss_clear
    ld hl, (test_bss_tail)
    ld de, 0xA55A
    or a
    sbc hl, de
    jp nz, test_fail
    ret
ENDIF
_spectrum_uart_background_pump:
    ret
test_error: defb 0
test_mode: defb 0
test_page: defb 0
test_mapped: defb 0
test_reserves: defb 0
test_reads: defb 0
test_frees: defb 0
test_ticks: defb 0
IFNDEF REAL_SEAM
_esx_handle: defb 0
_esx_buf: defw 0
_esx_count: defw 0
_esx_result: defw 0
test_bss_tail EQU 0x5B50
ELSE
test_bss_tail EQU 0x5B64
ENDIF
_netchesszx_piece_set_index: defb 0

__bss_user_size EQU 80

test_close_failure: defb 0

IFDEF REAL_SEAM
; The production resident seam (xfs_loader_spectranext.asm) against a ROM model
; of one launch mount plus slot 0. OPEN/READ/LSEEK/VCLOSE/SETMOUNTPOINT calls
; are logged as events: S, O<file>, L<fd+'0'>, R<fd+'0'> (only with log_reads),
; C<fd+'0'>, and F for the SPECTRANEXT build-version probe (operation 15).
; Every startup probes the firmware again (it clears the loader BSS); every
; scenario starts clean and flags its own failure.
_spxn_regs EQU 0x7100
log_buf EQU 0x7200
fail_flags EQU 0x7300
ROMM_OPEN EQU 0x3EB1
ROMM_READ EQU 0x3EC9
ROMM_LSEEK EQU 0x3ECF
ROMM_VCLOSE EQU 0x3ED2
ROMM_SETMOUNT EQU 0x3EE7
ROMM_SPECTRANEXT EQU 0x3EF0
LAUNCH_MOUNT EQU 2

_spxn_rom_error_clear:
    ret
_spxn_rom_hlcall:
_spxn_rom_ixcall:
    ld a, l
    cp ROMM_OPEN & 0xFF
    jp z, rom_open
    cp ROMM_READ & 0xFF
    jp z, rom_read
    cp ROMM_LSEEK & 0xFF
    jp z, rom_lseek
    cp ROMM_VCLOSE & 0xFF
    jp z, rom_vclose
    cp ROMM_SPECTRANEXT & 0xFF
    jp z, rom_firmware
    cp ROMM_SETMOUNT & 0xFF
    jp nz, test_fail
    ld a, 'S'
    call log_byte
    ld a, (_spxn_regs)
    ld (rom_mount), a
rom_ok:
    ld l, 0
    ret
rom_fail:
    ld l, 1
    ret

; Assets (SHATRANJ.*) live on files_mount; every other path on slot 0.
rom_open:
    ld a, 'O'
    call log_byte
    ld hl, (_spxn_regs + 5)
    ld a, (hl)
    cp 'S'
    jr nz, rom_open_local
    ld de, 9
    add hl, de
    ld a, (hl)                  ; "SHATRANJ.DAT" / "SHATRANJ.OVL"
    cp 'O'
    jr nz, rom_open_asset
    ld a, 'V'
rom_open_asset:
    call log_byte
    ld a, (rom_mount)
    ld hl, files_mount
    cp (hl)
    jp nz, rom_fail
    jr rom_open_mounted
rom_open_local:
    ld a, 'x'
    call log_byte
    ld a, (rom_mount)
    or a
    jp nz, rom_fail
rom_open_mounted:
    xor a
    ld (test_reserves), a
    ld (test_reads), a
    ld (test_frees), a
    ld a, (force_fd)
    or a
    jr z, rom_open_alloc
    cp 0xFF
    jr nz, rom_open_publish
    ld (ff_open), a
    jr rom_open_publish
rom_open_alloc:
    ld a, (fds_open)
    ld c, a
    ld b, 0
    ld d, 1
rom_open_scan:
    ld a, c
    and d
    jr z, rom_open_take
    inc b
    sla d
    jr rom_open_scan
rom_open_take:
    ld a, c
    or d
    ld (fds_open), a
    ld a, b
rom_open_publish:
    ld (_spxn_regs), a
    jp rom_ok

; D=bit of the descriptor in _spxn_regs; a closed descriptor fails the test.
rom_fd_check:
    ld a, (_spxn_regs)
    cp 0xFF
    ret z
    ld b, a
    ld d, 1
    inc b
rom_fd_bit:
    dec b
    jr z, rom_fd_have
    sla d
    jr rom_fd_bit
rom_fd_have:
    ld a, (fds_open)
    and d
    jp z, test_fail
    ret

; Operation 15 exists from firmware 1.0; older firmware returns carry. The
; probe must pass a reply buffer in RAM.
rom_firmware:
    ld a, 'F'
    call log_byte
    ld a, (_spxn_regs)
    cp 15
    jp nz, test_fail
    ld a, (_spxn_regs + 6)
    cp 0x40
    jp c, test_fail
    ld a, (firmware_old)
    or a
    jp nz, rom_fail
    jp rom_ok

rom_vclose:
    ld a, 'C'
    call log_byte
    ld a, (_spxn_regs)
    add a, '0'
    call log_byte
    call rom_fd_check
    ld a, (test_close_failure)
    or a
    jp nz, rom_fail
    ld a, (_spxn_regs)
    cp 0xFF
    jr nz, rom_vclose_mask
    xor a
    ld (ff_open), a
    jp rom_ok
rom_vclose_mask:
    ld a, d
    cpl
    ld hl, fds_open
    and (hl)
    ld (hl), a
    jp rom_ok

rom_lseek:
    call rom_fd_check
    ld hl, (_spxn_regs + 5)
    ld (rom_last_seek), hl
    ld a, 'L'
    call log_byte
    ld a, (_spxn_regs)
    add a, '0'
    call log_byte
    jp rom_ok

rom_read:
    call rom_fd_check
    ld a, (log_reads)
    or a
    jr z, rom_read_data
    ld a, 'R'
    call log_byte
    ld a, (_spxn_regs)
    add a, '0'
    call log_byte
rom_read_data:
    call model_read
    ld hl, (_esx_result)
    ld (_spxn_regs + 1), hl
    jp rom_ok

log_byte:
    push hl
    ld hl, (log_ptr)
    ld (hl), a
    inc hl
    ld (log_ptr), hl
    pop hl
    ret

; HL=expected ASCIIZ event log. Z when equal.
expect_log:
    ld de, log_buf
expect_log_next:
    ld a, (de)
    cp (hl)
    ret nz
    or a
    ret z
    inc hl
    inc de
    jr expect_log_next

mark_fail:
    ld hl, fail_flags
    ld a, (cur_scn)
    add a, l
    ld l, a
    inc (hl)
    ret

; A=scenario number, B=mount holding the assets.
sc_begin:
    ld (cur_scn), a
    ld a, b
    ld (files_mount), a
    ld a, LAUNCH_MOUNT
    ld (rom_mount), a
sc_clear_log:
    ld hl, log_buf
    ld (log_ptr), hl
    ld d, h
    ld e, l
    inc de
    ld bc, 63
    ld (hl), 0
    ldir
    ret
sc_model_reset:
    ld a, 1                     ; fd 0 is never handed out
    ld (fds_open), a
    xor a
    ld (ff_open), a
    ld (force_fd), a
    ld (test_close_failure), a
    ld (log_reads), a
    ld (test_mode), a
    ret

; Power-on startup (A=scenario, B=assets mount) with a clean model.
sc_startup:
    push af
    call sc_model_reset
    pop af
    call sc_begin
    jp _spectrum_assets_load

; HL=expected log: flags the scenario when it differs, then clears the log.
sc_log_is:
    call expect_log
    jr z, sc_clear_log
    call mark_fail
    ld a, (cur_scn)             ; keep the offending log at 0x7400 + 16 * n
    add a, a
    add a, a
    add a, a
    add a, a
    ld e, a
    ld d, 0x74
    ld hl, log_buf
    ld bc, 16
    ldir
    jr sc_clear_log

; A=expected value of (HL).
sc_byte_is:
    cp (hl)
    call nz, mark_fail
    ret

; Common end-of-startup state: DAT retained as fd 1 (id 2), nothing else open.
sc_retained_only:
    ld hl, fds_open
    ld a, 3
    call sc_byte_is
    ld hl, _spxn_dat_id
    ld a, 2
    call sc_byte_is
    ld hl, active_kind
    xor a
    jp sc_byte_is

real_scenarios:
    ld hl, fail_flags
    ld de, fail_flags + 1
    ld bc, 15
    ld (hl), 0
    ldir

    ; 0: the launch mount holds both files. DAT and OVL open before any
    ; SETMOUNTPOINT, the OVL is closed, the DAT stays open as a resource.
    ld a, 0
    ld b, LAUNCH_MOUNT
    call sc_startup
    ld a, l
    cp 1
    call nz, mark_fail
    ld hl, scn0_log
    call sc_log_is
    call sc_retained_only
    ld a, (test_reserves)
    cp ovl_atlas_count
    call nz, mark_fail

    ; 1: files only on slot 0. The inherited DAT open fails, slot 0 is
    ; selected once, and the OVL opens on that mount without selecting again.
    ld a, 1
    ld b, 0
    call sc_startup
    ld a, l
    cp 1
    call nz, mark_fail
    ld hl, scn1_log
    call sc_log_is
    call sc_retained_only

    ; 2: files nowhere. Nothing stays open or retained.
    ld a, 2
    ld b, 9
    call sc_startup
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn2_log
    call sc_log_is
    ld hl, fds_open
    ld a, 1
    call sc_byte_is
    ld hl, _spxn_dat_id
    xor a
    call sc_byte_is

    ; 3: a later slot-0 path open pins slot 0 and never closes the DAT.
    ld a, 3
    ld b, LAUNCH_MOUNT
    call sc_startup
    call sc_clear_log
    ld hl, cfg_path
    call _esx_fopen
    ld a, (_esx_handle)
    or a
    call z, mark_fail
    call _esx_fclose
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn3_log
    call sc_log_is
    call sc_retained_only

    ; 4: piece-set reload binds the retained fd, seeks, reads, detaches.
    ld a, 4
    ld b, LAUNCH_MOUNT
    call sc_startup
    call sc_clear_log
    ld a, 1
    ld (log_reads), a
    ld hl, 2
    call _netchesszx_piece_set_load
    ld a, l
    cp 1
    call nz, mark_fail
    ld hl, scn4_log
    call sc_log_is
    ld hl, (rom_last_seek)
    ld de, (piece_set_offsets + 4)
    or a
    sbc hl, de
    call nz, mark_fail
    call sc_retained_only

    ; 5: the About entry binds at its payload offset, reads and detaches.
    ld a, 5
    ld b, LAUNCH_MOUNT
    call sc_startup
    call sc_clear_log
    ld a, 1
    ld (log_reads), a
    ld hl, 2041
    call _spxn_dat_bind
    ld a, l
    cp 1
    call nz, mark_fail
    ld hl, (rom_last_seek)
    ld de, 2041
    or a
    sbc hl, de
    call nz, mark_fail
    ld hl, 0x6200
    ld (_esx_buf), hl
    ld hl, 16
    ld (_esx_count), hl
    call _esx_fread
    ld hl, (_esx_result)
    ld de, 16
    or a
    sbc hl, de
    call nz, mark_fail
    call _esx_fclose
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn5_log
    call sc_log_is
    call sc_retained_only

    ; 6: a slot-0 open while the DAT is borrowed detaches it without VCLOSE.
    ld a, 6
    ld b, LAUNCH_MOUNT
    call sc_startup
    call sc_clear_log
    ld hl, 0
    call _spxn_dat_bind
    ld hl, cfg_path
    call _esx_fopen
    call _esx_fclose
    ld hl, scn6_log
    call sc_log_is
    call sc_retained_only

    ; 7: ROM fd $FF cannot be encoded: it is closed and the open fails.
    ld b, LAUNCH_MOUNT
    call sc_model_reset
    ld a, 0xFF
    ld (force_fd), a
    ld a, 7
    call sc_begin
    call _spectrum_assets_load
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn7_log
    call sc_log_is
    ld hl, ff_open
    xor a
    call sc_byte_is
    ld hl, _spxn_dat_id
    xor a
    call sc_byte_is

    ; 8: partial startup failure (stale overlay file) releases the DAT.
    ld b, LAUNCH_MOUNT
    call sc_model_reset
    ld a, 1
    ld (test_mode), a
    ld a, 8
    call sc_begin
    call _spectrum_assets_load
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn8_log
    call sc_log_is
    ld hl, fds_open
    ld a, 1
    call sc_byte_is
    ld hl, _spxn_dat_id
    xor a
    call sc_byte_is

    ; 9: a refused release keeps the id; fatal then releases before halting.
    ld a, 9
    ld b, LAUNCH_MOUNT
    call sc_startup
    ld a, 1
    ld (test_close_failure), a
    call _spxn_dat_release
    call z, mark_fail
    ld hl, _spxn_dat_id
    ld a, 2
    call sc_byte_is
    xor a
    ld (test_close_failure), a
    call sc_clear_log
    ld hl, fatal_done           ; leave the halt loop for the checks below
    ld a, 0xC3
    ld (assets_fatal_halt), a
    ld (assets_fatal_halt + 1), hl
    jp _spectrum_assets_fatal
fatal_done:
    ld hl, scn9_log
    call sc_log_is
    ld hl, fds_open
    ld a, 1
    call sc_byte_is
    ld hl, _spxn_dat_id
    xor a
    call sc_byte_is

    ; 10: firmware older than 1.0. The probe fails before any open, startup
    ; fails with nothing retained, and the fatal screen names the firmware.
    ld b, LAUNCH_MOUNT
    call sc_model_reset
    ld a, 10
    call sc_begin
    xor a
    ld (ovl_xfs_ready), a
    inc a
    ld (firmware_old), a
    call _spectrum_assets_load
    ld a, l
    or a
    call nz, mark_fail
    ld hl, scn10_log
    call sc_log_is
    ld hl, ovl_xfs_ready
    ld a, 2
    call sc_byte_is
    ld hl, fds_open
    ld a, 1
    call sc_byte_is
    ld hl, _spxn_dat_id
    xor a
    call sc_byte_is
    ld hl, firmware_done
    ld (assets_fatal_halt + 1), hl
    jp _spectrum_assets_fatal
firmware_done:
    ld hl, 0x4000               ; cleared scanline 0, message from scanline 1
    xor a
    call sc_byte_is
    ld hl, 0x4100
    ld a, (assets_firmware_bitmap)
    call sc_byte_is
    ld hl, 0x460E               ; last byte of the sixth message row
    ld a, (assets_firmware_bitmap + 89)
    call sc_byte_is
    ld hl, 0x580E
    ld a, 0x42
    call sc_byte_is
    ld hl, 0x580F
    xor a
    call sc_byte_is

    ld hl, fail_flags
    ld b, 11
    xor a
real_collect:
    or (hl)
    inc hl
    djnz real_collect
    ld (0x7000), a
    jp 0

scn0_log: defm "FODOVC2"
    defb 0
scn1_log: defm "FODSODOVC2"
    defb 0
scn2_log: defm "FODSOD"
    defb 0
scn3_log: defm "SOxC2"
    defb 0
scn4_log: defm "L1R1"
    defb 0
scn5_log: defm "L1R1"
    defb 0
scn6_log: defm "L1SOxC2"
    defb 0
; fd $FF logs as '/' (0xFF + '0'); slot 0 holds no assets, so the retry fails.
scn7_log: defm "FODC/SOD"
    defb 0
scn8_log: defm "FODOVC2C1"
    defb 0
scn9_log: defm "C1"
    defb 0
scn10_log: defm "F"
    defb 0
cfg_path: defm "/CFG/SHATRANJ.CFG"
    defb 0
log_ptr: defw 0
rom_mount: defb 0
files_mount: defb 0
fds_open: defb 0
ff_open: defb 0
force_fd: defb 0
log_reads: defb 0
cur_scn: defb 0
firmware_old: defb 0
rom_last_seek: defw 0
ENDIF
