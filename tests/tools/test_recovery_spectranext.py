#!/usr/bin/env python3
"""SpectraNext game recovery: product ASM against the XFS adapter model.

Runs the SpectraNext builds of the journal append (SAVELOAD entry 3), the
RECOVERY checkpoint writer and move-list replay, and the CONTROL resume entry
together in the Z80 emulator. The model (tests/tools/xfs_model.py) keeps one handle, reports
errors only through spxn_rom_error() and clears uncommitted files on a power
cut, so the flows below prove what survives power-off.
"""

from __future__ import annotations

import tempfile
from pathlib import Path

from test_recovery import F_BLACK, F_HOST, F_PARTIAL, F_PLAYED, F_UNLINKED, OFFER
from test_recovery import crc16, record
from test_spectrum_asm_vectors import (
    GAME_STATS_FIXTURE,
    ROOT,
    assemble_vector,
    execute_vector,
    journal_body,
    journal_expected_rows,
    required_tool,
    symbol_address,
)
from xfs_model import XFS_PUBLIC, power_cut, xfs_files, xfs_init, xfs_model

SCRATCH = 0x672B
RESULTS = 0xB000
MOVE_LOG = 0x5CB6
REPLAY_CHUNK = 128


def asm_bytes(data: bytes) -> str:
    return "\n".join(
        "    DEFB " + ",".join(str(v) for v in data[i : i + 16])
        for i in range(0, len(data), 16)
    ) or "    DEFB 0"


def snap(seed: int) -> bytes:
    return bytes((seed + i) & 0xFF for i in range(60))


def step_code(index: int, step: tuple, snapshots: dict[bytes, str]) -> str:
    kind = step[0]
    store = (
        f"    ld ({RESULTS + 3 * index}), hl\n"
        f"    ld a, (0x5fe4)\n    ld ({RESULTS + 3 * index + 2}), a\n"
    )
    if kind in ("move", "undo", "apply"):
        value = {"undo": "0", "apply": "1"}.get(kind, f"move_{index}")
        text = f"move_{index}: DEFM \"{step[1]}\"\n    DEFB 0\n" if kind == "move" else ""
        return (
            f"    ld hl, {value}\n    ld (0x5fe0), hl\n"
            "    call _saveload_journal_append_ovl\n" + store
            + (f"    jr step_{index}_end\n{text}step_{index}_end:\n" if text else "")
        )
    if kind == "ckpt":
        label = snapshots.setdefault(step[1], f"snapshot_{len(snapshots)}")
        return (
            f"    ld hl, {label}\n    ld de, write_snapshot\n    ld bc, 60\n    ldir\n"
            "    ld hl, write_snapshot\n    ld (0x5fe2), hl\n"
            f"    ld a, {step[2]}\n    ld (0x5fe6), a\n"
            "    ld a, 0xff\n    ld (0x5fe4), a\n"
            "    call _recovery_write_ovl_entry\n" + store
        )
    if kind in ("probe", "stage"):
        pointer = "0" if kind == "probe" else "load_buffer"
        return (
            f"    ld hl, {pointer}\n    ld (0x5fe2), hl\n"
            "    ld a, 0xff\n    ld (0x5fe4), a\n"
            "    call _recovery_resume_ovl\n" + store
        )
    if kind == "replay":
        return (
            f"    ld hl, {step[1]}\n    ld (0x5fe0), hl\n"
            "    ld a, 0xff\n    ld (0x5fe4), a\n"
            "    call _recovery_replay_ovl_entry\n" + store
        )
    if kind == "seedlog":
        return (
            f"    ld hl, {MOVE_LOG}\n    ld de, {MOVE_LOG + 1}\n"
            "    ld bc, 223\n    ld (hl), 0xee\n    ldir\n"
        )
    if kind == "arm":
        return (
            "    ld hl, _spectrum_journal_pend\n    set 7, (hl)\n"
            "    xor a\n    ld (_spectrum_journal_warning), a\n"
        )
    if kind == "poke":
        return f"    ld a, {step[2]}\n    ld ({step[1]}), a\n"
    raise AssertionError(step)


