; Resident Spectranext XFS seam used only by the overlay/DAT loader.
; Product file operations link the complete driver adapter in their 4 KiB
; overlay pages, while both implementations share the fixed low-RAM state.
;
; The loader keeps SHATRANJ.DAT open for the whole session (_spxn_dat_id =
; fd+1) so a direct HTTP(S) launch downloads it once. A read-only borrow of
; that descriptor is an ordinary active FILE whose fd matches the id: the
; close detaches it instead of issuing VCLOSE. Borrows live only between
; _spxn_dat_bind and the caller's close, and no code that links the overlay
; adapter runs in between; release (startup failure, fatal) is the only VCLOSE.

SECTION code_user

PUBLIC _esx_fopen
PUBLIC _esx_fread
PUBLIC _esx_fclose
PUBLIC _spxn_xfs_fseek
PUBLIC _spxn_xfs_open_here
PUBLIC _spxn_xfs_select_slot0
PUBLIC _spxn_dat_bind
PUBLIC _spxn_dat_release
PUBLIC _spxn_xfs_firmware_probe
PUBLIC _spxn_xfs_init
PUBLIC _spxn_xfs_dir_scratch
PUBLIC _spxn_xfs_state_end
PUBLIC _esx_handle
PUBLIC _esx_buf
PUBLIC _esx_count
PUBLIC _esx_result

EXTERN _spxn_regs
EXTERN _spxn_rom_hlcall
EXTERN _spxn_rom_ixcall
EXTERN _spxn_rom_error_clear
EXTERN _spxn_dat_id

defc ROM_OPEN     = $3EB1
defc ROM_READ     = $3EC9
defc ROM_LSEEK    = $3ECF
defc ROM_VCLOSE   = $3ED2
defc ROM_SETMOUNTPOINT = $3EE7
defc ROM_SPECTRANEXT = $3EF0
defc XFS_RDONLY_DE = $0001
defc KIND_FILE = 1

IFNDEF SPXN_XFS_STATE_BASE
    ERROR "define SPXN_XFS_STATE_BASE from the consumer memory map"
ENDIF
IFNDEF SPXN_XFS_DIR_SCRATCH
    ERROR "define SPXN_XFS_DIR_SCRATCH from the consumer memory map"
ENDIF
IF SPXN_XFS_STATE_BASE < $4000
    ERROR "SPXN_XFS_STATE_BASE must be writable Spectrum RAM"
ENDIF
IF SPXN_XFS_STATE_BASE + 20 > $10000
    ERROR "SPXN_XFS_STATE_BASE range wraps past RAM"
ENDIF
IF SPXN_XFS_DIR_SCRATCH < $4000
    ERROR "SPXN_XFS_DIR_SCRATCH must be writable Spectrum RAM"
ENDIF
IF SPXN_XFS_DIR_SCRATCH + 256 > $10000
    ERROR "SPXN_XFS_DIR_SCRATCH range wraps past RAM"
ENDIF
IF SPXN_XFS_STATE_BASE < SPXN_XFS_DIR_SCRATCH + 256
IF SPXN_XFS_DIR_SCRATCH < SPXN_XFS_STATE_BASE + 20
    ERROR "XFS state and directory scratch overlap"
ENDIF
ENDIF

; HL=path. Open on the current mount: the launch mount of a direct HTTP(S)
; start, or slot 0 once the caller selected it.
_spxn_xfs_open_here:
    scf
    jr xfs_open_args

; void esx_fopen(const char *path) __z88dk_fastcall
_esx_fopen:
    or a                        ; NC: pin slot 0 before the open
xfs_open_args:
    ld a, 1
    ld de, XFS_RDONLY_DE
    ld bc, 0

xfs_open:
    ld (active_path), hl
    push hl
    push de
    push bc
    push af
    ld a, (active_kind)
    or a
    jr z, xfs_open_restore_args
    call _esx_fclose
    ld a, l
    or a
    jp nz, xfs_open_restore_fail
xfs_open_restore_args:
    pop af
    ld (active_mode), a
    pop bc
    pop de
    pop hl
    ld (active_path), hl
    call nc, xfs_select_slot0   ; flags still hold the entry carry
    ld hl, (active_path)
    ld (_spxn_regs + 5), hl
    ld (_spxn_regs + 3), de
    ld (_spxn_regs + 1), bc
    call _spxn_rom_error_clear
    ld hl, ROM_OPEN
    call _spxn_rom_ixcall
    bit 0, l
    jr nz, xfs_open_fail
    ld a, (_spxn_regs)
    ld (active_fd), a
    ld a, KIND_FILE
    ld (active_kind), a
    jp xfs_publish_handle
xfs_open_fail:
    call xfs_clear_state
    ret

; void esx_fread(void)
_esx_fread:
    ld a, (active_kind)
    cp KIND_FILE
    jr nz, xfs_io_fail
    ld a, (active_mode)
    cp 1
    jr nz, xfs_io_fail
    ld a, (active_fd)
    ld (_spxn_regs), a
    ld hl, (_esx_buf)
    ld (_spxn_regs + 3), hl
    ld hl, (_esx_count)
    ld (_spxn_regs + 1), hl
    call _spxn_rom_error_clear
    ld hl, ROM_READ
    call _spxn_rom_hlcall
    bit 0, l
    jr nz, xfs_io_fail
    ld hl, (_spxn_regs + 1)
    ld (_esx_result), hl
    ret
