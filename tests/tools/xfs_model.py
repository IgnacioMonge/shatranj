"""SpectraNext XFS adapter model for Z80 vectors (single handle, RAM/flash).

Implements the esx_* compatibility ABI of SpectraNext's adapters/xfs_compat.asm
and spxf_replace_atomic() closely enough to drive product ASM: one live
handle, errors reported through spxn_rom_error(), no carry, and new files in
cartridge RAM until committed. `flash` marks a committed file; a power cut
keeps only those. Hardware rules from SpectraNext docs/porting.md are
enforced: READ at end of file fails with EOF ($21) and 0 bytes; a committed
file cannot be unlinked (EIO) or committed again; O_CREAT on an existing
flash path is counted as a violation (use O_WRONLY|O_TRUNC); a committed
leftover temp makes spxf_replace_atomic() fail. Every path and the replace
buffer must be staged in low RAM (0x6000..0x67FF), because the real ROM pages
its module over the executing overlay.
"""

XFS_FILES = ("slot0", "slot1", "tmp", "new", "rcvtemp")
XFS_BASE = 0xC000
XFS_SPAN = 0x200

XFS_MODEL = r"""
XMB EQU 0xc000

; HL = path -> A = file index, NC; C when unknown or not staged in low RAM.
xm_path:
    ld a, h
    cp 0x60
    jr c, xm_path_unstaged
    cp 0x68
    jr nc, xm_path_unstaged
    ld b, 0
    ld de, xm_names
xm_path_next:
    push hl
xm_path_cmp:
    ld a, (de)
    cp (hl)
    jr nz, xm_path_skip
    inc de
    inc hl
    or a
    jr nz, xm_path_cmp
    pop hl
    ld a, b
    or a
    ret
xm_path_skip:
    ld a, (de)
    inc de
    or a
    jr nz, xm_path_skip
    pop hl
    inc b
    ld a, b
    cp 5
    jr c, xm_path_next
xm_path_unstaged:
    ld hl, xm_bad_paths
    inc (hl)
    scf
    ret

; A = index -> HL = header (exists, len lo, len hi, flash; data at +16).
xm_file:
    add a, a
    add a, XMB / 256
    ld h, a
    ld l, 0
    ret

xm_err:
    ld (xm_rom_error), a
    xor a
    ld (_esx_handle), a
    ret

; A = mode (1 read, 2 write), C = 0 existing only, 1 create/truncate
; ($0302, prescribed only for absent paths), 2 existing + truncate ($0202).
xm_open:
    ld (xm_req_mode), a
    ld a, c
    ld (xm_req_create), a
    xor a
    ld (xm_rom_error), a
    ld (xm_active), a           ; opening another file drops the old handle
    ld a, (xm_open_calls)
    inc a
    ld (xm_open_calls), a
    ld b, a
    ld a, (xm_fail_open_call)
    cp b
    ld a, 9
    jr z, xm_err
    call xm_path
    jr nc, xm_open_found
    ld a, 9
    jr xm_err
xm_open_found:
    ld (xm_idx), a
    call xm_file
    ld a, (xm_req_create)
    or a
    jr z, xm_open_existing
    cp 2
    jr z, xm_open_trunc_existing
    push hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    pop hl
    or a
    jr z, xm_create_ok
    ld a, (xm_bad_create)       ; O_CREAT on an existing flash path
    inc a
    ld (xm_bad_create), a
xm_create_ok:
    ld (hl), 1
xm_truncate:
    inc hl
    xor a
    ld (hl), a
    inc hl
    ld (hl), a                  ; truncate; a flash path stays flash
    jr xm_open_ok
xm_open_trunc_existing:
    ld a, (hl)
    or a
    ld a, 2                     ; ENOENT: O_TRUNC without O_CREAT
    jr z, xm_err
    jr xm_truncate
xm_open_existing:
    ld a, (hl)
    or a
    ld a, 2                     ; ENOENT
    jr z, xm_err
xm_open_ok:
    ld hl, 0
    ld (xm_pos), hl
    ld a, (xm_req_mode)
    ld (xm_mode), a
    ld a, 1
    ld (xm_active), a
    ld (_esx_handle), a
    ret

_esx_fopen:
    ld a, 1
    ld c, 0
    jp xm_open
_esx_fcreate:
    ld a, 2
    ld c, 1
    jp xm_open
_esx_fopen_update:
    ld a, 2
    ld c, 0
    jp xm_open
_esx_fopen_write_trunc:
    ld a, 2
    ld c, 2
    jp xm_open

_spxn_xfs_fseek:
    ld a, (xm_active)
    or a
    jr z, xm_seek_fail
    ld (xm_pos), hl
    ld hl, 1
    ret
xm_seek_fail:
    ld hl, 0
    ret

_esx_fseek_end:
    ld a, (xm_active)
    or a
    jr z, xm_seek_end_fail
    ld a, (xm_idx)
    call xm_file
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld (xm_pos), de
    ld hl, 0
    ret
xm_seek_end_fail:
    ld hl, 1
    ret

; DE = xm_pos -> HL = data address.
xm_data_at:
    ld a, (xm_idx)
    call xm_file
    ld de, 16
    add hl, de
    ld de, (xm_pos)
    add hl, de
    ret

_esx_fread:
    xor a
    ld (xm_rom_error), a
    ld hl, 0
    ld (_esx_result), hl
    ld a, (xm_active)
    or a
    ret z
    ld a, (xm_mode)
    cp 1
    ret nz
    ld a, (xm_read_calls)
    inc a
    ld (xm_read_calls), a
    ld b, a
    ld a, (xm_fail_read_call)
    cp b
    jr nz, xm_read_go
    ld a, 9
    ld (xm_rom_error), a
    ret
xm_read_go:
    ld a, (xm_idx)
    call xm_file
    inc hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl                   ; HL = length
    ld de, (xm_pos)
    or a
    sbc hl, de
    jr nc, xm_read_avail
    ld hl, 0
xm_read_avail:
    ld de, (_esx_count)
    push hl
    or a
    sbc hl, de
    pop hl
    jr c, xm_read_n
    ex de, hl
xm_read_n:
    ld (_esx_result), hl
    ld a, h
    or l
    jr nz, xm_read_copy
    ld a, 0x21                  ; physical READ at EOF: carry, EOF, 0 bytes
    ld (xm_rom_error), a
    ret
xm_read_copy:
    push hl
    call xm_data_at
    ld de, (_esx_buf)
    pop bc
    push bc
    ldir
    pop bc
    ld hl, (xm_pos)
    add hl, bc
    ld (xm_pos), hl
    ret

_esx_fwrite:
    xor a
    ld (xm_rom_error), a
    ld hl, 0
    ld (_esx_result), hl
    ld a, (xm_active)
    or a
    ret z
    ld a, (xm_mode)
    cp 2
    ret nz
    ld a, (xm_write_calls)
    inc a
    ld (xm_write_calls), a
    ld b, a
    ld a, (xm_fail_write_call)
    cp b
    ld bc, (_esx_count)
    jr nz, xm_write_go
    ld a, (xm_fail_write_bytes)
    ld c, a
    ld b, 0
    ld a, 9
    ld (xm_rom_error), a
xm_write_go:
    ld (_esx_result), bc
    ld a, b
    or c
    ret z
    push bc
    call xm_data_at
    ex de, hl
    ld hl, (_esx_buf)
    pop bc
    push bc
    ldir
    pop bc
    ld hl, (xm_pos)
    add hl, bc
    ld (xm_pos), hl
    ex de, hl                   ; DE = new position
    ld a, (xm_idx)
    call xm_file
    inc hl
    push hl
    ld a, (hl)
    inc hl
    ld h, (hl)
    ld l, a
    or a
    sbc hl, de
    pop hl
    ret nc
    ld (hl), e
    inc hl
    ld (hl), d
    ret

_esx_fclose:
    xor a
    ld (xm_rom_error), a        ; CLOSE clears the transient error
    ld a, (xm_active)
    or a
    jr z, xm_close_fail
    ld a, (xm_close_calls)
    inc a
    ld (xm_close_calls), a
    ld b, a
    xor a
    ld (xm_active), a
    ld (_esx_handle), a
    ld a, (xm_fail_close_call)
    cp b
    jr z, xm_close_fail
    ld hl, 0
    ret
xm_close_fail:
    ld hl, 0x00ff
    ret

; A missing path reports ENOENT; a committed file cannot be unlinked (EIO).
_esx_funlink:
    xor a
    ld (xm_active), a
    call xm_path
    jr c, xm_unlink_fail
    call xm_file
    ld a, (hl)
    or a
    ld a, 2
    jr z, xm_unlink_error
    push hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    pop hl
    or a
    ld a, 3
    jr nz, xm_unlink_error
    ld (hl), 0
    ld a, (xm_unlinks)
    inc a
    ld (xm_unlinks), a
    ld hl, 1
    ld (_esx_result), hl
    ret
xm_unlink_fail:
    ld a, 9
xm_unlink_error:
    ld (xm_rom_error), a
    ld hl, 0
    ld (_esx_result), hl
    ret

; HL = old path, esx_buf = new path: RENAME old over new (layer kept).
_esx_frename:
    xor a
    ld (xm_active), a
    call xm_path
    jr c, xm_rename_fail
    ld (xm_idx_tmp), a
    ld hl, (_esx_buf)
    call xm_path
    jr c, xm_rename_fail
    call xm_file
    ex de, hl                   ; DE = destination header
    ld a, (xm_idx_tmp)
    call xm_file                ; HL = source header
    ld a, (hl)
    or a
    jr z, xm_rename_fail
    push hl
    ld bc, 0x1f0
    ldir
    pop hl
    ld (hl), 0
    inc hl
    inc hl
    inc hl
    ld (hl), 0
    ld a, (xm_renames)
    inc a
    ld (xm_renames), a
    ld hl, 1
    ld (_esx_result), hl
    ret
xm_rename_fail:
    ld hl, 0
    ld (_esx_result), hl
    ret

_esx_mkdir:
    ld a, h
    cp 0x60
    jr c, xm_mkdir_unstaged
    cp 0x68
    jr c, xm_mkdir_ok
xm_mkdir_unstaged:
    ld a, (xm_bad_paths)
    inc a
    ld (xm_bad_paths), a
xm_mkdir_ok:
    ld hl, 1
    ld (_esx_result), hl
    ret

; A RAM file is committed once; a flash path refuses a second commit.
_esx_commit:
    call xm_path
    jr c, xm_commit_no
    call xm_file
    ld a, (hl)
    or a
    jr z, xm_commit_no          ; missing
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    or a
    jr nz, xm_commit_no         ; already in flash
    ld (hl), 1
    ld a, (xm_commits)
    inc a
    ld (xm_commits), a
    ld hl, 1
    jr xm_commit_done
xm_commit_no:
    ld hl, 0
xm_commit_done:
    ld (_esx_result), hl
    ret

; int16_t spxf_replace_atomic(target, temp, buf, len) __z88dk_callee.
_spxf_replace_atomic:
    pop bc                      ; return
    pop hl                      ; target
    ld (xm_rep_target), hl
    pop hl                      ; temp
    ld (xm_rep_temp), hl
    pop hl
    ld (xm_rep_buf), hl
    pop hl
    ld (xm_rep_len), hl
    push bc
    xor a
    ld (xm_active), a
    ld a, (xm_replace_calls)
    inc a
    ld (xm_replace_calls), a
    ld b, a
    ld a, (xm_fail_replace_call)
    cp b
    jr z, xm_replace_fail
    ld hl, (xm_rep_buf)         ; the record must be staged in low RAM
    ld a, h
    cp 0x60
    jr c, xm_replace_bad_buf
    cp 0x68
    jr nc, xm_replace_bad_buf
    ld hl, (xm_rep_temp)
    call xm_path
    jr c, xm_replace_fail
    cp 4
    jr nz, xm_replace_fail      ; the temp must be the private RCV temp
    ; A committed leftover temp: UNLINK is EIO, OPEN $0302 rewrites it and
    ; the second CHMOD fails, so the helper returns an error and the temp,
    ; now holding this record, stays committed.
    ld a, 4
    call xm_file
    push hl
    inc hl
    inc hl
    inc hl
    ld a, (hl)
    pop hl
    or a
    jr z, xm_replace_fresh
    call xm_replace_fill
    jr xm_replace_fail
xm_replace_fresh:
    ld hl, (xm_rep_target)
    call xm_path
    jr c, xm_replace_fail
    call xm_file                ; target header
    call xm_replace_fill        ; committed temp renamed over the target
    ld hl, 0
    ret
xm_replace_bad_buf:
    ld a, (xm_bad_paths)
    inc a
    ld (xm_bad_paths), a
xm_replace_fail:
    ld hl, -1
    ret

; HL = header <- committed file holding the helper's buffer.
xm_replace_fill:
    ld (hl), 1
    inc hl
    ld de, (xm_rep_len)
    ld (hl), e
    inc hl
    ld (hl), d
    inc hl
    ld (hl), 1
    ld de, 13
    add hl, de
    ex de, hl
    ld hl, (xm_rep_buf)
    ld bc, (xm_rep_len)
    ldir
    ret

_spxn_rom_error:
    ld a, (xm_rom_error)
    ld l, a
    ld h, 0
    ret

_esx_handle: DEFB 0
_esx_buf: DEFW 0
_esx_count: DEFW 0
_esx_result: DEFW 0
xm_rom_error: DEFB 0
xm_active: DEFB 0
xm_idx: DEFB 0
xm_idx_tmp: DEFB 0
xm_mode: DEFB 0
xm_req_mode: DEFB 0
xm_req_create: DEFB 0
xm_pos: DEFW 0
xm_rep_target: DEFW 0
xm_rep_temp: DEFW 0
xm_rep_buf: DEFW 0
xm_rep_len: DEFW 0
xm_open_calls: DEFB 0
xm_read_calls: DEFB 0
xm_write_calls: DEFB 0
xm_close_calls: DEFB 0
xm_unlinks: DEFB 0
xm_renames: DEFB 0
xm_bad_create: DEFB 0
xm_commits: DEFB 0
xm_replace_calls: DEFB 0
xm_bad_paths: DEFB 0
xm_fail_open_call: DEFB XM_FAIL_OPEN_CALL
xm_fail_read_call: DEFB XM_FAIL_READ_CALL
xm_fail_write_call: DEFB XM_FAIL_WRITE_CALL
xm_fail_write_bytes: DEFB XM_FAIL_WRITE_BYTES
xm_fail_close_call: DEFB XM_FAIL_CLOSE_CALL
xm_fail_replace_call: DEFB XM_FAIL_REPLACE_CALL
xm_names:
    DEFM "/CFG/SHATRCV0.RCV"
    DEFB 0
    DEFM "/CFG/SHATRCV1.RCV"
    DEFB 0
    DEFM "/CFG/SHATJRNL.TMP"
    DEFB 0
    DEFM "/CFG/SHATJRNL.NEW"
    DEFB 0
    DEFM "/CFG/SHATRCV.NEW"
    DEFB 0
"""