def run(tmp: Path, name: str, steps: list[tuple], *,
        files: dict[str, tuple[bytes, bool]] | None = None,
        pend: int = 0, fd: int = 0, local_black: int = 0, role: int = 0,
        **failures: int) -> dict[str, object]:
    snapshots: dict[bytes, str] = {}
    body = "\n".join(step_code(i, step, snapshots) for i, step in enumerate(steps))
    init, data = xfs_init(files or {})
    snapshot_data = "\n".join(
        f"{label}:\n{asm_bytes(content)}" for content, label in snapshots.items()
    )
    source = f"""
SECTION code_user
EXTERN _recovery_write_ovl_entry, _recovery_resume_ovl
EXTERN _recovery_replay_ovl_entry
EXTERN _saveload_journal_append_ovl
PUBLIC _spectrum_journal_pend, _spectrum_journal_fd, _spectrum_journal_warning
PUBLIC _spectrum_journal_count, _spectrum_journal_crc
PUBLIC _netchesszx_local_color, _netchesszx_session_role
PUBLIC _spectrum_net_background_drain, _recovery_capture_ovl, _esx_fsync
PUBLIC _recovery_retire_ovl
PUBLIC _move_line_count, _last_ply_seen
PUBLIC _spectrum_gui_side_panels_visible, _spectrum_render_moves
{XFS_PUBLIC}
test_start:
    ld sp, 0xff00
{init}
    ld a, 0xa5
    ld ({SCRATCH - 1}), a
    ld ({SCRATCH + 160}), a
    ld hl, load_buffer
    ld de, load_buffer + 1
    ld bc, 59
    ld (hl), 0xa5
    ldir
    ld a, {pend}
    ld (_spectrum_journal_pend), a
    ld a, {fd}
    ld (_spectrum_journal_fd), a
    ld a, {local_black}
    ld (_netchesszx_local_color), a
    ld a, {role}
    ld (_netchesszx_session_role), a
    ld iy, 0x5c3a
{body}
    ld (test_iy), iy
    xor a
    ld (test_result), a
test_done:
    jp test_done

_recovery_capture_ovl:
_recovery_retire_ovl:
    ld hl, 0
    ret
_esx_fsync:
    scf                         ; XFS builds must never reach F_SYNC
    ret
_spectrum_net_background_drain:
    ld hl, (drains)
    inc hl
    ld (drains), hl
    ld bc, 0xbcbc                ; the drain owns BC/DE/HL
    ld de, 0xdede
    ld hl, 0xabab
    ret
_spectrum_gui_side_panels_visible:
    ld l, 1
    ret
_spectrum_render_moves:
    ld (render_arg), hl
    ld hl, renders
    inc (hl)
    ret

test_result: DEFB 255
test_iy: DEFW 0
drains: DEFW 0
renders: DEFB 0
render_arg: DEFW 0
_move_line_count: DEFB 0
_last_ply_seen: DEFW 0
_spectrum_journal_pend: DEFB 0
_spectrum_journal_fd: DEFB 0
_spectrum_journal_warning: DEFB 0
_spectrum_journal_count: DEFW 0
_spectrum_journal_crc: DEFW 0xffff
_netchesszx_local_color: DEFB 0
_netchesszx_session_role: DEFB 0
write_snapshot: DEFS 60
load_buffer: DEFS 60
{snapshot_data}
{data}
""" + xfs_model(**failures)
    harness = tmp / f"spxn_recovery_{name}.asm"
    harness.write_text(source + GAME_STATS_FIXTURE, encoding="ascii")
    binary = harness.with_suffix(".bin")
    symbols = assemble_vector(
        required_tool("z80asm"),
        harness,
        [
            ROOT / "asm/overlay/saveload/journal_append.asm",
            ROOT / "asm/overlay/saveload/game_stats.asm",
            ROOT / "asm/overlay/recovery/entry_recovery.asm",
            ROOT / "asm/overlay/recovery/rcv_store.asm",
            ROOT / "asm/overlay/recovery/rcv_resume.asm",
            ROOT / "asm/overlay/recovery/rcv_replay.asm",
        ],
        binary,
        defines=[
            "NETCHESSZX_SPECTRANEXT",
            "NETCHESSZX_JOURNAL_HOOK",
            "NETCHESSZX_RECOVERY",
        ],
    )
    result, output, pc, _, _ = execute_vector(
        required_tool("z88dk-ticks"), binary, symbols, counter=8_000_000
    )
    assert result == 0 and pc == symbol_address(symbols, "test_done"), (name, output)
    ram = binary.with_suffix(".ram").read_bytes()

    def sym(label: str, size: int = 1) -> int:
        address = symbol_address(symbols, label)
        return int.from_bytes(ram[address : address + size], "little")

    load = symbol_address(symbols, "load_buffer")
    state: dict[str, object] = {
        "steps": [
            (int.from_bytes(ram[RESULTS + 3 * i : RESULTS + 3 * i + 2], "little"),
             ram[RESULTS + 3 * i + 2])
            for i in range(len(steps))
        ],
        "pend": sym("_spectrum_journal_pend"),
        "fd": sym("_spectrum_journal_fd"),
        "warning": sym("_spectrum_journal_warning"),
        "count": sym("_spectrum_journal_count", 2),
        "crc": sym("_spectrum_journal_crc", 2),
        "load": bytes(ram[load : load + 60]),
        "commits": sym("xm_commits"),
        "replaces": sym("xm_replace_calls"),
        "files": xfs_files(ram),
        "opens": sym("xm_open_calls"),
        "reads": sym("xm_read_calls"),
        "active": sym("xm_active"),
        "drains": sym("drains", 2),
        "renders": sym("renders"),
        "render_arg": sym("render_arg", 2),
        "move_log": bytes(ram[MOVE_LOG : MOVE_LOG + 224]),
        "move_rows": sym("_move_line_count"),
        "last_ply": sym("_last_ply_seen", 2),
    }
    assert sym("xm_bad_paths") == 0, (name, "unstaged or unknown XFS path")
    assert sym("xm_bad_create") == 0, (name, "O_CREAT on an existing flash path")
    assert (ram[SCRATCH - 1], ram[SCRATCH + 160]) == (0xA5, 0xA5), (name, "scratch overrun")
    assert sym("test_iy", 2) == 0x5C3A, (name, "IY clobbered")
    return state