xfs_io_fail:
    ld hl, 0
    ld (_esx_result), hl
    ret

; uint8_t esx_fclose(void)
_esx_fclose:
    ld a, (active_kind)
    or a
    jr z, xfs_close_fail
    ld a, (active_fd)
    inc a
    jr z, xfs_close_phys        ; ROM fd $FF can never be the retained id
    ld hl, _spxn_dat_id
    cp (hl)
    jr z, xfs_close_done        ; borrowed DAT: detach only
xfs_close_phys:
    ld a, (active_fd)
    call xfs_vclose
    ret nz
xfs_close_done:
    call xfs_clear_state
    ld hl, 0
    ret
xfs_open_restore_fail:
    pop af
    pop bc
    pop de
    pop hl
    xor a
    ld (_esx_handle), a
    ret
xfs_close_fail:
    ld hl, $00FF
    ret

; A=fd. Z with HL=0 when closed, NZ with HL=$00FF when the ROM refused.
xfs_vclose:
    ld (_spxn_regs), a
    call _spxn_rom_error_clear
    ld hl, ROM_VCLOSE
    call _spxn_rom_hlcall
    bit 0, l
    ld hl, 0
    ret z
    ld l, $FF
    ret

; Physically close the retained DAT. Z when none or closed (id cleared), NZ
; when the ROM refused (ownership kept for a retry).
_spxn_dat_release:
    ld a, (_spxn_dat_id)
    or a
    ret z
    dec a
    call xfs_vclose
    ret nz
    xor a
    ld (_spxn_dat_id), a
    ret

; HL=absolute offset. Borrow the retained DAT as the active read handle and
; seek; HL=1 on success, 0 on failure (a failed seek leaves the borrow active
; for the caller's close). Returns the handle in _esx_handle.
_spxn_dat_bind:
    ld a, (active_kind)
    or a
    jr z, dat_bind_free
    push hl
    call _esx_fclose            ; detach or close the predecessor
    ld a, l
    pop hl
    or a
    jr nz, dat_bind_fail
dat_bind_free:
    ld a, (_spxn_dat_id)
    or a
    jr z, dat_bind_fail
    ld (_esx_handle), a
    dec a
    ld (active_fd), a
    ld a, KIND_FILE
    ld (active_kind), a
    ld (active_mode), a         ; mode 1 = read-only
    jp _spxn_xfs_fseek
dat_bind_fail:
    ld hl, 0
    ret

; uint8_t spxn_xfs_fseek(uint16_t offset) __z88dk_fastcall
_spxn_xfs_fseek:
    ld a, (active_kind)
    cp KIND_FILE
    jr nz, xfs_seek_fail
    ld (_spxn_regs + 5), hl
    ld a, (active_fd)
    ld (_spxn_regs), a
    xor a
    ld (_spxn_regs + 1), a
    ld (_spxn_regs + 2), a
    ld (_spxn_regs + 3), a
    ld (_spxn_regs + 4), a
    call _spxn_rom_error_clear
    ld hl, ROM_LSEEK
    call _spxn_rom_ixcall
    bit 0, l
    jr nz, xfs_seek_fail
    ld hl, 1
    ret
xfs_seek_fail:
    ld hl, 0
    ret

; HL = reply buffer (128 free bytes above 0x3FFF). Firmware 1.0 added
; SPECTRANEXT operation 15 (build version) and fixed seeking in files read
; over HTTPS; older firmware returns carry. L bit 0 set: older than 1.0.
_spxn_xfs_firmware_probe:
    ld (_spxn_regs + 5), hl
    ld a, 15
    ld (_spxn_regs), a
    ld hl, ROM_SPECTRANEXT
    jp _spxn_rom_ixcall

_spxn_xfs_select_slot0:
xfs_select_slot0:
    push hl
    push bc
    push de
    xor a
    ld (_spxn_regs), a
    ld hl, ROM_SETMOUNTPOINT
    call _spxn_rom_hlcall
    pop de
    pop bc
    pop hl
    ret

defc _esx_handle = SPXN_XFS_STATE_BASE
defc _esx_buf = SPXN_XFS_STATE_BASE + 1
defc _esx_count = SPXN_XFS_STATE_BASE + 3
defc _esx_result = SPXN_XFS_STATE_BASE + 5
defc active_path = SPXN_XFS_STATE_BASE + 7
defc active_fd = SPXN_XFS_STATE_BASE + 11
defc active_kind = SPXN_XFS_STATE_BASE + 12
defc active_mode = SPXN_XFS_STATE_BASE + 13
defc _spxn_xfs_state_end = SPXN_XFS_STATE_BASE + 20
defc _spxn_xfs_dir_scratch = SPXN_XFS_DIR_SCRATCH

_spxn_xfs_init:
xfs_clear_state:
    xor a
    ld (_esx_handle), a
    ld (active_fd), a
    ld (active_kind), a
    ld (active_mode), a
    ld (active_path), a
    ld (active_path + 1), a
    ret

xfs_publish_handle:
    ld a, (active_fd)
    inc a
    jr nz, xfs_publish_store
    call _esx_fclose
    xor a
xfs_publish_store:
    ld (_esx_handle), a
    ret
