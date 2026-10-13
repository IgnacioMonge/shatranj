#!/usr/bin/env python3
"""Execute native SAVELOAD ERASE with the production STJ path codec."""

from __future__ import annotations

import tempfile
from pathlib import Path

from test_spectrum_asm_vectors import (
    ROOT,
    assemble_vector,
    execute_vector,
    required_tool,
    symbol_address,
)


# Link the complete codec, so ERASE cannot drift from LOAD/SAVE's parser or
# XFS staging. Any unrelated codec I/O reached by ERASE fails the test.
UNEXPECTED_CALLS = (
    "_esx_fopen", "_esx_fcreate", "_esx_fcreate_new", "_esx_fopen_update",
    "_esx_commit", "_esx_fseek_end", "_spxn_xfs_fseek", "_stj_spxn_adopt_new",
    "_esx_fsync", "_esx_fseek_set", "_esx_fread", "_esx_fwrite",
    "_esx_fclose", "_esx_mkdir", "_spxn_rom_error",
    "_spectrum_net_background_drain", "_spectrum_net_runtime_fat_date",
    "_spectrum_net_runtime_fat_time", "_spectrum_net_runtime_fat_day_delta",
)


def cases() -> list[tuple[str | None, bool, int]]:
    valid = ("A", "01", "_", "aZ09_", "abcdefgh", "AZ09_z1_")
    invalid = (
        None, "", "ABCDEFGHI", "a/b", "A.B", "a b", "a-b", ":",
        "[", "`", "{", "\x7f",
    )
    return ([(name, True, result) for name in valid for result in (0, 1, 256)]
            + [(name, False, 1) for name in invalid])