def content(state: dict[str, object], label: str) -> bytes | None:
    entry = state["files"][label]
    return entry[0] if entry else None


def link(moves: list[str | None]) -> tuple[int, int]:
    data = journal_body(moves)
    return len(data) // 2, crc16(data)


def check_game_and_power_cut(tmp: Path) -> None:
    s0, s1, s2, s3 = snap(0), snap(40), snap(80), snap(120)
    moves = ["e2e4", "e7e5", None]
    steps = [
        ("ckpt", s0, 0),                # GAME START: ply 0, switch armed
        ("move", "e2e4"), ("ckpt", s1, F_PLAYED),
        ("move", "e7e5"), ("ckpt", s2, F_PLAYED),
        ("undo",), ("ckpt", s3, F_PLAYED),
    ]
    s = run(tmp, "game", steps, pend=0x80)
    assert all(hl == 1 for hl, _ in s["steps"]), s["steps"]
    count, crc = link(moves)
    assert (s["count"], s["crc"], s["fd"]) == (count, crc, 1), s
    tmp_entry = s["files"]["tmp"]
    assert tmp_entry == (journal_body(moves), True), tmp_entry   # in flash
    assert s["commits"] == 1 and s["replaces"] == 4, s
    assert s["files"]["slot0"] == (
        record(3, s2, *link(["e2e4", "e7e5"]), OFFER), True
    ), s["files"]["slot0"]
    assert s["files"]["slot1"] == (record(4, s3, count, crc, OFFER), True), s
    first = run(tmp, "game_start_only", [("ckpt", s0, 0)], pend=0x80)
    assert first["files"]["slot0"] == (record(1, s0, 0, 0xFFFF, F_HOST | F_UNLINKED), True)

    # Power-off clears cartridge RAM. Everything the resume needs is in flash.
    survived = power_cut(s["files"])
    assert set(survived) == {"slot0", "slot1", "tmp"}, survived
    r = run(tmp, "resume", [
        ("probe",), ("stage",), ("arm",), ("apply",),
        ("ckpt", s3, F_PLAYED), ("move", "d7d6"),
    ], files=survived)
    (probe_hl, probe_status), (stage_hl, stage_status) = r["steps"][:2]
    assert (probe_hl, probe_status, stage_hl, stage_status) == (1, 0, 1, 0), r
    assert r["load"] == s3, r["load"].hex()
    after = moves + ["d7d6"]
    assert content(r, "tmp") == journal_body(after), content(r, "tmp")
    assert content(r, "new") is None and (r["count"], r["crc"]) == link(after), r
    assert r["files"]["slot0"] == (record(5, s3, count, crc, OFFER), True), r

    # A journal that never reached flash degrades to RESTORED, never to a
    # wrong history: its checkpoint prefix no longer verifies.
    ram_tmp = dict(survived)
    ram_tmp.pop("tmp")
    r = run(tmp, "resume_without_tmp", [("stage",)], files=ram_tmp)
    assert (r["steps"][0], r["load"], r["pend"]) == ((1, 0), s3, 0x40), r
    assert content(r, "new") is None, r