XFS_PUBLIC = (
    "PUBLIC _esx_fopen, _esx_fcreate, _esx_fopen_update, _esx_fread\n"
    "PUBLIC _esx_fwrite, _esx_fclose, _esx_funlink, _esx_mkdir, _esx_commit\n"
    "PUBLIC _esx_fopen_write_trunc, _esx_frename\n"
    "PUBLIC _esx_fseek_end, _spxn_xfs_fseek, _spxn_rom_error\n"
    "PUBLIC _spxf_replace_atomic\n"
    "PUBLIC _esx_handle, _esx_buf, _esx_count, _esx_result\n"
)


def xfs_model(**failures: int) -> str:
    names = {
        "XM_FAIL_OPEN_CALL": failures.get("fail_open_call", 0),
        "XM_FAIL_READ_CALL": failures.get("fail_read_call", 0),
        "XM_FAIL_WRITE_CALL": failures.get("fail_write_call", 0),
        "XM_FAIL_WRITE_BYTES": failures.get("fail_write_bytes", 0),
        "XM_FAIL_CLOSE_CALL": failures.get("fail_close_call", 0),
        "XM_FAIL_REPLACE_CALL": failures.get("fail_replace_call", 0),
    }
    source = XFS_MODEL
    for marker, value in names.items():
        source = source.replace(marker, str(value))
    return source