def run_target(temp: Path, target: str) -> int:
    scenarios = cases()
    lines = [
        "SECTION code_user",
        "PUBLIC test_start, test_done, test_result",
        "EXTERN _saveload_erase_nczs_ovl",
        "test_start: ld sp,0xff00",
    ]
    data = ["test_result: DEFB 255"]
    for index, (name, _, io_result) in enumerate(scenarios):
        prefix = f"case_{index}"
        if name is not None:
            data.append(f"{prefix}_name: DEFB " +
                        ",".join(str(b) for b in name.encode("ascii") + b"\0"))
        lines.extend([
            f" ld hl,{prefix}_name" if name is not None else " ld hl,0",
            " ld (0x5fe0),hl",
            " ld a,0xff\n ld (0x5fe4),a",
            " xor a\n ld (io_calls),a\n ld (io_kind),a\n ld (unexpected_calls),a",
            " ld hl,0\n ld (io_source),hl\n ld (_esx_buf),hl",
            f" ld hl,{io_result}\n ld (io_result),hl",
            f" ld hl,{prefix}_source\n ld (source_copy),hl",
            f" ld hl,{prefix}_destination\n ld (destination_copy),hl",
            " ld ix,0x1234\n ld iy,0x5c3a",
            " call _saveload_erase_nczs_ovl",
            f" ld ({prefix}_result),hl\n ld ({prefix}_ix),ix\n"
            f" ld ({prefix}_iy),iy\n ld ({prefix}_sp),sp",
            f" ld a,(0x5fe4)\n ld ({prefix}_error),a",
            f" ld a,(io_calls)\n ld ({prefix}_calls),a",
            f" ld a,(io_kind)\n ld ({prefix}_kind),a",
            f" ld a,(unexpected_calls)\n ld ({prefix}_unexpected),a",
            # Preserve a post-call copy too: the rename source must not be
            # changed into the tombstone when its staged destination changes.
            " ld hl,(io_source)\n ld a,h\n or l",
            f" jr z,{prefix}_done\n ld de,{prefix}_after",
            " call copy_path",
            f"{prefix}_done:",
        ])
        data.extend([
            f"{prefix}_result: DEFW 0\n{prefix}_error: DEFB 0",
            f"{prefix}_calls: DEFB 0\n{prefix}_kind: DEFB 0\n{prefix}_unexpected: DEFB 0",
            f"{prefix}_ix: DEFW 0\n{prefix}_iy: DEFW 0\n{prefix}_sp: DEFW 0",
            f"{prefix}_source: DEFS 32\n{prefix}_destination: DEFS 32",
            f"{prefix}_after: DEFS 32",
        ])
    lines.append(" xor a\n ld (test_result),a\n jp test_done\ntest_done: halt")
    lines.extend([
        "PUBLIC _esx_funlink, _esx_frename, _esx_result, _esx_buf",
        "_esx_frename:\n ld a,2\n ld (io_kind),a\n"
        " ld (io_source),hl\n ld de,(source_copy)\n call copy_path",
        " ld hl,(_esx_buf)\n ld de,(destination_copy)\n call copy_path\n jr io_done",
        "_esx_funlink:\n ld a,1\n ld (io_kind),a\n"
        " ld (io_source),hl\n ld de,(source_copy)\n call copy_path",
        "io_done:\n ld a,(io_calls)\n inc a\n ld (io_calls),a",
        " ld hl,(io_result)\n ld (_esx_result),hl\n ret",
        "copy_path:\n ld a,(hl)\n ld (de),a\n inc hl\n inc de\n or a",
        " jr nz,copy_path\n ret",
        "PUBLIC " + ",".join(UNEXPECTED_CALLS),
        "\n".join(f"{symbol}:" for symbol in UNEXPECTED_CALLS),
        " ld a,1\n ld (unexpected_calls),a\n ld hl,0\n ret",
    ])
    data.extend([
        "PUBLIC ja_io_buf, _esx_handle, _esx_count, _spxn_regs",
        "PUBLIC _spectrum_journal_fd, _spectrum_journal_pend",
        "_esx_result: DEFW 0\n_esx_buf: DEFW 0\n_esx_count: DEFW 0",
        "_esx_handle: DEFB 0\n_spxn_regs: DEFS 16",
        "_spectrum_journal_fd: DEFB 0\n_spectrum_journal_pend: DEFB 0",
        "io_calls: DEFB 0\nio_kind: DEFB 0\nunexpected_calls: DEFB 0\nio_result: DEFW 0",
        "io_source: DEFW 0\nsource_copy: DEFW 0\ndestination_copy: DEFW 0",
        "ja_io_buf: DEFS 160",
    ])
    harness = temp / f"erase_{target}.asm"
    harness.write_text("\n".join(lines + data) + "\n", encoding="ascii")
    binary = harness.with_suffix(".bin")
    sources = ([ROOT / "asm/overlay/saveload/erase_classic.asm"]
               if target == "classic" else [
                   ROOT / "asm/overlay/journal/stj_codec.asm",
                   ROOT / "asm/overlay/journal/stj_saveload.asm",
               ])
    defines = ({"classic": [],
                "next": ["NETCHESSZX_NEXT", "NETCHESSZX_NEXT_BANKING"],
                "spectranext": ["NETCHESSZX_SPECTRANEXT"]}[target])
    mapping = assemble_vector(required_tool("z80asm"), harness, sources,
                              binary, defines=defines)
    result, output, pc, _, _ = execute_vector(
        required_tool("z88dk-ticks"), binary, mapping, counter=1_000_000,
        cpu="z80n" if target == "next" else "z80",
    )
    assert result == 0 and pc == symbol_address(mapping, "test_done"), output
    memory = binary.with_suffix(".ram").read_bytes()

    def value(label: str, length: int = 1) -> int:
        address = symbol_address(mapping, label)
        return int.from_bytes(memory[address:address + length], "little")

    def path(label: str) -> bytes:
        address = symbol_address(mapping, label)
        return memory[address:address + 32].split(b"\0")[0]

    for index, (name, valid, io_result) in enumerate(scenarios):
        prefix = f"case_{index}"
        label = (target, name, io_result)
        success = valid and io_result != 0
        assert value(prefix + "_result", 2) == int(success), label
        assert value(prefix + "_error") == (0 if success else 2 if valid else 1), label
        assert value(prefix + "_calls") == int(valid), label
        assert value(prefix + "_kind") == (2 if target == "spectranext" else 1) * int(valid), label
        assert value(prefix + "_unexpected") == 0, label
        assert value(prefix + "_ix", 2) == 0x1234, label
        assert value(prefix + "_iy", 2) == 0x5C3A, label
        assert value(prefix + "_sp", 2) == 0xFF00, label
        if valid:
            directory = "/CFG/" if target == "spectranext" else "/SYS/CONFIG/"
            expected = (directory + name.upper() + ".STJ").encode("ascii")
            assert path(prefix + "_source") == expected, label
            assert path(prefix + "_after") == expected, label
            destination = (expected[:-3] + b"DEL" if target == "spectranext" else b"")
            assert path(prefix + "_destination") == destination, label
        else:
            assert path(prefix + "_source") == b"", label
            assert path(prefix + "_destination") == b"", label
    print(f"Native ERASE {target}: {len(scenarios)} path/I/O/register cases passed")
    return len(scenarios)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="game-stats-erase-") as directory:
        count = sum(run_target(Path(directory), target)
                    for target in ("classic", "next", "spectranext"))
    print(f"Native ERASE vectors: {count} cases passed")


if __name__ == "__main__":
    main()