def check_writer_faults(tmp: Path) -> None:
    old, new = snap(1), snap(2)
    r10 = (record(10, old), True)
    for label, failure, status in (
        ("replace", {"fail_replace_call": 1}, 3),
        ("scan_open", {"fail_open_call": 1}, 2),
        ("scan_read", {"fail_read_call": 1}, 2),
        ("readback_open", {"fail_open_call": 3}, 3),
        ("readback_read", {"fail_read_call": 2}, 3),
    ):
        s = run(tmp, "write_" + label, [("ckpt", new, F_PLAYED)],
                files={"slot0": r10}, **failure)
        assert s["steps"][0] == (0, status), (label, s["steps"])
        assert s["files"]["slot0"] == r10, (label, s)
        if status == 2:
            assert s["replaces"] == 0, (label, s)
    # A missing slot is absence (ENOENT), not doubt.
    s = run(tmp, "write_fresh", [("ckpt", new, F_PLAYED)])
    assert s["steps"][0] == (1, 0) and s["files"]["slot1"] is None, s

    # A cut between the helper's CHMOD and RENAME leaves a committed temp that
    # the helper can never replace; the writer rewrites it in place and
    # renames it, so checkpoints keep working.
    s = run(tmp, "write_committed_leftover", [("ckpt", new, F_PLAYED)],
            files={"slot0": r10, "rcvtemp": (b"stale", True)})
    assert s["steps"][0] == (1, 0), s["steps"]
    assert s["files"]["slot1"] == (record(11, new, 0, 0xFFFF, OFFER), True), s
    assert s["files"]["slot0"] == r10 and s["files"]["rcvtemp"] is None, s
    # Any other replace failure is reported, never papered over.
    s = run(tmp, "write_ram_leftover", [("ckpt", new, F_PLAYED)],
            files={"slot0": r10}, fail_replace_call=1)
    assert s["steps"][0] == (0, 3) and s["files"]["slot1"] is None, s