def xfs_init(files: dict[str, tuple[bytes, bool]]) -> tuple[str, str]:
    """Return (init code, data) that seed files as (content, committed)."""
    init: list[str] = []
    data: list[str] = []
    for index, label in enumerate(XFS_FILES):
        entry = files.get(label)
        if entry is None:
            continue
        content, flash = entry
        assert len(content) <= XFS_SPAN - 16
        base = XFS_BASE + index * XFS_SPAN
        init += [
            f"    ld a, 1\n    ld ({base}), a",
            f"    ld hl, {len(content)}\n    ld ({base + 1}), hl",
            f"    ld a, {1 if flash else 0}\n    ld ({base + 3}), a",
        ]
        if content:
            init.append(
                f"    ld hl, xfs_initial_{label}\n    ld de, {base + 16}\n"
                f"    ld bc, {len(content)}\n    ldir"
            )
            data.append(
                f"xfs_initial_{label}:\n"
                + "\n".join(
                    "    DEFB " + ",".join(str(v) for v in content[i : i + 16])
                    for i in range(0, len(content), 16)
                )
            )
    clear = (
        f"    ld hl, {XFS_BASE}\n    ld de, {XFS_BASE + 1}\n"
        f"    ld bc, {XFS_SPAN * len(XFS_FILES) - 1}\n    ld (hl), 0\n    ldir\n"
    )
    return clear + "\n".join(init), "\n".join(data)


def xfs_files(ram: bytes) -> dict[str, tuple[bytes, bool] | None]:
    out: dict[str, tuple[bytes, bool] | None] = {}
    for index, label in enumerate(XFS_FILES):
        base = XFS_BASE + index * XFS_SPAN
        if not ram[base]:
            out[label] = None
            continue
        length = int.from_bytes(ram[base + 1 : base + 3], "little")
        out[label] = (bytes(ram[base + 16 : base + 16 + length]), bool(ram[base + 3]))
    return out


def power_cut(files: dict[str, tuple[bytes, bool] | None]) -> dict[str, tuple[bytes, bool]]:
    """Cartridge RAM is cleared on power-off; committed files survive."""
    return {label: entry for label, entry in files.items() if entry and entry[1]}