def check_resume_rules(tmp: Path) -> None:
    old, new = snap(3), snap(4)
    b3 = journal_body(["e2e4", "e7e5", "g1f3"])
    linked = (record(11, new, 3, crc16(b3), OFFER), True)
    base = {"slot0": (record(10, old), True), "slot1": linked, "tmp": (b3, True)}
    s = run(tmp, "probe_none", [("probe",)])
    assert s["steps"][0] == (0, 1), s
    s = run(tmp, "probe_io", [("probe",)], files=base, fail_open_call=1)
    assert s["steps"][0] == (0, 2), s
    for label, flags, black in (
        ("guest", F_PLAYED, 0), ("ply_zero", F_HOST, 0),
        ("colour", OFFER | F_BLACK, 0),
    ):
        files = dict(base, slot1=(record(11, new, flags=flags), True))
        s = run(tmp, "probe_" + label, [("probe",)], files=files, local_black=black)
        assert s["steps"][0] == (0, 1), (label, s)
    s = run(tmp, "stage_partial", [("stage",)], files=dict(
        base, slot1=(record(11, new, 3, crc16(b3), OFFER | F_PARTIAL), True)))
    assert (content(s, "new"), s["pend"]) == (b3, 0x41), s

    long = journal_body(["a2a3", "a7a6"] * 75)      # 300 bytes: 128+128+44
    s = run(tmp, "stage_long", [("stage",)], files={
        "slot1": (record(11, new, 150, crc16(long)), True), "tmp": (long, True)})
    assert (content(s, "new"), s["pend"]) == (long, 0x01), s

    for label, files, failure in (
        ("crc", dict(base, slot1=(record(11, new, 3, crc16(b3) ^ 1), True)), {}),
        ("short", dict(base, tmp=(b3[:4], True)), {}),
        ("unlinked", dict(base, slot1=(record(11, new, 3, crc16(b3),
                                               OFFER | F_UNLINKED), True)), {}),
        # Opens: slot0, slot1, NEW create, then TMP (read), NEW (update).
        ("create_new", base, {"fail_open_call": 3}),
        ("tmp_open", base, {"fail_open_call": 4}),
        ("new_update", base, {"fail_open_call": 5}),
        ("tmp_read", base, {"fail_read_call": 3}),
        ("new_write", base, {"fail_write_call": 1, "fail_write_bytes": 2}),
    ):
        s = run(tmp, "stage_" + label, [("stage",)], files=files, **failure)
        assert (s["steps"][0], s["load"], s["pend"]) == ((1, 0), new, 0x40), (label, s)
        assert content(s, "new") is None and content(s, "tmp") == content(
            {"files": {"tmp": files["tmp"]}}, "tmp"), (label, s)


def check_apply_consume(tmp: Path) -> None:
    # A remote RESTORE (0xC0) creates an empty partial TMP at apply time.
    s = run(tmp, "apply_remote", [("apply",), ("move", "e7e5")], pend=0xC0)
    assert s["steps"][0][0] == 1 and s["fd"] == 0x41, s
    assert content(s, "tmp") == journal_body(["e7e5"]) and s["commits"] == 1, s
    assert (s["count"], s["crc"]) == link(["e7e5"]), s
    # Nothing armed: the apply call is a no-op.
    s = run(tmp, "apply_idle", [("apply",)], fd=1,
            files={"tmp": (journal_body(["e2e4"]), True)})
    assert s["steps"][0][0] == 1 and s["fd"] == 1 and s["commits"] == 0, s
    # A failed create at apply time degrades to partial with a warning.
    s = run(tmp, "apply_create_fail", [("apply",)], pend=0xC0, fail_open_call=2)
    assert (s["steps"][0][0], s["fd"], s["pend"], s["warning"]) == (0, 0x40, 0, 1), s
    # An existing committed TMP is truncated in place and not recommitted.
    s = run(tmp, "apply_reuse_flash", [("apply",)], pend=0xC0,
            files={"tmp": (journal_body(["e2e4", "e7e5"]), True)})
    assert s["files"]["tmp"] == (b"", True) and s["commits"] == 0, s
    # A TMP left in cartridge RAM (its first commit failed) reaches flash now.
    s = run(tmp, "apply_reuse_ram", [("apply",)], pend=0xC0,
            files={"tmp": (journal_body(["e2e4"]), False)})
    assert s["files"]["tmp"] == (b"", True) and s["commits"] == 1, s


def restored(marker: int) -> bytes:
    log = bytearray(224)
    offset = 0 if marker == 3 else 18
    log[offset:offset + 9] = b"RESTORED\0"
    return bytes(log)


def check_replay(tmp: Path) -> None:
    """RECOVERY entry 2 paints SHATJRNL.NEW like Classic/Next SAVELOAD."""
    # Cold resume end to end: stage NEW from the flash TMP, paint it, adopt it
    # at apply time, then keep appending.
    new = snap(5)
    b3 = journal_body(["e2e4", "e7e5", "g1f3"])
    files = {"slot1": (record(11, new, 3, crc16(b3), OFFER), True), "tmp": (b3, True)}
    s = run(tmp, "replay_resume", [
        ("probe",), ("stage",), ("arm",), ("seedlog",), ("replay", 3),
        ("apply",), ("move", "b8c6"),
    ], files=files)
    assert s["steps"][4] == (1, 0), s["steps"]
    assert s["move_log"] == journal_expected_rows(["e2e4", "e7e5", "g1f3"]), s
    assert (s["last_ply"], s["move_rows"], s["renders"]) == (3, 2, 1), s
    assert s["render_arg"] == MOVE_LOG, s
    after = ["e2e4", "e7e5", "g1f3", "b8c6"]
    assert content(s, "tmp") == journal_body(after) and content(s, "new") is None, s
    assert (s["count"], s["crc"]) == link(after), s

    # Chunk boundaries (128 bytes = 64 records) and the seven-row window. Each
    # chunk is its own OPEN/SEEK/READ/CLOSE; nothing is left open.
    cycle = ["a2a3", "a7a6", "b2b3", "b7b6", "e7e8q", "e2e1n", "h2h3", "h7h6"]
    for count in (0, 1, 14, 15, 63, 64, 65, 150, 240):
        moves = (cycle * 30)[:count]
        body = journal_body(moves)
        s = run(tmp, f"replay_{count}", [("seedlog",), ("replay", 1)],
                files={"new": (body, True)}, pend=0x01)
        assert s["steps"][1] == (1, 0), (count, s["steps"])
        assert s["move_log"] == journal_expected_rows(moves), count
        assert (s["last_ply"], s["move_rows"]) == (count, min(7, (count + 1) // 2)), count
        reads = 2 * ((len(body) + REPLAY_CHUNK - 1) // REPLAY_CHUNK + 1)
        assert (s["reads"], s["opens"], s["active"]) == (reads, reads, 0), (count, s)
        assert s["drains"] == 2 * count + 3, (count, s["drains"])
        assert (s["pend"], s["fd"], s["files"]["new"]) == (0x01, 0, (body, True)), count

    # UNDO records across the chunk seam remove rows before they are shown.
    moves = (cycle * 30)[:63] + [None, "a7a8r", "b2b1b"]
    shown = (cycle * 30)[:62] + ["a7a8r", "b2b1b"]
    s = run(tmp, "replay_undo_seam", [("replay", 1)],
            files={"new": (journal_body(moves), True)}, pend=0x01)
    assert s["move_log"] == journal_expected_rows(shown), s["move_log"]
    assert s["last_ply"] == len(shown), s

    # No trustworthy body: the RESTORED marker in the last mover's column.
    body = journal_body((cycle * 30)[:150])            # 300 bytes: 3 chunks
    for marker in (1, 3):
        for label, pend, files, failure in (
            ("partial", 0x41, {"new": (body, True)}, {}),
            ("no_body", 0x00, {"new": (body, True)}, {}),
            ("missing", 0x01, {}, {}),
            ("open", 0x01, {"new": (body, True)}, {"fail_open_call": 2}),
            ("read", 0x01, {"new": (body, True)}, {"fail_read_call": 2}),
            ("second_pass", 0x01, {"new": (body, True)}, {"fail_read_call": 5}),
            ("close", 0x01, {"new": (body, True)}, {"fail_close_call": 3}),
            ("torn", 0x01, {"new": (body + b"\x01", True)}, {}),
        ):
            s = run(tmp, f"replay_{label}_{marker}", [("seedlog",), ("replay", marker)],
                    files=files, pend=pend, **failure)
            assert s["steps"][1] == (1, 0), (label, marker, s["steps"])
            assert s["move_log"] == restored(marker), (label, marker, s["move_log"])
            assert (s["last_ply"], s["move_rows"]) == (2 if marker == 1 else 1, 1), label
            assert s["active"] == 0 and s["pend"] == pend, (label, s)
            assert s["files"]["new"] == files.get("new"), (label, s)
            if pend != 0x01:
                assert s["opens"] == 0, (label, s)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="recovery_spxn_") as directory:
        tmp = Path(directory)
        check_game_and_power_cut(tmp)
        check_writer_faults(tmp)
        check_resume_rules(tmp)
        check_apply_consume(tmp)
        check_replay(tmp)
    print("SpectraNext recovery ASM vectors passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
